/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for Shirrak the Dead Watcher: the tank and ranged spots, and getting out of Focus Fire.

#ifndef _PLAYERBOTS_AC_POLICY_H
#define _PLAYERBOTS_AC_POLICY_H

#include <algorithm>
#include <cstddef>

#include "DungeonTacticsPolicy.h"

namespace AuchenaiCrypts
{
// Shirrak the Dead Watcher

// Up the stairs, so the ranged spot sits near the edge of his 45-yard Inhibit Magic
constexpr float TankX = -65.171f;
constexpr float TankY = -162.920f;
constexpr float TankZ = 26.504f;
constexpr float TankArrivalDistance = 3.0f;
// Below this the tank stops dragging Shirrak and just holds him
constexpr float TankDragMinHealthPct = 30.0f;

constexpr float RangedX = -21.777f;
constexpr float RangedY = -162.700f;
constexpr float RangedZ = 26.062f;
constexpr float RangedSpreadRadius = 3.0f;
constexpr float RangedArrivalDistance = 4.0f;
constexpr float RangedMaxStep = 2.0f;
constexpr float RangedArcSpan = DungeonTactics::Pi / 2.0f;
constexpr float RangedArcCenter = DungeonTactics::Pi;

constexpr float FlareSearchRadius = 20.0f;
// Fiery Blast (32302, 38382) reaches 12 yards plus the target's 1.5-yard combat reach
constexpr float FieryBlastRadius = 12.0f;
constexpr float PlayerCombatReach = 1.5f;
constexpr float FocusFireSafeDistance = FieryBlastRadius + PlayerCombatReach + 1.0f;
// Other moves stay blocked a little past the safe distance so they don't walk straight back in
constexpr float FocusFireMoveBuffer = 2.0f;

constexpr bool DragsShirrak(bool holdsBoss, bool inMelee, float healthPct)
{
    return holdsBoss && inMelee && healthPct >= TankDragMinHealthPct;
}

// The ranged fan out over a quarter circle round their spot; a lone ranged takes its middle.
constexpr float RangedSpotAngle(std::size_t index, std::size_t count)
{
    if (count <= 1)
        return RangedArcCenter;
    return RangedArcCenter - RangedArcSpan / 2.0f +
           RangedArcSpan * static_cast<float>(index) / static_cast<float>(count - 1);
}

constexpr bool MovesToRangedSpot(float distanceToSpot) { return distanceToSpot > RangedArrivalDistance; }

constexpr float RangedStep(float distanceToSpot) { return std::min(RangedMaxStep, distanceToSpot); }

// How far to step away from a flare; 0 = already clear.
constexpr float FocusFireStep(float distanceToFlare)
{
    return distanceToFlare < FocusFireSafeDistance ? FocusFireSafeDistance - distanceToFlare : 0.0f;
}

struct ShirrakAction
{
    bool attack = false;
    bool fleeFocusFire = false;
    bool movementSpell = false;  // reach casts, Blink and Disengage
    bool movement = false;
};

// Near a flare only the flee may move; past the safe distance plain moves resume, movement spells don't.
constexpr bool ShirrakAllows(ShirrakAction action, bool flareNear, float distanceToFlare)
{
    if (action.attack || action.fleeFocusFire)
        return true;
    if (!action.movementSpell && !action.movement)
        return true;
    if (!flareNear)
        return true;
    if (action.movementSpell)
        return false;
    return distanceToFlare >= FocusFireSafeDistance + FocusFireMoveBuffer;
}
}  // namespace AuchenaiCrypts

#endif
