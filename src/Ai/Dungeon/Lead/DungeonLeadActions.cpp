/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Bot-led dungeon runs: route to the next boss, pack choice, the pull gate, marks, crowd control calls and oom talk.

#include "DungeonLeadActions.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "AttackAction.h"
#include "AttackersValue.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DungeonLead.h"
#include "Event.h"
#include "FollowActions.h"
#include "Group.h"
#include "Map.h"
#include "MarkPolicy.h"
#include "HadronoxPull.h"
#include "MythicPlusBotMgr.h"
#include "ObjectMgr.h"
#include "PackPolicy.h"
#include "PathGenerator.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "PoolMgr.h"
#include "PositionValue.h"
#include "PullStrategy.h"
#include "RtiTargetValue.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "WaitSpotPolicy.h"
#include "World.h"

using namespace DungeonLead;

namespace
{
uint8 constexpr ICON_MOON = RtiTargetValue::moonIndex;
uint8 constexpr ICON_CROSS = RtiTargetValue::crossIndex;
uint8 constexpr ICON_SKULL = RtiTargetValue::skullIndex;
uint32 constexpr GOAL_RESCAN_MS = 5000;
time_t constexpr BOSS_BACK_SOON_S = 120;  // an evaded boss respawning within this is still the goal
uint32 constexpr GOAL_BLOCKED_MS = 10000;
uint32 constexpr PACK_RESCAN_MS = 1000;
uint32 constexpr READY_RECHECK_MS = 500;
uint32 constexpr MARKS_RECHECK_MS = 1000;
uint32 constexpr WAIT_CALL_DELAY_MS = 4000;
uint32 constexpr OVERPULL_WINDOW_MS = 8000;
// A tank running in without a pull spell is not in combat until it reaches the pack.
uint32 constexpr PULL_GRACE_MS = 6000;
uint32 constexpr SPOT_STUCK_MS = 5000;
// The advance this long without getting 5 yards nearer to its goal, by either measure: that way on goes nowhere
uint32 constexpr NO_PROGRESS_MS = 30000;
uint32 constexpr ADVANCE_GAP_MS = 5000;  // a wait, a pack or a fight between two looks: the measure starts over
float constexpr PROGRESS_STEP = 5.0f;
uint32 constexpr MAX_FAILED_PULLS = 3;
uint32 constexpr MAX_PATH_TRIES = 4;
uint32 constexpr SKULL_CALL_CHANCE = 25;
uint32 constexpr PULL_CALL_CHANCE = 20;
float constexpr SPOT_REACHED = 3.0f;
float constexpr GOAL_REACHED = 15.0f;
float constexpr ADVANCE_STEP = 25.0f;
float constexpr SAMPLE_STEP = 3.0f;
size_t constexpr MAX_SAMPLES = 40;
float constexpr OVERPULL_REACH = 30.0f;
float constexpr EYE_HEIGHT = 2.0f;
// A mob that can fly this far over the floor under it is out of a walking tank's reach
float constexpr AIRBORNE_HEIGHT = 10.0f;
// A stretch's end the floor puts this far under the straight line is over a drop, not a dip
float constexpr MAX_DIP = 10.0f;

struct CcProvider
{
    Cc cc;
    char const* spell;
    uint8 playerClass;
    char const* text;
    char const* fallback;
};

// Only crowd control that class strategies cast on the moon target in dungeons.
CcProvider constexpr CC_PROVIDERS[] = {
    {Cc::Polymorph, "polymorph", CLASS_MAGE, "lead_cc_polymorph", "%name, sheep the moon. Kill skull first."},
    {Cc::Shackle, "shackle undead", CLASS_PRIEST, "lead_cc_shackle", "%name, shackle the moon. Kill skull first."},
    {Cc::Banish, "banish", CLASS_WARLOCK, "lead_cc_banish", "%name, banish the moon. Kill skull first."},
    {Cc::Hibernate, "hibernate", CLASS_DRUID, "lead_cc_hibernate", "%name, sleep the moon. Kill skull first."},
    {Cc::Trap, "freezing trap", CLASS_HUNTER, "lead_cc_trap", "%name, trap the moon. Kill skull first."},
};

struct CcOffer
{
    CcProvider const* provider;
    ObjectGuid caster;
    uint32 spellId;
};

struct BossSpawn
{
    ObjectGuid::LowType spawnId;
    Position pos;
    uint8 spawnMask;
    bool flies = false;  // its kind can fly, so it may hold the air with no floor under it
};

bool IsLeader(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return EnabledFor(bot) && bot->IsAlive() &&
           botAI->GetAiObjectContext()->GetValue<ObjectGuid>("dungeon leader")->Get() == bot->GetGUID();
}

void Say(PlayerbotAI* botAI, std::string const& name, std::string const& fallback,
         std::map<std::string, std::string> const& placeholders = {})
{
    botAI->SayToParty(PlayerbotTextMgr::instance().GetBotTextOrDefault(name, fallback, placeholders));
}

bool Elapsed(uint32 since, uint32 ms) { return !since || getMSTimeDiff(since, getMSTime()) >= ms; }

// Built once per map from the spawn data, because an instance only loads the grids near its players.
std::vector<BossSpawn> const& BossSpawns(uint32 mapId)
{
    static std::mutex lock;
    static std::unordered_map<uint32, std::vector<BossSpawn>> cache;
    std::lock_guard<std::mutex> guard(lock);
    if (auto found = cache.find(mapId); found != cache.end())
        return found->second;

    std::vector<BossSpawn>& spawns = cache[mapId];
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        if (data.mapid != mapId)
            continue;
        CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(data.id);
        if (creatureTemplate && (creatureTemplate->flags_extra & CREATURE_FLAG_EXTRA_DUNGEON_BOSS))
            spawns.push_back({spawnId, Position(data.posX, data.posY, data.posZ, data.orientation), data.spawnMask});
    }
    return spawns;
}

// Hostile trash the core's Mythic+ counts as enemy forces: no bosses, triggers, civilians, critters or vendors
bool IsTrash(CreatureTemplate const* info)
{
    uint32 const skipped =
        CREATURE_FLAG_EXTRA_TRIGGER | CREATURE_FLAG_EXTRA_CIVILIAN | CREATURE_FLAG_EXTRA_DUNGEON_BOSS;
    if (info->rank == CREATURE_ELITE_WORLDBOSS || info->npcflag || (info->flags_extra & skipped) ||
        info->type == CREATURE_TYPE_CRITTER || info->type == CREATURE_TYPE_NON_COMBAT_PET ||
        (info->unit_flags & (UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE)))
        return false;
    FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(info->faction);
    return faction && faction->IsHostileToPlayers();
}

std::vector<BossSpawn> const& TrashSpawns(uint32 mapId)
{
    static std::mutex lock;
    static std::unordered_map<uint32, std::vector<BossSpawn>> cache;
    std::lock_guard<std::mutex> guard(lock);
    if (auto found = cache.find(mapId); found != cache.end())
        return found->second;

    std::vector<BossSpawn>& spawns = cache[mapId];
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        if (data.mapid != mapId)
            continue;
        CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(data.id);
        if (creatureTemplate && IsTrash(creatureTemplate))
            spawns.push_back({spawnId, Position(data.posX, data.posY, data.posZ, data.orientation), data.spawnMask,
                              creatureTemplate->Movement.IsFlightAllowed()});
    }
    return spawns;
}

// A boss held out of the fight until its turn comes (Keristrasza in her prison until the other three die, a Violet
// Hold cell, an event's speaker): the tank standing by it waits for a pull that cannot happen
bool Sealed(Creature const* creature)
{
    return creature->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) || creature->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
           creature->IsImmuneToPC();
}

