/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Skarvald the Constructor and Dalronn the Controller bot tactics: dps kill Dalronn while the tank holds Skarvald.

#include "ChooseTargetActions.h"
#include "Playerbots.h"
#include "UKActions.h"
#include "UKMultipliers.h"
#include "UKPolicy.h"
#include "UKTriggers.h"

bool DalronnDpsTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "dalronn the controller");
    // Healers count as non-tanks here but keep healing anyway
    return boss && boss->isTargetableForAttack() && UtgardeKeep::AttacksDalronn(PlayerbotAI::IsTank(bot));
}

bool AttackDalronnAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "dalronn the controller");
    if (!boss || AI_VALUE(Unit*, "current target") == boss)
        return false;
    return Attack(boss);
}

float SkarvaldAndDalronnMultiplier::GetValue(Action* action)
{
    // Once Dalronn is down the normal dps strategy takes over
    Unit* dalronn = AI_VALUE2(Unit*, "find target", "dalronn the controller");
    if (!dalronn)
        return 1.0f;

    bool assist = dynamic_cast<DpsAssistAction*>(action) != nullptr;
    return UtgardeKeep::DalronnAllows(dalronn->isTargetableForAttack(), assist) ? 1.0f : 0.0f;
}
