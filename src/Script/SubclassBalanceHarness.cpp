/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The sub-class balance harness (solo-wow): two bots of a path's class, one plain and one on the path, at the same
// level with the same talents and the same gear, each fight a training dummy of their level for a set time while
// everything they and their pets deal is metered. The comparison and its verdict follow SubclassBalancePolicy.h; the
// rows go to the GM's chat, the server log and <LogsDir>/balance-<path>.md, to be copied into the path's folder.
//
//   .balance start <path id> <level> <plain spec no> <path spec no> [fights 3] [seconds 180]
//   .balance status
//   .balance stop
//
// Spec numbers are the class's premade specs in playerbots.conf (Playerbots.PremadeSpecName.<class>.<no>).

#include <algorithm>
#include <cmath>
#include <fstream>

#include "Chat.h"
#include "Config.h"
#include "DBCEnums.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "SubclassBalancePolicy.h"
#include "SubclassPaths.h"
#include "TemporarySummon.h"

using namespace Acore::ChatCommands;

namespace
{
constexpr uint32 DummyEntry = 31144;      // Grandmaster's Training Dummy: takes no damage, never dies
constexpr uint32 LoginTimeoutMs = 60000;
constexpr uint32 PrepareMs = 20000;       // buffs, pets and the path's minions
constexpr uint32 RestMs = 15000;
constexpr uint32 ReattackMs = 3000;       // a dummy drops a player 5 seconds after their last direct hit
constexpr float Apart = 40.0f;            // between the two bots' dummies

enum class Phase : uint8 { Idle, Login, Prepare, Fight, Rest };

struct Bot
{
    ObjectGuid guid;
    ObjectGuid dummy;
    SubclassBalance::Fight fight;
    std::vector<SubclassBalance::Fight> fights;
};

struct Run
{
    Phase phase = Phase::Idle;
    ObjectGuid gm;
    uint16 pathId = 0;
    std::string pathName;
    uint8 classId = 0;
    uint8 level = 0;
    int plainSpec = 0, pathSpec = 0;
    uint32 fightsWanted = 3;
    uint32 fightMs = 180000;
    uint32 elapsed = 0, sinceAttack = 0;
    Bot sides[2];  // 0 the plain class, 1 the path
    std::vector<ObjectGuid> before;  // the GM's bots before the run, so the two it adds are known
};

Run run;

Player* Gm() { return ObjectAccessor::FindConnectedPlayer(run.gm); }

Player* BotOf(int side)
{
    Player* bot = ObjectAccessor::FindConnectedPlayer(run.sides[side].guid);
    return bot && bot->IsInWorld() && GET_PLAYERBOT_AI(bot) ? bot : nullptr;
}

void Say(std::string const& text)
{
    LOG_INFO("playerbots", "Balance: {}", text);
    if (Player* gm = Gm())
        ChatHandler(gm->GetSession()).SendSysMessage("Balance: " + text);
}

void Stop(std::string const& why)
{
    if (Player* gm = Gm())
    {
        for (Bot const& side : run.sides)
            if (Creature* dummy = ObjectAccessor::GetCreature(*gm, side.dummy))
                dummy->DespawnOrUnsummon();
        if (PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(gm))
            for (Bot const& side : run.sides)
                if (!side.guid.IsEmpty())
                    mgr->LogoutPlayerBot(side.guid);
    }
    Say(why);
    run = Run{};
}

std::string SpecName(uint8 classId, int specNo)
{
    return specNo >= 0 && specNo < MAX_SPECNO ? sPlayerbotAIConfig.PremadeSpecName[classId][specNo] : "";
}

// Both bots at the run's level and spec, the path bot on its path, and the plain bot's gear cloned onto the path bot
void Prepare(Player* plain, Player* path)
{
    for (Player* bot : {plain, path})
    {
        PlayerbotFactory factory(bot, run.level);
        factory.Randomize(false);
    }
    if (std::string const why = SubclassPaths::ChangeActivePath(path, run.pathId, true); !why.empty())
    {
        Stop("the path bot couldn't take the path: " + why);
        return;
    }
    for (auto [bot, spec] : {std::pair{plain, run.plainSpec}, std::pair{path, run.pathSpec}})
    {
        PlayerbotFactory::InitTalentsBySpecNo(bot, spec, true);
        PlayerbotFactory(bot, bot->GetLevel()).InitGlyphs(false);
    }
    PlayerbotFactory::AutoGear(plain, ITEM_QUALITY_RARE, 0, false);
    PlayerbotFactory::DestroyEquippedGear(path);
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* worn = plain->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!worn)
            continue;
        Item* copy = worn->CloneItem(1, path);
        uint16 dest = 0;
        if (copy && path->CanEquipItem(slot, dest, copy, false) == EQUIP_ERR_OK)
            path->EquipItem(dest, copy, true);
        else
            delete copy;
    }
    for (Player* bot : {plain, path})
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        ai->ResetStrategies();
        ai->ChangeStrategy("-follow,+stay", BOT_STATE_NON_COMBAT);
        bot->SetFullHealth();
        bot->SetPower(bot->getPowerType(), bot->GetMaxPower(bot->getPowerType()));
    }
}

