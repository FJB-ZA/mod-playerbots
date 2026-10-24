/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// World-thread side of bot Mythic+ runs: forming groups around held keys, supervising the run, sending them home.

#include "MythicPlusBotMgr.h"

#include <algorithm>
#include <array>

#include "Containers.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "GroupMgr.h"
#include "InstanceSaveMgr.h"
#include "InstanceScript.h"
#include "LivingArenaMgr.h"
#include "LivingWorldMgr.h"
#include "Log.h"
#include "Map.h"
#include "MythicPlusBotPolicy.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Timer.h"

using namespace MythicPlusBots;

namespace
{
constexpr uint32 TickMs = 5000;
// The core records a finished run in mythic_plus_week; read it no more often than this
constexpr uint32 RecordCheckMs = 15000;
constexpr float RegroupDistance = 40.0f;

struct Candidate
{
    Player* bot;
    Role role;
    uint32 skill;
    uint8 comfort;
};

Role RoleOf(Player* bot)
{
    if (PlayerbotAI::IsTank(bot))
        return Role::Tank;
    return PlayerbotAI::IsHeal(bot) ? Role::Healer : Role::Damage;
}

// Out in the world with nothing else on: not grouped, queued, fighting, flying or in an instance
bool Free(Player* bot)
{
    Map* map = bot->IsInWorld() ? bot->GetMap() : nullptr;
    return map && !map->Instanceable() && bot->IsAlive() && !bot->IsInCombat() && !bot->GetGroup() &&
           bot->GetLevel() >= HeroicLevel && LivingWorldMgr::instance().GetLevelCap(bot) >= HeroicLevel &&
           !bot->InBattlegroundQueue() && !bot->IsInFlight() && !bot->IsBeingTeleported() && !bot->IsGameMaster() &&
           !MythicPlusBotMgr::instance().InRun(bot);
}

uint64 RunsOf(ObjectGuid guid)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT CAST(COALESCE(SUM(runs), 0) AS UNSIGNED) FROM mythic_plus_week WHERE guid = {}", guid.GetCounter());
    return result ? result->Fetch()[0].Get<uint64>() : 0;
}

std::string DungeonName(uint16 map)
{
    MapEntry const* entry = sMapStore.LookupEntry(map);
    return entry ? entry->name[0] : std::to_string(map);
}
}  // namespace

MythicPlusBotMgr& MythicPlusBotMgr::instance()
{
    static MythicPlusBotMgr instance;
    return instance;
}

bool MythicPlusBotMgr::InRun(Player* bot) const
{
    std::lock_guard<std::mutex> guard(_lock);
    return _members.count(bot->GetGUID()) > 0;
}

bool MythicPlusBotMgr::IsRunGroup(Group const* group) const
{
    if (!group)
        return false;
    std::lock_guard<std::mutex> guard(_lock);
    return _groups.count(group->GetGUID()) > 0;
}

void MythicPlusBotMgr::Update(uint32 diff)
{
    if (!sPlayerbotAIConfig.MythicPlusBotRuns && _runs.empty())
        return;
    _tickTimer += diff;
    if (_tickTimer < TickMs)
        return;
    uint32 const elapsed = _tickTimer;
    _tickTimer = 0;

    std::erase_if(_runs, [this](Run& run) { return Supervise(run); });

    _formTimer += elapsed;
    if (_formTimer < sPlayerbotAIConfig.MythicPlusBotRunInterval * IN_MILLISECONDS)
        return;
    _formTimer = 0;
    if (_runs.size() < sPlayerbotAIConfig.MythicPlusBotRuns)
        TryForm();
}

