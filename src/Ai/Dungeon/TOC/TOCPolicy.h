/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Trial of the Champion: the joust's ranges, lanes and Defend stacks, and Eadric's Radiance.

#ifndef _PLAYERBOTS_TOC_POLICY_H
#define _PLAYERBOTS_TOC_POLICY_H

#include <cmath>
#include <cstdint>
#include <vector>

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

// On foot with a free mount, only getting ready may move the bot, but it still heals, drinks and casts where it
// stands: holding every action left bots whose remount stalled as statues while the champions, which charge and
// thrust a player on foot for 8.5k and 8k, killed them (heals, a healing potion and shoot were all held).
// On a joust mount, only the joust moves it.
constexpr bool JoustAllows(JoustMoment fight, JoustAction action)
{
    if (fight.onVehicle)
        return !fight.onJoustMount || !action.movement || action.mountedJoust;
    return !fight.combat || action.setup || !action.movement || !fight.spareMount;
}

// Spare mounts for the riders on foot: the closest rider and mount pair first, each rider and each mount once. All
// three on foot ran to the one nearest warhorse, one boarded and the others turned for the next, 50-60 yards across the
// arena under the champions' charges, and died stunned on the way (role check, heroic Trial of the Champion,
// 2026-11-09). distance[rider][mount]; the mount rider `self` takes, or -1 when the mounts ran out before it.
// A mount of the other faction counts this much farther: only the faction's own can trample a dismounted champion
// Ride to a spare while the current mount can still carry its rider out of trample range.
constexpr bool NeedsFreshMount(float healthPct) { return healthPct <= 45.0f; }
constexpr bool HealthySpare(float healthPct) { return healthPct >= 80.0f; }

constexpr float OtherMountPenalty = 20.0f;
constexpr float MountCost(float distance, bool otherFaction)
{
    return distance + (otherFaction ? OtherMountPenalty : 0.0f);
}

inline int AssignedMount(std::vector<std::vector<float>> const& distance, size_t self,
                         std::vector<int> const& kept = {})
{
    size_t const riders = distance.size();
    size_t const mounts = riders ? distance[0].size() : 0;
    std::vector<bool> riderTaken(riders, false);
    std::vector<bool> mountTaken(mounts, false);
    // A reachable free horse stays assigned to its rider, even when another rider passes closer.
    for (size_t r = 0; r < riders && r < kept.size(); ++r)
    {
        int const m = kept[r];
        if (m < 0 || static_cast<size_t>(m) >= mounts || mountTaken[m])
            continue;
        if (r == self)
            return m;
        riderTaken[r] = true;
        mountTaken[m] = true;
    }
    for (size_t round = 0; round < riders && round < mounts; ++round)
    {
        int bestRider = -1;
        int bestMount = -1;
        for (size_t r = 0; r < riders; ++r)
            for (size_t m = 0; m < mounts; ++m)
                if (!riderTaken[r] && !mountTaken[m] &&
                    (bestRider < 0 || distance[r][m] < distance[bestRider][bestMount]))
                {
                    bestRider = int(r);
                    bestMount = int(m);
                }
        if (bestRider == int(self))
            return bestMount;
        riderTaken[bestRider] = true;
        mountTaken[bestMount] = true;
    }
    return -1;
}

// Stay outside aggro range while the three Argent packs assemble.
constexpr float SoldierKeepOff = 28.0f;
constexpr float SoldierWaitAt = 34.0f;

constexpr bool KeepsOffSoldiers(bool soldierFighting, float botToSoldier, float masterToSoldier)
{
    return !soldierFighting && (botToSoldier < SoldierKeepOff || masterToSoldier < SoldierKeepOff);
}

// Where it backs off to: SoldierWaitAt from the soldier, toward the middle of the arena
inline void SoldierWaitSpot(float soldierX, float soldierY, float& x, float& y)
{
    float dx = ArenaCenterX - soldierX;
    float dy = ArenaCenterY - soldierY;
    float const length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.01f)
    {
        dx = 0.0f;
        dy = -1.0f;
    }
    else
    {
        dx /= length;
        dy /= length;
    }
    x = soldierX + dx * SoldierWaitAt;
    y = soldierY + dy * SoldierWaitAt;
}

// Focus the heroic Priestess before she can Mind Control the tank.
constexpr bool MarksPriestess(bool tank, bool priestessFighting, bool skullOnLivePriestess)
{
    return tank && priestessFighting && !skullOnLivePriestess;
}

// Eadric the Pure

// Radiance blinds everyone facing him: turn the back to him for the cast.
inline float RadianceFacing(float angleToBoss) { return DungeonTactics::NormalizeAngle(angleToBoss + Pi); }
}  // namespace TrialOfTheChampion

#endif
