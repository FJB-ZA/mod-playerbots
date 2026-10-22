/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Forgotten One bot tactics: get clear of a Shadow Crash, and ranged spread so crashes don't overlap.

#include "AKActions.h"
#include "AKMultipliers.h"
#include "AKPolicy.h"
#include "AKTriggers.h"
#include "MovementActions.h"
#include "Playerbots.h"

bool ShadowCrashTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "forgotten one") &&
           AhnKahet::SpreadsForShadowCrash(PlayerbotAI::IsMelee(bot));
}

bool AvoidShadowCrashAction::Execute(Event /*event*/)
{
    // Handles one at a time: they should be pulled singly anyway
    Unit* caster = AI_VALUE2(Unit*, "find target", "forgotten one");
    if (!caster)
        return false;

    // Their only cast with a cast time is Shadow Crash
    if (caster->HasUnitState(UNIT_STATE_CASTING))
    {
        // The crash lands on the victim's spot: flee the spot, which also works when the victim is this bot
        Unit* victim = botAI->GetUnit(caster->GetTarget());
        if (victim && AhnKahet::ShadowCrashStepAway(bot->GetExactDist2d(victim)) > 0.0f)
            return FleePosition(victim->GetPosition(), AhnKahet::ShadowCrashClearance, 1000U);
    }

    if (!AhnKahet::SpreadsForShadowCrash(PlayerbotAI::IsMelee(bot)))
        return false;

    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        Unit* unit = botAI->GetUnit(member);
        if (!unit || member == bot->GetGUID())
            continue;

        float step = AhnKahet::ShadowCrashStepAway(bot->GetExactDist2d(unit));
        if (step > 0.0f)
            return MoveAway(unit, step);
    }
    return false;
}

float ForgottenOneMultiplier::GetValue(Action* action)
{
    if (!AI_VALUE2(Unit*, "find target", "forgotten one"))
        return 1.0f;

    bool movement = dynamic_cast<MovementAction*>(action) != nullptr;
    return AhnKahet::ForgottenOneAllows(bot->isMoving(), movement) ? 1.0f : 0.0f;
}