// Each bot to its own spot beside the GM, facing a fresh dummy of its level
void Lineup(Player* gm)
{
    for (int side = 0; side < 2; ++side)
    {
        Player* bot = BotOf(side);
        if (!bot)
            continue;
        float const angle = gm->GetOrientation() + (side ? -1.0f : 1.0f) * float(M_PI) / 2;
        float const x = gm->GetPositionX() + std::cos(angle) * Apart / 2;
        float const y = gm->GetPositionY() + std::sin(angle) * Apart / 2;
        float const z = gm->GetPositionZ();
        bot->TeleportTo(gm->GetMapId(), x, y, z, gm->GetOrientation());
        float const dx = x + std::cos(gm->GetOrientation()) * 15.0f, dy = y + std::sin(gm->GetOrientation()) * 15.0f;
        if (TempSummon* dummy = gm->SummonCreature(DummyEntry, dx, dy, z, gm->GetOrientation() + float(M_PI),
                                                   TEMPSUMMON_TIMED_DESPAWN, run.fightMs + RestMs + 60000))
        {
            dummy->SetLevel(run.level);
            dummy->SetFullHealth();
            run.sides[side].dummy = dummy->GetGUID();
        }
    }
}

// As the bots' own attack action does it: the dummy their current target, the combat engine on
void Attack(Player* bot, Creature* dummy)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    bot->SetSelection(dummy->GetGUID());
    ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Set(dummy);
    ai->ChangeEngine(BOT_STATE_COMBAT);
    if (bot->GetVictim() != dummy)
        bot->Attack(dummy, !PlayerbotAI::IsRanged(bot));
}

void EndFight()
{
    for (int side = 0; side < 2; ++side)
    {
        Bot& b = run.sides[side];
        b.fight.ms = run.fightMs;
        b.fights.push_back(b.fight);
        b.fight = {};
        if (Player* bot = BotOf(side))
        {
            bot->AttackStop();
            bot->CombatStop(true);
            GET_PLAYERBOT_AI(bot)->ChangeEngine(BOT_STATE_NON_COMBAT);
            bot->RemoveAllSpellCooldown();
            bot->SetFullHealth();
            bot->SetPower(bot->getPowerType(), bot->GetMaxPower(bot->getPowerType()));
        }
        if (Player* gm = Gm())
            if (Creature* dummy = ObjectAccessor::GetCreature(*gm, b.dummy))
                dummy->DespawnOrUnsummon();
        b.dummy.Clear();
    }
}

void Report()
{
    using namespace SubclassBalance;
    Row const row{Acore::Time::TimeToTimestampStr(GameTime::GetGameTime()).substr(0, 10), run.level,
                  SpecName(run.classId, run.plainSpec) + " / " + SpecName(run.classId, run.pathSpec), Scenario::Dummy};
    Comparison const c = Compare(run.sides[0].fights, run.sides[1].fights);
    std::string const line = TableRow(row, c);
    Say(run.pathName + ": " + line);
    std::string const file = sConfigMgr->GetOption<std::string>("LogsDir", ".") + "/balance-" + run.pathName + ".md";
    bool const fresh = !std::ifstream(file).good();
    std::ofstream out(file, std::ios::app);
    if (fresh)
        out << "# " << run.pathName << " balance\n\n" << TableHeader();
    out << line;
    Say("written to " + file);
}

