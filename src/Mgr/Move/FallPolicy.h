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

// A destination sunk under the floor. The core's collision check falls back to the grid height when it finds no ground,
// and in a dungeon built of one world model that height is 0: a healer's step back in heroic Azjol-Nerub came out at z 0
// under a floor at 813, and the bot walked straight down through it, out to 500 yards from the group (role check,
// 2026-11-10). Only a step (SinkReach yards or less) well down (more than SinkSlack under the bot) is looked at:
// ground is what a search finds below the bot's own height, SinkSearchDown yards down; a destination more than
// SinkSlack under that ground is put on it, and with no ground found the step is refused. A long walk is the path
// search's to judge.
constexpr float SinkReach = 40.0f;
constexpr float SinkSearchDown = 50.0f;
constexpr float SinkSlack = 5.0f;

enum class SinkFix
{
    Keep,
    OnGround,
    Refuse
};

constexpr bool SinkChecked(float horizontal, float destZ, float botZ)
{
    return horizontal <= SinkReach && destZ < botZ - SinkSlack;
}

constexpr SinkFix FixSunk(float destZ, bool groundFound, float groundZ)
{
    if (!groundFound)
        return SinkFix::Refuse;
    return destZ < groundZ - SinkSlack ? SinkFix::OnGround : SinkFix::Keep;
}
}  // namespace BotMove

#endif
