/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Oculus: Drakos's spheres, the drake seats and rotations, Urom's platforms and bombs, Eregos.

#ifndef _PLAYERBOTS_OC_POLICY_H
#define _PLAYERBOTS_OC_POLICY_H

#include <array>
#include <cstdint>

namespace Oculus
{
constexpr float Pi = 3.14159265f;

// Drakos the Interrogator

constexpr float SphereRadius = 12.0f;
constexpr float SphereMargin = 1.0f;
constexpr float SphereMaxStep = 3.0f;

// Tanks hold Drakos; everyone else steps away from a parked sphere.
constexpr bool AvoidsSpheres(bool tank) { return !tank; }

// How far to step out towards radius plus margin, a few yards per tick; 0 = clear of the sphere.
constexpr float SphereStepAway(float distanceToSphere)
{
    if (distanceToSphere >= SphereRadius + SphereMargin)
        return 0.0f;
    float step = SphereRadius + SphereMargin - distanceToSphere;
    return step < SphereMaxStep ? step : SphereMaxStep;
}

// Drakes

enum class Drake : int8_t
{
    None = -1,
    Amber = 0,
    Emerald = 1,
    Ruby = 2
};

// Seats per drake for the whole group, {Amber, Emerald, Ruby}; the master keeps whichever drake they picked.
constexpr std::array<int, 3> DrakeComposition = {2, 2, 1};

// The drake for the bot at this rank among the group's bots, in group order; None once every seat is taken.
constexpr Drake AssignedDrake(Drake masterDrake, int botRank)
{
    std::array<int, 3> left = DrakeComposition;
    if (masterDrake != Drake::None && left[static_cast<int>(masterDrake)] > 0)
        --left[static_cast<int>(masterDrake)];

    Drake drake = Drake::None;
    for (int seat = 0; seat <= botRank; ++seat)
    {
        drake = Drake::None;
        for (int i = 0; i < 3 && drake == Drake::None; ++i)
        {
            if (left[i] > 0)
            {
                --left[i];
                drake = static_cast<Drake>(i);
            }
        }
    }
    return drake;
}

constexpr bool MountsDrake(bool masterOnDrake, bool botOnDrake) { return masterOnDrake && !botOnDrake; }

constexpr bool DismountsDrake(bool masterOnDrake, bool botOnDrake) { return !masterOnDrake && botOnDrake; }

constexpr bool FliesWithGroup(bool masterOnDrake, bool botOnDrake) { return masterOnDrake && botOnDrake; }

// Any other action during the essence's 0.5 s cast interrupts it and puts the item on a 15 s cooldown.
constexpr bool MountingAllows(bool inOculus, bool masterOnDrake, bool botOnDrake, bool mountAction)
{
    return !(inOculus && MountsDrake(masterOnDrake, botOnDrake) && !mountAction);
}

// On a drake only the flight action moves it: follow and chase would fight the formation.
constexpr bool FlyingAllows(bool inOculus, bool botOnDrake, bool movement, bool flyAction)
{
    return !(inOculus && botOnDrake && movement && !flyAction);
}

constexpr float FormationRadius = 15.0f;
constexpr float FormationRegroupDistance = 20.0f;

// Three quarters of a circle round the master's drake, leaving its 90-degree frontal cone clear.
constexpr float FormationAngle(int groupSlot) { return groupSlot * (2.0f * Pi - Pi / 2.0f) / 5.0f + Pi / 2.0f; }

// Drake spells reach 60 yards
constexpr float DrakeEngageRange = 55.0f;

// Where a drake out of range stops, measured from its target, so it isn't back at the edge next tick
constexpr float DrakeApproachDistance = DrakeEngageRange - 10.0f;

enum class AmberSpell
{
    ShockLance,
    StopTime,
    TemporalRift,
    None
};

struct AmberState
{
    int shockCharges = 0;  // this drake's charges on the target
    bool enraged = false;  // target has Enraged Assault
    bool stopTimeOnTarget = false;
    bool stopTimeReady = false;
    bool channelingRift = false;
};

// Detonate at 9 charges rather than spend a GCD on the 10th; Stop Time adds 5 charges, so it comes after.
constexpr AmberSpell AmberNext(AmberState state)
{
    if (state.shockCharges > 8)
        return AmberSpell::ShockLance;
    if (state.enraged && !state.stopTimeOnTarget && state.stopTimeReady)
        return AmberSpell::StopTime;
    if (!state.channelingRift)
        return AmberSpell::TemporalRift;
    return AmberSpell::None;
}

constexpr uint32_t StopTimeCooldownMs = 60000;

enum class EmeraldSpell
{
    LeechingPoison,
    TouchTheNightmare,
    FlyToHealTarget,
    DreamFunnel,
    None
};

struct EmeraldState
{
    bool poisonUp = false;  // this drake's Leeching Poison on the target
    int poisonStacks = 0;
    int poisonMsLeft = 0;
    bool touchReady = false;
    bool touchOnTarget = false;
    bool healthAbove90 = false;  // own drake
    bool healTarget = false;     // an allied drake worth healing
    bool funnelOnHealTarget = false;
    bool healTargetInRange = false;
    bool funneling = false;
};

constexpr int PoisonStacksWanted = 3;
constexpr int PoisonRefreshMs = 4000;

// Poison first, Touch when it won't kill its own drake, then Dream Funnel; poison fills idle GCDs.
constexpr EmeraldSpell EmeraldNext(EmeraldState state)
{
    if (!state.poisonUp || state.poisonStacks < PoisonStacksWanted || state.poisonMsLeft < PoisonRefreshMs)
        return EmeraldSpell::LeechingPoison;
    if (state.touchReady && (!state.touchOnTarget || state.healthAbove90))
        return EmeraldSpell::TouchTheNightmare;
    if (state.healTarget && !state.funnelOnHealTarget)
        return state.healTargetInRange ? EmeraldSpell::DreamFunnel : EmeraldSpell::FlyToHealTarget;
    return state.funneling ? EmeraldSpell::None : EmeraldSpell::LeechingPoison;
}

// Switch heal targets only for a drake 15% lower, so the channel doesn't flip between two close ones.
constexpr bool PrefersHealTarget(bool haveTarget, float candidatePct, float currentPct)
{
    return !haveTarget || candidatePct < currentPct - 15.0f;
}

constexpr uint32_t TouchTheNightmareCooldownMs = 10000;

enum class RubySpell
{
    Martyr,
    EvasiveManeuvers,
    SearingWrath
};

struct RubyState
{
    int evasiveCharges = 0;
    bool maneuversUp = false;
    int maneuversMsLeft = 0;
    bool martyrReady = false;
    bool maneuversReady = false;
};

constexpr RubySpell RubyNext(RubyState state)
{
    if (state.maneuversUp && state.martyrReady && state.maneuversMsLeft > 10000 && state.evasiveCharges >= 5)
        return RubySpell::Martyr;
    if (state.maneuversReady && state.evasiveCharges >= 10)
        return RubySpell::EvasiveManeuvers;
    return RubySpell::SearingWrath;
}

constexpr uint32_t MartyrCooldownMs = 10000;
constexpr uint32_t EvasiveManeuversCooldownMs = 5000;

// Mage-Lord Urom

struct Point
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// The three outer platforms he summons from, in order; the fourth phase is the inner ring
constexpr std::array<Point, 3> UromPlatforms = {
    {{1177.47f, 937.722f, 527.405f}, {968.66f, 1042.53f, 527.32f}, {1164.02f, 1170.85f, 527.321f}}};
constexpr int UromRingPhase = 3;

// Large enough to cover a platform, small enough not to reach the inner ring
constexpr float PlatformRadius = 60.0f;

// The phase a bot is in by where it stands: the first platform within reach, else the ring.
constexpr int UromPhase(std::array<float, 3> distancesToPlatforms)
{
    for (int i = 0; i < 3; ++i)
        if (distancesToPlatforms[i] < PlatformRadius)
            return i;
    return UromRingPhase;
}

// On a platform he only summons and vanishes: attacking him there drags bots after him.
constexpr bool AttacksUromIn(int phase) { return phase >= UromRingPhase; }

// Behind the inner ring's pillars, out of sight of Empowered Arcane Explosion
constexpr std::array<Point, 3> UromSafeSpots = {
    {{1138.88f, 1052.22f, 508.36f}, {1084.62f, 1079.71f, 508.36f}, {1087.42f, 1020.132f, 508.36f}}};

constexpr float SafeSpotTolerance = 2.0f;

constexpr int ClosestSafeSpot(float x, float y, float z)
{
    int best = 0;
    float bestDistance = -1.0f;
    for (int i = 0; i < 3; ++i)
    {
        float dx = UromSafeSpots[i].x - x;
        float dy = UromSafeSpots[i].y - y;
        float dz = UromSafeSpots[i].z - z;
        float distance = dx * dx + dy * dy + dz * dz;
        if (bestDistance < 0.0f || distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

constexpr float TimeBombRadius = 10.0f;
constexpr float TimeBombMargin = 2.0f;

// 0 = nobody near enough to share the blast.
constexpr float TimeBombStepAway(float distanceToAlly)
{
    return distanceToAlly < TimeBombRadius ? TimeBombRadius + TimeBombMargin - distanceToAlly : 0.0f;
}

struct UromMoment
{
    bool explosion = false;  // Empowered Arcane Explosion being cast
    bool melee = false;
    bool timeBomb = false;  // this bot carries the Time Bomb
};

struct UromAction
{
    bool movement = false;
    bool avoidExplosion = false;
    bool avoidAoe = false;
    bool timeBombSpread = false;
};

constexpr bool UromAllows(UromMoment fight, UromAction action)
{
    if (fight.explosion && action.movement && !action.avoidExplosion)
        return false;
    // Frost Bomb lands on the tank: melee would only be chased out of melee range
    if (fight.melee && action.avoidAoe)
        return false;
    return !(fight.timeBomb && action.movement && !action.timeBombSpread);
}

// Ley-Guardian Eregos

// During Planar Shift he is immune: drakes hold their fire and their heals.
constexpr bool EregosAllows(bool planarShift, bool drakeAttack) { return !(planarShift && drakeAttack); }

// While he can be hit, drakes stop their formation and hold in range of him.
constexpr bool EngagesEregos(bool eregosUp, bool planarShift) { return eregosUp && !planarShift; }
}  // namespace Oculus

#endif