void Tick(uint32 diff)
{
    if (run.phase == Phase::Idle)
        return;
    Player* gm = Gm();
    if (!gm)
    {
        Stop("the GM left; stopped");
        return;
    }
    run.elapsed += diff;
    switch (run.phase)
    {
        case Phase::Login:
        {
            PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(gm);
            int found = 0;
            for (auto itr = mgr->GetPlayerBotsBegin(); itr != mgr->GetPlayerBotsEnd() && found < 2; ++itr)
            {
                Player* bot = itr->second;
                if (bot->getClass() != run.classId ||
                    std::find(run.before.begin(), run.before.end(), bot->GetGUID()) != run.before.end())
                    continue;
                if (!GET_PLAYERBOT_AI(bot) || !bot->IsInWorld())
                    continue;
                run.sides[found++].guid = bot->GetGUID();
            }
            if (found == 2)
            {
                Prepare(BotOf(0), BotOf(1));
                if (run.phase == Phase::Idle)
                    return;
                Say(Acore::StringFormat("{} (plain) and {} ({}) ready at level {}", BotOf(0)->GetName(),
                                        BotOf(1)->GetName(), run.pathName, run.level));
                Lineup(gm);
                run.phase = Phase::Prepare;
                run.elapsed = 0;
            }
            else if (run.elapsed > LoginTimeoutMs)
            {
                for (Bot& side : run.sides)
                    side.guid.Clear();
                Stop("two bots of the class didn't log in (addclass needs free characters of the class)");
            }
            return;
        }
        case Phase::Prepare:
        case Phase::Rest:
            if (run.elapsed < (run.phase == Phase::Prepare ? PrepareMs : RestMs))
                return;
            if (run.phase == Phase::Rest)
                Lineup(gm);
            run.phase = Phase::Fight;
            run.elapsed = run.sinceAttack = ReattackMs;
            return;
        case Phase::Fight:
        {
            if (!BotOf(0) || !BotOf(1))
            {
                Stop("a bot left mid-fight; stopped");
                return;
            }
            run.sinceAttack += diff;
            if (run.sinceAttack >= ReattackMs)
            {
                run.sinceAttack = 0;
                for (int side = 0; side < 2; ++side)
                    if (Creature* dummy = ObjectAccessor::GetCreature(*gm, run.sides[side].dummy))
                        Attack(BotOf(side), dummy);
            }
            if (run.elapsed < run.fightMs + ReattackMs)
                return;
            EndFight();
            uint32 const done = uint32(run.sides[0].fights.size());
            Say(Acore::StringFormat("fight {} of {}: plain {:.1f} DPS, path {:.1f} DPS", done, run.fightsWanted,
                                    SubclassBalance::PerSecond(run.sides[0].fights.back().damage, run.fightMs),
                                    SubclassBalance::PerSecond(run.sides[1].fights.back().damage, run.fightMs)));
            if (done >= run.fightsWanted)
            {
                Report();
                Stop("done");
                return;
            }
            run.phase = Phase::Rest;
            run.elapsed = 0;
            return;
        }
        default:
            return;
    }
}

// What a bot of the run, or its pet, guardian or totem, deals during a fight; and what they take
void Meter(Unit* attacker, Unit* victim, uint32 damage)
{
    if (run.phase != Phase::Fight || !damage)
        return;
    if (attacker)
        if (Player* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            for (Bot& side : run.sides)
                if (owner->GetGUID() == side.guid)
                {
                    side.fight.damage += damage;
                    if (attacker != owner)
                        side.fight.petDamage += damage;
                }
    if (victim)
        for (Bot& side : run.sides)
            if (victim->GetGUID() == side.guid)
                side.fight.taken += damage;
}
}

class SubclassBalanceUnitScript : public UnitScript
{
public:
    SubclassBalanceUnitScript() : UnitScript("SubclassBalanceUnitScript", true) {}

    // After absorbs, before the victim's AI: a training dummy zeroes what it takes only after this
    uint32 DealDamage(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType /*type*/) override
    {
        Meter(attacker, victim, damage);
        return damage;
    }
};

class SubclassBalanceWorldScript : public WorldScript
{
public:
    SubclassBalanceWorldScript() : WorldScript("SubclassBalanceWorldScript", {WORLDHOOK_ON_UPDATE}) {}

