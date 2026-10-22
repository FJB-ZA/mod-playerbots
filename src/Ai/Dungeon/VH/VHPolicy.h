/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Violet Hold bosses: Erekem first, Ichoron's globules, Zuramat's void, Cyanigosa's flank.

#ifndef _PLAYERBOTS_VH_POLICY_H
#define _PLAYERBOTS_VH_POLICY_H

namespace VioletHold
{
// Erekem

// Dps stay on Erekem: killing both guards enrages him, and area damage would split the damage between them.
constexpr bool ErekemAllows(bool dps, bool dpsAssist, bool aoe) { return !(dps && (dpsAssist || aoe)); }

// Ichoron

// The tank holds Ichoron while his bubble is up; once it bursts (Drained) everyone kills globules.
constexpr bool TankStaysOnIchoron(bool tank, bool drained) { return tank && !drained; }

// Assist and drop target would pull bots off the globules before they merge.
constexpr bool IchoronAllows(bool retarget) { return !retarget; }

// Zuramat the Obliterator

struct ZuramatAction
{
    bool assist = false;
    bool attack = false;
    bool sentry = false;
};

// A void-shifted bot is the only one who can see the sentries; attacking him through his shroud costs healing.
constexpr bool ZuramatAllows(bool voidShifted, bool shroud, ZuramatAction action)
{
    if (voidShifted && action.assist)
        return false;
    if (voidShifted && action.sentry)
        return true;
    return !(shroud && action.attack);
}

// Only a void-shifted bot already on a sentry keeps its target through the shroud.
constexpr bool DropsTargetForShroud(bool shroud, bool voidShifted, bool targetIsSentry)
{
    return shroud && !(voidShifted && targetIsSentry);
}

// Cyanigosa

// Her breath and tail are cones: everyone but the tank stands on a flank; ranged dps are far enough out already.
constexpr bool StandsOnFlank(bool tank, bool rangedDps) { return !tank && !rangedDps; }
}  // namespace VioletHold

#endif
