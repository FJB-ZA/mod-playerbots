// Time-Lost Controller bot tactics: skull its Charming Totem so the group kills it before it charms anyone.

#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "SethActions.h"
#include "SethShared.h"
#include "SethTriggers.h"

using namespace SethShared;

bool TimeLostControllerDropsCharmingTotemTrigger::IsActive()
{
    return EncounterHelpers::IsMechanicTrackerBot(bot, SETH_MAP_ID) &&
           AI_VALUE2(Unit*, "find target", "time-lost controller");
}

bool TimeLostControllerMarkCharmingTotemWithSkullAction::Execute(Event /*event*/)
{
    constexpr float searchRadius = 40.0f;
    Unit* totem = bot->FindNearestCreature(Id(SethNpcs::NPC_CHARMING_TOTEM), searchRadius);
    return totem && EncounterHelpers::MarkTargetWithSkull(bot, totem);
}
