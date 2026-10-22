// Eadric the Pure bot tactics: turn away from him while he casts Radiance.

#include "Playerbots.h"
#include "TOCActions.h"
#include "TOCPolicy.h"
#include "TOCTriggers.h"

bool ToCEadricTrigger::IsActive() { return AI_VALUE2(Unit*, "find target", "eadric the pure"); }

bool ToCEadricAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "eadric the pure");
    if (!boss)
        return false;

    // 66935 is the cast on both modes; only the blinding hit it triggers has a heroic id
    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_RADIANCE))
        return false;

    bot->SetFacingTo(TrialOfTheChampion::RadianceFacing(bot->GetAngle(boss)));
    return true;
}
