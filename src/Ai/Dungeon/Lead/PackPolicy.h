/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Which pack a bot tank pulls next on the way to the next boss, and which mobs belong to that pack.

#ifndef _PLAYERBOTS_PACK_POLICY_H
#define _PLAYERBOTS_PACK_POLICY_H

#include <cstddef>

namespace DungeonLead
{
struct PackCandidate
{
    float distToLeader = 0.0f;
    float distToGoal = 0.0f;  // to the next boss; ignored without a goal
    float heightDiff = 0.0f;  // above or below the tank
    bool reachable = true;    // a walkable path exists
};

constexpr float ScanRange = 45.0f;
// A pack this close aggroes on its own, so it is pulled whatever the route says.
constexpr float AlwaysPullRange = 18.0f;
constexpr float MaxHeightDiff = 12.0f;
// Lets a pack stand a little to the side of the straight line and still count as on the way.
constexpr float AheadSlack = 8.0f;
constexpr float PackRadius = 9.0f;

constexpr bool Eligible(PackCandidate const& pack, bool hasGoal, float leaderDistToGoal)
{
    if (!pack.reachable || pack.distToLeader > ScanRange)
        return false;
    if (pack.heightDiff > MaxHeightDiff || pack.heightDiff < -MaxHeightDiff)
        return false;
    if (pack.distToLeader <= AlwaysPullRange || !hasGoal)
        return true;
    return pack.distToGoal <= leaderDistToGoal + AheadSlack;
}

// Index of the nearest eligible pack, or -1 when the way to the boss is clear.
constexpr int Choose(PackCandidate const* packs, size_t count, bool hasGoal, float leaderDistToGoal)
{
    int best = -1;
    for (size_t i = 0; i < count; ++i)
    {
        if (!Eligible(packs[i], hasGoal, leaderDistToGoal))
            continue;
        if (best < 0 || packs[i].distToLeader < packs[best].distToLeader)
            best = static_cast<int>(i);
    }
    return best;
}

// Mobs in one creature formation always pull together; otherwise packs are mobs standing close to each other.
constexpr bool SamePack(float distToSeed, bool sameFormation) { return sameFormation || distToSeed <= PackRadius; }
}  // namespace DungeonLead

#endif
