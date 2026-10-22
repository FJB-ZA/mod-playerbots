// Darkweaver Syth bot tactics: skull his summoned elementals one at a time until the burn.

#include <array>

#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "SethActions.h"
#include "SethPolicy.h"
#include "SethShared.h"
#include "SethTriggers.h"

using namespace SethShared;

bool DarkweaverSythBossSummonsElementalsTrigger::IsActive()
{
    bool const tracker = EncounterHelpers::IsMechanicTrackerBot(bot, SETH_MAP_ID);
    if (!tracker)
        return false;

    Unit* syth = AI_VALUE2(Unit*, "find target", "darkweaver syth");
    return syth &&
           SethekkHalls::MarksElementals(tracker, syth->GetHealthPct() <= EncounterHelpers::BOSS_BURN_HEALTH_PCT);
}

bool DarkweaverSythMarkElementalsWithSkullAction::Execute(Event /*event*/)
{
    static constexpr std::array elementals = {
        "syth frost elemental",
        "syth shadow elemental",
        "syth arcane elemental",
        "syth fire elemental",
    };

    for (auto const& name : elementals)
    {
        if (Unit* elemental = AI_VALUE2(Unit*, "find target", name))
            return EncounterHelpers::MarkTargetWithSkull(bot, elemental);
    }

    return false;
}
