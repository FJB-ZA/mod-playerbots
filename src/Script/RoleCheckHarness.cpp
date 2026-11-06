/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The role check (solo-wow): does a bot group do its job in an instance without being told? The GM gets a group of
// addclass bots at the level cap of the instance's era (60, 70 or 80) in that era's epics, or for a classic dungeon at
// the dungeon's own level in blues, each on its role's spec and with the door's quests and keys, takes them into the
// instance and watches, the way a player would see it:
//   - dungeon: the GM (GM mode on, so no mob fights it) only keeps up with the tank. The bot tank is to lead: pull pack
//     by pack, hold the mobs and turn them away from the group, while the healer heals from range and the damage
//     dealers kill the tank's target without opening fights of their own. Heroic for a Wrath dungeon, else normal; a
//     party of the GM and four bots.
//   - raid: the GM (GM mode off once everyone is in, god mode on) pulls the nearest trash like a player and lets the
//     tank pick it up; with none in reach it goes on to the nearest left in the raid, then the bosses, taking the
//     group along. Ten player normal for a Wrath raid, else the raid's only size; the GM and nine bots.
// Twice a second in a fight, after its first five seconds, it counts who the mobs hit, which way they face, what the
// damage dealers attack and where the healer stands; per fight, who opened it and whether the hurt were healed. The
// rules are RoleCheckPolicy.h; the verdict goes to the GM's chat, the server log and <LogsDir>/rolecheck.md.
//
//   .rolecheck dungeon <map id> [minutes 10] [level: 60, 70, 80 or under 60; as above if left out]
//   .rolecheck raid <map id> [minutes 10] [level]
//   .rolecheck status
//   .rolecheck stop

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "AiFactory.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "DungeonLead.h"
#include "Engine.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "LivingWorldMgr.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "RoleCheckPolicy.h"
#include "ScriptMgr.h"
#include "Timer.h"
#include "World.h"

using namespace Acore::ChatCommands;

namespace
{
// The era a map belongs to sets the group: its level cap, gear fresh from that era's raids (epic quality), and heroic
// for a Wrath dungeon. A Burning Crusade heroic needs a key from its faction, which bots never earn, so those go in on
// normal; classic dungeons have no heroic
struct Era
{
    uint8 level;
    uint32 gearItemLevel;
    bool heroic;
    uint32 quality = ITEM_QUALITY_EPIC;
};

// A classic dungeon's own level: the top of the Dungeon Finder's recommended band for it (the lowest wing's, for a
// dungeon in wings). At 60 the group killed every mob before it struck anyone, so nothing about the roles showed.
uint32 DungeonLevel(uint32 mapId)
{
    uint32 level = 0;
    for (uint32 id = 0; id < sLFGDungeonStore.GetNumRows(); ++id)
    {
        LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(id);
        if (dungeon && dungeon->MapID == mapId && dungeon->TypeID == 1 /* LFG_TYPE_DUNGEON */ &&
            dungeon->Difficulty == DUNGEON_DIFFICULTY_NORMAL && dungeon->TargetLevelMax)
            level = level ? std::min(level, dungeon->TargetLevelMax) : dungeon->TargetLevelMax;
    }
    return level;
}

// The level asked for, or else: a classic dungeon's own level, or the cap of the map's own expansion. Onyxia's Lair
// and Naxxramas are classic maps that Wrath made level 80 raids, so their runs ask for 80
Era EraOf(MapEntry const* map, uint32 level)
{
    if (!level && map->IsNonRaidDungeon() && map->Expansion() == EXPANSION_CLASSIC)
        level = DungeonLevel(map->MapID);
    if (!level)
        level = map->Expansion() == EXPANSION_CLASSIC                 ? 60
                : map->Expansion() == EXPANSION_THE_BURNING_CRUSADE ? 70
                                                                     : 80;
    // Under the cap: dungeon blues of the level, what a group there would wear
    if (level < 60)
        return Era{uint8(level), level + 5, false, ITEM_QUALITY_RARE};
    if (level == 60)
        return Era{60, 66, false};  // Molten Core
    if (level <= 70)
        return Era{70, 115, false};  // Karazhan
    return Era{80, 200, map->Expansion() == EXPANSION_WRATH_OF_THE_LICH_KING};  // fresh heroic gear
}
constexpr uint32 LoginTimeoutMs = 180000;  // just after a start the world thread is slow to log bots in
constexpr uint32 SettleMs = 15000;      // after the teleport: everyone in, buffs up
constexpr uint32 AwayMs = 20000;        // how long a bot may be out of the world (a far teleport) before the run stops
constexpr uint32 SampleMs = 500;
constexpr uint32 StatusMs = 15000;
constexpr uint32 CalmMs = 1500;         // out of combat this long ends a fight
constexpr uint32 ReviveMs = 10000;      // out of combat this long, the dead get up beside the group
constexpr uint32 FollowMs = 2000;       // dungeon: how often the GM looks where the tank is
// dungeon: the tank this far ahead, the GM catches up; inside the lead's gather radius, which holds the pull for a
// player who is further away
constexpr float CatchUpAt = 12.0f;
constexpr uint32 GatherMs = 12000;      // raid: the group gathers behind the GM before it pulls
constexpr float PullFrom = 32.0f;       // raid: where the GM waits, short of the mob's aggro
constexpr float TrashReach = 150.0f;
constexpr uint32 WalkMs = 3000;         // raid: after going on to mobs further in, before looking about

enum class Phase : uint8 { Idle, Login, Settle, Observe };

struct Seat
{
    uint8 classId;
    char const* spec;  // its premade spec in playerbots.conf
    char const* name;  // for addclass
};

// The GM (a human, so Alliance) makes the fifth or tenth member; it stays out of the fight in a dungeon and only
// pulls in a raid. Classes every era has: in vanilla there are no Alliance shamans or death knights to add.
Seat const DungeonSeats[] = {{CLASS_WARRIOR, "prot pve", "warrior"}, {CLASS_PRIEST, "holy pve", "priest"},
                             {CLASS_MAGE, "frost pve", "mage"},      {CLASS_ROGUE, "combat pve", "rogue"}};
Seat const RaidSeats[] = {{CLASS_WARRIOR, "prot pve", "warrior"},  {CLASS_PALADIN, "prot pve", "paladin"},
                          {CLASS_PRIEST, "holy pve", "priest"},     {CLASS_DRUID, "resto pve", "druid"},
                          {CLASS_MAGE, "fire pve", "mage"},         {CLASS_ROGUE, "combat pve", "rogue"},
                          {CLASS_HUNTER, "mm pve", "hunter"},       {CLASS_WARLOCK, "destro pve", "warlock"},
                          {CLASS_WARRIOR, "fury pve", "warrior"}};

struct Member
{
    Seat seat;
    ObjectGuid guid;
    bool dead = false;
};

struct Run
{
    Phase phase = Phase::Idle;
    bool raid = false;
    ObjectGuid gm;
    uint32 mapId = 0;
    Era era{80, 200, true};
    uint32 durationMs = 0;
    uint32 elapsed = 0;  // in the phase
    std::vector<Member> bots;
    std::vector<ObjectGuid> before;  // the GM's bots before the run
    WorldLocation entrance = WorldLocation();
    RoleCheck::Tally tally;
    uint32 wipes = 0;

