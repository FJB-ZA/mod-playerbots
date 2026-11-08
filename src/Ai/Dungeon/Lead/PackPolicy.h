/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Which pack a bot tank pulls next on the way to the next boss, and which mobs belong to that pack.

#ifndef _PLAYERBOTS_PACK_POLICY_H
#define _PLAYERBOTS_PACK_POLICY_H

#include <cstddef>
#include <vector>

namespace DungeonLead
{
struct PackCandidate
{
    float distToLeader = 0.0f;
    float distToGoal = 0.0f;  // to the next boss; ignored without a goal
    float heightDiff = 0.0f;  // above or below the tank
    bool reachable = true;    // a walkable path exists
    float aggroReach = 0.0f;  // the pack's own aggro range on the tank plus AggroSlack; under AlwaysPullRange counts as that
    bool onPath = false;      // the walk on to the boss passes inside its aggro reach
};

constexpr float ScanRange = 45.0f;
// A pack this close aggroes on its own, so it is pulled whatever the route says.
constexpr float AlwaysPullRange = 18.0f;
// Past a mob's aggro range: the group behind the tank and a step of the tank's own walk
constexpr float AggroSlack = 6.0f;
constexpr float MaxHeightDiff = 12.0f;
// Lets a pack stand a little to the side of the straight line and still count as on the way.
constexpr float AheadSlack = 8.0f;
constexpr float PackRadius = 9.0f;

constexpr float PullReach(PackCandidate const& pack)
{
    return pack.aggroReach > AlwaysPullRange ? pack.aggroReach : AlwaysPullRange;
}

// A pack the walk to the boss passes counts whatever its height or straight-line distance to the boss: a dungeon's
// way winds round and up ramps, and a pack beside it aggroes all the same.
constexpr bool Eligible(PackCandidate const& pack, bool hasGoal, float leaderDistToGoal)
{
    if (!pack.reachable || pack.distToLeader > ScanRange)
        return false;
    if (pack.onPath)
        return true;
    if (pack.heightDiff > MaxHeightDiff || pack.heightDiff < -MaxHeightDiff)
        return false;
    if (pack.distToLeader <= PullReach(pack) || !hasGoal)
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

// The mobs that join one fight, by index: everyone reached hop by hop from the seed. A mob in a fight calls every idle
// mob of its faction in sight within the assistance radius (CreatureFamilyAssistanceRadius, 10 yards), at once and
// again every 3 seconds, and each one that comes calls its own. Counting only the mobs near the first, the lead took a
// Steamvault pack of 9 for 4 and pulled it from beside the rest (solo-wow role check, 2026-11-07).
template <typename Linked>
std::vector<bool> LinkedPack(size_t seed, size_t count, Linked const& linked)
{
    std::vector<bool> inPack(count, false);
    if (seed >= count)
        return inPack;
    inPack[seed] = true;
    std::vector<size_t> open = {seed};
    while (!open.empty())
    {
        size_t const from = open.back();
        open.pop_back();
        for (size_t i = 0; i < count; ++i)
            if (!inPack[i] && linked(from, i))
            {
                inPack[i] = true;
                open.push_back(i);
            }
    }
    return inPack;
}
}  // namespace DungeonLead

#endif
