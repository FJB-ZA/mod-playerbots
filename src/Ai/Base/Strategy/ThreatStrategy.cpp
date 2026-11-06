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

    // Hold off a mob the tank has only just taken, until its first swings put some threat between them
    // (ThreatHoldPolicy.h)
    if (Unit* target = AI_VALUE(Unit*, "current target"))
    {
        Player* holder = target->IsAlive() && target->GetVictim() ? target->GetVictim()->ToPlayer() : nullptr;
        bool const tankHolds =
            holder && holder != bot && PlayerbotAI::IsTank(holder) && holder->IsInSameGroupWith(bot);
        if (!tankHold.MayOpen(target->GetGUID().GetRawValue(), tankHolds, getMSTime()))
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
