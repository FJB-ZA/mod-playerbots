/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Where each member walks behind the bot tank leading a run. The usual follow angles spread a group around the whole
// circle, so one bot walked ahead of the tank, into the next pack's aggro range, and opened the fight. Everyone walks
// in the half circle behind the tank instead; casters and healers a few steps further back, so they start each fight
// out of the cleaves and breaths in front of the mobs the tank holds.

#ifndef _PLAYERBOTS_TRAIL_POLICY_H
#define _PLAYERBOTS_TRAIL_POLICY_H

#include <cstdint>

namespace DungeonLead
{
constexpr float Pi = 3.14159265f;
constexpr float MeleeTrail = 3.0f;    // yards behind the tank
constexpr float RangedTrail = 12.0f;  // past the 10 yards a frontal cleave reaches, well inside heal and spell range
// How wide the followers spread behind the tank: narrow, as the core stops a follow point at the first wall, and a
// wide spread put the back line at the tank's side in a corridor (12 yards back is under 5 yards to either side)
constexpr float TrailArc = Pi / 4.0f;

struct Trail
{
    float distance = 0.0f;
    float angle = 0.0f;  // from the tank's facing, as the core's follow takes it: Pi is straight behind
};

// slot: this member's place among the tank's followers, of count
constexpr Trail TrailFor(bool ranged, uint32_t slot, uint32_t count)
{
    float const spread = count > 1 ? (float(slot) / float(count - 1) - 0.5f) * TrailArc : 0.0f;
    return {ranged ? RangedTrail : MeleeTrail, Pi + spread};
}
}  // namespace DungeonLead

#endif
