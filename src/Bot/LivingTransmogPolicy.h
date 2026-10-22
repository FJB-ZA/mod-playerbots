/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOTS_LIVING_TRANSMOG_POLICY_H
#define _PLAYERBOTS_LIVING_TRANSMOG_POLICY_H

#include <cstdint>

#include "LivingWorldPolicy.h"

namespace LivingWorld
{
// mod-transmog's Warpweaver. Bots only restyle at one standing in these cities.
inline constexpr uint32_t WarpweaverEntry = 190010;
constexpr bool IsTransmogCity(uint32_t zoneId)
{
    return zoneId == 1519      // Stormwind
           || zoneId == 1637   // Orgrimmar
           || zoneId == 3703   // Shattrath
           || zoneId == 4395;  // Dalaran
}

inline constexpr uint32_t TransmogMinLevel = 20;
inline constexpr uint32_t TransmogMinQuality = 3;  // rare (blue) and better
inline constexpr float TransmogSearchRange = 250.0f;
// A bot that did not feel like restyling asks itself again after this; one that did waits a day.
inline constexpr uint32_t TransmogRetrySeconds = 2 * 3600;
inline constexpr uint32_t TransmogCooldownSeconds = 24 * 3600;
// A bot that cannot reach the Warpweaver gives up after this and waits out the retry window.
inline constexpr uint32_t TransmogTripSeconds = 5 * 60;

constexpr bool TransmogTripOver(uint64_t started, uint64_t now) { return now >= started + TransmogTripSeconds; }

// Chance that a bot in town with time on its hands goes to the Warpweaver: gear chasers care most
// about their look, power levellers least.
constexpr uint32_t TransmogVisitPercent(Motivation motivation)
{
    switch (motivation)
    {
        case Motivation::Gear:
            return 50;
        case Motivation::Pvp:
            return 40;
        case Motivation::Trader:
            return 30;
        case Motivation::Explorer:
            return 25;
        case Motivation::Profession:
            return 20;
        case Motivation::Leveller:
            return 5;
        default:
            return 0;
    }
}

constexpr uint32_t TransmogSlotsPerVisit(Motivation motivation)
{
    switch (motivation)
    {
        case Motivation::Gear:
            return 5;
        case Motivation::Pvp:
            return 4;
        default:
            return 2;
    }
}

// Every slot tried builds that slot's whole look menu, so a visit gives up after a couple of misses.
constexpr uint32_t TransmogSlotAttempts(uint32_t wanted) { return wanted + 2; }

// Too poor to bother walking over: 5 silver per level (4 gold at 80).
constexpr uint32_t TransmogMinPurse(uint32_t level) { return level * 500; }

constexpr bool WantsLook(uint32_t quality) { return quality >= TransmogMinQuality; }

// Index of the look to wear among `count` candidates listed best quality first, of which the first
// `bestCount` share the best quality: three picks in four come from that best group.
constexpr uint32_t PickLook(uint32_t bestCount, uint32_t count, uint32_t roll)
{
    if (!count)
        return 0;
    if (bestCount && bestCount <= count && roll % 4)
        return (roll / 4) % bestCount;
    return (roll / 4) % count;
}
}  // namespace LivingWorld

#endif