    bool inFight = false;
    uint32 fightAt = 0;
    uint32 calm = 0;
    uint32 sinceSample = 0, sinceStatus = 0, sinceFollow = 0;
    std::set<ObjectGuid> fightMobs;
    // this fight's own numbers, for its log line
    uint32 fightMobCount = 0, fightMobSamples = 0, fightOnTank = 0;
    std::map<ObjectGuid, uint32> fightOffTank;
    std::map<ObjectGuid, uint32> fightFaced;  // samples each member stood in front of a mob on the tank
    ObjectGuid fightOpener;
    std::string fightOpenerHow;  // how: what the opener hit, or what struck it
    std::set<ObjectGuid> dead;  // mobs counted as killed
    float tankTravel = 0.0f;
    Position tankWas;
    bool tankKnown = false;

    // raid: the mob the GM pulls next
    ObjectGuid pullTarget;
    uint32 gathering = 0;
    std::set<ObjectGuid::LowType> walkedTo;  // spawns the GM went on to
    uint32 walkWait = 0;  // after going on: the teleport lands at the next session update, and the grid loads
    std::set<ObjectGuid> tried;  // mobs pulled once: one that came back without a fight (no way to the GM) is let be
    uint32 away = 0;  // how long some bot has been out of the world

    // AccountInstancesPerHour before the run: lifted while it runs, since back-to-back runs (and each settle retry
    // into a reset instance) count against every bot's five new dungeons an hour and left bots outside
    uint32 instancesPerHour = 0;
};

Run run;

// Written from the damage and heal hooks, which run in the map's update
std::mutex hookLock;
ObjectGuid opener;  // who started the fight: a member who struck first, or the member a mob struck first
std::string openerHow;
bool fightHurt = false;
bool fightHealed = false;
bool fightStruck = false;  // a mob struck a member: a real fight

void Report();

Player* Gm() { return ObjectAccessor::FindConnectedPlayer(run.gm); }

Player* BotOf(Member const& member)
{
    Player* bot = ObjectAccessor::FindConnectedPlayer(member.guid);
    return bot && bot->IsInWorld() && GET_PLAYERBOT_AI(bot) ? bot : nullptr;
}

std::vector<Player*> Bots()
{
    std::vector<Player*> bots;
    for (Member const& member : run.bots)
        if (Player* bot = BotOf(member))
            bots.push_back(bot);
    return bots;
}

bool IsRunPlayer(ObjectGuid guid)
{
    if (guid == run.gm)
        return true;
    return std::any_of(run.bots.begin(), run.bots.end(), [&](Member const& m) { return m.guid == guid; });
}

// The run's player behind a unit: the player, or the owner of its pet, totem or guardian
Player* RunPlayerOf(Unit* unit)
{
    Player* owner = unit ? unit->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    return owner && IsRunPlayer(owner->GetGUID()) ? owner : nullptr;
}

void Say(std::string const& text)
{
    LOG_INFO("playerbots", "Rolecheck: {}", text);
    if (Player* gm = Gm())
        ChatHandler(gm->GetSession()).SendSysMessage("Rolecheck: " + text);
}

std::string MapName(uint32 mapId)
{
    MapEntry const* entry = sMapStore.LookupEntry(mapId);
    return entry ? entry->name[LOCALE_enUS] : std::to_string(mapId);
}

int SpecNo(Seat const& seat)
{
    for (int no = 0; no < MAX_SPECNO; ++no)
        if (sPlayerbotAIConfig.PremadeSpecName[seat.classId][no] == seat.spec)
            return no;
    return -1;
}

char Role(Player* bot)
{
    return PlayerbotAI::IsTank(bot, true) ? 'T' : PlayerbotAI::IsHeal(bot, true) ? 'H' : 'D';
}

Player* MainTank()
{
    Player* gm = Gm();
    Group* group = gm ? gm->GetGroup() : nullptr;
    ObjectGuid const guid = PlayerbotAI::GetMainTankGuid(group);
    Player* tank = guid.IsEmpty() ? nullptr : ObjectAccessor::FindConnectedPlayer(guid);
    return tank && tank->IsInWorld() && tank->IsAlive() ? tank : nullptr;
}

std::string Strategies(PlayerbotAI* ai, BotState state)
{
    std::string out;
    for (std::string const& name : ai->GetStrategies(state))
        out += (out.empty() ? "" : ",") + name;
    return out;
}

void Revive(Player* bot, Player* beside)
{
    if (!bot->IsAlive())
    {
        bot->ResurrectPlayer(1.0f);
        bot->SpawnCorpseBones();
    }
    bot->SetFullHealth();
    if (bot->getPowerType() == POWER_MANA)
        bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
    if (beside && beside != bot && beside->GetMapId() == bot->GetMapId())
        bot->NearTeleportTo(beside->GetPositionX(), beside->GetPositionY(), beside->GetPositionZ(),
                            beside->GetOrientation());
}

void Stop(std::string const& why)
{
    Player* gm = Gm();
    PlayerbotMgr* mgr = gm ? GET_PLAYERBOT_MGR(gm) : nullptr;
    for (Member const& member : run.bots)
    {
        if (Player* bot = BotOf(member))
            LivingWorldMgr::instance().Exempt(bot, false);
        if (mgr && !member.guid.IsEmpty())
            mgr->LogoutPlayerBot(member.guid);
    }
    if (run.instancesPerHour)
        sWorld->setIntConfig(CONFIG_MAX_INSTANCES_PER_HOUR, run.instancesPerHour);
    if (gm && run.raid)
    {
        gm->SetCommandStatusOff(CHEAT_GOD);
        gm->SetGameMaster(true);
    }
    Say("run over: " + why);  // .\wow.ps1 rolecheck waits for this line; "done" is the only measured ending
    run = Run{};
}

// The creature the tank's lead heads for, by name: its next boss, or the trash it fell back on
std::string GoalName(DungeonLead::State const& state)
{
    if (!state.hasGoal)
        return "none";
    CreatureData const* spawn = state.goalSpawn ? sObjectMgr->GetCreatureData(state.goalSpawn) : nullptr;
    CreatureTemplate const* creature = spawn ? sObjectMgr->GetCreatureTemplate(spawn->id) : nullptr;
    return creature ? creature->Name : "a spot";
}

// How the run went in, for its row and its word: "heroic 80", "normal 60", "raid 70"
std::string Mode()
{
    return Acore::StringFormat("{} {}", run.raid ? "raid" : run.era.heroic ? "heroic" : "normal", run.era.level);
}

void Prepare(Player* bot, Seat const& seat)
{
    LivingWorldMgr::instance().Exempt(bot, true);
    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    // An explicit quality and gear score: the factory's default is the phase's random-bot limit (item level 92 in
    // vanilla), far under the run's
    uint32 const gearScore = PlayerbotFactory::CalcMixedGearScore(run.era.gearItemLevel, run.era.quality);
    if (bot->GetLevel() != run.era.level)
        PlayerbotFactory(bot, run.era.level, run.era.quality, gearScore).Randomize(false);
    PlayerbotFactory::InitTalentsBySpecNo(bot, SpecNo(seat), true);
    PlayerbotFactory(bot, run.era.level, run.era.quality, gearScore).InitGlyphs(false);
    // Second chance: the old gear is destroyed, not put in the bags. With full bags a bot otherwise keeps its old item
    // in every slot, and an addclass bot made in an earlier era its vanilla gear
    PlayerbotFactory::AutoGear(bot, run.era.quality, run.era.gearItemLevel, false, true);
    LOG_INFO("playerbots", "Rolecheck: {} geared to item level {} ({} spec)", bot->GetName(),
             uint32(ai->GetEquipGearScore(bot)), seat.spec);
    Revive(bot, nullptr);
    ai->Reset(true);
    ai->ResetStrategies();
}

// What the door asks of a player who went the usual way: its quests done (Pit of Saron's "Echoes of Tortured Souls"),
// its key in the bags. Addclass bots have done nothing, so the door turned them all away.
void Attune(Player* bot, Difficulty difficulty)
{
    DungeonProgressionRequirements const* door = sObjectMgr->GetAccessRequirement(run.mapId, difficulty);
    if (!door)
        return;
    auto forBot = [bot](ProgressionRequirement const* need)
    { return need->faction == TEAM_NEUTRAL || need->faction == bot->GetTeamId(true); };
    for (ProgressionRequirement const* quest : door->quests)
        if (forBot(quest) && !bot->GetQuestRewardStatus(quest->id))
            bot->SetRewardedQuest(quest->id);
    for (ProgressionRequirement const* item : door->items)
        if (forBot(item) && !bot->HasItemCount(item->id, 1, true))
            bot->AddItem(item->id, 1);
    for (ProgressionRequirement const* achievement : door->achievements)
        if (forBot(achievement) && !bot->HasAchieved(achievement->id))
            if (AchievementEntry const* entry = sAchievementStore.LookupEntry(achievement->id))
                bot->CompletedAchievement(entry);
}

// Everyone in: the group set to the era's difficulty (heroic, normal or the raid's ten), old lockouts dropped, all taken
// to the entrance
bool Enter(Player* gm)
{
    Group* group = gm->GetGroup();
    if (!group || group->GetLeaderGUID() != gm->GetGUID())
    {
        Stop("the GM must lead the bots' group");
        return false;
    }
    Difficulty const difficulty = run.raid       ? RAID_DIFFICULTY_10MAN_NORMAL
                                  : run.era.heroic ? DUNGEON_DIFFICULTY_HEROIC
                                                   : DUNGEON_DIFFICULTY_NORMAL;
    if (run.raid)
        group->SetRaidDifficulty(difficulty);
    else
        group->SetDungeonDifficulty(difficulty);

    std::vector<Player*> everyone = Bots();
    for (Player* bot : everyone)
        Attune(bot, difficulty);
    everyone.insert(everyone.begin(), gm);
    for (Player* member : everyone)
    {
        sInstanceSaveMgr->PlayerUnbindInstance(member->GetGUID(), run.mapId, difficulty, true, member);
        member->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
        member->TeleportTo(run.entrance);
    }
    return true;
}

// What each bot will do, for the log: its role by spec, its strategies, and who leads
void Describe()
{
    for (Player* bot : Bots())
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        ObjectGuid const leader = ai->GetAiObjectContext()->GetValue<ObjectGuid>("dungeon leader")->Get();
        Player* lead = leader.IsEmpty() ? nullptr : ObjectAccessor::FindConnectedPlayer(leader);
        Say(Acore::StringFormat("{} ({}, talent tab {}, item level {}): leader {}; combat {}; non-combat {}",
                                bot->GetName(), Role(bot), AiFactory::GetPlayerSpecTab(bot),
                                uint32(ai->GetEquipGearScore(bot)), lead ? lead->GetName() : "none",
                                Strategies(ai, BOT_STATE_COMBAT), Strategies(ai, BOT_STATE_NON_COMBAT)));
    }
}

