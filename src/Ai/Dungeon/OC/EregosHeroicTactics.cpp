// Ley-Guardian Eregos heroic bot tactics: drakes hold fire and heals while Planar Shift makes him immune.

#include "OCActions.h"
#include "OCMultipliers.h"
#include "OCPolicy.h"
#include "OCTriggers.h"
#include "Playerbots.h"

float EregosMultiplier::GetValue(Action* action)
{
    if (!bot->GetMap()->IsHeroic())
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "ley-guardian eregos");
    if (!boss)
        return 1.0f;

    bool allowed = Oculus::EregosAllows(boss->HasAura(SPELL_PLANAR_SHIFT), dynamic_cast<OccDrakeAttackAction*>(action));
    return allowed ? 1.0f : 0.0f;
}
