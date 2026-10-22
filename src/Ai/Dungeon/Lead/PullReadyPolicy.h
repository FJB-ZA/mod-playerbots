/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Whether a bot tank may pull now, and if not, who it is waiting for and why.

#ifndef _PLAYERBOTS_PULL_READY_POLICY_H
#define _PLAYERBOTS_PULL_READY_POLICY_H

#include <cstddef>
#include <cstdint>

#include "LeadTierPolicy.h"

namespace DungeonLead
{
enum class Wait : uint8_t
{
    None,
    Dead,
    Behind,
    Health,
    Drinking,
    HealerMana,
    CalledOom
};

struct Member
{
    bool alive = true;
    float distance = 0.0f;  // from the tank
    float healthPct = 100.0f;
    bool usesMana = false;
    float manaPct = 100.0f;
    bool healer = false;
    bool resting = false;  // sitting with food or drink
    bool calledOom = false;
    bool bot = true;
    uint32_t waitedMs = 0;  // how long the tank has already waited on this member
};

// A called oom is cleared by the player's real mana, never by a timer.
constexpr bool OomCleared(Tier tier, float manaPct, bool resting)
{
    return manaPct >= CalledOomClearPct(tier) && !resting;
}

constexpr Wait WaitFor(Member const& member, Tier tier)
{
    if (!member.alive)
        return Wait::Dead;
    if (member.calledOom && !OomCleared(tier, member.manaPct, member.resting))
        return Wait::CalledOom;
    bool const givenUp = member.bot && member.waitedMs >= MaxBotWaitMs;
    if (GatherRadius(tier) > 0.0f && member.distance > GatherRadius(tier) && !givenUp)
        return Wait::Behind;
    if (givenUp)
        return Wait::None;
    if (member.resting && (member.healer || WaitsForDrinkers(tier)))
        return Wait::Drinking;
    if (member.healer && member.usesMana && member.manaPct < HealerManaToPull(tier))
        return Wait::HealerMana;
    if (member.healthPct < HealthToPull(tier))
        return Wait::Health;
    return Wait::None;
}

// Index of the first member holding the pull, or -1 when the group is ready.
constexpr int Blocker(Member const* members, size_t count, Tier tier)
{
    for (size_t i = 0; i < count; ++i)
        if (WaitFor(members[i], tier) != Wait::None)
            return static_cast<int>(i);
    return -1;
}
}  // namespace DungeonLead

#endif