void Status()
{
    Player* tank = MainTank();
    std::string lead = "no tank";
    if (tank)
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(tank);
        lead = tank->GetName();
        if (ai && !run.raid)
        {
            DungeonLead::State const& state = DungeonLead::StateOf(ai);
            Player* leader = DungeonLead::Leader(ai);
            Player* waitingOn = state.waitingOn.IsEmpty() ? nullptr : ObjectAccessor::FindConnectedPlayer(state.waitingOn);
            Player* gm = Gm();
            lead += Acore::StringFormat(
                " (leading {}, {} pulls, wait {} on {} at {:.0f} yd, GM at {:.0f} yd, done {}, idle {}, pack {}, spot {} "
                "at {:.0f} yd, pull spot {}, stepping {}, pulled {}, failed {}, skipped {}, goal {} at {:.0f} yd, "
                "{} blocked)",
                leader == tank ? "yes" : "no", state.pulls, uint32(state.wait), waitingOn ? waitingOn->GetName() : "-",
                waitingOn && waitingOn->GetMap() == tank->GetMap() ? waitingOn->GetExactDist(tank) : -1.0f,
                gm && gm->GetMap() == tank->GetMap() ? gm->GetExactDist(tank) : -1.0f,
                state.done, state.idle, !state.pack.IsEmpty(), state.hasSpot,
                state.hasSpot ? tank->GetExactDist(&state.spot) : 0.0f, state.hasPullSpot, state.stepping, state.pulled,
                state.failedPulls, state.skipped.size(), GoalName(state),
                state.hasGoal ? tank->GetExactDist(&state.goal) : -1.0f, state.blockedGoals.size());
            // A tank standing still out of a fight: what its engine last tried, to see what holds it
            if (!tank->IsInCombat() && !tank->isMoving())
                if (Engine* engine = ai->GetCurrentEngine())
                {
                    std::string trace = engine->GetLastAction();
                    if (trace.size() > 400)
                        trace = trace.substr(trace.size() - 400);
                    LOG_INFO("playerbots", "Rolecheck: tank's last actions ({}, combat {}): {}",
                             ai->GetState() == BOT_STATE_COMBAT ? "combat engine" : "non-combat engine",
                             tank->IsInCombat(), trace);
                }
        }
    }
    if (tank)
    {
        std::string where;
        for (Player* bot : Bots())
            if (bot != tank)
                where += Acore::StringFormat("{}{} {:.0f} yd{}{}", where.empty() ? "" : ", ", bot->GetName(),
                                             bot->GetMap() == tank->GetMap() ? bot->GetExactDist(tank) : -1.0f,
                                             bot->IsAlive() ? "" : " dead", bot->isMoving() ? " moving" : "");
        lead += "; from the tank: " + where;
    }
    RoleCheck::Tally const& t = run.tally;
    Say(Acore::StringFormat("{}s: tank {} moved {:.0f} yards; {} fights, {} kills, {} bosses, {} deaths", run.elapsed / 1000,
                            lead, run.tankTravel, t.fights, t.kills, t.bosses, t.deaths));
}