// Alive and open to a pull: the next goal. pos is where it stands now.
bool BossAlive(Map* map, BossSpawn const& spawn, Position& pos)
{
    auto const bounds = map->GetCreatureBySpawnIdStore().equal_range(spawn.spawnId);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        Creature* creature = itr->second;
        if (!creature || !creature->IsInWorld())
            continue;
        pos.Relocate(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ());
        return creature->IsAlive() && !Sealed(creature);
    }
    pos = spawn.pos;
    // Not up on this map at all, as the grid loader judges it: a pool member the pool did not pick, or a spawn group
    // that is off. The Oculus's ring pairs each Azure Spellbinder with an Azure Inquisitor in a one-of-two pool; the
    // tank killed the Inquisitor, walked to the Spellbinder that never spawned, and stood on its spot for 10 minutes
    // (role check, 2026-11-09)
    CreatureData const* data = sObjectMgr->GetCreatureData(spawn.spawnId);
    if (data && (!map->IsSpawnGroupActive(data->spawnGroupId) ||
                 (data->poolId && !map->GetPoolData().IsSpawnedObject<Creature>(spawn.spawnId))))
        return false;
    // A boss that evaded is taken out of the world and put back within the minute; a killed one stays away for a day
    // or more. Counted as gone for that minute, Krik'thir was passed over for Hadronox ten times in one run, and the
    // tank walked on past his spot at 22-37 yards as he came back: three wipes, no boss (role check, heroic
    // Azjol-Nerub, 2026-11-11)
    return map->GetCreatureRespawnTime(spawn.spawnId) <= time(nullptr) + BOSS_BACK_SOON_S;
}

// No floor within reach under where it is now
bool Airborne(Player* bot, Position const& pos)
{
    float const ground = bot->GetMap()->GetHeight(bot->GetPhaseMask(), pos.GetPositionX(), pos.GetPositionY(),
                                                  pos.GetPositionZ() + EYE_HEIGHT);
    return ground <= INVALID_HEIGHT || pos.GetPositionZ() - ground > AIRBORNE_HEIGHT;
}

bool NearestAlive(Player* bot, std::vector<BossSpawn> const& spawns, State& state, bool skipBlocked = true)
{
    Map* map = bot->GetMap();
    float best = 0.0f;
    for (BossSpawn const& spawn : spawns)
    {
        if (!(spawn.spawnMask & (1 << map->GetSpawnMode())))
            continue;
        if (skipBlocked && state.blockedGoals.count(spawn.spawnId))
            continue;
        Position pos;
        if (!BossAlive(map, spawn, pos))
            continue;
        // Sky trash is no goal on foot: the way to an Azure Ring Guardian 20 yards over the Oculus's entrance ring is a
        // straight line off the ring, and the tank took it to the bottom of the map (role check, 2026-11-09). It is
        // still fought where it comes down to the group.
        if (spawn.flies && Airborne(bot, pos))
            continue;
        float const distance = bot->GetExactDist(&pos);
        if (!state.hasGoal || distance < best)
        {
            best = distance;
            state.goal = pos;
            state.goalSpawn = spawn.spawnId;
            state.hasGoal = true;
        }
    }
    return state.hasGoal;
}

// The nearest living boss is the next one on the way; a bot Mythic+ run then clears trash until its forces are met.
// A boss whose way the tank cannot find from here (a long winding route the path search gives up on) waits: the
// nearest living trash is next, pack by pack, and every fight lets the bosses be tried again.
bool FindGoal(PlayerbotAI* botAI, State& state)
{
    if (!Elapsed(state.goalCheckedAt, GOAL_RESCAN_MS))
        return state.hasGoal;
    state.goalCheckedAt = getMSTime();
    state.hasGoal = false;
    uint32 const before = state.goalSpawn;
    state.goalSpawn = 0;

    Player* bot = botAI->GetBot();
    uint32 const mapId = bot->GetMapId();
    if (NearestAlive(bot, BossSpawns(mapId), state))
    {
        // A new boss to walk to: every boss of the map and why it is or is not a goal. After Watcher Silthik's pack the
        // tank's goal went from Krik'thir at 44 yards to Hadronox at 161 with nothing blocked, and it walked past
        // Krik'thir at 3 yards into a wipe (role check, Azjol-Nerub, 2026-11-11)
        if (before && before != state.goalSpawn)
        {
            std::string bosses;
            for (BossSpawn const& spawn : BossSpawns(mapId))
            {
                if (!(spawn.spawnMask & (1 << bot->GetMap()->GetSpawnMode())))
                    continue;
                Creature* creature = nullptr;
                auto const bounds = bot->GetMap()->GetCreatureBySpawnIdStore().equal_range(spawn.spawnId);
                for (auto itr = bounds.first; itr != bounds.second && !creature; ++itr)
                    if (itr->second && itr->second->IsInWorld())
                        creature = itr->second;
                Position pos;
                bool const alive = BossAlive(bot->GetMap(), spawn, pos);
                bosses += Acore::StringFormat(
                    " [{} spawn {}{}: goal {}, {:.0f} yd, {}, flags 0x{:X}, blocked {}]",
                    creature ? creature->GetName() : "not loaded", spawn.spawnId,
                    spawn.spawnId == state.goalSpawn ? " (chosen)" : spawn.spawnId == before ? " (was)" : "", alive,
                    bot->GetExactDist(&pos), !creature ? "-" : creature->IsAlive() ? "alive" : "dead",
                    creature ? creature->GetUInt32Value(UNIT_FIELD_FLAGS) : 0, state.blockedGoals.count(spawn.spawnId));
            }
            LOG_INFO("playerbots", "{} leads to a new boss:{}", bot->GetName(), bosses);
        }
        return true;
    }
    bool const keyRun = MythicPlusBotMgr::instance().IsRunGroup(bot->GetGroup());
    if ((keyRun || !state.blockedGoals.empty()) && NearestAlive(bot, TrashSpawns(mapId), state))
        return true;
    // Every way blocked: the bosses again, the way the tank first went
    if (!state.blockedGoals.empty() && NearestAlive(bot, BossSpawns(mapId), state, false))
    {
        state.blockedGoals.clear();
        return true;
    }
    return false;
}

bool GoalIsBoss(State const& state)
{
    CreatureData const* data = state.goalSpawn ? sObjectMgr->GetCreatureData(state.goalSpawn) : nullptr;
    CreatureTemplate const* creature = data ? sObjectMgr->GetCreatureTemplate(data->id) : nullptr;
    return creature && (creature->flags_extra & CREATURE_FLAG_EXTRA_DUNGEON_BOSS);
}

// The advance found no way on toward the goal: try another for a while
void BlockGoal(State& state)
{
    state.goalBlockedAt = getMSTime();
    if (state.goalSpawn)
        state.blockedGoals.insert(state.goalSpawn);
    state.goalCheckedAt = 0;
}

// Hostile mobs nobody is fighting yet, near enough to belong to a pack in scan range.
std::vector<Creature*> Hostiles(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    std::vector<Creature*> mobs;
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsAlive() || creature->IsInCombat() || creature->IsPet() || creature->IsTotem() ||
            creature->GetCreatureType() == CREATURE_TYPE_CRITTER || !creature->IsHostileTo(bot))
            continue;
        if (bot->GetExactDist(creature) <= ScanRange + PackRadius)
            mobs.push_back(creature);
    }
    return mobs;
}

bool SameFormation(Creature* a, Creature* b) { return a->GetFormation() && a->GetFormation() == b->GetFormation(); }

// Whether a mob in a fight brings the other one into it: by standing close or by a call for help it answers
// (Creature::CallAssistance, without the checks on the enemy).
bool Brings(Creature* from, Creature* to)
{
    float const distance = from->GetExactDist(to);
    if (SamePack(distance, SameFormation(from, to)))
        return true;
    float const radius = sWorld->getFloatConfig(CONFIG_CREATURE_FAMILY_ASSISTANCE_RADIUS);
    return distance <= radius && to->HasReactState(REACT_AGGRESSIVE) && !to->IsCivilian() &&
           to->GetFaction() == from->GetFaction() && to->RespondsToCallForHelp() &&
           !to->GetCreatureTemplate()->HasFlagsExtra(CREATURE_FLAG_EXTRA_IGNORE_ALL_ASSISTANCE_CALLS) &&
           !from->GetCreatureTemplate()->HasFlagsExtra(CREATURE_FLAG_EXTRA_DONT_CALL_ASSISTANCE) &&
           from->IsWithinLOSInMap(to);
}

std::vector<Creature*> LinkedTo(size_t seed, std::vector<Creature*> const& mobs, std::vector<bool>* used = nullptr)
{
    std::vector<bool> const inPack =
        LinkedPack(seed, mobs.size(), [&mobs](size_t from, size_t to) { return Brings(mobs[from], mobs[to]); });
    std::vector<Creature*> pack;
    for (size_t i = 0; i < mobs.size(); ++i)
        if (inPack[i])
        {
            pack.push_back(mobs[i]);
            if (used)
                (*used)[i] = true;
        }
    return pack;
}

