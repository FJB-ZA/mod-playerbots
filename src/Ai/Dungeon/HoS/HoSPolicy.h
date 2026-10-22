/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Halls of Stone bosses: spreading for Krystallus's Shatter and leaving Sjonnir's Lightning Ring.

#ifndef _PLAYERBOTS_HOS_POLICY_H
#define _PLAYERBOTS_HOS_POLICY_H

namespace HallsOfStone
{
// Krystallus

// Shatter hits allies within 30 yards of each stoned player, harder the closer they stand
constexpr float ShatterSpacing = 40.0f;
// Short steps so two bots running the same way can still correct course
constexpr float ShatterSpreadStepLength = 5.0f;

constexpr float ShatterSpreadStep(float distanceToClosestMember)
{
    return distanceToClosestMember < ShatterSpacing ? ShatterSpreadStepLength : 0.0f;
}

// Once slammed, only the spread may move: anything else pulls bots back together before they turn to stone.
constexpr bool KrystallusAllows(bool groundSlam, bool movement, bool spread)
{
    return !(groundSlam && movement && !spread);
}

// Sjonnir the Ironshaper

// Lightning Ring pulses on everyone within 10 yards
constexpr float LightningRingClearance = 12.0f;

constexpr float LightningRingStepAway(float distanceToBoss)
{
    return distanceToBoss < LightningRingClearance ? LightningRingClearance - distanceToBoss : 0.0f;
}

constexpr bool SjonnirAllows(bool lightningRing, bool movement, bool avoidRing)
{
    return !(lightningRing && movement && !avoidRing);
}
}  // namespace HallsOfStone

#endif
