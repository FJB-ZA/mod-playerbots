/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Where a healer, or a ranged damage dealer, stands while the tank holds the mobs: back from all of them, out of every
// mob's front, and still near the tank. Nothing else moved a healer in a fight: its flee only answers its own target,
// and a healer has none, so a healer stayed wherever the pull caught it, often a few yards from the pack the tank
// brought back. A caster's flee only answers a mob in melee reach, so a mage trailing the tank stood in the pack's
// front when it reached the tank (solo-wow role check, 2026-11-06).

#ifndef _PLAYERBOTS_HEALER_SPOT_POLICY_H
#define _PLAYERBOTS_HEALER_SPOT_POLICY_H

#include <cmath>
#include <cstddef>

#include "MeleeSpotPolicy.h"

namespace BotMove
{
constexpr float HealerTooClose = 10.0f;  // yards from a mob on someone else at which a healer steps back (cleave reach)
constexpr float HealerBack = 15.0f;      // yards from every mob the step back goes to: past TooClose, so it settles
constexpr float HealerToTank = 6.0f;     // a healer a mob turned on runs to within this of the tank, for its taunt
constexpr float HealerNearTank = 28.0f;  // the furthest from the tank a step back goes, well inside heal range

// Around the tank's mob, from that mob's facing: behind it, round either side, and on past the tank. At HealerBack every
// one is past a cleave's reach. Without the spots past the tank, a healer in a corridor had only walls to step to.
constexpr float HealerSpotAngles[] = {3.14159265f,         3.14159265f * 0.75f, -3.14159265f * 0.75f,
                                      3.14159265f * 0.5f,  -3.14159265f * 0.5f, 3.14159265f * 0.35f,
                                      -3.14159265f * 0.35f, 3.14159265f * 0.2f, -3.14159265f * 0.2f,
                                      0.0f};

struct Spot
{
    float x = 0.0f;
    float y = 0.0f;
};

inline float FlatDistance(float ax, float ay, float bx, float by)
{
    float const dx = ax - bx;
    float const dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

// Is (x, y) a spot a healer may stay at: clear of every mob by HealerBack, in front of none, near the tank?
inline bool HealerSpotClear(FacingMob const* mobs, size_t count, Spot tank, float x, float y)
{
    if (FlatDistance(x, y, tank.x, tank.y) > HealerNearTank)
        return false;
    for (size_t i = 0; i < count; ++i)
        if (FlatDistance(x, y, mobs[i].x, mobs[i].y) < HealerBack)
            return false;
    return !InFrontOfAny(mobs, count, x, y);
}

// A long run to the far side of a moving pack can leave the tank beyond healing range.
constexpr float HealerStepMax = 20.0f;

// Is the spot at (x, y) a step the healer at (fromX, fromY) may take?
constexpr bool HealerStepInReach(float distance) { return distance <= HealerStepMax; }

// How far (x, y) is from the nearest of the mobs; far away with none
inline float NearestMob(FacingMob const* mobs, size_t count, float x, float y)
{
    float nearest = 1000.0f;
    for (size_t i = 0; i < count; ++i)
        nearest = std::fmin(nearest, FlatDistance(x, y, mobs[i].x, mobs[i].y));
    return nearest;
}

// With no clear spot (a big pack spread round the tank), is (x, y) still worth the step: past a cleave's reach of every
// mob, in front of none, near the tank, and a clear gain on where the healer stands? In the solo-wow role check rerun
// (2026-11-06) a healer near a mob on another was a third of the samples not back: with no spot clear by HealerBack it
// stayed where it was
inline bool HealerSpotRoomier(FacingMob const* mobs, size_t count, Spot tank, float x, float y, float nearestHere)
{
    if (FlatDistance(x, y, tank.x, tank.y) > HealerNearTank || InFrontOfAny(mobs, count, x, y))
        return false;
    float const nearest = NearestMob(mobs, count, x, y);
    return nearest >= HealerTooClose + 1.0f && nearest >= nearestHere + 2.0f;
}

// A mob not yet in the fight, and the reach at which it would join it
struct IdleMob
{
    float x = 0.0f;
    float y = 0.0f;
    float aggro = 0.0f;
};

constexpr float IdleMargin = 4.0f;  // yards past an idle mob's aggro range a spot keeps, for its wander and the step

// Is (x, y) out of every idle mob's aggro range? A healer stepping back into the next pack pulled it onto itself: in the
// solo-wow role check rerun (2026-11-06) every healer sample not back was the healer struck, and most mobs that went to
// the healer first had no threat on anyone
inline bool OutOfIdleAggro(IdleMob const* idle, size_t count, float x, float y)
{
    for (size_t i = 0; i < count; ++i)
        if (FlatDistance(x, y, idle[i].x, idle[i].y) < idle[i].aggro + IdleMargin)
            return false;
    return true;
}

// Where a mob running at its victim will stand once it gets there: its melee reach short of the victim, on the side it
// comes from, facing the victim. A back line that answered only where the mob stood stayed put as the pack ran in from
// the pull and stood in its front when it reached the tank; in the solo-wow role check (2026-11-07) most samples in
// front of a mob were the first seconds of a fight
inline FacingMob Arriving(float mobX, float mobY, float victimX, float victimY, float reach)
{
    float const dx = mobX - victimX;
    float const dy = mobY - victimY;
    float const distance = std::sqrt(dx * dx + dy * dy);
    float const facing = std::atan2(-dy, -dx);
    if (distance <= reach || distance < 0.01f)
        return {mobX, mobY, facing, 0.0f};
    return {victimX + dx / distance * reach, victimY + dy / distance * reach, facing, 0.0f};
}

// The candidate spot at HealerSpotAngles[index] around the tank's mob, far enough out to clear it
inline Spot HealerSpot(FacingMob const& anchor, size_t index, float radius = HealerBack + 2.0f)
{
    float const angle = anchor.orientation + HealerSpotAngles[index];
    return Spot{anchor.x + radius * std::cos(angle), anchor.y + radius * std::sin(angle)};
}

constexpr size_t HealerSpotCount = sizeof(HealerSpotAngles) / sizeof(HealerSpotAngles[0]);
}  // namespace BotMove

#endif