void MythicPlusBotMgr::TryForm()
{
    std::unordered_map<ObjectGuid::LowType, Key> keys;
    if (QueryResult result = CharacterDatabase.Query("SELECT guid, map, level FROM mythic_plus_keystone"))
    {
        do
        {
            Field* fields = result->Fetch();
            keys[fields[0].Get<uint32>()] = {fields[1].Get<uint16>(), fields[2].Get<uint8>()};
        } while (result->NextRow());
    }
    if (keys.empty())
        return;

    std::vector<Candidate> pool;
    for (auto const& [guid, bot] : sRandomPlayerbotMgr.GetAllBots())
    {
        PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
        if (!botAI || !Free(bot))
            continue;
        uint32 const skill = LivingArenaMgr::instance().GetSkill(bot);
        uint8 const comfort = ComfortKeyLevel(botAI->GetEquipGearScore(bot), skill);
        if (comfort)
            pool.push_back({bot, RoleOf(bot), skill, comfort});
    }

    std::vector<Candidate const*> holders;
    for (Candidate const& candidate : pool)
        if (auto const key = keys.find(candidate.bot->GetGUID().GetCounter());
            key != keys.end() && FitsKey(key->second.level, candidate.comfort))
            holders.push_back(&candidate);
    Acore::Containers::RandomShuffle(holders);

    for (Candidate const* holder : holders)
    {
        Key const key = keys[holder->bot->GetGUID().GetCounter()];
        std::vector<Candidate const*> others;
        for (Candidate const& candidate : pool)
            if (&candidate != holder && candidate.bot->GetTeamId() == holder->bot->GetTeamId() &&
                FitsKey(key.level, candidate.comfort) && SkillMatches(candidate.skill, holder->skill))
                others.push_back(&candidate);
        auto const gap = [holder](Candidate const* c)
        { return c->skill > holder->skill ? c->skill - holder->skill : holder->skill - c->skill; };
        std::sort(others.begin(), others.end(),
                  [&gap](Candidate const* a, Candidate const* b) { return gap(a) < gap(b); });

        std::array<uint8, 3> open = RoleSlots;
        --open[uint8(holder->role)];
        std::vector<Player*> members = {holder->bot};
        for (Candidate const* candidate : others)
        {
            uint8& slots = open[uint8(candidate->role)];
            if (!slots)
                continue;
            --slots;
            members.push_back(candidate->bot);
        }
        if (members.size() == GroupSize && Launch(members, holder->bot, key))
            return;
    }
}

bool MythicPlusBotMgr::Launch(std::vector<Player*> const& members, Player* holder, Key key)
{
    AreaTriggerTeleport const* door = sObjectMgr->GetMapEntranceTrigger(key.map);
    auto const tank = std::find_if(members.begin(), members.end(), [](Player* m) { return RoleOf(m) == Role::Tank; });
    if (!door || tank == members.end())
        return false;

    Group* group = new Group();
    if (!group->Create(*tank))
    {
        delete group;
        return false;
    }
    sGroupMgr->AddGroup(group);
    for (Player* member : members)
    {
        if (member != *tank && !group->AddMember(member))
        {
            group->Disband();
            return false;
        }
    }

    Run run;
    run.group = group->GetGUID();
    run.holder = holder->GetGUID();
    run.key = key;
    run.entrance = WorldLocation(door->target_mapId, door->target_X, door->target_Y, door->target_Z,
                                 door->target_Orientation);
    run.startedAt = run.bossAt = getMSTime();
    run.runsBefore = RunsOf(run.holder);
    {
        std::lock_guard<std::mutex> guard(_lock);
        _groups.insert(run.group);
        for (Player* member : members)
            _members.insert(member->GetGUID());
    }

    group->SetDungeonDifficulty(DUNGEON_DIFFICULTY_HEROIC);
    std::string names;
    uint32 itemLevel = 0;
    for (Player* member : members)
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(member);
        run.members.push_back(member->GetGUID());
        run.homes[member->GetGUID()] = WorldLocation(member->GetMapId(), member->GetPositionX(),
                                                     member->GetPositionY(), member->GetPositionZ(),
                                                     member->GetOrientation());
        // A bot's last run of this dungeon leaves a heroic lockout with its bosses dead
        sInstanceSaveMgr->PlayerUnbindInstance(member->GetGUID(), key.map, DUNGEON_DIFFICULTY_HEROIC, true, member);
        if (botAI)
        {
            itemLevel += botAI->GetEquipGearScore(member);
            botAI->Reset();
            botAI->ResetStrategies();
        }
        names += (names.empty() ? "" : ", ") + member->GetName();
        member->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
        member->TeleportTo(run.entrance);
    }
    LOG_INFO("playerbots", "Mythic+ bots: {} brings {} into {} +{} (item level {}, skill {})", holder->GetName(),
             names, DungeonName(key.map), key.level, itemLevel / uint32(members.size()),
             LivingArenaMgr::instance().GetSkill(holder));
    _runs.push_back(std::move(run));
    return true;
}

