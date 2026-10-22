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

#include "AttackersValue.h"
#include "Creature.h"
#include "DungeonLead.h"
#include "Event.h"
#include "FollowActions.h"
#include "Group.h"
#include "Map.h"
#include "MarkPolicy.h"
#include "ObjectMgr.h"
#include "PackPolicy.h"
#include "PathGenerator.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "PullStrategy.h"
#include "RtiTargetValue.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "WaitSpotPolicy.h"

using namespace DungeonLead;

namespace
{
uint8 constexpr ICON_MOON = RtiTargetValue::moonIndex;
uint8 constexpr ICON_CROSS = RtiTargetValue::crossIndex;
uint8 constexpr ICON_SKULL = RtiTargetValue::skullIndex;
uint32 constexpr GOAL_RESCAN_MS = 5000;
uint32 constexpr GOAL_BLOCKED_MS = 10000;
uint32 constexpr PACK_RESCAN_MS = 1000;
uint32 constexpr READY_RECHECK_MS = 500;
uint32 constexpr MARKS_RECHECK_MS = 1000;
uint32 constexpr WAIT_CALL_DELAY_MS = 4000;
uint32 constexpr OVERPULL_WINDOW_MS = 8000;
// A tank running in without a pull spell is not in combat until it reaches the pack.
uint32 constexpr PULL_GRACE_MS = 6000;
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
};

bool IsLeader(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    return Enabled() && bot->IsAlive() &&
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

bool BossAlive(Map* map, BossSpawn const& spawn, Position& pos)
{
    auto const bounds = map->GetCreatureBySpawnIdStore().equal_range(spawn.spawnId);
    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        Creature* creature = itr->second;
        if (!creature || !creature->IsInWorld())
            continue;
        pos.Relocate(creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ());
        return creature->IsAlive();
    }
    pos = spawn.pos;
    return map->GetCreatureRespawnTime(spawn.spawnId) <= time(nullptr);
}

