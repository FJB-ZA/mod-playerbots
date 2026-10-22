// Pure rules for the Gundrak bosses: Slad'ran's nova, snake wraps and stack, Gal'darah's Whirling Slash, Eck's Spit.

#ifndef _PLAYERBOTS_GD_POLICY_H
#define _PLAYERBOTS_GD_POLICY_H

namespace Gundrak
{
// Slad'ran

// Poison Nova hits everyone within 15 yards
constexpr float PoisonNovaClearance = 17.0f;

constexpr float PoisonNovaStepAway(float distanceToBoss)
{
    return distanceToBoss < PoisonNovaClearance ? PoisonNovaClearance - distanceToBoss : 0.0f;
}

constexpr float StackLeash = 10.0f;
constexpr float StackCloseTo = 5.0f;
constexpr float TankPickupRange = 12.0f;

// Only dps free wrapped players, and a wrapped bot can't free anyone.
constexpr bool CanFreeSnakeWrap(bool dps, bool wrapped) { return dps && !wrapped; }

// The closest free dps takes a wrap; an exact tie goes to the lower guid.
constexpr bool BeatsToSnakeWrap(float memberDistance, float botDistance, bool memberGuidLower)
{
    return memberDistance < botDistance || (memberDistance == botDistance && memberGuidLower);
}

// Non-tanks stack on the tank unless they are freeing a wrap.
constexpr bool StacksOnTank(bool tank, bool freeingWrap, float distanceToTank)
{
    return !tank && !freeingWrap && distanceToTank > StackLeash;
}

// The tank goes back to Slad'ran unless he is on him or picking up an add that reached him.
constexpr bool TankReturnsToSladran(bool onBoss, bool onAdd, float distanceToAdd)
{
    return !onBoss && !(onAdd && distanceToAdd <= TankPickupRange);
}

struct SladranMoment
{
    bool poisonNova = false;  // being cast
    bool tank = false;
    bool freeingWrap = false;
};

struct SladranAction
{
    bool movement = false;
    bool avoidNova = false;
    bool flee = false;           // flee, run away, random moves and moving away from the group
    bool chaseFarAdd = false;    // tank assist or reach target towards an add beyond pickup range
    bool formationMove = false;  // combat formation moves other than getting behind the target
    bool dpsAssist = false;
};

constexpr bool SladranAllows(SladranMoment fight, SladranAction action)
{
    if (fight.poisonNova && action.movement && !action.avoidNova)
        return false;
    if (action.flee)
        return false;
    if (fight.tank)
        return !action.chaseFarAdd;
    if (action.formationMove)
        return false;
    return !(action.dpsAssist && fight.freeingWrap);
}

// Gal'darah

// Whirling Slash hits everyone within 5 yards
constexpr float WhirlingSlashRadius = 5.0f;
constexpr float WhirlingSlashMargin = 2.0f;

// He chases the tank while spinning: the tank steps twice as far or he stutter-steps.
constexpr float WhirlingSlashStepAway(float distanceToBoss, bool tank)
{
    if (distanceToBoss >= WhirlingSlashRadius + WhirlingSlashMargin)
        return 0.0f;
    float clearance = (tank ? 2.0f * WhirlingSlashRadius : WhirlingSlashRadius) + WhirlingSlashMargin;
    return clearance - distanceToBoss;
}

constexpr bool GaldarahAllows(bool whirlingSlash, bool movement, bool avoidSlash)
{
    return !(whirlingSlash && movement && !avoidSlash);
}

// Eck the Ferocious (heroic)

// Eck Spit is a 25-yard, 30-degree cone at his victim: dodge as if it were 50 degrees and 2 yards longer
constexpr float EckSpitReach = 27.0f;
constexpr float EckSpitDodgeSlope = 0.4663f;  // tan(25 degrees)
constexpr float EckSpitSidestepMargin = 1.0f;
// Cones hit anyone within 2 yards of the caster at any angle (Unit::IsWithinBoundaryRadius, MIN_MELEE_REACH)
constexpr float EckSpitHugRadius = 2.0f;

constexpr bool InEckHug(float forward, float lateral)
{
    return forward * forward + lateral * lateral <= EckSpitHugRadius * EckSpitHugRadius;
}

// forward/lateral: the bot's offset along and across Eck's facing; lateral is the absolute value.
constexpr bool InEckSpit(float forward, float lateral)
{
    bool cone = forward > 0.0f && forward <= EckSpitReach && lateral <= forward * EckSpitDodgeSlope;
    return cone || InEckHug(forward, lateral);
}

// He turns to follow his victim, so only the others can step out.
constexpr bool DodgesEckSpit(bool victim, float forward, float lateral)
{
    return !victim && InEckSpit(forward, lateral);
}

// Sideways is out of both the cone and his hug radius; a bot behind him only has the hug to clear.
constexpr float EckSpitSidestep(float forward, float lateral)
{
    float cone = forward > 0.0f ? forward * EckSpitDodgeSlope : 0.0f;
    float hug = InEckHug(forward, lateral) ? EckSpitHugRadius : 0.0f;
    float needed = (cone > hug ? cone : hug) + EckSpitSidestepMargin - lateral;
    return needed > 0.0f ? needed : 0.0f;
}

// Chasing or forming up during the spit would walk a bot back into the cone.
constexpr bool EckAllows(bool spit, bool victim, bool repositioning, bool dodge)
{
    return !(spit && !victim && repositioning && !dodge);
}
}  // namespace Gundrak

#endif