std::vector<Creature*> PackAround(Creature* seed, std::vector<Creature*> const& mobs)
{
    std::vector<Creature*> all = {seed};
    for (Creature* mob : mobs)
        if (mob != seed && !AzjolNerub::SkipHadronoxPull(mob))
            all.push_back(mob);
    return LinkedTo(0, all);
}

std::vector<std::vector<Creature*>> SplitPacks(std::vector<Creature*> const& mobs)
{
    std::vector<std::vector<Creature*>> packs;
    std::vector<bool> used(mobs.size(), false);
    for (size_t i = 0; i < mobs.size(); ++i)
        if (!used[i])
            packs.push_back(LinkedTo(i, mobs, &used));
    return packs;
}

Creature* Nearest(Player* bot, std::vector<Creature*> const& pack)
{
    Creature* nearest = nullptr;
    for (Creature* mob : pack)
        if (!nearest || bot->GetExactDist(mob) < bot->GetExactDist(nearest))
            nearest = mob;
    return nearest;
}

bool HasBoss(std::vector<Creature*> const& pack)
{
    return std::any_of(pack.begin(), pack.end(),
                       [](Creature* mob) { return mob->IsDungeonBoss() || mob->isWorldBoss(); });
}

std::vector<PackMob> Describe(std::vector<Creature*> const& pack, CcOffer const* offer)
{
    SpellInfo const* spell = offer ? sSpellMgr->GetSpellInfo(offer->spellId) : nullptr;
    std::vector<PackMob> mobs;
    for (Creature* creature : pack)
    {
        PackMob mob;
        uint32 const type = creature->GetCreatureType();
        mob.type = type <= static_cast<uint32>(MobType::Mechanical) ? static_cast<MobType>(type) : MobType::None;
        mob.caster = creature->getPowerType() == POWER_MANA && creature->GetMaxPower(POWER_MANA) > 0;
        mob.elite = creature->isElite();
        mob.boss = creature->IsDungeonBoss() || creature->isWorldBoss();
        mob.maxHealth = creature->GetMaxHealth();
        // A trap freezes through a separate spell, so it is checked against the freeze mechanic instead.
        if (offer && offer->provider->cc == Cc::Trap)
            mob.ccImmune = creature->HasMechanicTemplateImmunity(1ULL << MECHANIC_FREEZE);
        else if (spell)
            mob.ccImmune = creature->Unit::IsImmunedToSpell(spell, ObjectAccessor::GetUnit(*creature, offer->caster));
        mobs.push_back(mob);
    }
    return mobs;
}

// Group bots ready to crowd control, best kind first.
std::vector<CcOffer> GroupCc(Player* bot)
{
    std::vector<CcOffer> offers;
    Group* group = bot->GetGroup();
    if (!group)
        return offers;

    for (CcProvider const& provider : CC_PROVIDERS)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            PlayerbotAI* memberAI = member && member != bot ? GET_PLAYERBOT_AI(member) : nullptr;
            if (!memberAI || !member->IsAlive() || member->getClass() != provider.playerClass ||
                member->GetMap() != bot->GetMap() || !memberAI->HasStrategy("cc", BOT_STATE_COMBAT))
                continue;
            uint32 const spellId = memberAI->GetAiObjectContext()->GetValue<uint32>("spell id", provider.spell)->Get();
            if (!spellId)
                continue;
            offers.push_back({&provider, member->GetGUID(), spellId});
            break;
        }
    }
    return offers;
}

void PlanSpot(PlayerbotAI* botAI, State& state, Creature* seed, bool boss)
{
    Player* bot = botAI->GetBot();
    state.hasSpot = false;
    state.hasPullSpot = false;
    state.stepping = false;
    state.spotStuckAt = 0;
    // The other packs not yet pulled: the group does not gather inside their reach, or the pull brings them too
    std::vector<Creature*> others = Hostiles(botAI);
    std::vector<Creature*> const pack = PackAround(seed, others);
    std::erase_if(others, [&pack](Creature* mob) { return std::find(pack.begin(), pack.end(), mob) != pack.end(); });
    std::vector<PackMob> const mobs = Describe(pack, nullptr);
    bool const casters = std::any_of(mobs.begin(), mobs.end(), [](PackMob const& mob) { return mob.caster; });

    PullStyle style = StyleForPack(StyleFor(TierOf(bot)), casters, boss);
    // Each failed pull of this pack brings the tank closer: corner, then in sight, then straight in.
    if (state.failedPulls > 1)
        style = PullStyle::Charge;
    else if (state.failedPulls && style == PullStyle::Corner)
        style = PullStyle::Ranged;
    if (style == PullStyle::Charge)
        return;

    PathGenerator path(bot);
    path.CalculatePath(seed->GetPositionX(), seed->GetPositionY(), seed->GetPositionZ(), false);
    if (path.GetPathType() & PATHFIND_NOPATH)
        return;

    auto nearOther = [&](float x, float y, float z)
    {
        if (AzjolNerub::HadronoxTraffic(bot, x, y, z))
            return true;
        return std::any_of(others.begin(), others.end(), [&](Creature* mob)
                           { return mob->GetExactDist(x, y, z) <= mob->GetAggroRange(bot) + AggroSlack; });
    };
    auto inAggro = [&](float x, float y, float z)
    {
        auto const within = [&](Creature* mob) { return mob->GetExactDist(x, y, z) <= mob->GetAggroRange(bot); };
        return std::any_of(pack.begin(), pack.end(), within) || std::any_of(others.begin(), others.end(), within);
    };

    std::vector<PathSpot> spots;
    std::vector<G3D::Vector3> where;
    auto sample = [&](G3D::Vector3 const& point)
    {
        float const distance = seed->GetExactDist(point.x, point.y, point.z);
        bool const sees = distance <= MaxCornerDistance && seed->IsWithinLOS(point.x, point.y, point.z + EYE_HEIGHT);
        spots.push_back(
            {distance, sees, nearOther(point.x, point.y, point.z), inAggro(point.x, point.y, point.z)});
        where.push_back(point);
    };

    Movement::PointsArray const& points = path.GetPath();
    float walked = 0.0f;
    for (size_t i = 0; i < points.size() && spots.size() < MAX_SAMPLES; ++i)
    {
        if (i)
            walked += (points[i] - points[i - 1]).length();
        if (i && walked < SAMPLE_STEP && i + 1 < points.size())
            continue;
        walked = 0.0f;
        sample(points[i]);
    }

    int const index = WaitSpot(spots.data(), spots.size(), style);
    // No corner on the way when one is wanted, or no spot at all: back along the trail, and the tank steps out from
    // there to pull
    bool const wantsCorner = style == PullStyle::Corner && (index < 0 || spots[index].seesPack ||
                                                            spots[index].distToPack > MaxCornerDistance);
    int back = -1;
    if (index < 0 || wantsCorner)
    {
        std::vector<PathSpot> trail;
        for (Position const& point : state.trail)
        {
            float const distance = seed->GetExactDist(&point);
            bool const sees = distance <= MaxTrailDistance &&
                              seed->IsWithinLOS(point.GetPositionX(), point.GetPositionY(),
                                                point.GetPositionZ() + EYE_HEIGHT);
            trail.push_back({distance, sees,
                             nearOther(point.GetPositionX(), point.GetPositionY(), point.GetPositionZ())});
        }
        if (wantsCorner)
            back = TrailSpot(trail.data(), trail.size(), true);
        if (back < 0 && index < 0)
            back = TrailSpot(trail.data(), trail.size());
        if (back < 0 && index < 0)
            return;
    }
    if (back >= 0)
    {
        state.hasSpot = true;
        state.spot = state.trail[back];
        int const pullIndex = PullSpot(spots.data(), spots.size(), 0);
        if (pullIndex >= 0)
        {
            state.hasPullSpot = true;
            state.pullSpot.Relocate(where[pullIndex].x, where[pullIndex].y, where[pullIndex].z);
        }
        return;
    }
    state.hasSpot = true;
    state.spot.Relocate(where[index].x, where[index].y, where[index].z);

    int const pullIndex = PullSpot(spots.data(), spots.size(), index);
    if (pullIndex > index)
    {
        state.hasPullSpot = true;
        state.pullSpot.Relocate(where[pullIndex].x, where[pullIndex].y, where[pullIndex].z);
    }
}

void ForgetPack(State& state)
{
    state.pack.Clear();
    state.hasSpot = false;
    state.hasPullSpot = false;
    state.stepping = false;
    state.failedPulls = 0;
}

