/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// How a bot tank runs a dungeon at each skill tier: marks, crowd control, pull style, pacing and the mana it waits for.

#ifndef _PLAYERBOTS_LEAD_TIER_POLICY_H
#define _PLAYERBOTS_LEAD_TIER_POLICY_H

#include <cstdint>

namespace DungeonLead
{
enum class Tier : uint8_t
{
    Sloppy,
    Average,
    Good
};

enum class PullStyle : uint8_t
{
    Charge,  // runs straight in, no stop
    Ranged,  // stops the group in sight of the pack and pulls from there
    Corner   // stops the group out of sight and pulls the pack around the corner
};

// Takes the bot's hidden arena skill (0-100, most bots near 50), so a bot plays at one level everywhere.
constexpr Tier TierFor(uint32_t skill) { return skill >= 60 ? Tier::Good : skill >= 30 ? Tier::Average : Tier::Sloppy; }

constexpr float HealerManaToPull(Tier tier)
{
    // Good stops at 85 because bots only sit down to drink below 85% mana.
    return tier == Tier::Good ? 85.0f : tier == Tier::Average ? 75.0f : 60.0f;
}

constexpr float HealthToPull(Tier tier) { return tier == Tier::Good ? 85.0f : tier == Tier::Average ? 75.0f : 50.0f; }

// A player who called oom is waited for until this much mana, whatever else the tier skips.
constexpr float CalledOomClearPct(Tier tier)
{
    return tier == Tier::Good ? 90.0f : tier == Tier::Average ? 85.0f : 80.0f;
}

constexpr bool MarksTargets(Tier tier) { return tier != Tier::Sloppy; }

constexpr bool RequestsCc(Tier tier) { return tier == Tier::Good; }

// Sloppy tanks pull while the DPS are still sitting.
constexpr bool WaitsForDrinkers(Tier tier) { return tier != Tier::Sloppy; }

constexpr PullStyle StyleFor(Tier tier)
{
    return tier == Tier::Good ? PullStyle::Corner : tier == Tier::Average ? PullStyle::Ranged : PullStyle::Charge;
}

// Percent chance that a pull drags the next pack along.
constexpr uint32_t OverpullChance(Tier tier) { return tier == Tier::Good ? 0 : tier == Tier::Average ? 5 : 25; }

// Pause after a fight before the next pull, for loot and a breather.
constexpr uint32_t RestAfterFightMs(Tier tier)
{
    return tier == Tier::Good ? 5000 : tier == Tier::Average ? 3000 : 500;
}

// How close everyone must be before the pull; 0 means the tank does not wait for stragglers.
constexpr float GatherRadius(Tier tier) { return tier == Tier::Good ? 15.0f : tier == Tier::Average ? 25.0f : 0.0f; }

// A bot that never reaches the mark (no drinks, a bad spec) stops holding the run after this long.
constexpr uint32_t MaxBotWaitMs = 90000;
}  // namespace DungeonLead

#endif
