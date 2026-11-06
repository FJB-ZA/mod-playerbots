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

// The candidate spot at HealerSpotAngles[index] around the tank's mob, far enough out to clear it
inline Spot HealerSpot(FacingMob const& anchor, size_t index, float radius = HealerBack + 2.0f)
{
    float const angle = anchor.orientation + HealerSpotAngles[index];
    return Spot{anchor.x + radius * std::cos(angle), anchor.y + radius * std::sin(angle)};
}

constexpr size_t HealerSpotCount = sizeof(HealerSpotAngles) / sizeof(HealerSpotAngles[0]);
}  // namespace BotMove

#endif