void SkipPack(PlayerbotAI* botAI, State& state, Creature* seed)
{
    for (Creature* mob : PackAround(seed, Hostiles(botAI)))
        state.skipped.insert(mob->GetGUID().GetRawValue());
    ForgetPack(state);

    // Let go of it too: with one of the pack still its target, the tank's combat engine kept walking it back toward
    // the pack while the advance walked it on, and the run stood there for minutes
    Player* bot = botAI->GetBot();
    if (PullStrategy* strategy = PullStrategy::Get(botAI))
        strategy->OnPullEnded();
    Value<Unit*>* current = botAI->GetAiObjectContext()->GetValue<Unit*>("current target");
    Unit* target = current->Get();
    if (target && state.skipped.count(target->GetGUID().GetRawValue()) && !bot->IsInCombat())
    {
        current->Set(nullptr);
        bot->AttackStop();
        bot->SetTarget(ObjectGuid::Empty);
        bot->SetSelection(ObjectGuid());
        botAI->ChangeEngine(BOT_STATE_NON_COMBAT);
    }
}

void EndPull(PlayerbotAI* botAI, State& state)
{
    Player* bot = botAI->GetBot();
    Creature* seed = state.pack.IsEmpty() ? nullptr : bot->GetMap()->GetCreature(state.pack);
    if (seed && (!seed->IsAlive() || seed->IsInCombat()))
        seed = nullptr;

    if (state.fought)
    {
        state.restFrom = getMSTime();
        state.restMs = RestAfterFightMs(TierOf(bot));
        // Somewhere new: the ways that were blocked may be open from here
        state.blockedGoals.clear();
        state.goalCheckedAt = 0;
        // Every fight gives slow drinkers a fresh wait.
        state.waitStart.clear();
        ForgetPack(state);
    }
    else if (!seed)
        ForgetPack(state);
    else if (++state.failedPulls >= MAX_FAILED_PULLS)
        SkipPack(botAI, state, seed);
    else
        PlanSpot(botAI, state, seed, HasBoss(PackAround(seed, Hostiles(botAI))));

    state.pulled = false;
    state.fought = false;
    state.overpull.Clear();
}

void Refresh(PlayerbotAI* botAI, State& state)
{
    Player* bot = botAI->GetBot();
    if (state.instanceId != bot->GetInstanceId())
    {
        state = State();
        state.instanceId = bot->GetInstanceId();
    }
    // The ground walked, for a wait spot behind the tank: only out of a fight, and not the step out to pull
    if (!bot->IsInCombat() && !state.stepping && bot->IsAlive() &&
        (state.trail.empty() || bot->GetExactDist(&state.trail.back()) >= TrailStep))
    {
        state.trail.push_back(bot->GetPosition());
        if (state.trail.size() > TrailPoints)
            state.trail.erase(state.trail.begin());
    }
    if ((state.pulled || state.fought) && !bot->IsInCombat())
    {
        PullStrategy* strategy = PullStrategy::Get(botAI);
        bool const pulling = (strategy && strategy->HasTarget()) ||
                             (state.pulled && !state.fought && !Elapsed(state.pulledAt, PULL_GRACE_MS));
        if (!pulling)
            EndPull(botAI, state);
    }
    state.done = !FindGoal(botAI, state);
}

Wait CheckReady(PlayerbotAI* botAI, State& state)
{
    if (!Elapsed(state.readyCheckedAt, READY_RECHECK_MS))
        return state.wait;

    uint32 const now = getMSTime();
    state.readyCheckedAt = now;
    state.wait = Wait::None;

    Player* bot = botAI->GetBot();
    Tier const tier = TierOf(bot);
    // While the tank steps out to pull, the group is gathered at the spot, not around the tank.
    Position const from = state.stepping ? state.spot : bot->GetPosition();
    ObjectGuid blocker;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* player = ref->GetSource();
            // Someone alive outside the instance does not hold the run; a released ghost does.
            if (!player || (player->GetMap() != bot->GetMap() && player->IsAlive()))
                continue;

            Member member;
            member.alive = player->IsAlive();
            // Nor is one who came along with the tank behind: the GM following it out to the pull spot stood 86 yards
            // from the gathering spot beside the tank, the tank waited on them for good mid-step, and as a real player
            // they were never given up on (role check, heroic Azjol-Nerub, 2026-11-09)
            member.distance = player == bot || player->GetMap() != bot->GetMap()
                                  ? 0.0f
                                  : std::min(player->GetExactDist(&from), player->GetExactDist(bot));
            // A bot beside the master it follows stands where the master does (PullReadyPolicy.h)
            if (member.distance > 0.0f)
                if (PlayerbotAI* memberAI = GET_PLAYERBOT_AI(player))
                    if (Player* master = memberAI->GetMaster();
                        master && master != player && master->IsAlive() && master->GetMap() == bot->GetMap())
                        member.distance =
                            GroupDistance(member.distance, true, player->GetExactDist(master),
                                          std::min(master->GetExactDist(&from), master->GetExactDist(bot)));
            member.healthPct = player->GetHealthPct();
            member.usesMana = player->getPowerType() == POWER_MANA && player->GetMaxPower(POWER_MANA) > 0;
            member.manaPct = member.usesMana ? player->GetPowerPct(POWER_MANA) : 100.0f;
            member.healer = PlayerbotAI::IsHeal(player);
            member.resting = IsResting(player);
            member.bot = !IsRealPlayer(player) && !IsSelfBot(player);
            member.held = state.stepping && member.bot;

            uint64 const raw = player->GetGUID().GetRawValue();
            if (state.oomCalls.count(raw))
            {
                if (OomCleared(tier, member.manaPct, member.resting))
                {
                    state.oomCalls.erase(raw);
                    state.oomAcked.erase(raw);
                }
                else
                    member.calledOom = true;
            }

            // The give-up clock runs from when this member started holding the pull, whoever else holds it too.
            Wait wait = WaitFor(member, tier);
            if (wait == Wait::None)
                state.waitStart.erase(raw);
            else
            {
                member.waitedMs = getMSTimeDiff(state.waitStart.try_emplace(raw, now).first->second, now);
                wait = WaitFor(member, tier);
            }

            if (wait != Wait::None && blocker.IsEmpty())
            {
                state.wait = wait;
                blocker = player->GetGUID();
            }
        }
    }

    if (blocker != state.waitingOn)
    {
        state.waitingOn = blocker;
        state.waitSince = now;
        state.waitAnnounced = false;
    }
    return state.wait;
}

