// Living battlegrounds: per-team tactics tiers rolled at gate open, and the coordination and slip rolls that read them.

#include "BattleGroundTactics.h"
#include "Battleground.h"
#include "Group.h"
#include "LivingArenaMgr.h"
#include "LivingBattlegroundPolicy.h"
#include "Playerbots.h"

void BGTactics::RollTiers(Battleground* bg, BGStrategyData& data)
{
    bool human[2] = {false, false};
    bool leads[2] = {false, false};
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (!player || LivingArenaMgr::IsBot(player))
            continue;
        TeamId const team = player->GetBgTeamId();
        if (team != TEAM_ALLIANCE && team != TEAM_HORDE)
            continue;
        human[team] = true;
        if (Group* group = player->GetGroup(); group && group->IsLeader(player->GetGUID()))
            leads[team] = true;
    }
    auto rollTier = [&](TeamId team)
    {
        return static_cast<uint8>(human[team] ? LivingBattleground::AllyTier(urand(0, 7), leads[team])
                                              : LivingBattleground::EnemyTier(urand(0, 7)));
    };
    data.allianceTier = rollTier(TEAM_ALLIANCE);
    data.hordeTier = rollTier(TEAM_HORDE);
}

uint8 BGTactics::GetTeamTier(Battleground* bg, TeamId teamId)
{
    std::lock_guard<std::mutex> guard(bgStrategiesLock);
    auto itr = bgStrategies.find(bg->GetInstanceID());
    if (itr == bgStrategies.end())
        return BG_TIER_BASIC;

    return teamId == TEAM_ALLIANCE ? itr->second.allianceTier : itr->second.hordeTier;
}

uint8 BGTactics::teamTier()
{
    Battleground* bg = bot->GetBattleground();
    return bg ? GetTeamTier(bg, bot->GetBgTeamId()) : BG_TIER_BASIC;
}

// Rolled per decision, so a coordinated team still has the odd bot that plays alone
bool BGTactics::coordinated()
{
    auto const tier = static_cast<LivingBattleground::TeamTier>(teamTier());
    return urand(1, 100) <= LivingBattleground::CoordinationChance(tier, LivingArenaMgr::instance().GetSkill(bot));
}

bool BGTactics::slips(uint32 basePercent)
{
    auto const tier = static_cast<LivingBattleground::TeamTier>(teamTier());
    return urand(0, 99) < LivingBattleground::SlipChance(tier, basePercent);
}
