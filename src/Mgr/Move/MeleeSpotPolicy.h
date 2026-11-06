/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Where a melee damage dealer stands on a mob the tank holds: behind it, and out of the front of every other mob in
// the fight. Standing behind only its own target, a rogue on a pack of two or more stood in front of the next mob and
// took its cleave.

#ifndef _PLAYERBOTS_MELEE_SPOT_POLICY_H
#define _PLAYERBOTS_MELEE_SPOT_POLICY_H

#include <cmath>
#include <cstddef>

namespace BotMove
{
constexpr float FrontReach = 10.0f;            // yards in front of a mob a cleave or breath reaches
constexpr float FrontArc = 3.14159265f / 1.5f;  // 120 degrees: the 90 degree cone a frontal hits, with a margin

struct FacingMob
{
    float x = 0.0f;
    float y = 0.0f;
    float orientation = 0.0f;
    float reach = 0.0f;  // the mob's combat reach: a big mob's front starts at its edge, not its centre
};

inline float RelativeAngle(FacingMob const& mob, float x, float y)
{
    constexpr float pi = 3.14159265f;
    float angle = std::atan2(y - mob.y, x - mob.x) - mob.orientation;
    while (angle > pi)
        angle -= 2.0f * pi;
    while (angle < -pi)
        angle += 2.0f * pi;
    return angle;
}

// In reach of the mob's frontal attacks at (x, y)?
inline bool InFront(FacingMob const& mob, float x, float y)
{
    float const dx = x - mob.x;
    float const dy = y - mob.y;
    float const distance = std::sqrt(dx * dx + dy * dy);
    if (distance > FrontReach + mob.reach)
        return false;
    return distance < 0.01f || std::fabs(RelativeAngle(mob, x, y)) <= FrontArc / 2.0f;
}

inline size_t CountInFront(FacingMob const* mobs, size_t count, float x, float y)
{
    size_t fronts = 0;
    for (size_t i = 0; i < count; ++i)
        if (InFront(mobs[i], x, y))
            ++fronts;
    return fronts;
}

inline bool InFrontOfAny(FacingMob const* mobs, size_t count, float x, float y)
{
    return CountInFront(mobs, count, x, y) > 0;
}

// The angles around its target, from the target's facing, a melee bot tries: straight behind first, then round toward
// either flank, to just outside the target's own front. Five spots at one distance left a rogue on a pack spread round
// the tank with none clear, so it stood where it was, in front of the next mob (solo-wow role check, 2026-11-06).
constexpr float SpotAngles[] = {3.14159265f,         3.14159265f * 0.9f,  -3.14159265f * 0.9f, 3.14159265f * 0.8f,
                                -3.14159265f * 0.8f, 3.14159265f * 0.7f,  -3.14159265f * 0.7f, 3.14159265f * 0.6f,
                                -3.14159265f * 0.6f, 3.14159265f * 0.45f, -3.14159265f * 0.45f};
}  // namespace BotMove

#endif