// A wait holds the walk already under way, not only the next stretch. The core's walk to a stretch's end can run far
// past the stretch: in Azjol-Nerub the tank walked on for 90 seconds under "wait on Poskee at 52 yd", 630 yards and
// away from Krik'thir and then Hadronox as each was its goal, and later walked alone into Anub'arak with the group 160
// yards back (role check, 2026-11-09).
void HoldAdvance(PlayerbotAI* botAI, State& state)
{
    if (!state.advancing)
        return;
    state.advancing = false;
    Player* bot = botAI->GetBot();
    if (bot->IsInCombat() || !bot->isMoving() ||
        bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
        return;
    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    // Or the next stretch, once the group is up, waits out the old one's delay or is taken for the same move
    botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get().clear();
}

bool ReadyToLead(PlayerbotAI* botAI, State& state)
{
    Refresh(botAI, state);
    if (!state.done && (!state.restMs || Elapsed(state.restFrom, state.restMs)) &&
        CheckReady(botAI, state) == Wait::None)
        return true;
    HoldAdvance(botAI, state);
    return false;
}

// The walk on toward the boss as the tank would take it, a point every few yards, as far as the pack scan reaches
std::vector<G3D::Vector3> WayAhead(Player* bot, Position const& goal)
{
    std::vector<G3D::Vector3> way;
    PathGenerator path(bot);
    path.CalculatePath(goal.GetPositionX(), goal.GetPositionY(), goal.GetPositionZ(), false);
    Movement::PointsArray const& points = path.GetPath();
    if ((path.GetPathType() & PATHFIND_NOPATH) || points.empty())
        return way;

    float walked = 0.0f;
    way.push_back(points[0]);
    for (size_t i = 1; i < points.size() && walked < ScanRange; ++i)
    {
        G3D::Vector3 const leg = points[i] - points[i - 1];
        float const length = leg.length();
        for (float along = SAMPLE_STEP; along < length && walked + along < ScanRange; along += SAMPLE_STEP)
            way.push_back(points[i - 1] + leg * (along / length));
        walked += length;
        way.push_back(points[i]);
    }
    return way;
}

Creature* CurrentPack(PlayerbotAI* botAI, State& state)
{
    Player* bot = botAI->GetBot();
    if (!state.pack.IsEmpty())
    {
        Creature* seed = bot->GetMap()->GetCreature(state.pack);
        if (seed && seed->IsAlive() && !seed->IsInCombat() && !AzjolNerub::SkipHadronoxPull(seed))
            return seed;
        ForgetPack(state);
    }

    if (!Elapsed(state.packCheckedAt, PACK_RESCAN_MS))
        return nullptr;
    state.packCheckedAt = getMSTime();

    bool const hasGoal = FindGoal(botAI, state);
    float const leaderToGoal = hasGoal ? bot->GetExactDist(&state.goal) : 0.0f;
    std::vector<Creature*> mobs = Hostiles(botAI);
    std::erase_if(
        mobs, [&state](Creature* mob)
        { return state.skipped.count(mob->GetGUID().GetRawValue()) > 0 || AzjolNerub::SkipHadronoxPull(mob); });
    std::vector<std::vector<Creature*>> packs = SplitPacks(mobs);
    std::vector<G3D::Vector3> const way = hasGoal ? WayAhead(bot, state.goal) : std::vector<G3D::Vector3>();
    std::vector<Player*> members;
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource())
                if (member != bot && member->IsAlive() && member->GetMap() == bot->GetMap())
                    members.push_back(member);
    std::vector<PackCandidate> candidates;
    for (std::vector<Creature*> const& pack : packs)
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        for (Creature* mob : pack)
        {
            x += mob->GetPositionX();
            y += mob->GetPositionY();
            z += mob->GetPositionZ();
        }
        float const count = static_cast<float>(pack.size());
        Position const center(x / count, y / count, z / count);

        PackCandidate candidate;
        candidate.distToLeader = bot->GetExactDist(Nearest(bot, pack));
        candidate.distToGoal = hasGoal ? state.goal.GetExactDist(&center) : 0.0f;
        candidate.heightDiff = center.GetPositionZ() - bot->GetPositionZ();
        for (Creature* mob : pack)
        {
            float const reach = mob->GetAggroRange(bot) + AggroSlack;
            candidate.aggroReach = std::max(candidate.aggroReach, reach);
            candidate.onPath = candidate.onPath ||
                               std::any_of(way.begin(), way.end(), [&](G3D::Vector3 const& point)
                                           { return mob->GetExactDist(point.x, point.y, point.z) <= reach; });
            // A pack by any member is on the group's way too: the back line trails the tank by 30 yards on its own
            // route, and walked into goblins the tank's walk passed by (Deadmines foundry, 2026-11-07)
            candidate.onPath = candidate.onPath ||
                               std::any_of(members.begin(), members.end(),
                                           [&](Player* member) { return mob->GetExactDist(member) <= reach; });
        }
        candidates.push_back(candidate);
    }

    for (uint32 tries = 0; tries < MAX_PATH_TRIES; ++tries)
    {
        int const index = Choose(candidates.data(), candidates.size(), hasGoal, leaderToGoal);
        if (index < 0)
            break;
        Creature* seed = Nearest(bot, packs[index]);
        // Only no path at all rules a pack out. An incomplete one is a long way round (up a ramp, past the path's point
        // limit) that the advance, which walks the same way, takes all the same: skipping that pack walked the tank
        // into it unpulled.
        PathGenerator path(bot);
        path.CalculatePath(seed->GetPositionX(), seed->GetPositionY(), seed->GetPositionZ(), false);
        if (path.GetPathType() & PATHFIND_NOPATH)
        {
            candidates[index].reachable = false;
            continue;
        }
        state.pack = seed->GetGUID();
        PlanSpot(botAI, state, seed, HasBoss(packs[index]));
        return seed;
    }
    return nullptr;
}

Unit* KillTarget(PlayerbotAI* botAI, Creature* seed)
{
    std::vector<Creature*> pack = PackAround(seed, Hostiles(botAI));
    std::vector<PackMob> mobs = Describe(pack, nullptr);
    int const skull = KillFirst(mobs.data(), mobs.size());
    return skull >= 0 ? pack[skull] : nullptr;
}

void MarkPack(PlayerbotAI* botAI, State& state, Tier tier, std::vector<Creature*> const& pack, int skull)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return;

    group->SetTargetIcon(ICON_SKULL, bot->GetGUID(), pack[skull]->GetGUID());

    int moon = -1;
    CcOffer chosen{nullptr, ObjectGuid::Empty, 0};
    if (RequestsCc(tier))
    {
        for (CcOffer const& offer : GroupCc(bot))
        {
            std::vector<PackMob> mobs = Describe(pack, &offer);
            moon = CcTarget(mobs.data(), mobs.size(), offer.provider->cc, skull);
            if (moon >= 0)
            {
                chosen = offer;
                break;
            }
        }
    }
    group->SetTargetIcon(ICON_MOON, bot->GetGUID(), moon >= 0 ? pack[moon]->GetGUID() : ObjectGuid::Empty);

    std::vector<PackMob> mobs = Describe(pack, nullptr);
    int const cross = pack.size() > 2 ? KillFirst(mobs.data(), mobs.size(), skull, moon) : -1;
    group->SetTargetIcon(ICON_CROSS, bot->GetGUID(), cross >= 0 ? pack[cross]->GetGUID() : ObjectGuid::Empty);

    if (chosen.provider)
    {
        Player* caster = ObjectAccessor::GetPlayer(*bot, chosen.caster);
        if (caster)
            Say(botAI, chosen.provider->text, chosen.provider->fallback, {{"%name", caster->GetName()}});
    }
    else if (!state.pulls || urand(1, 100) <= SKULL_CALL_CHANCE)
        Say(botAI, "lead_skull", "Kill skull first.");
}

ObjectGuid ExtraPack(Player* bot, std::vector<Creature*> const& pulled, std::vector<Creature*> const& mobs)
{
    Creature* best = nullptr;
    float bestDistance = OVERPULL_REACH;
    for (Creature* mob : mobs)
    {
        if (AzjolNerub::SkipHadronoxPull(mob) || std::find(pulled.begin(), pulled.end(), mob) != pulled.end())
            continue;
        float const distance = bot->GetExactDist(mob);
        if (distance < bestDistance)
        {
            best = mob;
            bestDistance = distance;
        }
    }
    return best ? best->GetGUID() : ObjectGuid::Empty;
}

void NoteFight(State& state)
{
    if (state.fought)
        return;
    state.fought = true;
    state.overpullAt = getMSTime();
}
}  // namespace

void DungeonLeadStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Talk and marks never take the turn, so like "living arena call" they run first and let the rest follow.
    triggers.push_back(new TriggerNode(
        "dungeon lead", {NextAction("lead talk", ACTION_PASSTHROUGH), NextAction("lead marks", ACTION_PASSTHROUGH),
                         NextAction("lead overpull", ACTION_HIGH + 5.0f), NextAction("lead pull", ACTION_MOVE + 2.0f),
                         NextAction("lead advance", ACTION_MOVE + 1.0f)}));
    triggers.push_back(new TriggerNode("lead healer", {NextAction("lead healer call", ACTION_PASSTHROUGH)}));
    // Above following: a healer out of the fight goes to it instead of after its master
    triggers.push_back(new TriggerNode("lead fight away", {NextAction("lead join fight", ACTION_MOVE + 3.0f)}));
    // Above following, so a healer the group waits for sits down instead of walking after the tank.
    triggers.push_back(new TriggerNode("lead needs mana", {NextAction("drink", ACTION_NORMAL)}));
}

void DungeonLeadStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new LeadHoldMultiplier(botAI));
}

bool DungeonLeadTrigger::IsActive() { return IsLeader(botAI); }

bool LeadHealerTrigger::IsActive()
{
    return EnabledFor(bot) && bot->GetGroup() && bot->GetMaxPower(POWER_MANA) > 0 && botAI->IsInNonRaidDungeon() &&
           PlayerbotAI::IsHeal(bot);
}

