/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Culling of Stratholme bosses: Salramm's ghouls, stacking on Epoch, the Infinite Corruptor.

#ifndef _PLAYERBOTS_COS_POLICY_H
#define _PLAYERBOTS_COS_POLICY_H

#include <algorithm>

namespace CullingOfStratholme
{
// Salramm the Fleshcrafter

// Explode Ghoul (52480, heroic 58825) is instant and hits 10 yd round a living ghoul: clear it before he casts
constexpr float GhoulExplosionRadius = 10.0f;
constexpr float GhoulClearance = GhoulExplosionRadius + 2.0f;

// Melee and the tank stay on the ghouls to kill them fast; running from one that is hitting you only drags it along.
constexpr bool KeepsClearOfGhoul(bool melee, bool ghoulTargetsBot) { return !melee && !ghoulTargetsBot; }

// How far to step away from a ghoul to be clear of its explosion; 0 = far enough already.
constexpr float GhoulStepAway(float distanceToGhoul)
{
    return distanceToGhoul < GhoulClearance ? GhoulClearance - distanceToGhoul : 0.0f;
}

// The skull moves only once its ghoul dies, so bots never fight over the mark.
constexpr bool MarksGhoul(bool skullOnLivingGhoul, bool ghoulAlive) { return ghoulAlive && !skullOnLivingGhoul; }

// Chrono-Lord Epoch

constexpr float EpochStackRange = 5.0f;
constexpr float EpochMaxStep = 10.0f;

// Hunters stay out: close enough to stack, they melee between shots or the boss charges them.
constexpr bool EpochStacks(bool hunter, float distanceToBoss) { return !hunter && distanceToBoss > EpochStackRange; }

constexpr float EpochStep(float distanceToBoss) { return std::min(distanceToBoss, EpochMaxStep); }

// Fleeing would undo the stack.
constexpr bool EpochAllows(bool hunter, bool flee) { return hunter || !flee; }

// Infinite Corruptor (heroic)

// The 25-minute clock keeps running and Corrupting Blight (60588) can't be cured: only his death stops either.
constexpr bool CorruptorNeedsSkull(bool heroic, bool skullOnHim) { return heroic && !skullOnHim; }
}  // namespace CullingOfStratholme

#endif
