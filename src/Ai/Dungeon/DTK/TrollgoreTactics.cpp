/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Trollgore bot tactics: step away from an invader corpse before it explodes.

#include "DTKActions.h"
#include "DTKPolicy.h"
#include "DTKTriggers.h"
#include "Playerbots.h"

bool CorpseExplodeTrigger::IsActive()
{
    if (!AI_VALUE2(Unit*, "find target", "trollgore"))
        return false;

    GuidVector corpses = AI_VALUE(GuidVector, "nearest corpses");
    for (ObjectGuid const& guid : corpses)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NPC_DRAKKARI_INVADER && DrakTharon::InCorpseBlast(bot->GetExactDist2d(unit)))
            return true;
    }
    return false;
}

bool CorpseExplodeSpreadAction::Execute(Event /*event*/)
{
    if (!AI_VALUE2(Unit*, "find target", "trollgore"))
        return false;

    GuidVector corpses = AI_VALUE(GuidVector, "nearest corpses");
    for (ObjectGuid const& guid : corpses)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit->GetEntry() != NPC_DRAKKARI_INVADER)
            continue;

        float step = DrakTharon::CorpseStepAway(bot->GetExactDist2d(unit));
        if (step > 0.0f)
            return MoveAway(unit, step);
    }
    return false;
}
