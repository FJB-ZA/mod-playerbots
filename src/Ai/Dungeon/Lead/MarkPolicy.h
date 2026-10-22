/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Kill order and crowd control picks for a pack: skull on the most dangerous mob, moon on one the group can lock down.

#ifndef _PLAYERBOTS_MARK_POLICY_H
#define _PLAYERBOTS_MARK_POLICY_H

#include <cstddef>
#include <cstdint>

namespace DungeonLead
{
// Mirrors the core's CreatureType values.
enum class MobType : uint8_t
{
    None = 0,
    Beast = 1,
    Dragonkin = 2,
    Demon = 3,
    Elemental = 4,
    Giant = 5,
    Undead = 6,
    Humanoid = 7,
    Critter = 8,
    Mechanical = 9
};

enum class Cc : uint8_t
{
    None,
    Polymorph,
    Shackle,
    Banish,
    Hibernate,
    Trap
};

struct PackMob
{
    MobType type = MobType::None;
    bool caster = false;  // has mana: healers and casters die first
    bool elite = false;
    bool boss = false;
    bool ccImmune = false;  // to the crowd control being checked
    uint32_t maxHealth = 0;
};

constexpr bool CanCc(Cc cc, MobType type)
{
    switch (cc)
    {
        case Cc::Polymorph:
            return type == MobType::Humanoid || type == MobType::Beast || type == MobType::Critter;
        case Cc::Shackle:
            return type == MobType::Undead;
        case Cc::Banish:
            return type == MobType::Demon || type == MobType::Elemental;
        case Cc::Hibernate:
            return type == MobType::Beast || type == MobType::Dragonkin;
        case Cc::Trap:
            return type != MobType::None;
        default:
            return false;
    }
}

constexpr uint32_t Danger(PackMob const& mob)
{
    return (mob.boss ? 8 : 0) + (mob.caster ? 4 : 0) + (mob.elite ? 2 : 0);
}

// Most dangerous first; among equals the one that dies fastest.
constexpr int KillFirst(PackMob const* mobs, size_t count, int skip = -1, int skipToo = -1)
{
    int best = -1;
    for (size_t i = 0; i < count; ++i)
    {
        if (static_cast<int>(i) == skip || static_cast<int>(i) == skipToo)
            continue;
        if (best < 0 || Danger(mobs[i]) > Danger(mobs[best]) ||
            (Danger(mobs[i]) == Danger(mobs[best]) && mobs[i].maxHealth < mobs[best].maxHealth))
            best = static_cast<int>(i);
    }
    return best;
}

// Groups only spend crowd control on packs of three or more.
constexpr size_t MinPackForCc = 3;

// The toughest mob that the given crowd control holds and that is not the kill target, or -1.
constexpr int CcTarget(PackMob const* mobs, size_t count, Cc cc, int skull)
{
    if (cc == Cc::None || count < MinPackForCc)
        return -1;
    int best = -1;
    for (size_t i = 0; i < count; ++i)
    {
        if (static_cast<int>(i) == skull || mobs[i].boss || mobs[i].ccImmune || !CanCc(cc, mobs[i].type))
            continue;
        if (best < 0 || mobs[i].maxHealth > mobs[best].maxHealth)
            best = static_cast<int>(i);
    }
    return best;
}
}  // namespace DungeonLead

#endif
