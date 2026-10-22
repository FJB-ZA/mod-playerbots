/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// How a bot votes on a group loot roll: its spec decides need or greed, and a ninja needs on everything.

#ifndef _PLAYERBOTS_LOOT_ROLL_POLICY_H
#define _PLAYERBOTS_LOOT_ROLL_POLICY_H

#include <cstdint>

namespace LootRoll
{
// Same values as the core's RollVote.
enum class Vote : uint8_t
{
    Pass = 0,
    Need = 1,
    Greed = 2,
    Disenchant = 3
};

// A fixed trait per bot, so the same bots are the ninjas in every run.
constexpr bool IsNinja(uint32_t guidLow, uint32_t ninjaPercent)
{
    return ninjaPercent && (guidLow * 2654435761u) % 100u < ninjaPercent;
}

// The bot's own vote stands, except that a ninja needs whether or not the item helps it.
constexpr Vote Choose(Vote own, bool ninja) { return ninja ? Vote::Need : own; }
}  // namespace LootRoll

#endif