// The nearest living boss is the next one on the way.
bool FindGoal(PlayerbotAI* botAI, State& state)
{
    if (!Elapsed(state.goalCheckedAt, GOAL_RESCAN_MS))
        return state.hasGoal;
    state.goalCheckedAt = getMSTime();
    state.hasGoal = false;

    Player* bot = botAI->GetBot();
    Map* map = bot->GetMap();
    float best = 0.0f;
    for (BossSpawn const& spawn : BossSpawns(map->GetId()))
    {
        if (!(spawn.spawnMask & (1 << map->GetSpawnMode())))
            continue;
        Position pos;
        if (!BossAlive(map, spawn, pos))
            continue;
        float const distance = bot->GetExactDist(&pos);
        if (!state.hasGoal || distance < best)
        {
            best = distance;
            state.goal = pos;
            state.hasGoal = true;
        }
    }
    return state.hasGoal;
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

std::vector<Creature*> PackAround(Creature* seed, std::vector<Creature*> const& mobs)
{
    std::vector<Creature*> pack = {seed};
    for (Creature* mob : mobs)
        if (mob != seed && SamePack(mob->GetExactDist(seed), SameFormation(mob, seed)))
            pack.push_back(mob);
    return pack;
}

std::vector<std::vector<Creature*>> SplitPacks(std::vector<Creature*> const& mobs)
{
    std::vector<std::vector<Creature*>> packs;
    std::vector<bool> used(mobs.size(), false);
    for (size_t i = 0; i < mobs.size(); ++i)
    {
        if (used[i])
            continue;
        std::vector<Creature*> pack;
        for (size_t j = i; j < mobs.size(); ++j)
        {
            if (used[j] || (j != i && !SamePack(mobs[j]->GetExactDist(mobs[i]), SameFormation(mobs[j], mobs[i]))))
                continue;
            used[j] = true;
            pack.push_back(mobs[j]);
        }
        packs.push_back(pack);
    }
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
    PullStyle style = StyleFor(TierOf(bot));
    // Bosses do not follow around corners; they are pulled in sight.
    if (boss && style == PullStyle::Corner)
        style = PullStyle::Ranged;
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

    std::vector<PathSpot> spots;
    std::vector<G3D::Vector3> where;
    auto sample = [&](G3D::Vector3 const& point)
    {
        float const distance = seed->GetExactDist(point.x, point.y, point.z);
        bool const sees = distance <= MaxCornerDistance && seed->IsWithinLOS(point.x, point.y, point.z + EYE_HEIGHT);
        spots.push_back({distance, sees});
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
    if (index < 0)
        return;
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
            member.distance = player == bot || player->GetMap() != bot->GetMap() ? 0.0f : player->GetExactDist(&from);
            member.healthPct = player->GetHealthPct();
            member.usesMana = player->getPowerType() == POWER_MANA && player->GetMaxPower(POWER_MANA) > 0;
            member.manaPct = member.usesMana ? player->GetPowerPct(POWER_MANA) : 100.0f;
            member.healer = PlayerbotAI::IsHeal(player);
            member.resting = IsResting(player);
            member.bot = !IsRealPlayer(player) && !IsSelfBot(player);

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

bool ReadyToLead(PlayerbotAI* botAI, State& state)
{
    Refresh(botAI, state);
    if (state.done || (state.restMs && !Elapsed(state.restFrom, state.restMs)))
        return false;
    return CheckReady(botAI, state) == Wait::None;
}

Creature* CurrentPack(PlayerbotAI* botAI, State& state)
{
    Player* bot = botAI->GetBot();
    if (!state.pack.IsEmpty())
    {
        Creature* seed = bot->GetMap()->GetCreature(state.pack);
        if (seed && seed->IsAlive() && !seed->IsInCombat())
            return seed;
        ForgetPack(state);
    }

    if (!Elapsed(state.packCheckedAt, PACK_RESCAN_MS))
        return nullptr;
    state.packCheckedAt = getMSTime();

    bool const hasGoal = FindGoal(botAI, state);
    float const leaderToGoal = hasGoal ? bot->GetExactDist(&state.goal) : 0.0f;
    std::vector<Creature*> mobs = Hostiles(botAI);
    std::erase_if(mobs, [&state](Creature* mob) { return state.skipped.count(mob->GetGUID().GetRawValue()) > 0; });
    std::vector<std::vector<Creature*>> packs = SplitPacks(mobs);
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
        candidates.push_back(candidate);
    }

    for (uint32 tries = 0; tries < MAX_PATH_TRIES; ++tries)
    {
        int const index = Choose(candidates.data(), candidates.size(), hasGoal, leaderToGoal);
        if (index < 0)
            break;
        Creature* seed = Nearest(bot, packs[index]);
        PathGenerator path(bot);
        path.CalculatePath(seed->GetPositionX(), seed->GetPositionY(), seed->GetPositionZ(), false);
        if (path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE))
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
        if (std::find(pulled.begin(), pulled.end(), mob) != pulled.end())
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
    return Enabled() && bot->GetGroup() && bot->GetMaxPower(POWER_MANA) > 0 && botAI->IsInNonRaidDungeon() &&
           PlayerbotAI::IsHeal(bot);
}

bool LeadNeedsManaTrigger::IsActive()
{
    if (!Enabled() || bot->IsInCombat() || !bot->GetGroup() || bot->GetMaxPower(POWER_MANA) == 0)
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
    if (!dynamic_cast<FollowAction*>(action))
        return 1.0f;

    Player* leader = Leader(botAI);
    PlayerbotAI* leaderAI = leader && leader != bot ? GET_PLAYERBOT_AI(leader) : nullptr;
    if (!leaderAI)
        return 1.0f;
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

    // Nothing to pull and nowhere to go: the group follows its master until a pack turns up.
    state.idle = !state.hasGoal || !Elapsed(state.goalBlockedAt, GOAL_BLOCKED_MS) ||
                 bot->GetExactDist(&state.goal) <= GOAL_REACHED;
    return !state.idle;
}

bool LeadAdvanceAction::Execute(Event /*event*/)
{
    State& state = StateOf(botAI);
    if (!state.pack.IsEmpty() && state.hasSpot)
    {
        Position const& to = state.stepping ? state.pullSpot : state.spot;
        return MoveTo(bot->GetMapId(), to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(), false, false, false,
                      false, MovementPriority::MOVEMENT_NORMAL, true);
    }

    // No pack on the way: walk a stretch toward the boss, then look again.
    PathGenerator path(bot);
    path.CalculatePath(state.goal.GetPositionX(), state.goal.GetPositionY(), state.goal.GetPositionZ(), false);
    Movement::PointsArray const& points = path.GetPath();
    if ((path.GetPathType() & PATHFIND_NOPATH) || points.size() < 2)
    {
        state.goalBlockedAt = getMSTime();
        return false;
    }

    float walked = 0.0f;
    G3D::Vector3 stop = points.back();
    for (size_t i = 1; i < points.size(); ++i)
    {
        walked += (points[i] - points[i - 1]).length();
        if (walked >= ADVANCE_STEP)
        {
            stop = points[i];
            break;
        }
    }
    if (bot->GetExactDist(stop.x, stop.y, stop.z) < SPOT_REACHED)
    {
        state.goalBlockedAt = getMSTime();
        return false;
    }
    return MoveTo(bot->GetMapId(), stop.x, stop.y, stop.z, false, false, false, false,
                  MovementPriority::MOVEMENT_NORMAL, true);
}

bool LeadPullAction::isUseful()
{
    if (!IsLeader(botAI) || bot->IsInCombat())
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
