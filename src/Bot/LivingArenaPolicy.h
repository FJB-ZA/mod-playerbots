// Pure rules for living arena bots: hidden skill and dedication, playbook tiers, rating and gear caps.

#ifndef _PLAYERBOTS_LIVING_ARENA_POLICY_H
#define _PLAYERBOTS_LIVING_ARENA_POLICY_H

#include <algorithm>
#include <cstdint>

#include "LivingWorldPolicy.h"

namespace LivingArena
{
enum class Tier : uint8_t
{
    Rookie,
    Regular,
    Skilled,
    Expert
};

// Hidden, stable per character; the sum of two rolls keeps most bots near average and few at the extremes.
constexpr uint32_t Skill(uint32_t guid)
{
    return (LivingWorld::Mix(guid ^ 0x165667b1U) % 101 + LivingWorld::Mix(guid ^ 0xd3a2646cU) % 101) / 2;
}

// How hard a bot works at its gear and setup, independent of how well it plays.
constexpr uint32_t Dedication(uint32_t guid) { return LivingWorld::Mix(guid ^ 0xfd7046c5U) % 101; }

constexpr bool Eligible(LivingWorld::Motivation motivation, uint32_t guid)
{
    uint32_t const roll = LivingWorld::Mix(guid ^ 0x27d4eb2fU) % 100;
    switch (motivation)
    {
        case LivingWorld::Motivation::Pvp:
            return true;
        case LivingWorld::Motivation::Gear:
            return roll < 50;
        default:
            return roll < 15;
    }
}

constexpr Tier TierFor(uint32_t skill)
{
    return skill >= 80 ? Tier::Expert : skill >= 55 ? Tier::Skilled : skill >= 30 ? Tier::Regular : Tier::Rookie;
}

// Chance in percent that a bot notices and acts on a playbook decision.
constexpr uint32_t Adherence(uint32_t skill) { return 30 + std::min<uint32_t>(skill, 100) * 65 / 100; }

// Milliseconds between team target calls; rookies never call targets.
constexpr uint32_t CallInterval(Tier tier)
{
    switch (tier)
    {
        case Tier::Expert:
            return 1500;
        case Tier::Skilled:
            return 3000;
        case Tier::Regular:
            return 6000;
        default:
            return 0;
    }
}

constexpr bool WithinRatingCap(uint32_t human, uint32_t bot, uint32_t maxAbove, uint32_t maxBelow)
{
    return bot <= human + maxAbove && bot + maxBelow >= human;
}

constexpr bool WithinGearCap(uint32_t human, uint32_t bot, uint32_t percentAbove)
{
    return uint64_t(bot) * 100 <= uint64_t(human) * (100 + percentAbove);
}

// Percent chance per check that a bot spends its points; low dedication hoards and gears late.
constexpr uint32_t ShoppingChance(uint32_t dedication) { return 20 + std::min<uint32_t>(dedication, 100) * 70 / 100; }

// Enchants and gems follow rating, sooner for dedicated players.
constexpr bool Tunes(uint32_t personalRating, uint32_t dedication)
{
    return personalRating >= 1900 || (personalRating >= 1600 && dedication >= 40) ||
           (personalRating >= 1400 && dedication >= 90);
}

// Minutes a bot team rests between rated games.
constexpr uint32_t RestMinutes(uint32_t dedication) { return 45 - std::min<uint32_t>(dedication, 100) * 35 / 100; }
}  // namespace LivingArena

#endif
