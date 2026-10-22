/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Watchkeeper Gargolmar bot tactics: skull the Hellfire Watchers so they die before they can heal him.

#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "RampActions.h"
#include "RampShared.h"
#include "RampTriggers.h"

bool GargolmarHellfireWatchersAreActiveTrigger::IsActive()
{
    return EncounterHelpers::IsMechanicTrackerBot(bot, RampShared::RAMP_MAP_ID) &&
           AI_VALUE2(Unit*, "find target", "hellfire watcher");
}

bool GargolmarMarkHellfireWatchersAction::Execute(Event /*event*/)
{
    Unit* watcher = AI_VALUE2(Unit*, "find target", "hellfire watcher");
    if (!watcher)
        return false;

    return EncounterHelpers::MarkTargetWithSkull(bot, watcher);
}