std::vector<Creature*> FightMobs(std::vector<Player*> const& members)
{
    std::vector<Creature*> mobs;
    auto add = [&](Unit* unit) {
        Creature* mob = unit ? unit->ToCreature() : nullptr;
        if (!mob || !mob->IsAlive() || RunPlayerOf(mob) || std::find(mobs.begin(), mobs.end(), mob) != mobs.end())
            return;
        mobs.push_back(mob);
        run.fightMobs.insert(mob->GetGUID());
    };
    for (Player* member : members)
    {
        if (!member->IsAlive())
            continue;
        for (Unit* attacker : member->getAttackers())
            add(attacker);
        add(member->GetVictim());
    }
    return mobs;
}

void Sample(Player* gm, std::vector<Player*> const& bots)
{
    RoleCheck::Tally& t = run.tally;
    std::vector<Player*> members = bots;
    members.push_back(gm);
    std::vector<Creature*> const mobs = FightMobs(members);
    Player* tank = MainTank();
    run.fightMobCount = std::max<uint32>(run.fightMobCount, mobs.size());

    // With the tank dead nobody holds the mobs: that shows in the deaths, not here
    for (Creature* mob : tank ? mobs : std::vector<Creature*>())
    {
        Unit* victim = mob->GetVictim();
        Player* hit = victim ? victim->ToPlayer() : nullptr;
        if (!victim || !RunPlayerOf(victim))
            continue;
        ++t.mobSamples;
        ++run.fightMobSamples;
        if (hit && PlayerbotAI::IsTank(hit, true))
        {
            ++t.onTank;
            ++run.fightOnTank;
        }
        else if (Player* who = RunPlayerOf(victim))
            ++run.fightOffTank[who->GetGUID()];
        if (!tank || victim != tank || !mob->IsWithinMeleeRange(tank))
            continue;
        ++t.facedSamples;
        // (in a dungeon the GM only trails the tank, out of the fight)
        bool exposed = false;
        for (Player* bot : bots)
            if (bot != tank && bot->IsAlive() && !PlayerbotAI::IsTank(bot, true) &&
                mob->GetExactDist(bot) <= RoleCheck::FrontReach && mob->HasInArc(float(M_PI) / 2.0f, bot))
            {
                exposed = true;
                ++run.fightFaced[bot->GetGUID()];
            }
        if (exposed)
            ++t.facedAtGroup;
    }

    Unit* tankTarget = tank ? tank->GetVictim() : nullptr;
    Group* group = gm->GetGroup();
    ObjectGuid const skull = group ? group->GetTargetIcon(7) : ObjectGuid::Empty;
    for (Player* bot : bots)
    {
        if (!bot->IsAlive() || bot == tank)
            continue;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        if (PlayerbotAI::IsHeal(bot, true))
        {
            ++t.healerSamples;
            float nearest = 1000.0f;
            for (Creature* mob : mobs)
                nearest = std::min(nearest, bot->GetExactDist(mob));
            // Back from the mobs, but still in reach of the tank: a healer left at the door is not standing back
            bool const inReach = tank && tank->GetMap() == bot->GetMap() &&
                                 bot->GetExactDist(tank) <= RoleCheck::HealReach && bot->IsWithinLOSInMap(tank);
            if (nearest >= RoleCheck::HealerRange && inReach)
                ++t.healerAtRange;
            continue;
        }
        if (PlayerbotAI::IsTank(bot, true) || !tankTarget || !tankTarget->IsAlive())
            continue;
        ++t.dpsSamples;
        Unit* target = ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        if (target && (target == tankTarget || (!skull.IsEmpty() && target->GetGUID() == skull)))
            ++t.dpsOnFocus;
    }
}

// The fight's start: who opened it, from the hooks, or else the first member found fighting
void StartFight(std::vector<Player*> const& members)
{
    ObjectGuid first;
    {
        std::lock_guard<std::mutex> guard(hookLock);
        first = opener;
        run.fightOpenerHow = first.IsEmpty() ? "first in combat" : openerHow;
        opener.Clear();
        fightHurt = false;
        fightHealed = false;
    }
    if (first.IsEmpty())
        for (Player* member : members)
            if (member->IsInCombat())
            {
                first = member->GetGUID();
                break;
            }
    run.inFight = true;
    run.fightAt = run.elapsed;
    run.calm = 0;
    run.fightMobCount = run.fightMobSamples = run.fightOnTank = 0;
    run.fightOffTank.clear();
    run.fightFaced.clear();
    run.fightOpener = first;
    if (Player* tank = MainTank())
    {
        std::string mobs;
        for (Creature* mob : FightMobs(members))
        {
            Unit* victim = mob->GetVictim();
            mobs += Acore::StringFormat(" [{} at {:.0f} yd from the tank, aggro {:.0f}, on {}]", mob->GetName(),
                                        mob->GetExactDist(tank), mob->GetAggroRange(tank),
                                        victim ? victim->GetName() : "-");
        }
        PlayerbotAI* ai = GET_PLAYERBOT_AI(tank);
        DungeonLead::State const* lead = ai && !run.raid ? &DungeonLead::StateOf(ai) : nullptr;
        LOG_INFO("playerbots", "Rolecheck: fight {} starts; tank pulled {}, pack {}:{}", run.tally.fights + 1,
                 lead && lead->pulled, lead && !lead->pack.IsEmpty(), mobs);
    }
}

// Who opened the fight, once it proved one
void CountFight()
{
    ++run.tally.fights;
    Player* who = ObjectAccessor::FindConnectedPlayer(run.fightOpener);
    if (run.raid && run.fightOpener == run.gm)
        return;  // the player's pull, as asked
    ++run.tally.pulls;
    Player* tank = MainTank();
    bool const byTank = who && (who == tank || PlayerbotAI::IsTank(who, true));
    if (byTank)
        ++run.tally.tankPulls;
    else
        Say(Acore::StringFormat("fight {} opened by {} ({}), not the tank", run.tally.fights,
                                who ? who->GetName() : "nobody known", who ? std::string(1, Role(who)) : "?"));
}

