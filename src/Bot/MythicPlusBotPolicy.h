/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Bots and solo-wow's Mythic+ (core scripts/Custom/MythicPlus): its items are earned in runs, never randomly geared.

#ifndef _PLAYERBOTS_MYTHIC_PLUS_BOT_POLICY_H
#define _PLAYERBOTS_MYTHIC_PLUS_BOT_POLICY_H

#include <cstdint>

namespace MythicPlusBots
{
// The core's MythicItemPolicy.h rank ids and its keystone item
inline constexpr uint32_t FirstRankId = 900000;
inline constexpr uint32_t RankIdEnd = 990000;
inline constexpr uint32_t KeystoneItem = 990300;
// The core's Explosive Orb creature, which bots kill before it bursts
inline constexpr uint32_t ExplosiveOrbEntry = 990921;
// The core's Quake spell: it ticks an interrupt, so casters stop casting until it ends
inline constexpr uint32_t QuakeSpell = 990310;

// Mythic+ items come only from the end-of-run cache and the vault: random gearing skips them and keeps earned ones
constexpr bool IsMythicPlusItem(uint32_t itemId)
{
    return (itemId >= FirstRankId && itemId < RankIdEnd) || itemId == KeystoneItem;
}
}  // namespace MythicPlusBots

#endif