    void OnUpdate(uint32 diff) override { Tick(diff); }
};

class SubclassBalanceCommandScript : public CommandScript
{
public:
    SubclassBalanceCommandScript() : CommandScript("SubclassBalanceCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable balanceTable = {
            {"start", HandleStart, SEC_GAMEMASTER, Console::No},
            {"status", HandleStatus, SEC_GAMEMASTER, Console::No},
            {"stop", HandleStop, SEC_GAMEMASTER, Console::No}};
        static ChatCommandTable commandTable = {{"balance", balanceTable}};
        return commandTable;
    }

    static bool HandleStart(ChatHandler* handler, uint16 pathId, uint8 level, uint8 plainSpec, uint8 pathSpec,
                            Optional<uint32> fights, Optional<uint32> seconds)
    {
        if (run.phase != Phase::Idle)
        {
            handler->SendErrorMessage("A balance run is going; .balance stop ends it.", false);
            return false;
        }
        Player* gm = handler->GetPlayer();
        SubclassPaths::Path const* path = nullptr;
        for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES && !path; ++cls)
            for (SubclassPaths::Path const* candidate : SubclassPaths::ClassPaths(cls))
                if (candidate->id == pathId)
                    path = candidate;
        if (!path)
        {
            handler->SendErrorMessage(Acore::StringFormat("No path {} (.subclass list shows them).", pathId), false);
            return false;
        }
        uint8 const cls = path->rules.classId;
        if (SpecName(cls, plainSpec).empty() || SpecName(cls, pathSpec).empty())
        {
            handler->SendErrorMessage("Spec numbers are the class's premade specs in playerbots.conf.", false);
            return false;
        }
        if (level < path->rules.unlockLevel || level > DEFAULT_MAX_LEVEL)
        {
            handler->SendErrorMessage(Acore::StringFormat("{} opens at level {}.", path->name, path->rules.unlockLevel),
                                      false);
            return false;
        }
        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(gm);
        if (!mgr)
        {
            handler->SendErrorMessage("No bot manager for this character.", false);
            return false;
        }
        run = Run{};
        run.gm = gm->GetGUID();
        run.pathId = path->id;
        run.pathName = path->name;
        run.classId = cls;
        run.level = level;
        run.plainSpec = plainSpec;
        run.pathSpec = pathSpec;
        run.fightsWanted = std::max<uint32>(fights.value_or(SubclassBalance::MinFights), 1);
        run.fightMs = std::clamp<uint32>(seconds.value_or(180), 30, 900) * IN_MILLISECONDS;
        for (auto itr = mgr->GetPlayerBotsBegin(); itr != mgr->GetPlayerBotsEnd(); ++itr)
            run.before.push_back(itr->first);
        static char const* const names[MAX_CLASSES] = {"", "warrior", "paladin", "hunter", "rogue", "priest",
                                                       "dk", "shaman", "mage", "warlock", "", "druid"};
        std::string const add = Acore::StringFormat("addclass {}", names[cls]);
        for (int i = 0; i < 2; ++i)
            mgr->HandlePlayerbotCommand(add.c_str(), gm);
        run.phase = Phase::Login;
        handler->PSendSysMessage("Balance: {} at level {}, {} fights of {} seconds; logging in two bots.", path->name,
                                 level, run.fightsWanted, run.fightMs / IN_MILLISECONDS);
        return true;
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        static char const* const phases[] = {"idle", "logging in", "preparing", "fighting", "resting"};
        handler->PSendSysMessage("Balance: {}{}", phases[uint8(run.phase)],
                                 run.phase == Phase::Idle ? std::string()
                                     : Acore::StringFormat(", {} at level {}, fight {} of {}", run.pathName, run.level,
                                                           run.sides[0].fights.size() + 1, run.fightsWanted));
        return true;
    }

    static bool HandleStop(ChatHandler* handler)
    {
        if (run.phase == Phase::Idle)
        {
            handler->SendSysMessage("Balance: no run going.");
            return true;
        }
        Stop("stopped");
        return true;
    }
};

void AddSubclassBalanceScripts()
{
    new SubclassBalanceUnitScript();
    new SubclassBalanceWorldScript();
    new SubclassBalanceCommandScript();
}