void EndFight()
{
    run.inFight = false;
    bool struck;
    {
        std::lock_guard<std::mutex> guard(hookLock);
        struck = fightStruck;
        fightStruck = false;
    }
    // A fight is a mob striking one of the group. Combat with nothing that ever hits back (a mob out of reach on a
    // ledge, a burrowed one) is not a pull to judge.
    if (!struck)
    {
        LOG_INFO("playerbots", "Rolecheck: combat for {}s with no mob striking anyone, not counted",
                 (run.elapsed - run.fightAt) / 1000);
        std::lock_guard<std::mutex> guard(hookLock);
        fightHurt = fightHealed = false;
        opener.Clear();
        return;
    }
    CountFight();
    // One line a fight, to see where a run's numbers come from
    std::string offTank;
    for (auto const& [guid, samples] : run.fightOffTank)
        if (Player* who = ObjectAccessor::FindConnectedPlayer(guid))
            offTank += Acore::StringFormat("{}{} ({}) {}", offTank.empty() ? "" : ", ", who->GetName(),
                                           guid == run.gm ? 'P' : Role(who), samples);
    std::string faced;
    for (auto const& [guid, samples] : run.fightFaced)
        if (Player* who = ObjectAccessor::FindConnectedPlayer(guid))
            faced += Acore::StringFormat("{}{} ({}) {}", faced.empty() ? "" : ", ", who->GetName(), Role(who), samples);
    Player* opened = ObjectAccessor::FindConnectedPlayer(run.fightOpener);
    LOG_INFO("playerbots", "Rolecheck: fight {}: {}s, up to {} mobs, opened by {} ({}), on the tank {}/{} samples{}{}{}{}",
             run.tally.fights, (run.elapsed - run.fightAt) / 1000, run.fightMobCount,
             opened ? opened->GetName() : "?", run.fightOpenerHow, run.fightOnTank, run.fightMobSamples,
             offTank.empty() ? "" : "; off the tank on ", offTank, faced.empty() ? "" : "; in front of a mob: ",
             faced);
    std::lock_guard<std::mutex> guard(hookLock);
    if (fightHurt)
    {
        ++run.tally.hurtFights;
        if (fightHealed)
            ++run.tally.healedFights;
    }
    fightHurt = fightHealed = false;
    opener.Clear();
}

void CountDeaths(Player* gm)
{
    for (Member& member : run.bots)
        if (Player* bot = BotOf(member))
        {
            bool const dead = !bot->IsAlive();
            if (dead && !member.dead)
            {
                ++run.tally.deaths;
                Say(Acore::StringFormat("{} ({}) died", bot->GetName(), Role(bot)));
            }
            member.dead = dead;
        }
    for (ObjectGuid const& guid : run.fightMobs)
    {
        if (run.dead.count(guid))
            continue;
        Creature* mob = gm->GetMap()->GetCreature(guid);
        if (!mob || mob->IsAlive())
            continue;
        run.dead.insert(guid);
        ++run.tally.kills;
        if (mob->IsDungeonBoss() || mob->isWorldBoss())
        {
            ++run.tally.bosses;
            Say(Acore::StringFormat("boss {} down", mob->GetName()));
        }
    }
}

// Out of combat a while, the dead get up beside the group; when nobody is left standing it is a wipe, back at the door
void ReviveDead(Player* gm, std::vector<Player*> const& bots)
{
    if (run.calm < ReviveMs)
        return;
    bool const anyDead = std::any_of(bots.begin(), bots.end(), [](Player* bot) { return !bot->IsAlive(); });
    if (!anyDead)
        return;
    Player* beside = MainTank();
    if (!beside)
        for (Player* bot : bots)
            if (bot->IsAlive())
                beside = bot;
    if (!beside)
    {
        ++run.wipes;
        Say(Acore::StringFormat("wipe {}: everyone back at the entrance", run.wipes));
        gm->TeleportTo(run.entrance);
        for (Player* bot : bots)
        {
            Revive(bot, nullptr);
            bot->TeleportTo(run.entrance);
        }
        return;
    }
    for (Player* bot : bots)
        if (!bot->IsAlive())
            Revive(bot, beside);
}

// Dungeon: once the tank has led on, the GM catches up the way it came, a few yards short of the tank. Not every few
// seconds and not ahead of it: the group's formation follows the GM, so a GM that keeps landing beside the tank drags
// a waiting tank on, pack after pack. Not in a fight either, where the tank steps out to pull and back.
void Follow(Player* gm)
{
    Player* tank = MainTank();
    if (!tank || tank->GetMapId() != gm->GetMapId() || tank->IsInCombat() || gm->GetExactDist(tank) < CatchUpAt)
        return;
    float const angle = tank->GetAbsoluteAngle(gm);
    gm->NearTeleportTo(tank->GetPositionX() + 5.0f * std::cos(angle), tank->GetPositionY() + 5.0f * std::sin(angle),
                       tank->GetPositionZ() + 0.5f, angle + float(M_PI));
}

bool Pullable(Creature const* mob, Player* gm)
{
    return mob->IsAlive() && !mob->IsInCombat() && mob->IsHostileTo(gm) && !mob->IsTotem() &&
           !mob->GetCharmerOrOwnerGUID().IsPlayer() &&
           !mob->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE) && !mob->IsImmuneToPC() &&
           mob->GetCreatureTemplate()->type != CREATURE_TYPE_CRITTER;
}

bool IsBoss(Creature const* mob) { return mob->IsDungeonBoss() || mob->isWorldBoss(); }

// Raid: the nearest mob the GM can pull, not already fighting; trash, or a boss once no trash is left about
Creature* NextTrash(Player* gm, bool bosses)
{
    std::list<Creature*> near;
    Acore::AnyUnitInObjectRangeCheck check(gm, TrashReach);
    Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(gm, near, check);
    Cell::VisitObjects(gm, searcher, TrashReach);
    Creature* best = nullptr;
    for (Creature* mob : near)
    {
        if (!Pullable(mob, gm) || IsBoss(mob) != bosses || run.tried.count(mob->GetGUID()))
            continue;
        if (!best || gm->GetExactDist(mob) < gm->GetExactDist(best))
            best = mob;
    }
    return best;
}

struct RaidSpawn
{
    ObjectGuid::LowType spawnId;
    Position pos;
    uint8 spawnMask;
    bool boss;
};

// Every hostile spawn of the raid, from the spawn data: the map only loads the grids near its players, so a raid with
// nothing in reach of the door (Vault of Archavon's four bosses, Ulduar's long way in, Icecrown's ramparts) looked
// empty and the run ended at once
std::vector<RaidSpawn> const& RaidSpawns(uint32 mapId)
{
    static std::map<uint32, std::vector<RaidSpawn>> cache;
    if (auto found = cache.find(mapId); found != cache.end())
        return found->second;
    std::vector<RaidSpawn>& spawns = cache[mapId];
    uint32 const skipped = CREATURE_FLAG_EXTRA_TRIGGER | CREATURE_FLAG_EXTRA_CIVILIAN;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        if (data.mapid != mapId)
            continue;
        CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(data.id);
        FactionTemplateEntry const* faction = info ? sFactionTemplateStore.LookupEntry(info->faction) : nullptr;
        if (!info || !faction || !faction->IsHostileToPlayers() || info->npcflag || (info->flags_extra & skipped) ||
            info->type == CREATURE_TYPE_CRITTER || info->type == CREATURE_TYPE_NON_COMBAT_PET ||
            (info->unit_flags & (UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE)))
            continue;
        bool const boss = (info->flags_extra & CREATURE_FLAG_EXTRA_DUNGEON_BOSS) || info->rank == CREATURE_ELITE_WORLDBOSS;
        spawns.push_back({spawnId, Position(data.posX, data.posY, data.posZ), data.spawnMask, boss});
    }
    return spawns;
}

