/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Check a dungeon follower's shortcut against the aggro ranges along it.

#ifndef _PLAYERBOTS_FOLLOW_TRAIL_POLICY_H
#define _PLAYERBOTS_FOLLOW_TRAIL_POLICY_H

#include <algorithm>
#include <cstddef>
#include <vector>

namespace DungeonLead
{
struct WalkPoint
{
    float x, y, z;
};

inline float DistanceSquared(WalkPoint a, WalkPoint b)
{
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

inline WalkPoint NearestOnWalk(WalkPoint a, WalkPoint b, WalkPoint mob)
{
    float const length2 = DistanceSquared(a, b);
    if (length2 < 0.0001f)
        return a;
    float const dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    float const t = std::clamp(((mob.x - a.x) * dx + (mob.y - a.y) * dy + (mob.z - a.z) * dz) / length2, 0.0f, 1.0f);
    return {a.x + t * dx, a.y + t * dy, a.z + t * dz};
}

// A follower already inside the margin can still retreat directly away from the pack.
inline bool WalkPulls(WalkPoint from, WalkPoint closest, WalkPoint mob, float aggro)
{
    constexpr float margin = 3.0f;
    float const d2 = DistanceSquared(closest, mob);
    return d2 < (aggro + margin) * (aggro + margin) && d2 < DistanceSquared(from, mob);
}

template <class Point, class Pulls>
int SafeTrailPoint(std::vector<Point> const& trail, Point const& from, Pulls pulls)
{
    for (size_t i = trail.size(); i > 0; --i)
    {
        auto const& point = trail[i - 1];
        if (from.GetExactDist(&point) < 3.0f)
            break;
        if (!pulls(point))
            return static_cast<int>(i - 1);
    }
    return -1;
}
}  // namespace DungeonLead

#endif
