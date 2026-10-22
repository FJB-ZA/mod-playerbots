/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Whether a bot can cross a gap between two pieces of navmesh by stepping, jumping or dropping.

#ifndef _PLAYERBOTS_GAP_POLICY_H
#define _PLAYERBOTS_GAP_POLICY_H

#include <cmath>

#include "JumpPhysics.h"

namespace BotMove
{
enum class GapKind
{
    Seam,
    Jump,
    Drop,
    TooFar,
    TooHigh,
    TooDeep
};

struct GapLimits
{
    // A split in the mesh this small is a seam the bot walks over.
    float seamReach = 1.0f;
    float seamRise = 1.0f;
    // Below the jump apex, so the bot never lands short of a ledge.
    float maxRise = 1.4f;
    float dropFrom = 3.0f;
    // Fall damage kills a player at about 69 yards; whether a shorter drop is worth the damage is FallPolicy's call.
    float maxDrop = 69.0f;
    float reachMargin = 0.9f;
};

inline GapKind ClassifyGap(float horizontal, float rise, float runSpeed, GapLimits const& limits = {})
{
    if (horizontal <= limits.seamReach && std::fabs(rise) <= limits.seamRise)
        return GapKind::Seam;
    if (rise > limits.maxRise)
        return GapKind::TooHigh;
    if (-rise > limits.maxDrop)
        return GapKind::TooDeep;
    if (horizontal > JumpReach(runSpeed, rise) * limits.reachMargin)
        return GapKind::TooFar;
    return -rise >= limits.dropFrom ? GapKind::Drop : GapKind::Jump;
}

constexpr bool CanCross(GapKind kind)
{
    return kind == GapKind::Seam || kind == GapKind::Jump || kind == GapKind::Drop;
}
}  // namespace BotMove

#endif
