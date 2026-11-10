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
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include "AiFactory.h"
#include "AttackersValue.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "DungeonLead.h"
#include "Engine.h"
#include "GameObject.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "InstanceScript.h"
#include "LivingWorldMgr.h"
#include "Log.h"
#include "Map.h"
#include "LastMovementValue.h"
#include "MovementActions.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "RoleCheckPolicy.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "ScriptedGossip.h"
#include "Timer.h"
#include "UBShared.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

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
Era EraOfLevel(MapEntry const* map, uint32 level)
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

// A raid's own gear: what a group coming in would wear, the tier before it. All of TBC at Karazhan's 115 left The Eye
// (8 wipes, 0 kills) and Sunwell measuring the gear and not the roles
uint32 RaidItemLevel(uint32 mapId)
{
    switch (mapId)
    {
        case 565:  // Gruul's Lair
        case 544:  // Magtheridon's Lair
        case 568:  // Zul'Aman
            return 120;
        case 548:  // Serpentshrine Cavern
        case 550:  // Tempest Keep: The Eye
            return 128;
        case 534:  // Hyjal Summit
        case 564:  // Black Temple
            return 138;
        case 580:  // Sunwell Plateau
            return 146;
        case 603:  // Ulduar
            return 213;
        case 649:  // Trial of the Crusader
            return 226;
        case 631:  // Icecrown Citadel
            return 245;
        case 724:  // The Ruby Sanctum
            return 258;
        default:
            return 0;
    }
}

