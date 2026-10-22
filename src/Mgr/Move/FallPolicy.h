/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Fall damage as the core deals it, and when a bot takes a drop: never a leader's fatal fall unless the group wipes.

#ifndef _PLAYERBOTS_FALL_POLICY_H
#define _PLAYERBOTS_FALL_POLICY_H

#include <algorithm>

namespace BotMove
{
// Player::HandleFall: no damage below 13.48 yards, then 1.8% of max health per yard less 24.26%.
constexpr float FallDamageFrom = 13.48f;
constexpr float FallDamageSlope = 0.018f;
constexpr float FallDamageIntercept = -0.2426f;

// Percent of max health a fall deals; safe fall shortens the counted height, feather fall cancels it.
inline float FallDamagePct(float height, float safeFall = 0.0f, bool featherFall = false, float rate = 1.0f)
{
    if (featherFall || height < FallDamageFrom)
        return 0.0f;
    float pct = (FallDamageSlope * (height - safeFall) + FallDamageIntercept) * 100.0f * rate;
    return std::clamp(pct, 0.0f, 100.0f);
}

// Health a bot keeps in hand after a drop, so it never lands nearly dead.
constexpr float FallHealthMargin = 25.0f;

struct DropMoment
{
    float height = 0.0f;
    float safeFall = 0.0f;
    bool featherFall = false;
    float healthPct = 100.0f;
    bool fatalForLeader = false;
    bool wiping = false;
    // World.Rate.Damage.Fall
    float rate = 1.0f;
};

// A leader who jumped to their death is only joined in a wipe; every other drop must be survived comfortably.
inline bool TakesDrop(DropMoment drop, float maxDamagePct)
{
    if (drop.fatalForLeader)
        return drop.wiping;
    float damage = FallDamagePct(drop.height, drop.safeFall, drop.featherFall, drop.rate);
    return damage <= maxDamagePct && damage <= drop.healthPct - FallHealthMargin;
}

// The leader died while the group is fighting: the group is going down together.
constexpr bool IsWipe(bool leaderDead, bool inCombat) { return leaderDead && inCombat; }
}  // namespace BotMove

#endif
