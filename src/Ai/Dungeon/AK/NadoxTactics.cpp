// Elder Nadox bot tactics: kill the Ahn'kahar Guardian while it shields him.

#include "AKActions.h"
#include "AKMultipliers.h"
#include "AKPolicy.h"
#include "AKTriggers.h"
#include "ChooseTargetActions.h"
#include "Playerbots.h"

bool NadoxGuardianTrigger::IsActive()
{
    if (!AhnKahet::AttacksNadoxGuardian(PlayerbotAI::IsHeal(bot)))
        return false;

    return AI_VALUE2(Unit*, "find target", "elder nadox") && AI_VALUE2(Unit*, "find target", "ahn'kahar guardian");
}

bool AttackNadoxGuardianAction::Execute(Event /*event*/)
{
    Unit* target = AI_VALUE2(Unit*, "find target", "ahn'kahar guardian");
    if (!target || AI_VALUE(Unit*, "current target") == target)
        return false;
    return Attack(target);
}

float ElderNadoxMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<DpsAssistAction*>(action) || !AI_VALUE2(Unit*, "find target", "elder nadox"))
        return 1.0f;

    bool guardianUp = AI_VALUE2(Unit*, "find target", "ahn'kahar guardian") != nullptr;
    return AhnKahet::NadoxAllows(guardianUp, true) ? 1.0f : 0.0f;
}
