/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Utgarde Keep bosses: Keleseth's tombs, Dalronn first, and Ingvar's smashes and roars.

#ifndef _PLAYERBOTS_UK_POLICY_H
#define _PLAYERBOTS_UK_POLICY_H

namespace UtgardeKeep
{
// Prince Keleseth

// Healers keep healing; everyone else breaks the tomb.
constexpr bool AttacksFrostTomb(bool healer) { return !healer; }

// Assist would pull dps back onto the boss while a tomb is up.
constexpr bool KelesethAllows(bool tombUp, bool dpsAssist) { return !(tombUp && dpsAssist); }

// Skarvald the Constructor and Dalronn the Controller

constexpr bool AttacksDalronn(bool tank) { return !tank; }

constexpr bool DalronnAllows(bool dalronnTargetable, bool dpsAssist) { return !(dalronnTargetable && dpsAssist); }

// Ingvar the Plunderer

// Run this far past his middle to get out of the Smash cone, not just level with him
constexpr float SmashDodgeOvershoot = 2.0f;

constexpr float SmashDodgeStep(float distanceToBoss) { return distanceToBoss + SmashDodgeOvershoot; }

// Smash cones are frontal; melee only reposition once the tank has stopped dodging, or they chase every turn.
constexpr bool GetsBehindIngvar(bool tank, bool melee, bool behind, bool settled)
{
    return !tank && melee && !behind && settled;
}

struct IngvarMoment
{
    bool tank = false;
    bool tankMoving = false;
    bool roar = false;   // Staggering or Dreadful Roar being cast
    bool smash = false;  // Smash or Dark Smash being cast
};

struct IngvarAction
{
    bool movement = false;
    bool castWithCastTime = false;
    bool dodgeSmash = false;
};

constexpr bool IngvarAllows(IngvarMoment fight, IngvarAction action)
{
    // A moving tank is probably dodging a smash: nothing may redirect it
    if (fight.tank && fight.tankMoving && action.movement)
        return false;
    // The roar interrupts and locks the school: only instants start during it
    if (fight.roar && action.castWithCastTime)
        return false;
    // Any other move during a smash drags him round and can swing the cone onto the group
    return !(fight.tank && fight.smash && action.movement && !action.dodgeSmash);
}
}  // namespace UtgardeKeep

#endif
