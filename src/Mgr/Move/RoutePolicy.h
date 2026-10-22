/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// When a bot looks for a jump instead of walking, and which jump route it takes.

#ifndef _PLAYERBOTS_ROUTE_POLICY_H
#define _PLAYERBOTS_ROUTE_POLICY_H

#include <cstddef>
#include <cstdint>

namespace BotMove
{
// Close enough to the goal that the path counts as arriving.
constexpr float ArriveSlack = 3.0f;
// A walk this much longer than the straight line is a long way round, e.g. past a gap the leader jumped.
constexpr float DetourFactor = 2.5f;
constexpr float DetourSlack = 30.0f;
// A jump route must save at least this share of the walk to be worth taking.
constexpr float JumpBeatsWalk = 0.75f;
constexpr float TakeoffSlack = 1.5f;
// Learned jumps further than this from the bot or the goal are not considered.
constexpr float LinkSearchRange = 80.0f;

struct PathShape
{
    bool reachable = false;
    float endToGoal = 0.0f;
    float length = 0.0f;
    float straight = 0.0f;
};

constexpr bool StopsShort(PathShape path) { return !path.reachable || path.endToGoal > ArriveSlack; }

constexpr bool WalksFarAround(PathShape path)
{
    return path.reachable && path.length > path.straight * DetourFactor && path.length - path.straight > DetourSlack;
}

constexpr bool LooksForJump(PathShape path) { return StopsShort(path) || WalksFarAround(path); }

constexpr float WalkCost(PathShape path) { return StopsShort(path) ? -1.0f : path.length; }

struct RouteCost
{
    bool usable = false;
    float cost = 0.0f;
};

// Index of the cheapest jump route that clearly beats walking, or -1 to walk.
inline int ChooseRoute(PathShape walk, RouteCost const* routes, std::size_t count)
{
    float walkCost = WalkCost(walk);
    int best = -1;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!routes[i].usable)
            continue;
        if (walkCost >= 0.0f && routes[i].cost > walkCost * JumpBeatsWalk)
            continue;
        if (best < 0 || routes[i].cost < routes[best].cost)
            best = static_cast<int>(i);
    }
    return best;
}

enum class JumpStep
{
    WalkToTakeoff,
    Jump
};

constexpr JumpStep NextJumpStep(float distanceToTakeoff)
{
    return distanceToTakeoff <= TakeoffSlack ? JumpStep::Jump : JumpStep::WalkToTakeoff;
}

// A planned jump stays valid while the goal is still near where it was planned.
constexpr float PlanGoalDrift = 15.0f;
constexpr uint32_t PlanLifetimeMs = 15000;
constexpr bool PlanStillValid(float goalDrift, uint32_t ageMs)
{
    return goalDrift <= PlanGoalDrift && ageMs <= PlanLifetimeMs;
}

// Short moves on one level never cross a gap worth planning, so they skip the path search entirely.
constexpr float NearMoveDistance = 5.0f;
constexpr float NearMoveRise = 2.0f;
constexpr bool IsNearMove(float straight, float rise)
{
    return straight <= NearMoveDistance && rise <= NearMoveRise && rise >= -NearMoveRise;
}

// Straight-line legs are the cheapest a jump route can be; skip its path searches when even that loses to walking.
constexpr bool RouteCouldBeat(float straightLineCost, PathShape walk)
{
    return WalkCost(walk) < 0.0f || straightLineCost <= WalkCost(walk) * JumpBeatsWalk;
}

// A goal that jumps further than CacheGoalDrift re-plans early, but never sooner than MinReplanMs.
constexpr float CacheGoalDrift = 8.0f;
constexpr uint32_t MinReplanMs = 250;
constexpr uint32_t CacheLifetimeMs = 1000;
constexpr uint32_t FailedSearchLifetimeMs = 5000;
constexpr bool CacheStillValid(float goalDrift, uint32_t ageMs, bool searched = false)
{
    if (ageMs < MinReplanMs)
        return true;
    return goalDrift <= CacheGoalDrift && ageMs <= (searched ? FailedSearchLifetimeMs : CacheLifetimeMs);
}
}  // namespace BotMove

#endif
