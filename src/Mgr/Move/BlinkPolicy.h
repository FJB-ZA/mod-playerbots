/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Where a mage's Blink back may land in a group: by the tank, or nearer it than the mage is now. Blinking straight away
// from a mob on it, a mage at Krik'thir landed 34 yards from the tank, and the next watcher wave the boss sent went for
// it, the nearest one, with the tank too far to take it: the group wiped (role check, heroic Azjol-Nerub, 2026-11-10).

#ifndef _PLAYERBOTS_BLINK_POLICY_H
#define _PLAYERBOTS_BLINK_POLICY_H

namespace BotMove
{
constexpr float BlinkTankReach = 15.0f;  // a landing this near the tank leaves the mob a short run into its taunt

constexpr bool BlinkKeepsGroup(bool hasTank, float tankFromBot, float tankFromLanding)
{
    return !hasTank || tankFromLanding <= BlinkTankReach || tankFromLanding < tankFromBot;
}
}  // namespace BotMove

#endif
