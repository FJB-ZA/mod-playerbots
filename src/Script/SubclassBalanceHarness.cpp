/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The sub-class balance harness (solo-wow): two bots of a path's class, one plain and one on the path, at the same
// level with the same talents and the same gear, each fight a training dummy of their level for a set time while
// everything they and their pets deal is metered, after an uncounted warm-up fight. A run fights at least `fights`
// times and on, up to SubclassBalance::MaxFights, while the difference is too close to call. The comparison and its
// verdict follow SubclassBalancePolicy.h; the rows go to the GM's chat, the server log and
// <LogsDir>/balance-<path>.md, to be copied into the path's folder.
//
//   .balance start <path id> <level> <plain spec no> <path spec no> [fights 3] [seconds 180] [swap 0]
//   .balance status
//   .balance stop
//
// Spec numbers are the class's premade specs in playerbots.conf (Playerbots.PremadeSpecName.<class>.<no>). With
// swap 1 the bot that would have been plain takes the path, to check a result against the two bots' races.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <unordered_set>

#include "Chat.h"
#include "Config.h"
#include "DBCEnums.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
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
constexpr uint32 WarmUpMs = 60000;        // a first fight that isn't counted: pets, buffs and shards settle in it
constexpr uint32 ReattackMs = 3000;       // a dummy drops a player 5 seconds after their last direct hit
constexpr float Apart = 40.0f;            // between the two bots' dummies

enum class Phase : uint8 { Idle, Login, Prepare, Fight, Rest };

struct Bot
{
    ObjectGuid guid;
    ObjectGuid dummy;
    SubclassBalance::Fight fight;
    std::vector<SubclassBalance::Fight> fights;
    std::string trace;  // this fight, every renewal: mana and health in tenths, "-" while out of combat
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
    bool swap = false;  // the first bot found takes the path, so the two can trade sides between runs
    uint32 elapsed = 0, sinceAttack = 0;
    bool warm = false;  // the warm-up fight is over
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
    // The bots are the shared addclass pool's: the path bot goes back to its plain class before it leaves, GM or not
    if (Player* path = BotOf(1); path && SubclassPaths::ActivePathId(path) != SubclassPaths::PlainClass)
        SubclassPaths::ChangeActivePath(path, SubclassPaths::PlainClass, true);
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
    // (either may still follow a path from a run that ended without its GM)
    for (auto [bot, wanted] : {std::pair{plain, SubclassPaths::PlainClass}, std::pair{path, run.pathId}})
    {
        if (SubclassPaths::ActivePathId(bot) == wanted)
            continue;
        if (std::string const why = SubclassPaths::ChangeActivePath(bot, wanted, true); !why.empty())
        {
            Stop(Acore::StringFormat("{} couldn't take path {}: {}", bot->GetName(), wanted, why));
            return;
        }
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
        if (!copy || path->CanEquipItem(slot, dest, copy, false) != EQUIP_ERR_OK)
        {
            delete copy;
            continue;
        }
        // CloneItem keeps the item and its random stats only: its enchants and gems come across here, before the
        // item is worn and they take effect (at level 60 they were a tenth of the plain bot's damage)
        for (uint8 enchant = PERM_ENCHANTMENT_SLOT; enchant < MAX_ENCHANTMENT_SLOT; ++enchant)
        {
            EnchantmentSlot const each = EnchantmentSlot(enchant);
            if (uint32 const id = worn->GetEnchantmentId(each))
                copy->SetEnchantment(each, id, worn->GetEnchantmentDuration(each), worn->GetEnchantmentCharges(each));
        }
        path->EquipItem(dest, copy, true);
    }
    auto gear = [](Player* bot) {
        uint32 items = 0, enchants = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                ++items;
                for (uint8 enchant = PERM_ENCHANTMENT_SLOT; enchant < MAX_ENCHANTMENT_SLOT; ++enchant)
                    enchants += item->GetEnchantmentId(EnchantmentSlot(enchant)) ? 1 : 0;
            }
        return Acore::StringFormat("{} items, {} enchants and gems, {} shadow spell power, {} attack power", items,
                                   enchants, bot->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW),
                                   uint32(bot->GetTotalAttackPowerValue(BASE_ATTACK)));
    };
    Say("plain " + gear(plain) + "; path " + gear(path));
    for (Player* bot : {plain, path})
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        ai->ResetStrategies();
        ai->ChangeStrategy("-follow,+stay", BOT_STATE_NON_COMBAT);
        // (a bot assisting its group takes the target most of the group attacks: the other bot's dummy, pet and all)
        ai->ChangeStrategy("-dps assist", BOT_STATE_COMBAT);
        bot->SetFullHealth();
        bot->SetPower(bot->getPowerType(), bot->GetMaxPower(bot->getPowerType()));
    }
}

