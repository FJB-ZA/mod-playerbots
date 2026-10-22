// Pure rules for the Trial of the Champion: the joust's ranges, lanes and Defend stacks, and Eadric's Radiance.

#ifndef _PLAYERBOTS_TOC_POLICY_H
#define _PLAYERBOTS_TOC_POLICY_H

#include <cstdint>

#include "DungeonTacticsPolicy.h"

namespace TrialOfTheChampion
{
// Grand Champions: the joust

constexpr float Pi = DungeonTactics::Pi;

constexpr float ArenaCenterX = 748.309f;
constexpr float ArenaCenterY = 619.448f;
constexpr float ArenaCenterZ = 411.3f;
constexpr float ArenaRadius = 42.0f;
constexpr float ArenaMargin = 3.0f;

constexpr uint32_t IdleScanMs = 500;
constexpr uint32_t SpareMountCheckMs = 500;
constexpr float ScanRange = 60.0f;
constexpr float ThrustRange = 6.0f;
constexpr float ChargeMin = 8.0f;
constexpr float ChargeMax = 25.0f;
constexpr float ChargeIdeal = 14.0f;
constexpr float FleeDistance = 20.0f;
constexpr float FleeLane = Pi / 5.0f;
constexpr float FleeCone = Pi / 6.0f;
constexpr float FleeArrival = 2.0f;
constexpr uint32_t FleeTimeoutMs = 3000;
constexpr float TrampleRange = 3.0f;
constexpr uint32_t DefendCheckMs = 1000;
constexpr float KiteRadius = 12.0f;
constexpr float KiteRingGap = 3.0f;
constexpr uint8_t KiteRings = 3;
constexpr float KiteArc = 12.0f;
constexpr float MoveRefreshDistance = 5.0f;
constexpr uint32_t MoveRefreshMs = 750;
constexpr uint8_t DefendMaxStacks = 3;

enum class ChargeStep
{
    Approach,  // too far for Charge: close to the ideal distance
    Charge,
    Flee  // too close for Charge: open the gap again
};

constexpr ChargeStep ChargeStepAt(float distance)
{
    if (distance > ChargeMax)
        return ChargeStep::Approach;
    if (distance >= ChargeMin)
        return ChargeStep::Charge;
    return ChargeStep::Flee;
}

constexpr bool InThrustRange(float distance) { return distance <= ThrustRange; }

// Shield-Breaker only pays off on a shielded target, out of Thrust range and inside Charge range.
constexpr bool BreaksShield(float distance, bool shielded)
{
    return distance > ThrustRange && distance <= ChargeMax && shielded;
}

constexpr bool NeedsDefend(uint8_t stacks, uint8_t belowStacks) { return stacks < belowStacks; }

// A flee ends once the gap is back, the timeout runs out, or the bot reached its spot.
constexpr bool FleeDone(float distance, bool timedOut, float distanceToSpot)
{
    return distance >= ChargeIdeal || timedOut || distanceToSpot < FleeArrival;
}

// Each rider flees down its own lane so the group doesn't bunch up behind one champion.
constexpr float FleeLaneOffset(int lane, int riders)
{
    return (static_cast<float>(lane) - static_cast<float>(riders - 1) * 0.5f) * FleeLane;
}

// Angle between two bearings, folded into [0, pi].
inline float BearingGap(float a, float b)
{
    float delta = DungeonTactics::NormalizeAngle(a - b);
    return delta > Pi ? 2.0f * Pi - delta : delta;
}

// A hostile within the flee distance and cone blocks that direction.
inline bool BlocksFlee(float bearingToHostile, float fleeBearing, float distanceToHostile)
{
    return BearingGap(bearingToHostile, fleeBearing) < FleeCone && distanceToHostile < FleeDistance;
}

constexpr bool InsideArena(float distanceToCenter) { return distanceToCenter <= ArenaRadius; }

constexpr float KiteLimit = ArenaRadius - ArenaMargin;

// Riders kite on staggered rings, alternating direction, so they don't collide circling one target.
constexpr float KiteRingRadius(int lane) { return KiteRadius + static_cast<float>(lane % KiteRings) * KiteRingGap; }

constexpr int8_t KiteDirection(int lane) { return lane % 2 ? -1 : 1; }

struct JoustMoment
{
    bool combat = false;
    bool onVehicle = false;
    bool onJoustMount = false;
    bool spareMount = false;  // a free joust mount is near
};

struct JoustAction
{
    bool setup = false;  // mount, lance, unequip lance or wipe
    bool movement = false;
    bool mountedJoust = false;
};

// On foot with a free mount, only getting ready may run; on a joust mount, only the joust moves it.
constexpr bool JoustAllows(JoustMoment fight, JoustAction action)
{
    if (!fight.combat)
        return true;
    if (!fight.onVehicle)
        return action.setup || !fight.spareMount;
    if (!fight.onJoustMount)
        return true;
    return !(action.movement && !action.mountedJoust);
}

// Eadric the Pure

// Radiance blinds everyone facing him: turn the back to him for the cast.
inline float RadianceFacing(float angleToBoss) { return DungeonTactics::NormalizeAngle(angleToBoss + Pi); }
}  // namespace TrialOfTheChampion

#endif