namespace
{
// Out of a fight, this far from the member under attack the healer still walks to it
float constexpr JOIN_FIGHT_RANGE = 80.0f;
// Into heal range by this much, so a step of the fight keeps the member in it
float constexpr JOIN_FIGHT_MARGIN = 6.0f;

// The member a mob is beating on that the healer, out of the fight, cannot heal from where it stands: the tank first.
// Healers out of combat followed their master away and round corners while the group fought, and a third of the hurt
// fights got no heal (solo-wow role check, 2026-11-07). Not while the tank's pull is still under way: the pack comes
// back to the group.
Player* FightOutOfReach(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!EnabledFor(bot) || !group || !bot->IsAlive() || bot->IsInCombat() || !PlayerbotAI::IsHeal(bot) ||
        !botAI->IsInNonRaidDungeon())
        return nullptr;
    if (Player* leader = Leader(botAI))
        if (PlayerbotAI* leaderAI = GET_PLAYERBOT_AI(leader))
            if (PullStrategy const* pull = PullStrategy::Get(leaderAI))
                if (pull->HasPullStarted() || pull->IsPullPendingToStart())
                    return nullptr;

    float const range = botAI->GetRange("heal") - JOIN_FIGHT_MARGIN;
    // Not its master: following takes it there anyway, and a raid opened by the master walked both healers toward
    // the pack it pulled 50 yards out (Karazhan, 2026-11-08)
    Player* master = botAI->GetMaster();
    Player* best = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || member == master || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            bot->GetExactDist(member) > JOIN_FIGHT_RANGE)
            continue;
        bool const beaten = std::any_of(member->getAttackers().begin(), member->getAttackers().end(),
                                        [member](Unit* mob)
                                        { return mob->IsAlive() && mob->IsCreature() && mob->GetVictim() == member; });
        if (!beaten || (bot->GetExactDist(member) <= range && bot->IsWithinLOSInMap(member)))
            continue;
        if (!best || (PlayerbotAI::IsTank(member) && !PlayerbotAI::IsTank(best)))
            best = member;
    }
    return best;
}
}  // namespace

bool LeadFightAwayTrigger::IsActive() { return FightOutOfReach(botAI) != nullptr; }

bool LeadJoinFightAction::isUseful() { return botAI->CanMove() && FightOutOfReach(botAI) != nullptr; }

bool LeadJoinFightAction::Execute(Event /*event*/)
{
    Player* member = FightOutOfReach(botAI);
    if (!member)
        return false;
    float const range = botAI->GetRange("heal") - JOIN_FIGHT_MARGIN;
    // In sight: straight into heal range
    if (bot->IsWithinLOSInMap(member))
        return MoveTo(member, range, MovementPriority::MOVEMENT_NORMAL);
    // Round a corner: along the path to the member, to the first point that sees it in heal range. Walking all the
    // way in put the healer among the mobs.
    PathGenerator path(bot);
    path.CalculatePath(member->GetPositionX(), member->GetPositionY(), member->GetPositionZ(), false);
    if (path.GetPathType() & PATHFIND_NOPATH)
        return false;
    Movement::PointsArray const& points = path.GetPath();
    G3D::Vector3 stop = points.empty() ? G3D::Vector3(member->GetPositionX(), member->GetPositionY(),
                                                      member->GetPositionZ())
                                       : points.back();
    for (size_t i = 1; i < points.size(); ++i)
    {
        // Every few yards along each leg of the path
        G3D::Vector3 const from = points[i - 1];
        G3D::Vector3 const leg = points[i] - from;
        int const steps = std::max(1, static_cast<int>(leg.length() / 3.0f));
        bool found = false;
        for (int s = 1; s <= steps && !found; ++s)
        {
            G3D::Vector3 const point = from + leg * (static_cast<float>(s) / steps);
            if (member->GetExactDist(point.x, point.y, point.z) <= range &&
                member->IsWithinLOS(point.x, point.y, point.z + EYE_HEIGHT))
            {
                stop = point;
                found = true;
            }
        }
        if (found)
            break;
    }
    return MoveTo(member->GetMapId(), stop.x, stop.y, stop.z, false, false, false, false,
                  MovementPriority::MOVEMENT_NORMAL, true);
}

bool LeadNeedsManaTrigger::IsActive()
{
    if (!EnabledFor(bot) || bot->IsInCombat() || !bot->GetGroup() || bot->GetMaxPower(POWER_MANA) == 0)
        return false;

    float const mana = bot->GetPowerPct(POWER_MANA);
    if (mana >= HealerManaToPull(Tier::Good) || IsResting(bot) || !botAI->IsInNonRaidDungeon() ||
        !PlayerbotAI::IsHeal(bot))
        return false;

    Player* leader = Leader(botAI);
    return leader && mana < HealerManaToPull(TierOf(leader));
}

float LeadHoldMultiplier::GetValue(Action* action)
{
    bool const follow = dynamic_cast<FollowAction*>(action) != nullptr;
    bool const attack = !follow && dynamic_cast<AttackAction*>(action) != nullptr;
    if (!follow && !attack)
        return 1.0f;

    Player* leader = Leader(botAI);
    PlayerbotAI* leaderAI = leader && leader != bot ? GET_PLAYERBOT_AI(leader) : nullptr;
    if (!leaderAI)
        return 1.0f;

    // The tank opens every fight: until it is fighting, the others start none. Their assist picks the skull as soon as
    // it is marked, so a ranged opener beat the tank's pull to the pack, and kept shooting a pack the tank could not
    // reach. A member something attacks fights back, and the player's own "attack" (or a fight of theirs) is obeyed.
    if (attack)
    {
        if (dynamic_cast<AttackMyTargetAction*>(action) || PlayerbotAI::IsTank(bot) || leader->IsInCombat() ||
            bot->IsInCombat())
            return 1.0f;
        Player* master = botAI->GetMaster();
        return master && master != bot && master->IsInCombat() ? 1.0f : 0.0f;
    }

    if (StateOf(leaderAI).stepping)
        return 0.0f;

    PullStrategy const* strategy = PullStrategy::Get(leaderAI);
    if (!strategy || !strategy->HasTarget())
        return 1.0f;

    return strategy->IsPullPendingToStart() || strategy->HasPullStarted() ? 0.0f : 1.0f;
}

bool LeadTalkAction::isUseful() { return IsLeader(botAI) && !bot->IsInCombat(); }

bool LeadTalkAction::Execute(Event /*event*/)
{
    State& state = StateOf(botAI);
    Refresh(botAI, state);
    if (state.done)
        return false;

    if (!state.announced)
    {
        state.announced = true;
        Say(botAI, "lead_start", "I'll tank. Stay behind me and let me pull.");
        return false;
    }

    for (auto const& [raw, calledAt] : state.oomCalls)
    {
        if (state.oomAcked[raw])
            continue;
        state.oomAcked[raw] = true;
        if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid(raw)))
            Say(botAI, "lead_oom_ack", "Ok %name, waiting for your mana.", {{"%name", player->GetName()}});
        return false;
    }

    bool const manaWait = state.wait == Wait::HealerMana || state.wait == Wait::Drinking;
    if (manaWait && !state.waitAnnounced && MarksTargets(TierOf(bot)) && Elapsed(state.waitSince, WAIT_CALL_DELAY_MS))
    {
        state.waitAnnounced = true;
        Say(botAI, "lead_wait_mana", "Waiting on mana.");
    }
    return false;
}

bool LeadAdvanceAction::isUseful()
{
    if (!IsLeader(botAI) || bot->IsInCombat() || !botAI->CanMove())
        return false;

    State& state = StateOf(botAI);
    if (!ReadyToLead(botAI, state) || state.pulled)
        return false;

    if (CurrentPack(botAI, state))
    {
        state.idle = false;
        if (!state.hasSpot)
            return false;
        if (!state.stepping)
        {
            if (bot->GetExactDist(&state.spot) > SPOT_REACHED)
                return true;
            if (!state.hasPullSpot)
                return false;
            // The group has only just caught up: look again before stepping out on its own.
            state.readyCheckedAt = 0;
            if (CheckReady(botAI, state) != Wait::None)
                return false;
            state.stepping = true;
        }
        return bot->GetExactDist(&state.pullSpot) > SPOT_REACHED;
    }

    if (AzjolNerub::WaitForHadronox(bot))
    {
        HoldAdvance(botAI, state);
        state.idle = false;
        return false;
    }

    // At a trash goal with nothing there to pull: the next one. Idle there, the tank was no leader any more and nothing
    // ever looked for another goal: the run stood on one spot for 10 minutes (the Oculus, role check 2026-11-09)
    if (state.hasGoal && bot->GetExactDist(&state.goal) <= GOAL_REACHED && !GoalIsBoss(state))
    {
        BlockGoal(state);
        state.idle = false;
        return false;
    }

    // Nothing to pull and nowhere to go: the group follows its master until a pack turns up. A blocked goal with
    // another to head for instead is no reason to stop.
    state.idle = !state.hasGoal || (state.blockedGoals.empty() && !Elapsed(state.goalBlockedAt, GOAL_BLOCKED_MS)) ||
                 bot->GetExactDist(&state.goal) <= GOAL_REACHED;
    return !state.idle;
}

