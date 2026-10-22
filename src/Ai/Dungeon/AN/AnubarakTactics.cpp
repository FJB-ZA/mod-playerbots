/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Anub'arak bot tactics: run through him to get out of the Pound cone.

#include "ANActions.h"
#include "ANPolicy.h"
#include "ANTriggers.h"
#include "Playerbots.h"

bool AnubarakPoundTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'arak");
    return boss && boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_POUND);
}

bool AnubarakDodgePoundAction::isUseful()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'arak");
    return boss && AzjolNerub::DodgesPound(AI_VALUE2(bool, "behind", "current target"),
                                           bot->GetExactDist2d(boss->GetPosition()));
}

bool AnubarakDodgePoundAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anub'arak");
    if (!boss)
        return false;

    return Move(bot->GetAngle(boss), AzjolNerub::PoundDodgeStep(bot->GetExactDist2d(boss->GetPosition())));
}
