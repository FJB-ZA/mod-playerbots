/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_SUBCLASS_BOT_POLICY_H
#define PLAYERBOTS_SUBCLASS_BOT_POLICY_H

#include <cstdint>
#include <vector>

namespace SubclassBots
{
struct PathShare
{
    uint8_t path;
    uint8_t sharePct;
};

// A roll in [0, 100) that is the same for a character on every startup, so a bot keeps its path.
inline uint32_t Roll(uint32_t guid)
{
    uint32_t hash = guid;
    hash ^= hash >> 16;
    hash *= 0x85EBCA6Bu;
    hash ^= hash >> 13;
    hash *= 0xC2B2AE35u;
    hash ^= hash >> 16;
    return hash % 100;
}

// The path a random bot of the class takes, its shares stacked in order; 0 for the plain class.
inline uint8_t PickPath(uint32_t guid, std::vector<PathShare> const& shares)
{
    uint32_t const roll = Roll(guid);
    uint32_t stacked = 0;
    for (PathShare const& share : shares)
    {
        stacked += share.sharePct;
        if (roll < stacked)
            return share.path;
    }

    return 0;
}
}  // namespace SubclassBots

#endif