namespace
{
// A start the way on is cut short from: Ahn'kahet's door stands on a scrap of walkable ground the path search finds no
// way off, every way on came back cut short at two points, and the group stood at the door for the whole run with 119
// goals set aside. A spot a few yards off in sight, at about the same height, that the way on does run from: walked to
// in a straight line, as a player steps off the door's step.
bool OffIsland(Player* bot, Position const& goal, G3D::Vector3& to)
{
    Map* map = bot->GetMap();
    for (float radius : {4.0f, 8.0f, 12.0f, 16.0f, 20.0f})
        for (int i = 0; i < 16; ++i)
        {
            float const angle = bot->GetAbsoluteAngle(&goal) + float(i) * float(M_PI) / 8.0f;
            float const x = bot->GetPositionX() + radius * std::cos(angle);
            float const y = bot->GetPositionY() + radius * std::sin(angle);
            float const z = map->GetHeight(bot->GetPhaseMask(), x, y, bot->GetPositionZ() + 3.0f);
            if (z <= INVALID_HEIGHT || std::fabs(z - bot->GetPositionZ()) > 4.0f ||
                !bot->IsWithinLOS(x, y, z + EYE_HEIGHT))
                continue;
            PathGenerator path(bot);
            path.SetUseStraightPath(true);
            path.CalculatePath(x, y, z, goal.GetPositionX(), goal.GetPositionY(), goal.GetPositionZ(), false);
            // Nor a straight line through the air, which the search draws a player when either end is off the ground:
            // the tank stepped off toward a mob in the Oculus's sky as if the way ran from there
            if (path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_NOT_USING_PATH))
                continue;
            to = G3D::Vector3(x, y, z);
            return true;
        }
    return false;
}
}  // namespace

bool LeadAdvanceAction::Execute(Event /*event*/)
{
    State& state = StateOf(botAI);
    // Off a scrap of ground the way on is cut short from: a straight walk, no path search
    auto const StepOffIsland = [this](State const& lead) -> bool
    {
        G3D::Vector3 to;
        if (!lead.hasGoal || !OffIsland(bot, lead.goal, to))
            return false;
        LOG_INFO("playerbots", "{} steps off to {:.1f} {:.1f} {:.1f}: no way on from {:.1f} {:.1f} {:.1f}", bot->GetName(),
                  to.x, to.y, to.z, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
        if (bot->IsSitState())
            bot->SetStandState(UNIT_STAND_STATE_STAND);
        bot->GetMotionMaster()->MovePoint(0, to.x, to.y, to.z, FORCED_MOVEMENT_NONE, 0.0f, 0.0f, false);
        AI_VALUE(LastMovement&, "last movement")
            .Set(bot->GetMapId(), to.x, to.y, to.z, bot->GetOrientation(),
                 IN_MILLISECONDS * MoveDelay(bot->GetExactDist(to.x, to.y, to.z), false), MovementPriority::MOVEMENT_NORMAL);
        return true;
    };
    if (!state.pack.IsEmpty() && state.hasSpot)
    {
        state.advancing = false;
        Position const& to = state.stepping ? state.pullSpot : state.spot;
        if (MoveTo(bot->GetMapId(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false, false, false,
                   false, MovementPriority::MOVEMENT_NORMAL, true))
        {
            state.spotStuckAt = 0;
            return true;
        }
        if (bot->isMoving())
            return false;
        if (!state.spotStuckAt)
        {
            state.spotStuckAt = getMSTime();
            return false;
        }
        if (!Elapsed(state.spotStuckAt, SPOT_STUCK_MS))
            return false;
        // A spot off the walkable ground (deep water, past a ledge) failed every move, and the tank stood at the
        // gathering spot for the rest of the run (Steamvault, solo-wow role check 2026-11-07): pull from here instead
        state.spotStuckAt = 0;
        if (state.stepping)
            state.stepping = false;
        else
            state.hasSpot = false;
        state.hasPullSpot = false;
        return false;
    }

    // No pack on the way: walk a stretch toward the boss, then look again. The corners of the way, not the smoothed
    // path: a smoothed point every few yards ran out of points on a long winding route (Utgarde Keep's to Prince
    // Keleseth), the search called that no path, and the tank set every boss aside in turn.
    PathGenerator path(bot);
    path.SetUseStraightPath(true);
    path.CalculatePath(state.goal.GetPositionX(), state.goal.GetPositionY(), state.goal.GetPositionZ(), false);
    Movement::PointsArray const& points = path.GetPath();
    if ((path.GetPathType() & PATHFIND_NOPATH) || points.size() < 2)
    {
        if (StepOffIsland(state))
            return true;
        BlockGoal(state);
        return false;
    }

    float length = 0.0f;
    for (size_t i = 1; i < points.size(); ++i)
        length += (points[i] - points[i - 1]).length();
    // A long way round the path search gives up on ends in a dead end close by: the tank would shuffle there for good
    if ((path.GetPathType() & PATHFIND_INCOMPLETE) && length < ADVANCE_STEP)
    {
        if (StepOffIsland(state))
            return true;
        BlockGoal(state);
        return false;
    }

    // No nearer for a while, in a straight line or along the way: the advance goes nowhere. After the Anub'arak fight
    // in Azjol-Nerub it went "OK" tick after tick for six minutes with the tank 222-224 yards from the boss, "0
    // blocked" (role check, 2026-11-09). Set aside, the goal is tried again after a fight or once every way is blocked.
    float const toGoal = bot->GetExactDist(&state.goal);
    bool const restart = state.progressGoal != state.goalSpawn || Elapsed(state.advancedAt, ADVANCE_GAP_MS);
    state.advancedAt = getMSTime();
    if (restart || toGoal < state.bestToGoal - PROGRESS_STEP || length < state.bestWay - PROGRESS_STEP)
    {
        state.progressGoal = state.goalSpawn;
        state.progressAt = getMSTime();
        state.bestToGoal = toGoal;
        state.bestWay = length;
    }
    else if (Elapsed(state.progressAt, NO_PROGRESS_MS))
    {
        LOG_INFO("playerbots",
                 "{} gets no nearer to {:.1f} {:.1f} {:.1f} from {:.1f} {:.1f} {:.1f}: {:.0f} yd, way {:.0f} yd, path "
                 "type {}",
                 bot->GetName(), state.goal.GetPositionX(), state.goal.GetPositionY(), state.goal.GetPositionZ(),
                 bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), toGoal, length,
                 uint32(path.GetPathType()));
        state.progressGoal = 0;
        BlockGoal(state);
        return false;
    }

    // The stretch ends ADVANCE_STEP along the way, partway down a long straight leg between two corners
    float walked = 0.0f;
    G3D::Vector3 stop = points.back();
    G3D::Vector3 corner = points.back();  // the corner the stretch ends before: a point on the ground for certain
    for (size_t i = 1; i < points.size(); ++i)
    {
        float const leg = (points[i] - points[i - 1]).length();
        if (walked + leg >= ADVANCE_STEP)
        {
            stop = points[i - 1] + (points[i] - points[i - 1]) * ((ADVANCE_STEP - walked) / leg);
            corner = points[i];
            break;
        }
        walked += leg;
    }
    if (bot->GetExactDist(stop.x, stop.y, stop.z) < SPOT_REACHED)
    {
        BlockGoal(state);
        return false;
    }
    // Still on the way to the last stretch's end: no new move yet
    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_NORMAL))
        return false;

    // Partway down a leg the straight line between two corners can run under a rise or over a dip, where the move
    // found no ground and failed, tick after tick: the tank stood there for the rest of the run (Ragefire Chasm, Pit of
    // Saron). Onto the floor there, else to the corner itself; neither, and the way counts as blocked.
    float const lineZ = stop.z;
    bot->UpdateAllowedPositionZ(stop.x, stop.y, stop.z);
    // Far under the line is no dip but a drop. With no ground found at the goal's end the search draws a player a
    // straight line to it; the floor under that line's stretch lay 250 yards down, and the tank walked off the Oculus's
    // entrance ring toward a mob in the air and spent the run under the rings: 1 fight in 12 minutes (role check,
    // 2026-11-09).
    bool const throughAir = (path.GetPathType() & PATHFIND_NOT_USING_PATH) != 0;
    bool const drop = lineZ - stop.z > MAX_DIP;
    if (throughAir && drop)
    {
        BlockGoal(state);
        return false;
    }
    // A floor that far under a leg on the ground is another level (a bridge or ledge the leg runs over): the walk to it
    // is no stretch but the way round to that level. The corner is on the way for certain.
    if (!drop && MoveTo(bot->GetMapId(), stop.x, stop.y, stop.z, false, false, false, false,
                        MovementPriority::MOVEMENT_NORMAL, true))
    {
        state.advancing = true;
        return true;
    }
    // A line through the air has no corner on the ground: its end is the goal itself
    if (!throughAir && bot->GetExactDist(corner.x, corner.y, corner.z) >= SPOT_REACHED &&
        MoveTo(bot->GetMapId(), corner.x, corner.y, corner.z, false, false, false, false,
               MovementPriority::MOVEMENT_NORMAL, true))
    {
        state.advancing = true;
        return true;
    }
    // Why neither move went: after a wipe the tank stood at the Azjol-Nerub entrance for six minutes, this failing on
    // goal after goal with a path found to each (role check, 2026-11-11)
    LOG_INFO("playerbots",
             "{} cannot step toward its goal from {:.1f} {:.1f} {:.1f}: stop {:.1f} {:.1f} {:.1f} (line z {:.1f}), corner "
             "{:.1f} {:.1f} {:.1f}, drop {}, through air {}, can move {}, unit state 0x{:X}, stand state {}, path type {}",
             bot->GetName(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), stop.x, stop.y, stop.z, lineZ,
             corner.x, corner.y, corner.z, drop, throughAir, botAI->CanMove(), bot->GetUnitState(),
             uint32(bot->getStandState()), uint32(path.GetPathType()));
    BlockGoal(state);
    return false;
}

