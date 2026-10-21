// Pure rules for living battlegrounds: per-match team tactics tiers and the gear mix of the team facing real players.

#ifndef _PLAYERBOTS_LIVING_BATTLEGROUND_POLICY_H
#define _PLAYERBOTS_LIVING_BATTLEGROUND_POLICY_H

#include <algorithm>
#include <cstdint>

#include "LivingArenaPolicy.h"
#include "LivingWorldPolicy.h"

namespace LivingBattleground
{
// Values match BGTacticsTier in BattleGroundTactics.h
enum class TeamTier : uint8_t
{
    Basic,        // competent but uncoordinated
    Coordinated,  // plays as a team most of the time
    Perfect       // the full playbook, no slips
};

// A team without real players: 5 in 8 basic, 2 in 8 coordinated, 1 in 8 perfect.
constexpr TeamTier EnemyTier(uint32_t roll)
{
    roll %= 8;
    return roll < 5 ? TeamTier::Basic : roll < 7 ? TeamTier::Coordinated : TeamTier::Perfect;
}

// A real player's team plays about as well as the enemy on average, so the player decides the match;
// it only reaches perfect when that player leads the raid.
constexpr TeamTier AllyTier(uint32_t roll, bool playerLeads)
{
    roll %= 8;
    if (roll < 4)
        return TeamTier::Basic;
    return roll == 7 && playerLeads ? TeamTier::Perfect : TeamTier::Coordinated;
}

// Percent chance a bot follows a team play (escort, split, rotation); basic teams never do.
constexpr uint32_t CoordinationChance(TeamTier tier, uint32_t skill)
{
    switch (tier)
    {
        case TeamTier::Perfect:
            return 100;
        case TeamTier::Coordinated:
            return LivingArena::Adherence(skill);
        default:
            return 0;
    }
}

// Percent chance a bot wanders off the plan (chases a random enemy, picks a far node).
constexpr uint32_t SlipChance(TeamTier tier, uint32_t base)
{
    return tier == TeamTier::Perfect ? 0 : tier == TeamTier::Coordinated ? base / 2 : base;
}

// Without living battlegrounds there are no tiers: every bot makes the team plays, as upstream did.
constexpr uint32_t TeamPlayChance(bool tiered, TeamTier tier, uint32_t skill)
{
    return tiered ? CoordinationChance(tier, skill) : 100;
}

// Without living battlegrounds a bot wanders off at the upstream odds.
constexpr uint32_t WanderChance(bool tiered, TeamTier tier, uint32_t base)
{
    return tiered ? SlipChance(tier, base) : base;
}

// Odds that a bot escorts its own carrier while both flags are out; upstream sent one in five.
constexpr uint32_t EscortChance(bool tiered) { return tiered ? 40 : 20; }

// One queue window in four sends a team that is nearly all below the players' gear.
constexpr bool GreenTeam(uint32_t queue, uint64_t gameTimeSeconds)
{
    return LivingWorld::Mix(queue * 0x85ebca6bU ^ static_cast<uint32_t>(gameTimeSeconds / 300)) % 4 == 0;
}

// Gear for the team facing real players: mostly greener than them, a few at their level, rarely above.
constexpr uint32_t EnemyGearChance(uint32_t botGearScore, uint32_t playerGearScore, bool greenTeam,
                                   LivingWorld::Temperament temperament)
{
    if (!playerGearScore)
        return 100;
    uint64_t const ratio = uint64_t(botGearScore) * 100 / playerGearScore;
    if (temperament == LivingWorld::Temperament::Twink)
        return greenTeam ? 5 : 30;
    if (ratio <= 85)
        return 90;
    if (ratio <= 100)
        return greenTeam ? 10 : 45;
    if (ratio <= 105)
        return greenTeam ? 5 : 20;
    return greenTeam ? 0 : 5;
}
}  // namespace LivingBattleground

#endif