// Where the GM stands to pull a mob out of reach: PullFrom yards off it on open ground it can see, else nearer
bool PullSpot(Map* map, Position const& mob, float facing, Position& spot)
{
    for (float const reach : {PullFrom, PullFrom / 2.0f, 8.0f})
        for (int turn = 0; turn < 8; ++turn)
        {
            float const angle = facing + float(turn) * float(M_PI) / 4.0f;
            float const x = mob.GetPositionX() + reach * std::cos(angle);
            float const y = mob.GetPositionY() + reach * std::sin(angle);
            float const z = map->GetHeight(PHASEMASK_NORMAL, x, y, mob.GetPositionZ() + 5.0f);
            if (z <= INVALID_HEIGHT || std::fabs(z - mob.GetPositionZ()) > 6.0f ||
                !map->isInLineOfSight(x, y, z + 2.0f, mob.GetPositionX(), mob.GetPositionY(), mob.GetPositionZ() + 2.0f,
                                      PHASEMASK_NORMAL, LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
                continue;
            spot.Relocate(x, y, z + 0.5f, angle + float(M_PI));
            return true;
        }
    return false;
}

// Nothing to pull in reach: on to the nearest spawn still alive further in, trash before bosses, as a player walks the
// raid. Each spawn is gone to once; one that turns out sealed or empty is passed over next time. The group comes along.
bool WalkOn(Player* gm, std::vector<Player*> const& bots)
{
    Map* map = gm->GetMap();
    RaidSpawn const* best = nullptr;
    for (RaidSpawn const& spawn : RaidSpawns(run.mapId))
    {
        if (!(spawn.spawnMask & (1 << map->GetSpawnMode())) || run.walkedTo.count(spawn.spawnId))
            continue;
        auto const bounds = map->GetCreatureBySpawnIdStore().equal_range(spawn.spawnId);
        bool alive = bounds.first == bounds.second ? map->GetCreatureRespawnTime(spawn.spawnId) <= time(nullptr) : false;
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            alive = alive || (itr->second && itr->second->IsInWorld() && Pullable(itr->second, gm));
        if (!alive)
            continue;
        if (!best || best->boss > spawn.boss ||
            (best->boss == spawn.boss && gm->GetExactDist(&spawn.pos) < gm->GetExactDist(&best->pos)))
            best = &spawn;
    }
    if (!best)
        return false;
    run.walkedTo.insert(best->spawnId);
    run.walkWait = WalkMs;
    Position spot;
    if (!PullSpot(map, best->pos, best->pos.GetAbsoluteAngle(gm), spot))
        return true;  // no ground to stand on near it: the next one
    LOG_INFO("playerbots", "Rolecheck: on to {} at {:.0f} yd", best->boss ? "a boss" : "trash", gm->GetExactDist(&spot));
    gm->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), spot.GetOrientation());
    for (Player* bot : bots)
        if (bot->IsAlive() && bot->GetMapId() == gm->GetMapId())
            bot->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), spot.GetOrientation());
    return true;
}

void RaidPull(Player* gm, std::vector<Player*> const& bots, uint32 diff)
{
    if (run.walkWait)
    {
        run.walkWait = run.walkWait > diff ? run.walkWait - diff : 0;
        return;
    }
    Creature* mob = run.pullTarget.IsEmpty() ? nullptr : gm->GetMap()->GetCreature(run.pullTarget);
    if (!mob || !mob->IsAlive() || mob->IsInCombat())
    {
        mob = NextTrash(gm, false);
        if (!mob && WalkOn(gm, bots))
            return;
        if (!mob)
            mob = NextTrash(gm, true);
        if (!mob)
        {
            Report();
            return;
        }
        run.pullTarget = mob->GetGUID();
        run.gathering = 0;
        float const angle = mob->GetAngle(gm);
        float const reach = std::min(PullFrom, gm->GetExactDist(mob));
        gm->NearTeleportTo(mob->GetPositionX() + reach * std::cos(angle), mob->GetPositionY() + reach * std::sin(angle),
                           mob->GetPositionZ() + 0.5f, angle + float(M_PI));
        return;
    }
    run.gathering += diff;
    if (run.gathering < GatherMs)
        return;
    // The player's pull: the mob comes for the GM, and its pack with it
    {
        std::lock_guard<std::mutex> guard(hookLock);
        opener = gm->GetGUID();
        openerHow = "the pull";
    }
    run.tried.insert(mob->GetGUID());
    mob->AI()->AttackStart(gm);
    mob->CallAssistance();
    run.pullTarget.Clear();
}

void Observe(Player* gm, uint32 diff)
{
    std::vector<Player*> const bots = Bots();
    // Out of the world for a moment is a far teleport; the run waits it out, and stops only for a bot that stays gone
    if (bots.size() != run.bots.size())
    {
        run.away += diff;
        if (run.away < AwayMs)
            return;
        std::string gone;
        for (Member const& member : run.bots)
            if (!BotOf(member))
            {
                Player* bot = ObjectAccessor::FindConnectedPlayer(member.guid);
                gone += Acore::StringFormat("{}{}{}", gone.empty() ? "" : ", ",
                                            bot ? bot->GetName() : std::string(member.seat.name),
                                            !bot ? " (logged out)" : bot->IsBeingTeleported() ? " (teleporting)" : "");
            }
        Stop("a bot left the run: " + gone);
        return;
    }
    if (run.away)
    {
        LOG_INFO("playerbots", "Rolecheck: a bot was out of the world for {}s", run.away / 1000);
        run.away = 0;
    }
    std::vector<Player*> members = bots;
    members.push_back(gm);
    bool const fighting = std::any_of(members.begin(), members.end(),
                                      [](Player* m) { return m->IsAlive() && m->IsInCombat(); });
    if (fighting)
    {
        run.calm = 0;
        if (!run.inFight)
            StartFight(members);
    }
    else
    {
        run.calm += diff;
        if (run.inFight && run.calm >= CalmMs)
            EndFight();
    }

    run.sinceSample += diff;
    if (run.sinceSample >= SampleMs)
    {
        run.sinceSample = 0;
        CountDeaths(gm);
        if (run.inFight && run.elapsed - run.fightAt >= RoleCheck::GraceMs)
            Sample(gm, bots);
        if (Player* tank = MainTank())
        {
            if (run.tankKnown && tank->GetMapId() == run.mapId)
                run.tankTravel += tank->GetExactDist(&run.tankWas);
            run.tankWas = tank->GetPosition();
            run.tankKnown = tank->GetMapId() == run.mapId;
        }
    }
    if (!run.inFight)
        ReviveDead(gm, bots);

    run.sinceFollow += diff;
    if (!run.raid && run.sinceFollow >= FollowMs)
    {
        run.sinceFollow = 0;
        Follow(gm);
    }
    if (run.raid && !run.inFight && run.calm >= ReviveMs)
        RaidPull(gm, bots, diff);
    if (run.phase != Phase::Observe)
        return;

    run.sinceStatus += diff;
    if (run.sinceStatus >= StatusMs)
    {
        run.sinceStatus = 0;
        Status();
    }
    if (run.elapsed >= run.durationMs)
        Report();
}

