/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Erekem bot tactics: dps focus Erekem and hold area damage, so his guards live and don't enrage him.

#include "ChooseTargetActions.h"
#include "Playerbots.h"
#include "VHActions.h"
#include "VHMultipliers.h"
#include "VHPolicy.h"
#include "VHTriggers.h"

bool ErekemTargetTrigger::IsActive() { return PlayerbotAI::IsDps(bot) && AI_VALUE2(Unit*, "find target", "erekem"); }

bool AttackErekemAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "erekem");
    if (!boss || AI_VALUE(Unit*, "current target") == boss)
        return false;
    return Attack(boss);
}

float ErekemMultiplier::GetValue(Action* action)
{
    if (!AI_VALUE2(Unit*, "find target", "erekem"))
        return 1.0f;

    bool assist = dynamic_cast<DpsAssistAction*>(action) != nullptr;
    bool aoe = action->getThreatType() == Action::ActionThreatType::Aoe;
    return VioletHold::ErekemAllows(PlayerbotAI::IsDps(bot), assist, aoe) ? 1.0f : 0.0f;
}
