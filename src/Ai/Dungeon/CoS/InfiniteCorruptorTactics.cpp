/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Infinite Corruptor bot tactics (heroic only): skull him so the whole group burns him before the clock runs out.

#include "CoSActions.h"
#include "CoSPolicy.h"
#include "CoSTriggers.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"

bool InfiniteCorruptorTrigger::IsActive()
{
    if (!bot->GetMap()->IsHeroic())
        return false;

    // His pull skips the zone-wide combat, so only bots already fighting him see him here
    Unit* boss = AI_VALUE2(Unit*, "find target", "infinite corruptor");
    Group* group = bot->GetGroup();
    return boss && group &&
           CullingOfStratholme::CorruptorNeedsSkull(
               true, group->GetTargetIcon(RtiTargetValue::skullIndex) == boss->GetGUID());
}

bool MarkInfiniteCorruptorAction::Execute(Event /*event*/)
{
    // Marking does not take the bot's action for the tick
    EncounterHelpers::MarkTargetWithSkull(bot, AI_VALUE2(Unit*, "find target", "infinite corruptor"));
    return false;
}
