/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Bots and solo-wow's Mythic+ (core scripts/Custom/MythicPlus): its items are earned in runs, never randomly geared,
// and bot groups run their own keys at the level their gear and skill can handle.

#ifndef _PLAYERBOTS_MYTHIC_PLUS_BOT_POLICY_H
#define _PLAYERBOTS_MYTHIC_PLUS_BOT_POLICY_H

#include <algorithm>
#include <array>
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

// The core's keystone range and the level its keys and Font of Power ask for
inline constexpr uint8_t FirstKeyLevel = 2;
inline constexpr uint8_t TopKeyLevel = 30;
inline constexpr uint8_t HeroicLevel = 80;

// Fresh heroic gear runs a +2; every 5 item levels more carries a bot one key level higher (ICC 25 gear: about +14)
inline constexpr uint32_t ReadyItemLevel = 200;
inline constexpr uint32_t ItemLevelPerKeyLevel = 5;
// Hidden skill (0-100) moves that up to two levels either way, one per 25 points from the middle
inline constexpr uint32_t SkillPerKeyLevel = 25;
// Bots group with others this close in hidden skill
inline constexpr uint32_t SkillBand = 25;

// The highest key a bot handles, or 0 while its gear is below heroic
constexpr uint8_t ComfortKeyLevel(uint32_t itemLevel, uint32_t skill)
{
    if (itemLevel < ReadyItemLevel)
        return 0;
    int const gear = int((itemLevel - ReadyItemLevel) / ItemLevelPerKeyLevel);
    int const knack = (int(std::min<uint32_t>(skill, 100)) - 50) / int(SkillPerKeyLevel);
    return uint8_t(std::clamp(int(FirstKeyLevel) + gear + knack, int(FirstKeyLevel), int(TopKeyLevel)));
}

// Like players, a bot stretches one level past what it handles, never more
constexpr bool FitsKey(uint8_t keyLevel, uint8_t comfort) { return comfort && keyLevel <= comfort + 1; }

constexpr bool SkillMatches(uint32_t a, uint32_t b) { return (a > b ? a - b : b - a) <= SkillBand; }

enum class Role : uint8_t
{
    Tank = 0,
    Healer = 1,
    Damage = 2
};

// One tank, one healer, three damage dealers
inline constexpr std::array<uint8_t, 3> RoleSlots = {1, 1, 3};
inline constexpr uint8_t GroupSize = 5;

// A run is given up after this long, or this long without a boss kill
inline constexpr uint32_t RunLimitMs = 100 * 60 * 1000;
inline constexpr uint32_t StallMs = 20 * 60 * 1000;
// The group's dead get up after this long out of combat: a healer's resurrection, or the run back after a wipe
inline constexpr uint32_t ReviveAfterMs = 15 * 1000;
// The group stays this long after the run ends, while the cache and the Crest Forge pay out
inline constexpr uint32_t LingerMs = 30 * 1000;

constexpr bool GivesUp(uint32_t runMs, uint32_t sinceBossMs) { return runMs >= RunLimitMs || sinceBossMs >= StallMs; }
}  // namespace MythicPlusBots

#endif
