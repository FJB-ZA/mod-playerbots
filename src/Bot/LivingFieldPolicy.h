/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_LIVING_FIELD_POLICY_H
#define PLAYERBOTS_LIVING_FIELD_POLICY_H

#include "LivingWorldPolicy.h"

namespace LivingWorld
{
inline uint32_t FieldWorkSeconds(Motivation motivation)
{
    return motivation == Motivation::Profession ? 360U : motivation == Motivation::Trader ? 240U : 120U;
}

inline bool FieldWorkWindow(uint32_t guid, uint64_t seconds, Motivation motivation)
{
    constexpr uint32_t cycle = 600;
    return (seconds + Mix(guid) % cycle) % cycle < FieldWorkSeconds(motivation);
}

inline uint32_t MaterialDeficit(uint32_t owned, uint32_t target) { return owned < target ? target - owned : 0; }

// Skinning hunts: the skill a creature of a level asks for (the core's own formula). Skinning it still teaches
// until 100 points above that; a hunt goes where it teaches best, up to 50 above.
constexpr uint32_t SkinningRequirement(uint32_t creatureLevel)
{
    return creatureLevel < 10 ? 0 : creatureLevel < 20 ? (creatureLevel - 10) * 10 : creatureLevel * 5;
}

constexpr bool SkinningTeaches(uint32_t skill, uint32_t creatureLevel)
{
    uint32_t const required = SkinningRequirement(creatureLevel);
    return skill >= required && skill < required + 50;
}

// How often a skinner with skill to learn grinds among skinnable beasts, by motivation.
inline uint32_t SkinningHuntChance(Motivation motivation)
{
    return motivation == Motivation::Profession || motivation == Motivation::Trader ? 60U : 30U;
}
}  // namespace LivingWorld

#endif
