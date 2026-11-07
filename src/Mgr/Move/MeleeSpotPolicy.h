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
#include <cstdint>

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

// A pack round the tank. Mobs the tank holds from many sides each face it, so every spot behind one is in the front of
// another across the tank: a rogue on such a pack had no clear spot and stood in a front for whole fights (solo-wow role
// check, 2026-11-07). The tank steps out through the widest gap between them, and the pack, chasing it, closes up on one
// side of it with their backs free.
constexpr float PackSpreadLimit = 3.14159265f / 2.0f;  // at 90 degrees apart the spot behind one clears the other's front
constexpr float GatherStep = 6.0f;                     // yards the tank steps: the far mobs end up within 60 degrees
constexpr uint32_t GatherEveryMs = 6000;               // one step per few seconds, so the pack settles between them
constexpr size_t GatherMaxMobs = 16;

struct Gap
{
    float heading = 0.0f;  // the direction from the tank through the middle of the gap
    float width = 0.0f;    // radians free of mobs
};

// The widest gap between the directions from the tank to its mobs (radians, any range; at most GatherMaxMobs are
// looked at); the full circle with fewer than two
inline Gap WidestGap(float const* headings, size_t count)
{
    constexpr float pi = 3.14159265f;
    if (count < 2)
        return {count ? std::fmod(headings[0] + pi, 2.0f * pi) : 0.0f, 2.0f * pi};
    float sorted[GatherMaxMobs];
    size_t const n = count < GatherMaxMobs ? count : GatherMaxMobs;
    for (size_t i = 0; i < n; ++i)
    {
        float angle = std::fmod(headings[i], 2.0f * pi);
        if (angle < 0.0f)
            angle += 2.0f * pi;
        size_t j = i;
        while (j > 0 && sorted[j - 1] > angle)
        {
            sorted[j] = sorted[j - 1];
            --j;
        }
        sorted[j] = angle;
    }
    float const wrap = sorted[0] + 2.0f * pi - sorted[n - 1];
    Gap widest{sorted[n - 1] + wrap / 2.0f, wrap};
    for (size_t i = 1; i < n; ++i)
    {
        float const width = sorted[i] - sorted[i - 1];
        if (width > widest.width)
            widest = {sorted[i - 1] + width / 2.0f, width};
    }
    if (widest.heading >= 2.0f * pi)
        widest.heading -= 2.0f * pi;
    return widest;
}

// Do the mobs ring the tank: spread round it wider than PackSpreadLimit?
inline bool PackRings(float const* headings, size_t count)
{
    return count >= 2 && 2.0f * 3.14159265f - WidestGap(headings, count).width > PackSpreadLimit;
}
}  // namespace BotMove

#endif