void Report()
{
    if (run.inFight)
        EndFight();
    Status();
    std::vector<RoleCheck::Line> const lines = RoleCheck::Judge(run.tally, !run.raid);
    bool const passed = RoleCheck::Passed(lines);
    std::string row = Acore::StringFormat("| {} | {} | {} | {}m |", Acore::Time::TimeToTimestampStr(GameTime::GetGameTime()).substr(0, 10),
                                          Mode(), MapName(run.mapId), run.elapsed / 60000);
    for (RoleCheck::Line const& line : lines)
    {
        bool const count = line.what == "fights";
        std::string const value = line.pct < 0.0 ? "-" : Acore::StringFormat("{:.0f}{}", line.pct, count ? "" : "%");
        Say(Acore::StringFormat("{}: {} ({} {:.0f}{}) {}", line.what, value, line.atMost ? "at most" : "at least",
                                line.bound, count ? "" : "%", line.pass ? "pass" : "FAIL"));
        row += " " + value + (line.pass ? "" : " ✗") + " |";
        // A raid has no tank-pull column: the player pulls
        if (count && run.raid)
            row += " - |";
    }
    row += Acore::StringFormat(" {} / {} / {} / {} | {} |", run.tally.kills, run.tally.bosses, run.tally.deaths,
                               run.wipes, passed ? "pass" : "FAIL");
    Say("row: " + row);
    std::string const file = sConfigMgr->GetOption<std::string>("LogsDir", ".") + "/rolecheck.md";
    bool const fresh = !std::ifstream(file).good();
    std::ofstream out(file, std::ios::app);
    if (fresh)
        out << "# Role check\n\n| Date | Kind | Instance | Time | Fights | Tank pulls | Mobs on tank | Facing group "
               "| DPS on tank's target | Healer back | Hurt fights healed | Kills / bosses / deaths / wipes | Verdict |\n"
               "|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    out << row << "\n";
    Stop("done");
}

void Tick(uint32 diff)
{
    if (run.phase == Phase::Idle)
        return;
    Player* gm = Gm();
    if (!gm)
    {
        Stop("the GM left");
        return;
    }
    run.elapsed += diff;
    switch (run.phase)
    {
        case Phase::Login:
        {
            PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(gm);
            for (Member& member : run.bots)
            {
                if (!member.guid.IsEmpty())
                    continue;
                for (auto itr = mgr->GetPlayerBotsBegin(); itr != mgr->GetPlayerBotsEnd(); ++itr)
                {
                    Player* bot = itr->second;
                    bool const taken = std::find(run.before.begin(), run.before.end(), bot->GetGUID()) != run.before.end() ||
                                       std::any_of(run.bots.begin(), run.bots.end(),
                                                   [&](Member const& m) { return m.guid == bot->GetGUID(); });
                    if (!taken && bot->getClass() == member.seat.classId && bot->IsInWorld() && GET_PLAYERBOT_AI(bot))
                    {
                        member.guid = bot->GetGUID();
                        break;
                    }
                }
            }
            bool const allIn = std::all_of(run.bots.begin(), run.bots.end(), [&](Member const& m) {
                Player* bot = BotOf(m);
                return bot && bot->GetGroup() && bot->GetGroup() == gm->GetGroup();
            });
            if (allIn)
            {
                for (Member const& member : run.bots)
                    Prepare(BotOf(member), member.seat);
                if (!Enter(gm))
                    return;
                Say(Acore::StringFormat("{} bots ready at level {}; into {} ({})", run.bots.size(), run.era.level,
                                        MapName(run.mapId), Mode()));
                run.phase = Phase::Settle;
                run.elapsed = 0;
            }
            else if (run.elapsed > LoginTimeoutMs)
                Stop("the bots didn't all log in and join the group (addclass needs free characters of each class)");
            return;
        }
        case Phase::Settle:
        {
            if (run.elapsed < SettleMs)
                return;
            std::vector<Player*> everyone = Bots();
            if (everyone.size() != run.bots.size())
            {
                Stop("a bot left the run");
                return;
            }
            everyone.push_back(gm);
            Map* inside = nullptr;
            for (Player* member : everyone)
                if (member->GetMapId() == run.mapId)
                    inside = member->GetMap();
            std::string outside;
            for (Player* member : everyone)
                if (member->GetMapId() != run.mapId)
                {
                    // Why the map turns it away, if it does (Map::EnterState: 5 not in a raid, 7 bind mismatch,
                    // 8 too many instances, 9 full, 10 a boss fight on): the entry check, then the instance the rest
                    // are in, whose own refusal at the far end of the teleport sends a player home
                    Map::EnterState refused = sMapMgr->PlayerCannotEnter(run.mapId, member);
                    if (!refused && inside)
                        refused = inside->CannotEnter(member, false);
                    outside += Acore::StringFormat("{}{} (map {}{}{})", outside.empty() ? "" : ", ", member->GetName(),
                                                   member->GetMapId(), member->IsBeingTeleported() ? ", teleporting" : "",
                                                   refused ? Acore::StringFormat(", refused {}", uint32(refused)) : "");
                    // Once more: a far teleport into an instance can be refused while the instance is being made
                    if (run.elapsed < 2 * SettleMs && !member->IsBeingTeleported())
                        member->TeleportTo(run.entrance);
                }
            if (!outside.empty())
            {
                if (run.elapsed < 3 * SettleMs)
                    return;
                Stop(Acore::StringFormat("not everyone reached {}: {}", MapName(run.mapId), outside));
                return;
            }
            Describe();
            if (run.raid)
                gm->SetGameMaster(false);  // the mobs must see the GM to come for it
            run.phase = Phase::Observe;
            run.elapsed = 0;
            return;
        }
        case Phase::Observe:
            Observe(gm, diff);
            return;
        default:
            return;
    }
}

class RoleCheckUnitScript : public UnitScript
{
public:
    RoleCheckUnitScript() : UnitScript("RoleCheckUnitScript", true) {}