Era EraOf(MapEntry const* map, uint32 level)
{
    Era era = EraOfLevel(map, level);
    if (uint32 const raidLevel = map->IsRaid() && era.level >= 70 ? RaidItemLevel(map->MapID) : 0)
        era.gearItemLevel = raidLevel;
    return era;
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
// An event's own mobs, which come at the group on their own clock: pulled one at a time with 12 seconds' gathering
// each, the rifts' mobs drained Medivh's shield in under 4 minutes (The Black Morass)
constexpr uint32 EventGatherMs = 3000;
constexpr float WaveReach = 20.0f;  // an event's mobs this near the one pulled come with it
// raid: the GM pulls only with the tank this near, as a player waits for the tank; past GatherMaxMs the group is brought
// over. Pulling after GatherMs whatever, the GM held Naxxramas trash 110-180 yards from the tank, a quarter of the samples
constexpr float GatherReach = 40.0f;
constexpr uint32 GatherMaxMs = 60000;
constexpr float GatherRaid = 25.0f;  // raid: every bot this near the GM, and out of a fight, before it pulls
// raid: where the GM waits, short of the aggro of a mob and of the group spread round it (at 32 yards a healer
// walked into The Eye's pack)
constexpr float PullFrom = 40.0f;
constexpr float PackClear = 25.0f;     // raid: that far from any other pack, where there is such a spot
constexpr float PackOf = 12.0f;        // raid: mobs this near the one pulled are its pack
constexpr float TrashReach = 150.0f;
constexpr float EventReach = 400.0f;  // an event dungeon's mobs, once nothing is in TrashReach
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
    // healer samples not standing back, by why: a mob on the healer near it, a mob on another near it, out of reach
    uint32 fightHealerHit = 0, fightHealerNear = 0, fightHealerFar = 0;
    uint32 fightTraceAt = 0, fightTraceLogs = 0;  // the damage dealers' action traces in a big fight, every 5 s
    // each mob's victim at the last sample, to log what took a mob off the tank (a few a fight)
    std::map<ObjectGuid, ObjectGuid> mobVictim;
    uint32 fightSwitchLogs = 0;
    std::map<ObjectGuid, uint32> mobUntouched;  // samples each mob was on a member with no member on it
    ObjectGuid fightOpener;
    std::string fightOpenerHow;  // how: what the opener hit, or what struck it
    std::set<ObjectGuid> dead;  // mobs counted as killed
    float tankTravel = 0.0f;
    Position tankWas;
    bool tankKnown = false;

    // raid: the mob the GM pulls next
    ObjectGuid pullTarget;
    uint32 gathering = 0;
    bool broughtOver = false;  // the group was brought to the GM for this pull: pull on with the tank near
    bool eventPull = false;    // the mob pulled next is one of an event's own
    std::set<ObjectGuid::LowType> walkedTo;  // spawns the GM went on to
    uint32 walkWait = 0;  // after going on: the teleport lands at the next session update, and the grid loads
    std::set<ObjectGuid> tried;  // mobs pulled once: one that came back without a fight (no way to the GM) is let be
    uint32 away = 0;  // how long some bot has been out of the world

    // event instances: after a step, the event's own time to start; the starters walked up to or used, once each
    uint32 eventWait = 0;
    std::set<ObjectGuid> eventDone;
    bool eventStarted = false;  // a step was taken: from here mobs come at the group by the event's own doing
    // where the GM stood when it took the step: a step that moves the GM (Chromie's skip ahead puts the player in town)
    // brings the group after it, as the players would come along
    Position eventFrom;
    bool eventMoves = false;
    uint32 lastEventAt = 0;  // run.elapsed at the last step taken
    std::set<uint32> eventVisited;  // starters gone to while their grid was not loaded, by entry
    ObjectGuid lastStarter;  // the starter of the last step, for the status
    Position lastStarterAt;  // where it stood at the step: one that walks on takes the event with it
    // how often the GM was put beside each starter: three times at most, then the step, wherever the GM landed (a GM
    // put beside the Focusing Iris, still more than 4 yards off it, was put beside it again every 3 seconds for the
    // whole run: Eye of Eternity, 2026-11-08)
    std::map<ObjectGuid, uint8> eventBeside;

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
ObjectGuid fightHurtWho;  // the first member hurt in the fight, for the log of a hurt fight left unhealed
bool fightStruck = false;  // a mob struck a member: a real fight
// Each member's casts this fight, by spell, logged after a big fight to show whether area spells went out
std::map<ObjectGuid, std::map<uint32, uint32>> fightCasts;
uint32 constexpr CAST_LOG_MOBS = 10;

void Report();

Player* Gm() { return ObjectAccessor::FindConnectedPlayer(run.gm); }

// What a back-line member's step to its spot last did, or why no spot would do
std::string BackLineNote(PlayerbotAI* ai)
{
    if (!ai || !(PlayerbotAI::IsHeal(ai->GetBot()) || PlayerbotAI::IsRanged(ai->GetBot())))
        return "-";
    auto* action = dynamic_cast<BackLineTakeSpotAction*>(ai->GetAiObjectContext()->GetAction("back line take spot"));
    return action ? action->LastNote() : "-";
}

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

// Mobs a boss's own script sends at the group: struck by one, nobody pulled it. Azjol-Nerub's Krik'thir sends a watcher
// group every 60-70 seconds once one is engaged, and Hadronox climbs his tunnel to the group; a damage dealer they
// struck first failed the row on tank pulls (role check, heroic Azjol-Nerub, 2026-11-09).
bool SentByScript(uint32 mapId, uint32 entry)
{
    switch (mapId)
    {
        case 601:  // Azjol-Nerub: Narjil, Gashra and Silthik, their warriors, shadowcasters and skirmishers, Hadronox
            return (entry >= 28729 && entry <= 28734) || entry == 28921;
        case 546:  // The Underbog: the mushrooms Hungarfen grows, still bursting after him
            return entry == 17990;
        default:
            return false;
    }
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

std::string NearestHostiles(Player* gm);

void Status()
{
    Player* tank = MainTank();
    std::string lead = "no tank";
    if (tank)
    {
        PlayerbotAI* ai = GET_PLAYERBOT_AI(tank);
        lead = tank->GetName();
        // A raid: what the GM is pulling, and what holds the pull (a raid sat 10 minutes with nothing in its log)
        if (run.raid || run.eventStarted)
        {
            Player* gm = Gm();
            // The event's last starter, where it went and whether it wants talking to again (Culling of Stratholme
            // stood 11 minutes after Arthas's step with no wave)
            Creature* starter = gm && run.lastStarter.IsCreature() ? gm->GetMap()->GetCreature(run.lastStarter) : nullptr;
            if (starter)
                lead += Acore::StringFormat(" (starter {} at {:.0f} yd, {:.0f} {:.0f} {:.0f}, moving {}, gossip {}, in combat {}, "
                                            "casting {})",
                                            starter->GetName(), gm->GetExactDist(starter), starter->GetPositionX(),
                                            starter->GetPositionY(), starter->GetPositionZ(), starter->isMoving(),
                                            starter->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP), starter->IsInCombat(),
                                            starter->IsNonMeleeSpellCast(false));
            // Medivh's shield, which the event mobs drain: at 0 he dies and the event starts over (The Black Morass)
            if (gm && run.mapId == 269)
                if (InstanceScript* instance = gm->GetInstanceScript())
                    lead += Acore::StringFormat(" (shield {}%, rift {})", instance->GetData(13), instance->GetData(14));
            Creature* target = gm && !run.pullTarget.IsEmpty() ? gm->GetMap()->GetCreature(run.pullTarget) : nullptr;
            uint32 fighting = 0;
            for (Player* bot : Bots())
                fighting += bot->IsAlive() && bot->IsInCombat();
            lead += Acore::StringFormat(
                " (pulling {} at {:.0f} yd, gathering {}s, in a fight {}, calm {}s, bots in combat {}, GM in combat {}, "
                "event wait {}s, walk wait {}s, tried {}, walked to {})",
                target ? target->GetName() : "-", target ? gm->GetExactDist(target) : -1.0f, run.gathering / 1000,
                run.inFight, run.calm / 1000, fighting, gm && gm->IsInCombat(), run.eventWait / 1000,
                run.walkWait / 1000, run.tried.size(), run.walkedTo.size());
            // Nothing to pull: the nearest hostiles about and why each is passed over (Culling of Stratholme and the
            // Black Morass stood minutes with their waves and rifts out in the streets and nothing pulled)
            if (gm && !target && !run.inFight)
                lead += " (nothing pullable: " + NearestHostiles(gm) + ")";
        }
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
            // The member waited on: its mana and whether it sits to eat or drink (after a wipe the tank waited 90 s on
            // the healer 21 yards off, "behind", and pulled without the log saying whether it was drinking)
            if (waitingOn && waitingOn->IsAlive())
            {
                // and, standing off and not following (a healer 23 yards off at full mana, still), whom it follows
                PlayerbotAI* waitAi = GET_PLAYERBOT_AI(waitingOn);
                Player* master = waitAi ? waitAi->GetMaster() : nullptr;
                Engine* engine = waitAi ? waitAi->GetCurrentEngine() : nullptr;
                std::string trace = engine ? engine->GetLastAction() : "";
                if (trace.size() > 250)
                    trace = trace.substr(trace.size() - 250);
                lead += Acore::StringFormat(
                    " (waited-on mana {:.0f}%, health {:.0f}%, sitting {}, moving {}, motion {}, master {} at {:.0f} yd; "
                    "last actions {})",
                    waitingOn->GetMaxPower(POWER_MANA) > 0 ? waitingOn->GetPowerPct(POWER_MANA) : -1.0f,
                    waitingOn->GetHealthPct(), waitingOn->IsSitState(), waitingOn->isMoving(),
                    uint32(waitingOn->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                    master ? master->GetName() : "-",
                    master && master->GetMap() == waitingOn->GetMap() ? waitingOn->GetExactDist(master) : -1.0f, trace);
            }
            // Every goal blocked from the door: where the tank stands and what the path search makes of the way on
            // (Ahn'kahet: 119 goals blocked in 30 seconds, nobody moved a yard)
            if (state.hasGoal && !state.blockedGoals.empty() && !tank->IsInCombat())
            {
                PathGenerator path(tank);
                path.SetUseStraightPath(true);
                path.CalculatePath(state.goal.GetPositionX(), state.goal.GetPositionY(), state.goal.GetPositionZ(), false);
                lead += Acore::StringFormat(" (tank at {:.1f} {:.1f} {:.1f}, path type {}, {} points)", tank->GetPositionX(),
                                            tank->GetPositionY(), tank->GetPositionZ(), uint32(path.GetPathType()),
                                            path.GetPath().size());
            }
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
        // A member far from the tank: where it is and what it does (heroic Azjol-Nerub: a mage alive and moving out to
        // 568 yards after the first pull, the tank waiting on it for 90 seconds)
        for (Player* bot : Bots())
        {
            PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
            Engine* engine = ai ? ai->GetCurrentEngine() : nullptr;
            if (bot == tank || bot->GetMap() != tank->GetMap() || bot->GetExactDist(tank) < 60.0f || !engine)
                continue;
            std::string trace = engine->GetLastAction();
            if (trace.size() > 400)
                trace = trace.substr(trace.size() - 400);
            LOG_INFO("playerbots",
                     "Rolecheck: {} far from the tank: at {:.1f} {:.1f} {:.1f} (tank z {:.1f}), alive {}, combat {}, "
                     "motion {}, falling {}; its last actions: {}",
                     bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), tank->GetPositionZ(),
                     bot->IsAlive(), bot->IsInCombat(), uint32(bot->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                     bot->HasUnitMovementFlag(MOVEMENTFLAG_FALLING), trace);
        }
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
        // A rider is fought through its vehicle: a joust's champions strike the mount, never the rider, so the
        // Trial of the Champion waves showed "up to 0 mobs" and their kills went uncounted
        if (Unit* vehicle = member->GetVehicleBase())
            for (Unit* attacker : vehicle->getAttackers())
                add(attacker);
        add(member->GetVictim());
    }
    return mobs;
}

// A bot on a point move to well below where it stands: what sent it there, caught as it starts. In heroic Azjol-Nerub
// two bots sank straight down through the floor, x and y fixed and z falling 68 yards each 15 s, out to 500 yards
// from the tank, on a point move begun mid-fight (role check, 2026-11-09).
void WatchSinking(std::vector<Player*> const& bots)
{
    static std::map<ObjectGuid, uint32> loggedAt;
    for (Player* bot : bots)
    {
        float x, y, z;
        if (!bot->IsAlive() || bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE ||
            !bot->GetMotionMaster()->GetDestination(x, y, z) || bot->GetPositionZ() - z < 25.0f ||
            bot->GetExactDist2d(x, y) > 15.0f)
            continue;
        uint32& at = loggedAt[bot->GetGUID()];
        if (at && run.elapsed - at < 10000)
            continue;
        at = run.elapsed;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
        Engine* engine = ai ? ai->GetCurrentEngine() : nullptr;
        std::string trace = engine ? engine->GetLastAction() : "";
        if (trace.size() > 800)
            trace = trace.substr(trace.size() - 800);
        std::string last;
        if (ai)
        {
            LastMovement& move = ai->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
            last = Acore::StringFormat("{:.1f} {:.1f} {:.1f}, priority {}, {} ms ago", move.lastMoveToX,
                                       move.lastMoveToY, move.lastMoveToZ, uint32(move.priority),
                                       getMSTimeDiff(move.msTime, getMSTime()));
        }
        LOG_INFO("playerbots",
                 "Rolecheck: {} sinking: at {:.1f} {:.1f} {:.1f}, moving to {:.1f} {:.1f} {:.1f}; combat {}, flying {}, "
                 "swimming {}, on transport {}; last bot move to {}; its last actions: {}",
                 bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), x, y, z,
                 bot->IsInCombat(), bot->IsFlying(), bot->IsInWater(), bot->GetTransport() != nullptr, last, trace);
    }
}

void Sample(Player* gm, std::vector<Player*> const& bots)
{
    RoleCheck::Tally& t = run.tally;
    std::vector<Player*> members = bots;
    members.push_back(gm);
    std::vector<Creature*> const mobs = FightMobs(members);
    Player* tank = MainTank();
    run.fightMobCount = std::max<uint32>(run.fightMobCount, mobs.size());

    // Vehicle combat has no roles: riders cast their vehicle's spells and mobs strike the vehicles, so nobody holds a
    // mob, heals or follows the tank's target. The Trial of the Champion joust had the tank "off the tank" on its own
    // warhorse and dps focus at 0%; while a bot rides, the sample is left out, not failed.
    if (std::any_of(bots.begin(), bots.end(), [](Player* bot) { return bot->IsAlive() && bot->GetVehicle(); }))
        return;
    // Nor while the mobs ride: a bot unhorsed in the joust is on foot against mounted champions, its role's place
    // nowhere to be had until it is back on a mount (Trial of the Champion, "healer back 0%" on those samples)
    if (std::any_of(mobs.begin(), mobs.end(), [](Creature* mob) { return mob->GetVehicle() || mob->GetVehicleKit(); }))
        return;

    // In a big fight, each damage dealer's whole last-action trace now and then: why its area spells did or did not
    // go out (the traces logged elsewhere are cut to their last 300 characters)
    if (mobs.size() >= CAST_LOG_MOBS && run.fightTraceLogs < 4 * bots.size() &&
        (run.elapsed - run.fightAt) / 5000 > run.fightTraceAt)
    {
        run.fightTraceAt = (run.elapsed - run.fightAt) / 5000;
        for (Player* bot : bots)
        {
            PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
            Engine* engine = ai ? ai->GetCurrentEngine() : nullptr;
            if (!engine || !bot->IsAlive() || !PlayerbotAI::IsDps(bot))
                continue;
            ++run.fightTraceLogs;
            LOG_INFO("playerbots", "Rolecheck: {} ({}) at {}s into a {}-mob fight, casting {}: {}", bot->GetName(),
                     Role(bot), (run.elapsed - run.fightAt) / 1000, mobs.size(),
                     bot->IsNonMeleeSpellCast(false), engine->GetLastAction());
        }
    }

    // With the tank dead nobody holds the mobs: that shows in the deaths, not here
    for (Creature* mob : tank ? mobs : std::vector<Creature*>())
    {
        Unit* victim = mob->GetVictim();
        Player* hit = victim ? victim->ToPlayer() : nullptr;
        if (!victim || !RunPlayerOf(victim))
            continue;
        ++t.mobSamples;
        ++run.fightMobSamples;
        // A mob that goes to someone other than the tank, first or off it: the threat on both, and what the one it
        // went to last did
        ObjectGuid& was = run.mobVictim[mob->GetGUID()];
        if (victim->GetGUID() != was)
        {
            if (hit && !PlayerbotAI::IsTank(hit, true) && (was.IsEmpty() || was == tank->GetGUID()) &&
                run.fightSwitchLogs < 12)
            {
                ++run.fightSwitchLogs;
                std::string trace;
                if (PlayerbotAI* hitAi = GET_PLAYERBOT_AI(hit))
                    if (Engine* engine = hitAi->GetCurrentEngine())
                    {
                        trace = engine->GetLastAction();
                        if (trace.size() > 200)
                            trace = trace.substr(trace.size() - 200);
                    }
                // And what the tank was at: a low-level tank's threat is what it does with little rage
                std::string tankTrace;
                if (PlayerbotAI* tankAi = GET_PLAYERBOT_AI(tank))
                    if (Engine* engine = tankAi->GetCurrentEngine())
                    {
                        tankTrace = engine->GetLastAction();
                        if (tankTrace.size() > 300)
                            tankTrace = tankTrace.substr(tankTrace.size() - 300);
                    }
                Unit* tankOn = tank->GetVictim();
                LOG_INFO("playerbots",
                         "Rolecheck: {} {} {} ({}) at {}s into the fight: threat tank {:.0f}, {} {:.0f}, {:.0f} yd "
                         "from the tank; its last actions: {}; the tank on {} ({} mobs on it), rage {}, {:.0f} yd from "
                         "this mob, moving {}, melee {}; its last actions: {}",
                         mob->GetName(), was.IsEmpty() ? "went first to" : "left the tank for", hit->GetName(),
                         Role(hit), (run.elapsed - run.fightAt) / 1000, mob->GetThreatMgr().GetThreat(tank),
                         hit->GetName(), mob->GetThreatMgr().GetThreat(hit), hit->GetExactDist(tank), trace,
                         tankOn ? tankOn->GetName() : "-", uint32(tank->getAttackers().size()),
                         tank->GetPower(POWER_RAGE) / 10, tank->GetExactDist(mob), tank->isMoving(),
                         tank->HasUnitState(UNIT_STATE_MELEE_ATTACKING), tankTrace);
            }
            was = victim->GetGUID();
        }
        // A mob on a member that nobody fights back: why the tank leaves it, once a fight for each such mob (Auchenai
        // Crypts: a Phasing Soldier beside the tank for 500 seconds, no bot on it, the fight never ending)
        // (a pack mob waiting its turn while the group fights another is no stall: only a group fighting nothing at all)
        bool fought = false;
        for (Player* member : members)
            if (member->IsAlive() && member->GetVictim())
                fought = true;
        uint32& untouched = run.mobUntouched[mob->GetGUID()];
        untouched = fought ? 0 : untouched + 1;
        if (untouched == 40)
        {
            Creature* c = mob;
            LOG_INFO("playerbots",
                     "Rolecheck: {} on {} with no member on it for {} samples: {:.1f} yd from the tank, z {:.1f} to the "
                     "tank's {:.1f}; visible {}, evading {}, flags 0x{:X}, immune to melee {} and to magic {}, the "
                     "tank sees it {}, in sight {}, may attack it {}, possible target {}, in combat with the tank {}, "
                     "threat tank {:.0f}, moving {}, casting {}",
                     mob->GetName(), victim->GetName(), untouched, mob->GetExactDist(tank), mob->GetPositionZ(),
                     tank->GetPositionZ(), mob->IsVisible(), c->IsInEvadeMode(), mob->GetUInt32Value(UNIT_FIELD_FLAGS),
                     mob->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL), mob->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC),
                     tank->CanSeeOrDetect(mob), tank->IsWithinLOSInMap(mob), tank->IsValidAttackTarget(mob),
                     AttackersValue::IsPossibleTarget(mob, tank), c->IsInCombatWith(tank),
                     mob->GetThreatMgr().GetThreat(tank), mob->isMoving(), mob->IsNonMeleeSpellCast(false));
        }
        if (hit && PlayerbotAI::IsTank(hit, true))
        {
            ++t.onTank;
            ++run.fightOnTank;
        }
        else if (Player* who = RunPlayerOf(victim))
            ++run.fightOffTank[who->GetGUID()];
        // Only a mob standing at the tank: one still running in, or flying round it (Underbog bats, Onyxia's whelps), faces
        // wherever it goes, and the tank has no say in that. Nor one whose front is where melee belong by the dungeon's own
        // tactic: an Underbat lashes whoever stands behind or beside it (UBPolicy.h)
        if (!tank || victim != tank || !mob->IsWithinMeleeRange(tank) || mob->isMoving() ||
            mob->GetEntry() == UnderbogHungarfen::NPC_UNDERBAT)
            continue;
        ++t.facedSamples;
        // (in a dungeon the GM only trails the tank, out of the fight)
        bool exposed = false;
        for (Player* bot : bots)
            if (bot != tank && bot->IsAlive() && !PlayerbotAI::IsTank(bot, true) &&
                mob->GetExactDist(bot) <= RoleCheck::FrontReach && mob->HasInArc(float(M_PI) / 2.0f, bot))
            {
                exposed = true;
                uint32 const faced = ++run.fightFaced[bot->GetGUID()];
                // Why, a few times a fight for each member: what it is on, and what it last did
                if (faced == 1 || faced == 25 || faced == 100)
                {
                    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
                    Unit* target = ai ? ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() : nullptr;
                    std::string trace;
                    if (Engine* engine = ai ? ai->GetCurrentEngine() : nullptr)
                    {
                        trace = engine->GetLastAction();
                        if (trace.size() > 240)
                            trace = trace.substr(trace.size() - 240);
                    }
                    LOG_INFO("playerbots",
                             "Rolecheck: {} ({}) in front of {} at {}s into the fight (sample {}): {:.1f} yd from it; "
                             "its target {}{}, {:.1f} yd away, in melee {}, moving {}, behind it {}; it is moving {}, "
                             "casting {}; back line {}; its last actions: {}",
                             bot->GetName(), Role(bot), mob->GetName(), (run.elapsed - run.fightAt) / 1000, faced,
                             mob->GetExactDist(bot), target ? target->GetName() : "-",
                             target == mob ? " (this mob)" : "", target ? bot->GetExactDist(target) : -1.0f,
                             target && bot->IsWithinMeleeRange(target), target && target->isMoving(),
                             target && !target->HasInArc(float(M_PI), bot), bot->isMoving(),
                             bot->IsNonMeleeSpellCast(false), BackLineNote(ai), trace);
                }
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
            // With the tank down the fight is a wipe and its mobs are loose: where they find the healer is no measure
            // of where it stood (The Eye and Sunwell failed healer back on the samples of their wipes)
            if (!tank || !tank->IsAlive())
                continue;
            ++t.healerSamples;
            float nearest = 1000.0f;
            Creature* nearestMob = nullptr;
            for (Creature* mob : mobs)
                if (bot->GetExactDist(mob) < nearest)
                {
                    nearest = bot->GetExactDist(mob);
                    nearestMob = mob;
                }
            // Back from the mobs, but still in reach of the tank: a healer left at the door is not standing back
            bool const inReach = tank && tank->GetMap() == bot->GetMap() &&
                                 bot->GetExactDist(tank) <= RoleCheck::HealReach && bot->IsWithinLOSInMap(tank);
            if (nearest >= RoleCheck::HealerRange && inReach)
                ++t.healerAtRange;
            else if (!inReach)
            {
                ++run.fightHealerFar;
                // Why, a few times a fight: how far, whether the tank is in sight, and what the healer is doing
                uint32 const far = run.fightHealerFar;
                if (tank && (far == 1 || far == 20 || far == 100 || far == 400))
                {
                    std::string trace;
                    if (Engine* engine = ai ? ai->GetCurrentEngine() : nullptr)
                    {
                        trace = engine->GetLastAction();
                        if (trace.size() > 200)
                            trace = trace.substr(trace.size() - 200);
                    }
                    Player* master = ai ? ai->GetMaster() : nullptr;
                    bool const sameMap = tank->GetMap() == bot->GetMap();
                    LOG_INFO("playerbots",
                             "Rolecheck: healer {} out of reach at {}s into the fight (sample {}): {:.0f} yd from the "
                             "tank, tank in sight {}, moving {}, motion {}, {:.0f} yd from its master {}; in combat "
                             "{}; back line {}; its last actions: {}",
                             bot->GetName(), (run.elapsed - run.fightAt) / 1000, far,
                             sameMap ? bot->GetExactDist(tank) : -1.0f, sameMap && bot->IsWithinLOSInMap(tank),
                             bot->isMoving(), uint32(bot->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                             master ? bot->GetExactDist(master) : -1.0f, master ? master->GetName() : "-",
                             bot->IsInCombat(), BackLineNote(ai), trace);
                }
            }
            else if (nearestMob && nearestMob->GetVictim() == bot)
                ++run.fightHealerHit;
            else
            {
                // Why, a few times a fight: the mob, and what the healer's step back last found
                uint32 const near = ++run.fightHealerNear;
                if (nearestMob && (near == 1 || near == 20 || near == 100))
                    LOG_INFO("playerbots",
                             "Rolecheck: healer {} near {} at {}s into the fight (sample {}): {:.1f} yd, it on {}, "
                             "moving {}; healer in combat {}, moving {}, casting {}; back line {}",
                             bot->GetName(), nearestMob->GetName(), (run.elapsed - run.fightAt) / 1000, near,
                             nearest, nearestMob->GetVictim() ? nearestMob->GetVictim()->GetName() : "-",
                             nearestMob->isMoving(), bot->IsInCombat(), bot->isMoving(),
                             bot->IsNonMeleeSpellCast(false), BackLineNote(ai));
            }
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
        // A fight that starts with the tank down is no pull to judge: Trial of the Champion's Argent wave came with the
        // tank dead, the last damage dealer standing struck first, and the row failed tank pulls at 0%
        if (!MainTank())
            run.fightOpenerHow += ", tank down";
        opener.Clear();
        fightHurt = false;
        fightHealed = false;
        fightCasts.clear();
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
    run.fightHealerHit = run.fightHealerNear = run.fightHealerFar = 0;
    run.fightTraceAt = run.fightTraceLogs = 0;
    run.mobVictim.clear();
    run.fightSwitchLogs = 0;
    run.mobUntouched.clear();
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

struct EventGather;
EventGather const* HeldAt();

// Who opened the fight, once it proved one
void CountFight()
{
    ++run.tally.fights;
    Player* who = ObjectAccessor::FindConnectedPlayer(run.fightOpener);
    if (run.raid && run.fightOpener == run.gm)
        return;  // the player's pull, as asked
    // An event's wave (Violet Hold's portals, the Black Morass's rifts): the mobs came at the group, nobody pulled them. Only a
    // fight a member opened with a hit of its own is a pull to judge.
    // A hit on a wave mob already sent at the group is no pull either: Trial of the Champion's Argent trash ran at the
    // tank as the herald released it, a rogue struck one on its way, and the row failed tank pulls at 0%.
    if (run.eventStarted && (run.fightOpenerHow.find(" hit ") == std::string::npos ||
                             run.fightOpenerHow.find("already fighting") != std::string::npos))
        return;
    // Nor a mob a boss's script sent at the group (SentByScript), nor a fight with no mob in it: a rogue freeing a webbed
    // player from Krik'thir's Web Wrap, already fighting, "opened" a fight with no mob in it
    if (run.fightOpenerHow.find("sent by its boss") != std::string::npos ||
        run.fightOpenerHow.find(", tank down") != std::string::npos ||
        (run.fightMobCount == 0 && run.fightOpenerHow.find("already fighting") != std::string::npos))
        return;
    // Nor any fight where the group is held for the waves: a mage's hit on a Chronomancer already at Medivh was the run's
    // one pull, and failed it (The Black Morass)
    if (HeldAt())
        return;
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
    std::string healer;
    if (run.fightHealerHit + run.fightHealerNear + run.fightHealerFar)
        healer = Acore::StringFormat("; healer not back: struck {}, near a mob on another {}, out of reach {}",
                                     run.fightHealerHit, run.fightHealerNear, run.fightHealerFar);
    Player* opened = ObjectAccessor::FindConnectedPlayer(run.fightOpener);
    LOG_INFO("playerbots", "Rolecheck: fight {}: {}s, up to {} mobs, opened by {} ({}), on the tank {}/{} samples{}{}{}{}{}",
             run.tally.fights, (run.elapsed - run.fightAt) / 1000, run.fightMobCount,
             opened ? opened->GetName() : "?", run.fightOpenerHow, run.fightOnTank, run.fightMobSamples,
             offTank.empty() ? "" : "; off the tank on ", offTank, faced.empty() ? "" : "; in front of a mob: ",
             faced, healer);
    std::lock_guard<std::mutex> guard(hookLock);
    if (run.fightMobCount >= CAST_LOG_MOBS)
        for (auto const& [guid, spells] : fightCasts)
        {
            Player* member = ObjectAccessor::FindConnectedPlayer(guid);
            std::vector<std::pair<uint32, uint32>> counts(spells.begin(), spells.end());
            std::sort(counts.begin(), counts.end(), [](auto const& a, auto const& b) { return a.second > b.second; });
            std::string casts;
            for (size_t i = 0; i < counts.size() && i < 8; ++i)
            {
                SpellInfo const* info = sSpellMgr->GetSpellInfo(counts[i].first);
                casts += Acore::StringFormat("{}{} {}", casts.empty() ? "" : ", ",
                                             info ? info->SpellName[0] : "?", counts[i].second);
            }
            LOG_INFO("playerbots", "Rolecheck: fight {} casts by {} ({}): {}", run.tally.fights,
                     member ? member->GetName() : "?", member ? std::string(1, Role(member)) : "?", casts);
        }
    fightCasts.clear();
    if (fightHurt)
    {
        ++run.tally.hurtFights;
        if (fightHealed)
            ++run.tally.healedFights;
        else if (Player* hurt = ObjectAccessor::FindConnectedPlayer(fightHurtWho))
            // Hurt and not healed: where the healer was to the one hurt, and what it last did
            for (Player* healer : Bots())
            {
                PlayerbotAI* ai = GET_PLAYERBOT_AI(healer);
                if (!ai || !PlayerbotAI::IsHeal(healer, true) || healer->GetMap() != hurt->GetMap())
                    continue;
                std::string trace;
                if (Engine* engine = ai->GetCurrentEngine())
                {
                    trace = engine->GetLastAction();
                    if (trace.size() > 300)
                        trace = trace.substr(trace.size() - 300);
                }
                LOG_INFO("playerbots",
                         "Rolecheck: fight {} hurt {} ({}, {} now at {:.0f}%) and no heal landed; healer {} {:.0f} yd "
                         "from it, in sight {}, alive {}, mana {:.0f}%, in combat {}; its last actions: {}",
                         run.tally.fights, hurt->GetName(), Role(hurt), hurt->IsAlive() ? "alive" : "dead",
                         hurt->GetHealthPct(), healer->GetName(), healer->GetExactDist(hurt),
                         healer->IsWithinLOSInMap(hurt), healer->IsAlive(), healer->GetPowerPct(POWER_MANA),
                         healer->IsInCombat(), trace);
            }
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
                // Where: a death out of any fight, again and again where it was revived (Underbog), is the place
                LOG_INFO("playerbots",
                         "Rolecheck: {} died at {:.1f} {:.1f} {:.1f} (water level {:.1f}), in a fight {}, in water {}, under "
                         "water {}, attackers {}",
                         bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                         bot->GetMap()->GetWaterLevel(bot->GetPositionX(), bot->GetPositionY()), run.inFight,
                         bot->IsInWater(), bot->IsUnderWater(), uint32(bot->getAttackers().size()));
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
    if (beside && !beside->IsAlive())
        beside = nullptr;
    if (!beside)
        for (Player* bot : bots)
            if (bot->IsAlive())
                beside = bot;
    if (!beside)
    {
        ++run.wipes;
        Say(Acore::StringFormat("wipe {}: everyone back at the entrance", run.wipes));
        // The event resets with the wipe, as for players going again: its starters are taken anew (Eye of Eternity
        // readies the same Focusing Iris, and the run stood 90 s and ended at one fight)
        run.eventDone.clear();
        run.eventBeside.clear();
        run.eventVisited.clear();
        // ...and so does the pull: kept, the GM pulled its last mob from the entrance, 116 yards off, and the pack came
        // through the halls to a group that had just got up (Sunwell)
        run.pullTarget.Clear();
        run.gathering = 0;
        run.broughtOver = false;
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

// Event instances have no packs to lead to until their event starts: a player talks to its starter, uses its object or
// walks up to it, and the waves come to the group. The lead was done at once and none of these runs had a fight
// (Violet Hold, the Caverns of Time, the Frozen Halls, the Coliseum, the Eye of Eternity, 2026-11-08). The GM does
// what the player would, the group brought along; a step whose starter is not up (yet) is passed over.
enum class EventAct
{
    Gossip,    // talk to the creature and take its first option, as a player clicks it
    Use,       // use the game object
    Approach,  // walk up to the creature: its sight of the group starts the event
};

struct EventStarter
{
    uint32 mapId;
    uint32 entry;
    EventAct act;
    // Gossip: the option to take when the menu offers it, else the first; and a quest the GM is taken to have done,
    // as a player who has run the instance before (the Culling of Stratholme lets one skip its intro, 3.3.3)
    int32 option = -1;
    uint32 doneQuest = 0;
};

constexpr EventStarter EventStarters[] = {
    {608, 30658, EventAct::Gossip},                                   // Violet Hold: Lieutenant Sinclari
    {595, 26527, EventAct::Gossip, 2, 13151},                         // Culling of Stratholme: Chromie's skip ahead,
    {595, 27915, EventAct::Gossip}, {595, 26499, EventAct::Gossip},   // Chromie in town, Arthas at each stop
    {668, 37221, EventAct::Gossip}, {668, 37223, EventAct::Gossip},   // Halls of Reflection: Jaina or Sylvanas,
    {668, 36955, EventAct::Gossip}, {668, 37554, EventAct::Gossip},   // and again for the escape
    {650, 35004, EventAct::Gossip}, {650, 35005, EventAct::Gossip},   // Trial of the Champion: the announcer
    {649, 34816, EventAct::Gossip}, {649, 35909, EventAct::Gossip},   // Trial of the Crusader: Barrett Ramsey, one for
    {649, 35035, EventAct::Gossip}, {649, 35766, EventAct::Gossip},   // each stage (the beasts, normal and heroic,
    {649, 35770, EventAct::Gossip},                                   // Jaraxxus, the champions, the twins)
    {560, 182589, EventAct::Use},  {560, 17876, EventAct::Gossip},    // Old Hillsbrad: the barrels, then Thrall
    {269, 15608, EventAct::Approach},                                 // The Black Morass: Medivh
    {616, 193958, EventAct::Use},  {616, 193960, EventAct::Use},      // Eye of Eternity: the Focusing Iris
};

uint32 constexpr EventWaitMs = 20000;  // after a step: the event's intro, before the next step is looked for

// Where an event's waves come once it is started, and only when the whole group stands there: Halls of Reflection holds
// its waves until every player is within 40 yards of the throne room's centre (instance_halls_of_reflection.cpp)
struct EventGather
{
    uint32 mapId;
    float x, y, z;
    uint32 untilBosses;  // the waves are over once the group has killed this many bosses
};

constexpr EventGather EventGathers[] = {
    {668, 5309.46f, 2006.48f, 711.60f, 2},  // Halls of Reflection: the throne room, until Falric and Marwyn are down
    // The Black Morass: before Medivh, where the rifts' mobs and bosses walk to, until Aeonus is down. Sent after the
    // wildlife, the group left him to the portals' mobs
    {269, -2033.2f, 7120.7f, 23.2f, 3},
};

// The spot a started event keeps the group at while its waves last, and how far out it pulls from there: picking the
// nearest wildlife pull after pull, the group drifted 400 yards from Medivh (The Black Morass)
constexpr float HeldReach = 45.0f;

EventGather const* HeldAt()
{
    if (!run.eventStarted)
        return nullptr;
    for (EventGather const& spot : EventGathers)
        if (spot.mapId == run.mapId && run.tally.bosses < spot.untilBosses)
            return &spot;
    return nullptr;
}

// Where a starter stands, from the spawn data: a far one's grid is not loaded until someone goes near it
bool StarterSpawn(uint32 mapId, uint32 entry, bool object, Position& at)
{
    if (object)
    {
        for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
            if (data.mapid == mapId && data.id == entry)
            {
                at.Relocate(data.posX, data.posY, data.posZ);
                return true;
            }
        return false;
    }
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
        if (data.mapid == mapId && (data.id == entry || data.id2 == entry || data.id3 == entry))
        {
            at.Relocate(data.posX, data.posY, data.posZ);
            return true;
        }
    return false;
}

void BringGroup(Player* gm, std::vector<Player*> const& bots)
{
    for (Player* bot : bots)
        if (bot->IsAlive() && bot->GetMapId() == gm->GetMapId())
            bot->NearTeleportTo(gm->GetPositionX(), gm->GetPositionY(), gm->GetPositionZ(), gm->GetOrientation());
}

// The next event step the GM can take, taken; false when the map has none ready
Creature* NextTrash(Player* gm, bool bosses, float reach = TrashReach, bool summoned = false);

bool StartEvent(Player* gm, std::vector<Player*> const& bots, uint32 diff)
{
    if (run.eventWait)
    {
        if (run.eventMoves && gm->GetExactDist(&run.eventFrom) > 30.0f && !gm->IsBeingTeleported())
        {
            run.eventMoves = false;
            BringGroup(gm, bots);
        }
        run.eventWait = run.eventWait > diff ? run.eventWait - diff : 0;
        if (!run.eventWait)
            run.eventMoves = false;
        return true;
    }
    Map* map = gm->GetMap();
    for (EventStarter const& step : EventStarters)
    {
        if (step.mapId != run.mapId)
            continue;
        bool const object = step.act == EventAct::Use;
        WorldObject* starter = nullptr;
        if (object)
        {
            for (auto const& [spawnId, go] : map->GetGameObjectBySpawnIdStore())
                if (go->GetEntry() == step.entry && go->isSpawned() && go->GetGoState() == GO_STATE_READY &&
                    !run.eventDone.count(go->GetGUID()) && (!starter || gm->GetExactDist(go) < gm->GetExactDist(starter)))
                    starter = go;
        }
        else
        {
            for (auto const& [spawnId, creature] : map->GetCreatureBySpawnIdStore())
            {
                bool const ready = step.act == EventAct::Gossip ? creature->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP)
                                                                : !run.eventDone.count(creature->GetGUID());
                if (creature->GetEntry() == step.entry && creature->IsAlive() && ready &&
                    (!starter || gm->GetExactDist(creature) < gm->GetExactDist(starter)))
                    starter = creature;
            }
            // A summoned one is in no spawn store (the Coliseum summons a Barrett Ramsey for each stage): the creatures
            // around the GM
            if (!starter)
            {
                std::list<Creature*> near;
                gm->GetCreatureListWithEntryInGrid(near, step.entry, 300.0f);
                for (Creature* creature : near)
                {
                    bool const ready = step.act == EventAct::Gossip ? creature->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP)
                                                                    : !run.eventDone.count(creature->GetGUID());
                    if (creature->IsAlive() && ready && (!starter || gm->GetExactDist(creature) < gm->GetExactDist(starter)))
                        starter = creature;
                }
            }
        }
        if (!starter)
        {
            // Not loaded: go to where it stands, once, and look again once the grid is up
            Position at;
            if (run.eventVisited.count(step.entry) || !StarterSpawn(run.mapId, step.entry, object, at) ||
                gm->GetExactDist(&at) < 60.0f)
                continue;
            run.eventVisited.insert(step.entry);
            gm->NearTeleportTo(at.GetPositionX(), at.GetPositionY(), at.GetPositionZ() + 0.5f, gm->GetOrientation());
            run.eventFrom = gm->GetPosition();  // the group follows once the teleport has landed
            run.eventMoves = true;
            run.eventWait = 5000;
            return true;
        }
        // Beside it first (a teleport lands at the GM's next session update), then the step
        if (gm->GetExactDist(starter) > 4.0f && ++run.eventBeside[starter->GetGUID()] <= 3)
        {
            float const angle = starter->GetAbsoluteAngle(gm);
            gm->NearTeleportTo(starter->GetPositionX() + 2.5f * std::cos(angle),
                               starter->GetPositionY() + 2.5f * std::sin(angle), starter->GetPositionZ() + 0.5f,
                               angle + float(M_PI));
            run.eventFrom = gm->GetPosition();
            run.eventMoves = true;
            run.eventWait = 3000;
            return true;
        }
        std::string what;
        if (GameObject* go = starter->ToGameObject())
        {
            run.eventDone.insert(go->GetGUID());
            go->Use(gm);
            what = "used";
        }
        else if (step.act == EventAct::Approach)
        {
            run.eventDone.insert(starter->GetGUID());
            what = "went up to";
            // With the group: a GM is no player in a creature's sight. Medivh starts on a player within 20 yards, and
            // the group stood 26 yards off for the whole run (The Black Morass). He still did not begin with them
            // beside him: an open issue in his script, not the harness
            for (Player* bot : bots)
                if (bot->IsAlive() && bot->GetMapId() == gm->GetMapId())
                    bot->NearTeleportTo(gm->GetPositionX(), gm->GetPositionY(), gm->GetPositionZ(), gm->GetOrientation());
        }
        else
        {
            // As the client asks: the menu, then its first option; a script with no option on offer to the GM (a
            // quest it never did) is asked for its start as that option would
            Creature* npc = starter->ToCreature();
            if (step.doneQuest && !gm->IsQuestRewarded(step.doneQuest))
                gm->SetRewardedQuest(step.doneQuest);
            WorldPacket hello(CMSG_GOSSIP_HELLO, 8);
            hello << npc->GetGUID();
            gm->GetSession()->HandleGossipHelloOpcode(hello);
            GossipMenu const& menu = gm->PlayerTalkClass->GetGossipMenu();
            if (!menu.GetMenuItems().empty())
            {
                // The starter's own option or the first, then the first of each menu it opens, as a player clicks
                // through (Violet Hold: "Activate the crystals" opens the menu whose option starts the event); a menu
                // that comes back the same is not clicked again
                what = "talked to";
                std::string last;
                for (uint32 click = 0; click < 4 && !menu.GetMenuItems().empty(); ++click)
                {
                    // A script that opens the next menu clears this one first; one that closes leaves it as it was
                    auto chosen = menu.GetMenuItems().begin();
                    if (click == 0 && step.option >= 0 && menu.GetMenuItems().count(uint32(step.option)))
                        chosen = menu.GetMenuItems().find(uint32(step.option));
                    uint32 const option = chosen->first;
                    std::string const text = chosen->second.Message;
                    std::string const said = std::to_string(menu.GetMenuId()) + ":" + text;
                    if (said == last)
                        break;
                    last = said;
                    what += " (\"" + text + "\")";
                    WorldPacket select(CMSG_GOSSIP_SELECT_OPTION, 16);
                    select << npc->GetGUID() << menu.GetMenuId() << option;
                    gm->GetSession()->HandleGossipSelectOptionOpcode(select);
                }
            }
            else
            {
                // Its menus as the client would click them, option by option, for a SmartAI that starts on a later
                // menu: Barrett Ramsey walks on after he is summoned, the GM was still 11 yards off after three
                // goes, and his first menu only leads to the one that starts the stage (Trial of the Crusader)
                uint32 menuId = npc->GetCreatureTemplate()->GossipMenuId;
                for (uint32 click = 0; click < 4 && menuId; ++click)
                {
                    npc->AI()->sGossipSelect(gm, menuId, 0);
                    uint32 next = 0;
                    auto const options = sObjectMgr->GetGossipMenuItemsMapBounds(menuId);
                    for (auto option = options.first; option != options.second; ++option)
                        if (option->second.OptionID == 0)
                            next = option->second.ActionMenuID;
                    menuId = next;
                }
                sScriptMgr->OnGossipSelect(gm, npc, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
                what = "asked to start";
            }
            gm->PlayerTalkClass->SendCloseGossip();
            // One that keeps its gossip (a menu the GM can't use) is not asked again and again
            if (npc->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP))
                npc->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        }
        run.eventStarted = true;
        run.lastEventAt = run.elapsed;
        run.lastStarter = starter->GetGUID();
        run.lastStarterAt = starter->GetPosition();
        run.eventFrom = gm->GetPosition();
        run.eventMoves = true;
        Say(Acore::StringFormat("event: {} {} ({:.1f} yd off)", what, starter->GetName(), gm->GetExactDist(starter)));
        run.eventWait = EventWaitMs;
        return true;
    }
    // Nothing left to start: the group goes where the started event wants it
    if (run.eventStarted)
        for (EventGather const& spot : EventGathers)
            if (spot.mapId == run.mapId && run.tally.bosses < spot.untilBosses &&
                gm->GetExactDist2d(spot.x, spot.y) > 20.0f &&
                // Not while an event mob is left out there: the Rift Keepers stood at their rifts, 95-138 yards out, as
                // the group waited at Medivh for their mobs and his shield ran out (The Black Morass)
                !NextTrash(gm, false, EventReach, true))
            {
                gm->NearTeleportTo(spot.x, spot.y, spot.z, gm->GetOrientation());
                run.eventFrom = gm->GetPosition();
                run.eventMoves = true;
                run.eventWait = 5000;
                Say("event: gathering the group where its waves come");
                return true;
            }
    return false;
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
    // Hostile to the raid, judged by its tank: to a GM in GM mode every mob is neutral, so between pulls none was
    // pullable and Karazhan ended at its first fight
    Player* tank = MainTank();
    Unit const* judge = tank ? static_cast<Unit const*>(tank) : gm;
    return mob->IsAlive() && !mob->IsInCombat() && mob->IsHostileTo(judge) && !mob->IsTotem() &&
           !mob->GetCharmerOrOwnerGUID().IsPlayer() &&
           !mob->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE) && !mob->IsImmuneToPC() &&
           mob->GetCreatureTemplate()->type != CREATURE_TYPE_CRITTER;
}

bool IsBoss(Creature const* mob) { return mob->IsDungeonBoss() || mob->isWorldBoss(); }

// The three nearest mobs hostile to the group within TrashReach of the GM, and why each is not pulled
std::string NearestHostiles(Player* gm)
{
    Player* tank = MainTank();
    Unit const* judge = tank ? static_cast<Unit const*>(tank) : gm;
    std::list<Creature*> near;
    Acore::AnyUnitInObjectRangeCheck check(gm, TrashReach);
    Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(gm, near, check);
    Cell::VisitObjects(gm, searcher, TrashReach);
    std::vector<Creature*> hostiles;
    for (Creature* mob : near)
        if (mob->IsAlive() && mob->IsHostileTo(judge) && mob->GetCreatureTemplate()->type != CREATURE_TYPE_CRITTER)
            hostiles.push_back(mob);
    std::sort(hostiles.begin(), hostiles.end(),
              [gm](Creature* a, Creature* b) { return gm->GetExactDist(a) < gm->GetExactDist(b); });
    std::string out;
    for (size_t i = 0; i < hostiles.size() && i < 3; ++i)
    {
        Creature* mob = hostiles[i];
        std::string why;
        if (mob->IsInCombat())
            why += " in combat";
        if (mob->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
            why += " not attackable";
        if (mob->IsImmuneToPC())
            why += " immune to players";
        if (mob->GetCharmerOrOwnerGUID().IsPlayer())
            why += " a player's";
        if (run.tried.count(mob->GetGUID()))
            why += " tried";
        if (IsBoss(mob))
            why += " boss";
        out += Acore::StringFormat("{}{} {} at {:.0f} yd{}", out.empty() ? "" : ", ", mob->GetName(), mob->GetEntry(),
                                   gm->GetExactDist(mob), why.empty() ? " pullable" : why);
    }
    return out.empty() ? "no hostiles in reach" : out;
}

// Raid: the nearest mob the GM can pull, not already fighting; trash, or a boss once no trash is left about
Creature* NextTrash(Player* gm, bool bosses, float reach, bool summoned)
{
    std::list<Creature*> near;
    Acore::AnyUnitInObjectRangeCheck check(gm, reach);
    Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(gm, near, check);
    Cell::VisitObjects(gm, searcher, reach);
    Creature* best = nullptr;
    EventGather const* held = HeldAt();
    for (Creature* mob : near)
    {
        if (!Pullable(mob, gm) || IsBoss(mob) != bosses || run.tried.count(mob->GetGUID()) ||
            (summoned && !mob->IsSummon()) ||
            (held && !summoned && mob->GetExactDist2d(held->x, held->y) > HeldReach))
            continue;
        // Held: the one nearest where the group is held first, the mobs at Medivh before the Rift Keepers out at their
        // rifts. Nearest the GM first, the group fought 40-50 yards out as the mobs at Medivh drained his shield
        if (!best || (held ? mob->GetExactDist2d(held->x, held->y) < best->GetExactDist2d(held->x, held->y)
                           : gm->GetExactDist(mob) < gm->GetExactDist(best)))
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
bool PullSpot(Map* map, Position const& mob, float facing, Position& spot, Creature* target = nullptr,
              Player* gm = nullptr)
{
    // Clear of the other packs about, as a raid leader picks where the raid waits: at the first spot found, the raid
    // gathered beside the next pack, and Karazhan's first pulls were 13, 7 and 6 mobs
    std::vector<Position> others;
    if (target && gm)
    {
        std::list<Creature*> near;
        Acore::AnyUnitInObjectRangeCheck check(target, PullFrom + PackClear);
        Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(target, near, check);
        Cell::VisitObjects(target, searcher, PullFrom + PackClear);
        for (Creature* other : near)
            if (other != target && Pullable(other, gm) && other->GetExactDist(target) > PackOf)
                others.push_back(other->GetPosition());
    }
    for (bool const clear : {true, false})
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
                if (clear && std::any_of(others.begin(), others.end(), [x, y](Position const& other)
                                         { return other.GetExactDist2d(x, y) < PackClear; }))
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
    Position spot;
    if (!PullSpot(map, best->pos, best->pos.GetAbsoluteAngle(gm), spot))
        return true;  // no ground to stand on near it: the next one, at once
    run.walkWait = WalkMs;
    LOG_INFO("playerbots", "Rolecheck: on to {} at {:.0f} yd", best->boss ? "a boss" : "trash", gm->GetExactDist(&spot));
    gm->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), spot.GetOrientation());
    for (Player* bot : bots)
        if (bot->IsAlive() && bot->GetMapId() == gm->GetMapId())
            bot->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), spot.GetOrientation());
    return true;
}

// An event whose starter walks on goes with it, and the group with it, as players walk with Arthas into Stratholme:
// his waves come about him, 170 yards from where he was talked to, and the group stood there for the whole run
bool WithStarter(Player* gm, std::vector<Player*> const& bots)
{
    // (not in a raid: the Coliseum's Barrett walks out of the arena once he has started a stage)
    Creature* starter = !run.raid && run.lastStarter.IsCreature() ? gm->GetMap()->GetCreature(run.lastStarter) : nullptr;
    if (!starter || !starter->IsAlive() || starter->GetExactDist(&run.lastStarterAt) < 30.0f ||
        gm->GetExactDist(starter) < 30.0f)
        return false;
    for (EventGather const& spot : EventGathers)
        if (spot.mapId == run.mapId && run.tally.bosses < spot.untilBosses)
            return false;  // the event has its own place for the group
    float const angle = starter->GetAbsoluteAngle(gm);
    float const x = starter->GetPositionX() + 8.0f * std::cos(angle);
    float const y = starter->GetPositionY() + 8.0f * std::sin(angle);
    float const z = gm->GetMap()->GetHeight(PHASEMASK_NORMAL, x, y, starter->GetPositionZ() + 5.0f);
    if (z <= INVALID_HEIGHT)
        return false;
    LOG_INFO("playerbots", "Rolecheck: with {} at {:.0f} yd", starter->GetName(), gm->GetExactDist(starter));
    run.walkWait = WalkMs;
    // Followed to here: again only once it moves on. Left where it was first seen, the group was taken back to
    // Arthas, who waits at his waypoint 11 for the waves to be killed, from every wave it went out to (Culling of
    // Stratholme)
    run.lastStarterAt = starter->GetPosition();
    gm->NearTeleportTo(x, y, z + 0.5f, angle + float(M_PI));
    for (Player* bot : bots)
        if (bot->IsAlive() && bot->GetMapId() == gm->GetMapId())
            bot->NearTeleportTo(x, y, z + 0.5f, angle + float(M_PI));
    return true;
}

// Held where an event's waves come: the group stays there and the tank takes each wave as it comes, the nearest the spot
// first, as a group at Medivh does. Pulled by the GM out where each mob was seen, the group went back and forth between
// the rifts and Medivh as the mobs walked on, nothing died for 100 seconds and his shield ran out (The Black Morass).
void HeldPull(Player* gm, std::vector<Player*> const& bots, EventGather const& held, uint32 diff)
{
    if (std::any_of(bots.begin(), bots.end(), [](Player* bot) { return bot->IsAlive() && bot->IsInCombat(); }))
    {
        run.gathering = 0;
        return;
    }
    Player* tank = MainTank();
    if (!tank || !tank->IsAlive() || tank->GetMap() != gm->GetMap())
        return;
    // Back to the spot after a fight that drew the group off it
    if (tank->GetExactDist2d(held.x, held.y) > 30.0f)
    {
        gm->NearTeleportTo(held.x, held.y, held.z, gm->GetOrientation());
        for (Player* bot : bots)
            if (bot->IsAlive() && bot->GetMapId() == gm->GetMapId())
                bot->NearTeleportTo(held.x, held.y, held.z, gm->GetOrientation());
        run.gathering = 0;
        return;
    }
    run.gathering += diff;
    if (run.gathering < EventGatherMs)
        return;
    Creature* mob = NextTrash(gm, false, EventReach, true);
    if (!mob)
        mob = NextTrash(gm, true, EventReach, true);
    if (!mob)
        mob = NextTrash(gm, false);
    if (!mob)
        return;
    {
        std::lock_guard<std::mutex> guard(hookLock);
        opener = tank->GetGUID();
        openerHow = "the tank's pull";
    }
    std::list<Creature*> wave;
    Acore::AnyUnitInObjectRangeCheck check(mob, WaveReach);
    Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(mob, wave, check);
    Cell::VisitObjects(mob, searcher, WaveReach);
    wave.remove(mob);
    wave.push_front(mob);
    for (Creature* one : wave)
        if (one == mob || (one->IsSummon() && Pullable(one, gm)))
        {
            run.tried.insert(one->GetGUID());
            one->AI()->AttackStart(tank);
        }
    run.gathering = 0;
}

void RaidPull(Player* gm, std::vector<Player*> const& bots, uint32 diff)
{
    if (run.walkWait)
    {
        run.walkWait = run.walkWait > diff ? run.walkWait - diff : 0;
        return;
    }
    Creature* mob = run.pullTarget.IsEmpty() ? nullptr : gm->GetMap()->GetCreature(run.pullTarget);
    if (!mob || !mob->IsAlive() || mob->IsInCombat() || gm->GetExactDist(mob) > PullFrom + 10.0f)
    {
        // Unseen between pulls, as a raid leader keeps clear of the packs it is not pulling: seen all run, the GM drew
        // every pack it walked by or waited near, 9-12 mobs a fight and three wipes in Karazhan's first rooms
        if (!gm->IsGameMaster() && !gm->IsInCombat())
            gm->SetGameMaster(true);
        // Held where an event's waves come, the tank takes them there
        EventGather const* held = HeldAt();
        mob = held ? nullptr : NextTrash(gm, false);
        if (!mob && WithStarter(gm, bots))
            return;
        // The trash in reach done: an event raid's boss comes by its starter, before any pull of it or any walk on to spawns
        // (Malygos circles out of reach until the Focusing Iris is used; the Coliseum's bosses come when Barrett Ramsey is
        // asked, and its Nerubians under the arena floor took the whole run to walk on to)
        if (!mob && StartEvent(gm, bots, diff))
            return;
        if (!mob && held)
        {
            HeldPull(gm, bots, *held, diff);
            return;
        }
        // An event's mobs out in the streets, further than any pack in reach: Culling of Stratholme puts each wave at
        // its own spot, up to 250 yards from the last, and the group goes to it as players follow the map's marker.
        // Only the event's own, summoned mobs: the swamp's wildlife out to 400 yards drew the group away from Medivh as
        // soon as he began (The Black Morass)
        if (!mob && !run.raid && run.eventStarted)
            mob = NextTrash(gm, false, EventReach, true);
        if (!mob && WalkOn(gm, bots))
            return;
        if (!mob)
            mob = NextTrash(gm, true);
        if (!mob)
        {
            // An event raid's starter can come a while after the group or the last fight (the Coliseum summons a Barrett
            // for each stage), and its mobs a while after the step: wait for them. An event dungeon waits out its time:
            // its intros run past that (Halls of Reflection's and Culling of Stratholme's ended the run at 1-2 minutes)
            if (!run.raid)
                return;
            for (EventStarter const& step : EventStarters)
                if (step.mapId == run.mapId && (run.calm < 90000 || run.elapsed - run.lastEventAt < 90000))
                    return;
            run.tally.cleared = run.tally.bosses > 0;
            Report();
            return;
        }
        // On ground it can see, as WalkOn: at the mob's height off a ledge the GM hung in the air (the driver does not
        // fall), and the bots brought over fell and walked back 175 yards below it, minute after minute (Naxxramas)
        Position spot;
        run.tried.insert(mob->GetGUID());
        if (!PullSpot(gm->GetMap(), mob->GetPosition(), mob->GetAngle(gm), spot, mob, gm))
            return;
        run.pullTarget = mob->GetGUID();
        run.gathering = 0;
        run.broughtOver = false;
        run.eventPull = run.eventStarted && mob->IsSummon();
        gm->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), spot.GetOrientation());
        return;
    }
    run.gathering += diff;
    if (run.gathering < (run.eventPull ? EventGatherMs : GatherMs))
        return;
    // Not while any bot still fights
    if (std::any_of(bots.begin(), bots.end(), [](Player* bot) { return bot->IsAlive() && bot->IsInCombat(); }))
        return;
    Player* tank = MainTank();
    // The whole raid gathered, as a raid leader waits for it: with only the tank near, Karazhan's first pull caught the
    // raid strung out 20-60 yards behind the GM and a second pack on the way (2026-11-08). Once brought over, the tank
    // near is enough: in Naxxramas the bots scattered again within seconds, and nothing was pulled for 9 minutes.
    bool const strung = !run.broughtOver && std::any_of(bots.begin(), bots.end(), [gm](Player* bot)
                                    {
                                        return bot->IsAlive() &&
                                               (bot->GetMap() != gm->GetMap() || bot->GetExactDist(gm) > GatherRaid);
                                    });
    if (strung || (tank && tank->IsAlive() && (tank->GetMap() != gm->GetMap() || tank->GetExactDist(gm) > GatherReach)))
    {
        if (run.gathering < GatherMaxMs)
            return;
        if (tank)
            LOG_INFO("playerbots",
                     "Rolecheck: the tank is {:.0f} yd behind the pull ({:.0f} yd below); the group is brought over",
                     tank->GetMap() == gm->GetMap() ? tank->GetExactDist(gm) : -1.0f,
                     gm->GetPositionZ() - tank->GetPositionZ());
        for (Player* bot : bots)
            if (bot->IsAlive() && bot->GetMapId() == gm->GetMapId())
                bot->NearTeleportTo(gm->GetPositionX(), gm->GetPositionY(), gm->GetPositionZ(), gm->GetOrientation());
        run.gathering = 0;
        run.broughtOver = true;
        return;
    }
    // The player's pull: the mob comes for the GM, and its pack with it
    {
        std::lock_guard<std::mutex> guard(hookLock);
        opener = gm->GetGUID();
        openerHow = "the pull";
    }
    run.tried.insert(mob->GetGUID());
    gm->SetGameMaster(false);  // the mob must see the GM to come for it
    mob->AI()->AttackStart(gm);
    mob->CallAssistance();
    // The wave it came with
    if (run.eventPull)
    {
        std::list<Creature*> wave;
        Acore::AnyUnitInObjectRangeCheck check(mob, WaveReach);
        Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(mob, wave, check);
        Cell::VisitObjects(mob, searcher, WaveReach);
        for (Creature* other : wave)
            if (other != mob && other->IsSummon() && Pullable(other, gm))
            {
                run.tried.insert(other->GetGUID());
                other->AI()->AttackStart(gm);
            }
    }
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
    // Every bot down is a wipe, whoever still stands: the mobs that pulled on the GM kept beating on it, unhurt, and
    // the fight never ended, so nobody got up and the rest of the run was 8 minutes of nothing (Karazhan, 2026-11-08)
    bool const botsDown = std::none_of(bots.begin(), bots.end(), [](Player* bot) { return bot->IsAlive(); });
    if (botsDown && gm->IsInCombat())
        gm->CombatStop(true);
    bool const fighting = !botsDown && std::any_of(members.begin(), members.end(),
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
        WatchSinking(bots);
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
    // Dungeon: the lead found no pack left to lead to (or no way to one), and all is calm: an event to start
    if (!run.raid && !run.inFight && run.calm >= ReviveMs)
        if (Player* tank = MainTank())
            if (PlayerbotAI* ai = GET_PLAYERBOT_AI(tank))
            {
                DungeonLead::State const& lead = DungeonLead::StateOf(ai);
                // Once the event is on, its mobs stand where the event puts them, which the lead's packs never had:
                // Culling of Stratholme's waves wait in the streets, and the next comes only once they die. The
                // group stood at Arthas's side for 11 minutes. The GM goes to them and pulls, as in a raid.
                if (lead.done || lead.idle)
                {
                    if (run.eventStarted)
                        RaidPull(gm, bots, diff);
                    else
                        StartEvent(gm, bots, diff);
                }
            }

    run.sinceFollow += diff;
    // (not while an event step has the GM away, the group still to come after it)
    if (!run.raid && run.sinceFollow >= FollowMs && !run.eventMoves && run.pullTarget.IsEmpty())
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
            // A mob already fighting before the hit was on its way to the group: CountFight takes no pull from it
            openerHow = striker ? Acore::StringFormat("{} hit {}{}", attacker == striker ? "it" : attacker->GetName(),
                                                      victim->GetName(), victim->IsInCombat() ? ", already fighting" : "")
                                : Acore::StringFormat("struck by {}{}", attacker->GetName(),
                                                      SentByScript(attacker->GetMapId(), attacker->GetEntry())
                                                          ? ", sent by its boss"
                                                          : "");
        }
        if (struck && attacker->ToCreature())
            fightStruck = true;
        // Hurt: under the health a bot healer heals at, not a scratch it rightly leaves alone. A member's vehicle is
        // nobody a healer can heal: each joust wave logged "hurt Eleria (alive at 100%)" for her warhorse's hits.
        // Nor a mounted champion's hit on a member it unhorsed: the healer rides nothing that heals
        if (struck && !victim->GetVehicleKit() && !attacker->GetVehicle() && !attacker->GetVehicleKit() &&
            (victim->GetHealth() <= damage ||
             float(victim->GetHealth() - damage) * 100.0f < float(victim->GetMaxHealth()) * RoleCheck::HurtHealthPct))
        {
            if (!fightHurt)
                fightHurtWho = struck->GetGUID();
            fightHurt = true;
        }
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
        // It stays on, so no mob fights the GM; in a raid it goes off only for each pull
        if (!gm->IsGameMaster())
        {
            handler->SendErrorMessage("A role check needs GM mode on (.gm on); a raid turns it off for each pull.",
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

class RoleCheckPlayerScript : public PlayerScript
{
public:
    RoleCheckPlayerScript() : PlayerScript("RoleCheckPlayerScript", {PLAYERHOOK_ON_SPELL_CAST}) {}

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        // The bot's own casts, not the procs and auras they trigger
        if (run.phase != Phase::Observe || !run.inFight || !player || !spell || spell->IsTriggered() ||
            !IsRunPlayer(player->GetGUID()))
            return;
        std::lock_guard<std::mutex> guard(hookLock);
        ++fightCasts[player->GetGUID()][spell->GetSpellInfo()->Id];
    }
};
}  // namespace

void AddRoleCheckScripts()
{
    new RoleCheckPlayerScript();
    new RoleCheckUnitScript();
    new RoleCheckWorldScript();
    new RoleCheckCommandScript();
}