bool MythicPlusBotMgr::Supervise(Run& run)
{
    uint32 const now = getMSTime();
    if (run.endedAt && getMSTimeDiff(run.endedAt, now) >= LingerMs)
    {
        Finish(run, "the run is over");
        return true;
    }
    if (!sGroupMgr->GetGroupByGUID(run.group.GetCounter()))
    {
        Finish(run, "its group broke up");
        return true;
    }

    std::vector<Player*> online;
    Map* map = nullptr;
    for (ObjectGuid const& guid : run.members)
    {
        Player* member = ObjectAccessor::FindConnectedPlayer(guid);
        if (!member || !member->IsInWorld())
            continue;
        online.push_back(member);
        if (member->GetMapId() == run.key.map && !member->IsBeingTeleported())
            map = member->GetMap();
    }
    if (online.size() + 1 < run.members.size())
    {
        Finish(run, "its members logged out");
        return true;
    }

    InstanceMap* instance = map ? map->ToInstanceMap() : nullptr;
    if (InstanceScript* script = instance ? instance->GetInstanceScript() : nullptr;
        script && script->GetCompletedEncounterMask() != run.bossMask)
    {
        run.bossMask = script->GetCompletedEncounterMask();
        run.bossAt = now;
    }
    if (!run.endedAt)
    {
        CheckRecord(run, now);
        if (!run.endedAt && GivesUp(getMSTimeDiff(run.startedAt, now), getMSTimeDiff(run.bossAt, now)))
        {
            Finish(run, "it stalled");
            return true;
        }
    }
    Revive(run, online, map);
    return false;
}

void MythicPlusBotMgr::CheckRecord(Run& run, uint32 now)
{
    if (getMSTimeDiff(run.checkedAt, now) < RecordCheckMs)
        return;
    run.checkedAt = now;
    if (RunsOf(run.holder) <= run.runsBefore)
        return;
    run.endedAt = now;
    LOG_INFO("playerbots", "Mythic+ bots: {} +{} finished after {} minutes", DungeonName(run.key.map), run.key.level,
             getMSTimeDiff(run.startedAt, now) / MINUTE / IN_MILLISECONDS);
}

// Out of combat for a while, the dead get up beside the tank, or at the entrance after a wipe, like a run back
void MythicPlusBotMgr::Revive(Run& run, std::vector<Player*> const& online, Map* map)
{
    Player* anchor = nullptr;
    bool fighting = false;
    bool waiting = false;
    for (Player* member : online)
    {
        if (!member->IsAlive() || member->GetMapId() != run.key.map)
        {
            waiting = waiting || !member->IsBeingTeleported();
            continue;
        }
        fighting = fighting || member->IsInCombat();
        if (member->GetMap() == map && (!anchor || PlayerbotAI::IsTank(member)))
            anchor = member;
    }
    uint32 const now = getMSTime();
    if (fighting || !waiting)
    {
        run.calmSince = 0;
        return;
    }
    if (!run.calmSince)
    {
        run.calmSince = now;
        return;
    }
    if (getMSTimeDiff(run.calmSince, now) < ReviveAfterMs)
        return;
    run.calmSince = 0;

    for (Player* member : online)
    {
        if (member->IsBeingTeleported() || (member->IsAlive() && member->GetMapId() == run.key.map))
            continue;
        if (!member->IsAlive())
        {
            member->ResurrectPlayer(0.5f);
            member->SpawnCorpseBones();
        }
        if (!anchor)
            member->TeleportTo(run.entrance);
        else if (member->GetMapId() != run.key.map || member->GetExactDist(anchor) > RegroupDistance)
            member->TeleportTo(anchor->GetMapId(), anchor->GetPositionX(), anchor->GetPositionY(),
                               anchor->GetPositionZ(), anchor->GetOrientation());
    }
}

void MythicPlusBotMgr::Finish(Run& run, char const* why)
{
    LOG_INFO("playerbots", "Mythic+ bots: {} +{} ends after {} minutes: {}", DungeonName(run.key.map), run.key.level,
             getMSTimeDiff(run.startedAt, getMSTime()) / MINUTE / IN_MILLISECONDS, why);
    {
        std::lock_guard<std::mutex> guard(_lock);
        _groups.erase(run.group);
        for (ObjectGuid const& guid : run.members)
            _members.erase(guid);
    }
    if (Group* group = sGroupMgr->GetGroupByGUID(run.group.GetCounter()))
        group->Disband();
    for (ObjectGuid const& guid : run.members)
    {
        Player* member = ObjectAccessor::FindConnectedPlayer(guid);
        if (!member || !member->IsInWorld())
            continue;
        if (!member->IsAlive())
        {
            member->ResurrectPlayer(1.0f);
            member->SpawnCorpseBones();
        }
        if (auto const home = run.homes.find(guid); home != run.homes.end())
            member->TeleportTo(home->second);
        if (PlayerbotAI* botAI = GET_PLAYERBOT_AI(member))
        {
            botAI->Reset();
            botAI->ResetStrategies();
        }
    }
}
