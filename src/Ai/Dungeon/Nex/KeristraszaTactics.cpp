/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Keristrasza bot tactics: hop to shed Intense Cold stacks and stand on her flank.

#include "NexActions.h"
#include "NexPolicy.h"
#include "NexTriggers.h"
#include "Playerbots.h"

bool IntenseColdTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "keristrasza") &&
           botAI->GetAura("intense cold", bot, false, false, Nexus::IntenseColdStackLimit(bot->GetMap()->IsHeroic()));
}

// Healers count: left in front or behind they stand in the breath or the tail
bool KeristraszaPositioningTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "keristrasza") &&
           Nexus::StandsOnFlank(PlayerbotAI::IsTank(bot), PlayerbotAI::IsRangedDps(bot));
}

bool IntenseColdJumpAction::Execute(Event /*event*/)
{
    // A jump in place needs a tiny height offset; any relocation removes the move-interrupted stacks
    return JumpTo(bot->GetMap()->GetId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ() + 0.01f);
}