    uint32 DealDamage(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType /*type*/) override
    {
        if (run.phase != Phase::Observe || !damage || !attacker || !victim || attacker == victim)
            return damage;
        Player* striker = RunPlayerOf(attacker);
        Player* struck = RunPlayerOf(victim);
        // Only a hit between the run and the world counts; anything else stays off the lock every map thread shares
        if ((striker && struck) || (!striker && !struck))
            return damage;
        std::lock_guard<std::mutex> guard(hookLock);
        if (!run.inFight && opener.IsEmpty() && (striker || struck))
        {
            opener = striker ? striker->GetGUID() : struck->GetGUID();
            openerHow = striker ? Acore::StringFormat("{} hit {}", attacker == striker ? "it" : attacker->GetName(),
                                                      victim->GetName())
                                : Acore::StringFormat("struck by {}", attacker->GetName());
        }
        if (struck && attacker->ToCreature())
            fightStruck = true;
        // Hurt: under the health a bot healer heals at, not a scratch it rightly leaves alone
        if (struck && (victim->GetHealth() <= damage || float(victim->GetHealth() - damage) * 100.0f <
                                                            float(victim->GetMaxHealth()) * RoleCheck::HurtHealthPct))
            fightHurt = true;
        return damage;
    }

    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* /*spellInfo*/) override
    {
        if (run.phase != Phase::Observe || !heal || !healer || !target)
            return;
        Player* from = healer->ToPlayer();
        if (!from || !IsRunPlayer(from->GetGUID()) || !PlayerbotAI::IsHeal(from, true) || !RunPlayerOf(target) ||
            target->GetHealth() >= target->GetMaxHealth())
            return;
        std::lock_guard<std::mutex> guard(hookLock);
        fightHealed = true;
    }
};

class RoleCheckWorldScript : public WorldScript
{
public:
    RoleCheckWorldScript() : WorldScript("RoleCheckWorldScript", {WORLDHOOK_ON_UPDATE}) {}

    void OnUpdate(uint32 diff) override { Tick(diff); }
};

class RoleCheckCommandScript : public CommandScript
{
public:
    RoleCheckCommandScript() : CommandScript("RoleCheckCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable roleTable = {
            {"dungeon", HandleDungeon, SEC_GAMEMASTER, Console::No},
            {"raid", HandleRaid, SEC_GAMEMASTER, Console::No},
            {"status", HandleStatus, SEC_GAMEMASTER, Console::No},
            {"stop", HandleStop, SEC_GAMEMASTER, Console::No}};
        static ChatCommandTable commandTable = {{"rolecheck", roleTable}};
        return commandTable;
    }

    static bool Begin(ChatHandler* handler, bool raid, uint32 mapId, uint32 minutes, uint32 level)
    {
        if (run.phase != Phase::Idle)
        {
            handler->SendErrorMessage("A role check is going; .rolecheck stop ends it.", false);
            return false;
        }
        Player* gm = handler->GetPlayer();
        MapEntry const* map = sMapStore.LookupEntry(mapId);
        if (!map || (raid ? !map->IsRaid() : !map->IsNonRaidDungeon()))
        {
            handler->SendErrorMessage(Acore::StringFormat("Map {} isn't a {}.", mapId, raid ? "raid" : "dungeon"), false);
            return false;
        }
        AreaTriggerTeleport const* door = sObjectMgr->GetMapEntranceTrigger(mapId);
        if (!door)
        {
            handler->SendErrorMessage(Acore::StringFormat("No entrance known for map {}.", mapId), false);
            return false;
        }
        Era const era = EraOf(map, level);
        if (gm->GetLevel() < era.level)
        {
            handler->SendErrorMessage(Acore::StringFormat("The GM needs level {} or more for {}.", era.level,
                                                          MapName(mapId)),
                                      false);
            return false;
        }
        // GM mode on to get there: the progression gate keeps a player without the era behind it out of Northrend.
        // In a dungeon it stays on, so no mob fights the GM; in a raid it goes off once everyone is in, for the pulls
        if (!gm->IsGameMaster())
        {
            handler->SendErrorMessage("A role check needs GM mode on (.gm on); a raid turns it off once inside.",
                                      false);
            return false;
        }
        if (gm->GetGroup())
        {
            handler->SendErrorMessage("Leave your group first: the check makes its own.", false);
            return false;
        }
        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(gm);
        if (!mgr)
        {
            handler->SendErrorMessage("No bot manager for this character.", false);
            return false;
        }
        run = Run{};
        run.raid = raid;
        run.gm = gm->GetGUID();
        run.mapId = mapId;
        run.era = era;
        run.durationMs = std::clamp<uint32>(minutes, 2, 60) * MINUTE * IN_MILLISECONDS;
        run.entrance = WorldLocation(door->target_mapId, door->target_X, door->target_Y, door->target_Z,
                                     door->target_Orientation);
        for (auto itr = mgr->GetPlayerBotsBegin(); itr != mgr->GetPlayerBotsEnd(); ++itr)
            run.before.push_back(itr->first);
        if (raid)
            for (Seat const& seat : RaidSeats)
                run.bots.push_back(Member{seat});
        else
            for (Seat const& seat : DungeonSeats)
                run.bots.push_back(Member{seat});
        // Every seat's spec first: a refused start leaves no god mode on and no bots logged in
        for (Member const& member : run.bots)
            if (SpecNo(member.seat) < 0)
            {
                std::string const error = Acore::StringFormat("No premade spec '{}' for class {}.", member.seat.spec,
                                                              member.seat.classId);
                run = Run{};
                handler->SendErrorMessage(error, false);
                return false;
            }
        if (raid)
            gm->SetCommandStatusOn(CHEAT_GOD);
        for (Member const& member : run.bots)
        {
            std::string add = Acore::StringFormat("addclass {}", member.seat.name);
            for (std::string const& reply : mgr->HandlePlayerbotCommand(add.data(), gm))
                LOG_INFO("playerbots", "Rolecheck: {}", reply);
        }
        {
            std::lock_guard<std::mutex> guard(hookLock);
            opener.Clear();
            fightHurt = fightHealed = false;
        }
        run.instancesPerHour = sWorld->getIntConfig(CONFIG_MAX_INSTANCES_PER_HOUR);
        sWorld->setIntConfig(CONFIG_MAX_INSTANCES_PER_HOUR, std::numeric_limits<uint32>::max());
        run.phase = Phase::Login;
        handler->PSendSysMessage("Rolecheck: {} in {} for {} minutes; logging in {} bots.", Mode(),
                                 MapName(mapId), run.durationMs / 60000, run.bots.size());
        return true;
    }

    static bool HandleDungeon(ChatHandler* handler, uint32 mapId, Optional<uint32> minutes, Optional<uint32> level)
    {
        return Begin(handler, false, mapId, minutes.value_or(10), level.value_or(0));
    }

    static bool HandleRaid(ChatHandler* handler, uint32 mapId, Optional<uint32> minutes, Optional<uint32> level)
    {
        return Begin(handler, true, mapId, minutes.value_or(10), level.value_or(0));
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        if (run.phase == Phase::Idle)
            handler->SendSysMessage("Rolecheck: no run going.");
        else
            Status();
        return true;
    }

    static bool HandleStop(ChatHandler* handler)
    {
        if (run.phase == Phase::Idle)
        {
            handler->SendSysMessage("Rolecheck: no run going.");
            return true;
        }
        Stop("stopped");
        return true;
    }
};
}  // namespace

void AddRoleCheckScripts()
{
    new RoleCheckUnitScript();
    new RoleCheckWorldScript();
    new RoleCheckCommandScript();
}