// The ground under (x, y), near the GM's height
float GroundZ(Player* gm, float x, float y)
{
    float const z = gm->GetMap()->GetHeight(gm->GetPhaseMask(), x, y, gm->GetPositionZ() + 10.0f);
    return z > INVALID_HEIGHT ? z : gm->GetPositionZ();
}

// Each bot to its own spot beside the GM, on the ground and facing a fresh dummy of its level. A bot drops a target
// it can't see, so a spot with a slope or a tree between the bot and its dummy stops the run.
bool Lineup(Player* gm)
{
    for (int side = 0; side < 2; ++side)
    {
        Player* bot = BotOf(side);
        if (!bot)
            continue;
        float const angle = gm->GetOrientation() + (side ? -1.0f : 1.0f) * float(M_PI) / 2;
        float const x = gm->GetPositionX() + std::cos(angle) * Apart / 2;
        float const y = gm->GetPositionY() + std::sin(angle) * Apart / 2;
        float const z = GroundZ(gm, x, y);
        float const dx = x + std::cos(gm->GetOrientation()) * 15.0f, dy = y + std::sin(gm->GetOrientation()) * 15.0f;
        float const dz = GroundZ(gm, dx, dy);
        if (!gm->GetMap()->isInLineOfSight(x, y, z + 2.0f, dx, dy, dz + 2.0f, gm->GetPhaseMask(),
                                           LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
        {
            Stop(Acore::StringFormat("{} can't see its dummy from its spot; stand on flat, open ground and start again",
                                     bot->GetName()));
            return false;
        }
        bot->TeleportTo(gm->GetMapId(), x, y, z, gm->GetOrientation());
        if (TempSummon* dummy = gm->SummonCreature(DummyEntry, dx, dy, dz, gm->GetOrientation() + float(M_PI),
                                                   TEMPSUMMON_TIMED_DESPAWN, run.fightMs + RestMs + 60000))
        {
            dummy->SetLevel(run.level);
            dummy->SetFullHealth();
            run.sides[side].dummy = dummy->GetGUID();
        }
    }
    return true;
}

// As the bots' own attack action does it: the dummy their current target, the combat engine on
void Attack(Player* bot, Creature* dummy)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    // A bot that dropped out of combat after the first renewal: once a fight, what might keep it from fighting
    if (!bot->IsInCombat() && run.elapsed > 2 * ReattackMs && !run.sides[bot == BotOf(0) ? 0 : 1].fight.damage)
    {
        static std::unordered_set<uint64> told;
        uint64 const key = bot->GetGUID().GetRawValue() ^ (uint64(run.sides[0].fights.size()) << 56);
        if (told.insert(key).second)
        {
            Say(Acore::StringFormat("{} isn't fighting: valid target {}, in sight {}, invalid to its AI {}, height {:.1f} "
                                    "against its dummy's {:.1f}, power {}/{}, state {}",
                                    bot->GetName(), bot->IsValidAttackTarget(dummy), bot->IsWithinLOSInMap(dummy),
                                    ai->GetAiObjectContext()->GetValue<bool>("invalid target", "current target")->Get(),
                                    bot->GetPositionZ(), dummy->GetPositionZ(), bot->GetPower(bot->getPowerType()),
                                    bot->GetMaxPower(bot->getPowerType()), uint32(ai->GetState())));
        }
    }
    std::string& trace = run.sides[bot == BotOf(0) ? 0 : 1].trace;
    trace += Acore::StringFormat("{}{}/{} ", bot->IsInCombat() ? "" : "-",
                                 bot->GetMaxPower(bot->getPowerType()) ?
                                     bot->GetPower(bot->getPowerType()) * 10 / bot->GetMaxPower(bot->getPowerType()) : 0,
                                 bot->GetHealthPct() >= 100.0f ? 10 : uint32(bot->GetHealthPct() / 10));
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
        if (Player* bot = BotOf(side))
        {
            Creature* dummy = Gm() ? ObjectAccessor::GetCreature(*Gm(), b.dummy) : nullptr;
            Say(Acore::StringFormat("{}: level {}, {}, {} health, targeting {}, dummy {} at {:.0f} yards, {} damage",
                                    bot->GetName(), bot->GetLevel(), bot->IsInCombat() ? "in combat" : "out of combat",
                                    bot->IsAlive() ? "alive" : "dead", bot->GetTarget().ToString(),
                                    dummy ? dummy->GetGUID().ToString() : "missing",
                                    dummy ? bot->GetDistance(dummy) : 0.0f, b.fight.damage));
            Say(Acore::StringFormat("{} mana/health: {}", bot->GetName(), b.trace));
        }
        b.fight.ms = run.fightMs;
        if (run.warm)
            b.fights.push_back(b.fight);
        b.fight = {};
        b.trace.clear();
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
                  SpecName(run.classId, run.plainSpec) + " / " + SpecName(run.classId, run.pathSpec) +
                      (run.swap ? " (sides swapped)" : ""),
                  Scenario::Dummy};
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
                run.sides[run.swap ? 1 - found : found].guid = bot->GetGUID();
                ++found;
            }
            if (found == 2)
            {
                Prepare(BotOf(0), BotOf(1));
                if (run.phase == Phase::Idle)
                    return;
                auto race = [](Player* bot) {
                    static char const* const races[] = {"", "human", "orc", "dwarf", "night elf", "undead",
                                                        "tauren", "gnome", "troll", "", "blood elf", "draenei"};
                    return bot->getRace() < std::size(races) ? races[bot->getRace()] : "";
                };
                Say(Acore::StringFormat("{} ({}, plain) and {} ({}, {}) ready at level {}", BotOf(0)->GetName(),
                                        race(BotOf(0)), BotOf(1)->GetName(), race(BotOf(1)), run.pathName,
                                        run.level));
                if (!Lineup(gm))
                    return;
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
            if (run.phase == Phase::Rest && !Lineup(gm))
                return;
            // Each fight starts fresh: what a bot spent resting (buffs, a pet, a Life Tap to fill up) isn't carried in
            for (int side = 0; side < 2; ++side)
                if (Player* bot = BotOf(side))
                {
                    bot->RemoveAllSpellCooldown();
                    bot->SetFullHealth();
                    bot->SetPower(bot->getPowerType(), bot->GetMaxPower(bot->getPowerType()));
                }
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
            if (run.elapsed < (run.warm ? run.fightMs : WarmUpMs) + ReattackMs)
                return;
            if (!run.warm)
            {
                Say(Acore::StringFormat("warm-up: plain {:.1f} DPS, path {:.1f} DPS (not counted)",
                                        SubclassBalance::PerSecond(run.sides[0].fight.damage, WarmUpMs),
                                        SubclassBalance::PerSecond(run.sides[1].fight.damage, WarmUpMs)));
                EndFight();
                run.warm = true;
                run.phase = Phase::Rest;
                run.elapsed = 0;
                return;
            }
            EndFight();
            uint32 const done = uint32(run.sides[0].fights.size());
            Say(Acore::StringFormat("fight {} (of at least {}): plain {:.1f} DPS, path {:.1f} DPS", done, run.fightsWanted,
                                    SubclassBalance::PerSecond(run.sides[0].fights.back().damage, run.fightMs),
                                    SubclassBalance::PerSecond(run.sides[1].fights.back().damage, run.fightMs)));
            if (done >= run.fightsWanted)
            {
                // Fights on while the difference is too close to the budget to call, up to the cap
                SubclassBalance::Comparison const c = SubclassBalance::Compare(run.sides[0].fights, run.sides[1].fights);
                if (SubclassBalance::FightOn(c, done, std::max(run.fightsWanted, SubclassBalance::MaxFights)))
                    Say(Acore::StringFormat("{:+.1f}% ± {:.1f}% is too close to call; another fight", c.dpsDiffPct,
                                            c.marginPct));
                else
                {
                    Report();
                    Stop("done");
                    return;
                }
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
                            Optional<uint32> fights, Optional<uint32> seconds, Optional<uint32> swap)
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
        run.swap = swap.value_or(0) != 0;
        for (auto itr = mgr->GetPlayerBotsBegin(); itr != mgr->GetPlayerBotsEnd(); ++itr)
            run.before.push_back(itr->first);
        static char const* const names[MAX_CLASSES] = {"", "warrior", "paladin", "hunter", "rogue", "priest",
                                                       "dk", "shaman", "mage", "warlock", "", "druid"};
        // The command is split in place (strtok): each call gets its own copy
        for (int i = 0; i < 2; ++i)
        {
            std::string add = Acore::StringFormat("addclass {}", names[cls]);
            for (std::string const& reply : mgr->HandlePlayerbotCommand(add.data(), gm))
                LOG_INFO("playerbots", "Balance: {}", reply);
        }
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
