/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Where the group stops before a pull: around a corner for good tanks, in sight of the pack for average ones.

#ifndef _PLAYERBOTS_WAIT_SPOT_POLICY_H
#define _PLAYERBOTS_WAIT_SPOT_POLICY_H

#include <cstddef>

#include "LeadTierPolicy.h"

namespace DungeonLead
{
// A point on the walk from the tank to the pack, in walking order.
struct PathSpot
{
    float distToPack = 0.0f;
    bool seesPack = false;
    bool nearOtherPack = false;  // inside the aggro reach of another pack not yet pulled: no place to gather
    bool inAggro = false;        // inside some mob's aggro range: walking there starts the fight before the pull
};

// Mobs aggro at about 20 yards at equal level; the group stands beyond that.
constexpr float SafeDistance = 26.0f;
constexpr float RangedPullDistance = 30.0f;
// A corner further than this from the pack means a long walk back after the pull; pull in sight instead.
constexpr float MaxCornerDistance = 42.0f;

// Index of the spot to gather at, or -1 to walk straight in.
constexpr int WaitSpot(PathSpot const* spots, size_t count, PullStyle style)
{
    if (style == PullStyle::Charge || !count)
        return -1;
    if (style == PullStyle::Corner)
    {
        int corner = -1;
        for (size_t i = 0; i < count; ++i)
            if (!spots[i].seesPack && !spots[i].nearOtherPack && spots[i].distToPack >= SafeDistance &&
                spots[i].distToPack <= MaxCornerDistance)
                corner = static_cast<int>(i);
        if (corner >= 0)
            return corner;
    }
    int ranged = -1;
    for (size_t i = 0; i < count; ++i)
    {
        if (spots[i].distToPack < SafeDistance)
            break;
        if (spots[i].nearOtherPack)
            continue;
        ranged = static_cast<int>(i);
        if (spots[i].seesPack && spots[i].distToPack <= RangedPullDistance)
            break;
    }
    return ranged;
}

// Inside the shortest pull range (shoot, throw, faerie fire) with some margin.
constexpr float PullSpotDistance = 28.0f;

// Where the tank steps to from the gathering spot to see the pack in pull range, or -1 if no spot on the path does.
// Not inside any mob's aggro range, nor near another pack: stepping into the middle of Trollgore's room in heroic
// Drak'Tharon Keep brought 12 mobs before the pull went off (solo-wow role check, 2026-11-07).
constexpr int PullSpot(PathSpot const* spots, size_t count, int from)
{
    for (size_t i = from < 0 ? 0 : static_cast<size_t>(from); i < count; ++i)
        if (spots[i].seesPack && spots[i].distToPack <= PullSpotDistance && !spots[i].inAggro &&
            !spots[i].nearOtherPack)
            return static_cast<int>(i);
    return -1;
}

// The ground the tank has walked, newest last, one point every TrailStep yards out of a fight
constexpr float TrailStep = 5.0f;
constexpr size_t TrailPoints = 24;
// Back along the trail no further than this from the pack: a longer walk back after the pull loses the pack
constexpr float MaxTrailDistance = 45.0f;

// With no spot on the way to the pack (the tank already stands inside its reach, as it does in a crowded room once the
// last pack is dead), the group falls back along the trail it came by: the newest point clear of this pack and of every
// other. Walking straight in from the middle of the Deadmines goblin foundry pulled 12-16 mobs and wiped the group
// (solo-wow role check, 2026-11-07). Index into the trail, or -1.
constexpr int TrailSpot(PathSpot const* trail, size_t count)
{
    for (size_t i = count; i > 0; --i)
    {
        PathSpot const& spot = trail[i - 1];
        if (spot.distToPack > MaxTrailDistance)
            return -1;
        if (spot.distToPack >= SafeDistance && !spot.nearOtherPack)
            return static_cast<int>(i - 1);
    }
    return -1;
}
}  // namespace DungeonLead

#endif
