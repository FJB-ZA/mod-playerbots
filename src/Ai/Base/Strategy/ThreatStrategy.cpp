/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ThreatStrategy.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "Map.h"
#include "Playerbots.h"
#include "Timer.h"

float ThreatMultiplier::GetValue(Action* action)
{
    if (AI_VALUE(bool, "neglect threat"))
    {
        return 1.0f;
    }

    // The swing that starts auto attack makes threat too, though it is no spell
    bool const melee = dynamic_cast<MeleeAction*>(action) != nullptr;
    if (!action || (action->getThreatType() == Action::ActionThreatType::None && !melee))
        return 1.0f;

    if (!AI_VALUE(bool, "group"))
        return 1.0f;

    // Hold off a mob until the tank has taken it and its first swings put some threat between them
    // (ThreatHoldPolicy.h)
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive() && !PlayerbotAI::IsTank(bot))
    {
        Unit* tank = AI_VALUE(Unit*, "main tank");
        bool const tankInFight = tank && tank != bot && tank->IsAlive() && tank->IsInCombat() &&
                                 tank->GetMap() == bot->GetMap() && tank->GetExactDist2d(bot) < 60.0f;
        Unit* victim = target->GetVictim();
        Player* holder = victim ? victim->ToPlayer() : nullptr;
        BotThreat::MobOn on = BotThreat::MobOn::Other;
        if (!tankInFight)
            on = BotThreat::MobOn::NoTankInFight;
        else if (victim == bot)
            on = BotThreat::MobOn::Self;
        else if (holder && PlayerbotAI::IsTank(holder) && holder->IsInSameGroupWith(bot))
            on = BotThreat::MobOn::Tank;
        if (!tankHold.MayOpen(target->GetGUID().GetRawValue(), on, getMSTime()))
            return 0.0f;
    }

    if (melee)
        return 1.0f;

    if (action->getThreatType() == Action::ActionThreatType::Aoe)
    {
        uint8 threat = AI_VALUE2(uint8, "threat", "aoe");
        if (threat >= 50)
            return 0.0f;
    }

    uint8 threat = AI_VALUE2(uint8, "threat", "current target");
    if (threat >= 80)
        return 0.0f;

    return 1.0f;
}

void ThreatStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new ThreatMultiplier(botAI));
}

float FocusMultiplier::GetValue(Action* action)
{
    if (!action)
    {
        return 1.0f;
    }
    if (action->getThreatType() == Action::ActionThreatType::Aoe && !dynamic_cast<CastHealingSpellAction*>(action))
    {
        return 0.0f;
    }
    if (dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
    {
        return 0.0f;
    }
    return 1.0f;
}

void FocusStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FocusMultiplier(botAI));
}
