/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Halls of Lightning bosses: Bjarngrim's lieutenants, Volkhan first, Ionar's sparks, Loken's nova.

#ifndef _PLAYERBOTS_HOL_POLICY_H
#define _PLAYERBOTS_HOL_POLICY_H

namespace HallsOfLightning
{
// General Bjarngrim

// Whirlwind hits everyone within 8 yards
constexpr float WhirlwindClearance = 10.0f;

constexpr float WhirlwindStepAway(float distanceToBoss)
{
    return distanceToBoss < WhirlwindClearance ? WhirlwindClearance - distanceToBoss : 0.0f;
}

constexpr bool AttacksLieutenant(bool dps) { return dps; }

// There are two lieutenants: a bot already on one stays on it instead of ping-ponging.
constexpr bool SwitchesToLieutenant(bool onLieutenant) { return !onLieutenant; }

struct BjarngrimMoment
{
    bool healer = false;
    bool tank = false;
    bool whirlwind = false;     // being cast
    bool lieutenantUp = false;  // anywhere nearby, line of sight or not
};

struct BjarngrimAction
{
    bool movement = false;
    bool avoidWhirlwind = false;
    bool dpsAssist = false;
    bool aoe = false;
};

constexpr bool BjarngrimAllows(BjarngrimMoment fight, BjarngrimAction action)
{
    if (fight.healer)
        return true;
    if (fight.whirlwind && action.movement && !action.avoidWhirlwind)
        return false;
    // Dps stay on the lieutenants: assist and area damage would drag them onto the general
    if (!fight.lieutenantUp || fight.tank)
        return true;
    return !(action.dpsAssist || action.aoe);
}

// Volkhan

// Dps stay on Volkhan; area damage would shatter the brittle golems into the group.
constexpr bool AttacksVolkhan(bool tank, bool healer) { return !tank && !healer; }

constexpr bool VolkhanAllows(bool tank, bool healer, bool dpsAssist, bool aoe)
{
    return !(AttacksVolkhan(tank, healer) && (dpsAssist || aoe));
}

// Ionar

// Static Overload hits everyone within 8 yards of its target
constexpr float StaticOverloadRadius = 8.0f;
constexpr float StaticOverloadMargin = 2.0f;

constexpr float StaticOverloadStepAway(float distanceToTarget)
{
    return distanceToTarget < StaticOverloadRadius ? StaticOverloadRadius + StaticOverloadMargin - distanceToTarget
                                                   : 0.0f;
}

// Ball Lightning splashes 6 yards round its target: ranged keep apart while it is cast
constexpr float BallLightningSpacing = 7.0f;

constexpr bool SpreadsForBallLightning(bool melee) { return !melee; }

constexpr float BallLightningStepAway(float distanceToMember)
{
    return distanceToMember < BallLightningSpacing ? BallLightningSpacing - distanceToMember : 0.0f;
}

constexpr float IonarTankLeash = 10.0f;
constexpr float DisperseLeash = 8.0f;

struct IonarAction
{
    bool movement = false;
    bool toDispersePosition = false;
    bool staticOverloadSpread = false;
    bool charge = false;
};

// While he is dispersed only the run to the safe spot and the overload spread may move; nobody charges a spark.
constexpr bool IonarAllows(bool dispersed, bool disperseCast, IonarAction action)
{
    if (dispersed && action.movement && !action.toDispersePosition && !action.staticOverloadSpread)
        return false;
    return !(disperseCast && action.charge);
}

// Loken

// Lightning Nova hits everyone within 20 yards
constexpr float LightningNovaClearance = 22.0f;

constexpr float LightningNovaStepAway(float distanceToBoss)
{
    return distanceToBoss < LightningNovaClearance ? LightningNovaClearance - distanceToBoss : 0.0f;
}

// Pulsing Shockwave grows with distance, so ranged stack on him; hunters stay past their 5-yard dead zone.
constexpr float LokenStackDistance(bool hunter) { return hunter ? 6.5f : 2.0f; }

constexpr float LokenStackMaxStep = 10.0f;

constexpr bool StacksOnLoken(bool melee) { return !melee; }

constexpr float LokenStackStep(float distanceToBoss, bool hunter)
{
    float step = hunter ? distanceToBoss - LokenStackDistance(true) : distanceToBoss;
    return step < LokenStackMaxStep ? step : LokenStackMaxStep;
}

struct LokenAction
{
    bool flee = false;
    bool movement = false;
    bool avoidNova = false;
    bool charge = false;
};

constexpr bool LokenAllows(bool lightningNova, LokenAction action)
{
    if (action.flee)
        return false;
    return !(lightningNova && ((action.movement && !action.avoidNova) || action.charge));
}
}  // namespace HallsOfLightning

#endif