bool LeadPullAction::isUseful()
{
    // Not with a mob already on someone: that fight is the tank's first (PullEndTrigger)
    if (!IsLeader(botAI) || bot->IsInCombat() || PullStrategy::FightOnMember(bot))
        return false;

    State& state = StateOf(botAI);
    if (!ReadyToLead(botAI, state) || state.pulled)
        return false;

    Creature* seed = CurrentPack(botAI, state);
    if (!seed)
        return false;
    if (state.hasSpot)
    {
        if (state.hasPullSpot && !state.stepping)
            return false;
        Position const& from = state.hasPullSpot ? state.pullSpot : state.spot;
        if (bot->GetExactDist(&from) > SPOT_REACHED)
            return false;
    }

    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy || strategy->HasTarget())
        return false;

    Unit* target = KillTarget(botAI, seed);
    return target && bot->GetDistance(target) <= sPlayerbotAIConfig.ReactDistance * 3.0f;
}

bool LeadPullAction::Execute(Event event)
{
    State& state = StateOf(botAI);
    Creature* seed = CurrentPack(botAI, state);
    if (!seed)
        return false;

    Tier const tier = TierOf(bot);
    std::vector<Creature*> const hostiles = Hostiles(botAI);
    std::vector<Creature*> const pack = PackAround(seed, hostiles);
    std::vector<PackMob> const mobs = Describe(pack, nullptr);
    int const skull = KillFirst(mobs.data(), mobs.size());
    if (skull < 0)
        return false;

    Creature* target = pack[skull];
    if (!AttackersValue::IsPossibleTarget(target, bot))
    {
        SkipPack(botAI, state, seed);
        return false;
    }

    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (strategy && strategy->CanDoPullAction(target))
    {
        _target = target->GetGUID();
        if (!PullRequestAction::Execute(event))
            return false;
        // The tank brings the pack back to the gathering spot, out of its sight.
        if (state.hasSpot)
        {
            PositionInfo& back = AI_VALUE(PositionMap&, "position")["pull"];
            back.Set(state.spot.GetPositionX(), state.spot.GetPositionY(), state.spot.GetPositionZ(), bot->GetMapId());
        }
    }
    else
    {
        // No pull spell or ranged weapon: run in and hit the skull.
        Group* group = bot->GetGroup();
        if (!group)
            return false;
        group->SetTargetIcon(ICON_SKULL, bot->GetGUID(), target->GetGUID());
        if (!botAI->DoSpecificAction("attack rti target", Event(), true))
            return false;
    }

    if (MarksTargets(tier))
        MarkPack(botAI, state, tier, pack, skull);
    else if (urand(1, 100) <= PULL_CALL_CHANCE)
        Say(botAI, "lead_pulling", "Pulling.");

    state.pulled = true;
    state.pulledAt = getMSTime();
    state.stepping = false;
    ++state.pulls;
    if (urand(1, 100) <= OverpullChance(tier))
        state.overpull = ExtraPack(bot, pack, hostiles);
    return true;
}

Unit* LeadPullAction::GetPullTarget(Event /*event*/) { return botAI->GetUnit(_target); }

bool LeadMarksAction::isUseful()
{
    if (!IsLeader(botAI) || !bot->IsInCombat())
        return false;

    State& state = StateOf(botAI);
    NoteFight(state);
    if (!bot->GetGroup() || !MarksTargets(TierOf(bot)) || !Elapsed(state.marksCheckedAt, MARKS_RECHECK_MS))
        return false;
    state.marksCheckedAt = getMSTime();

    Unit* skull = botAI->GetUnit(bot->GetGroup()->GetTargetIcon(ICON_SKULL));
    return !skull || !skull->IsAlive() || !skull->IsInCombat();
}

bool LeadMarksAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    Unit* moon = botAI->GetUnit(group->GetTargetIcon(ICON_MOON));
    if (moon && !moon->IsAlive())
        moon = nullptr;

    std::vector<Creature*> rest;
    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* unit = botAI->GetUnit(guid);
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (creature && creature->IsAlive() && creature != moon)
            rest.push_back(creature);
    }

    // Only the crowd-controlled mob is left: it dies last, under skull.
    if (rest.empty())
    {
        if (moon)
        {
            group->SetTargetIcon(ICON_MOON, bot->GetGUID(), ObjectGuid::Empty);
            group->SetTargetIcon(ICON_SKULL, bot->GetGUID(), moon->GetGUID());
        }
        return false;
    }

    std::vector<PackMob> const mobs = Describe(rest, nullptr);
    int const skull = KillFirst(mobs.data(), mobs.size());
    if (skull >= 0)
        group->SetTargetIcon(ICON_SKULL, bot->GetGUID(), rest[skull]->GetGUID());
    return false;
}

bool LeadOverpullAction::isUseful()
{
    if (!IsLeader(botAI) || !bot->IsInCombat())
        return false;

    // The window opens when the pack reaches the tank, not when the pull was asked for.
    State& state = StateOf(botAI);
    NoteFight(state);
    if (state.overpull.IsEmpty())
        return false;

    Unit* mob = botAI->GetUnit(state.overpull);
    if (Elapsed(state.overpullAt, OVERPULL_WINDOW_MS) || !mob || !mob->IsAlive() || mob->IsInCombat())
    {
        state.overpull.Clear();
        return false;
    }
    return bot->IsWithinDistInMap(mob, OVERPULL_REACH) && bot->IsWithinLOSInMap(mob);
}

bool LeadOverpullAction::Execute(Event /*event*/)
{
    State& state = StateOf(botAI);
    Unit* mob = botAI->GetUnit(state.overpull);
    PullStrategy* strategy = PullStrategy::Get(botAI);
    std::string const spell = strategy ? strategy->GetSpellName() : "";
    if (!mob || spell.empty() || !botAI->CastSpell(spell, mob))
        return false;

    state.overpull.Clear();
    Say(botAI, "lead_overpull", "Oops, grabbed the next pack too!");
    return true;
}

bool LeadHealerCallAction::Execute(Event /*event*/)
{
    State& state = StateOf(botAI);
    switch (NextCall(state.calls, bot->GetPowerPct(POWER_MANA), IsResting(bot)))
    {
        case Call::Oom:
            Say(botAI, "lead_healer_oom", "OOM");
            break;
        case Call::Drinking:
            Say(botAI, "lead_healer_drinking", "Drinking, give me a moment.");
            break;
        default:
            break;
    }
    return false;
}
