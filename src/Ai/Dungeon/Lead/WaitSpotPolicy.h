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
            if (!spots[i].seesPack && spots[i].distToPack >= SafeDistance && spots[i].distToPack <= MaxCornerDistance)
                corner = static_cast<int>(i);
        if (corner >= 0)
            return corner;
    }
    int ranged = -1;
    for (size_t i = 0; i < count; ++i)
    {
        if (spots[i].distToPack < SafeDistance)
            break;
        ranged = static_cast<int>(i);
        if (spots[i].seesPack && spots[i].distToPack <= RangedPullDistance)
            break;
    }
    return ranged;
}

// Inside the shortest pull range (shoot, throw, faerie fire) with some margin.
constexpr float PullSpotDistance = 28.0f;

// Where the tank steps to from the gathering spot to see the pack in pull range, or -1 if no spot on the path does.
constexpr int PullSpot(PathSpot const* spots, size_t count, int from)
{
    for (size_t i = from < 0 ? 0 : static_cast<size_t>(from); i < count; ++i)
        if (spots[i].seesPack && spots[i].distToPack <= PullSpotDistance)
            return static_cast<int>(i);
    return -1;
}
}  // namespace DungeonLead

#endif
