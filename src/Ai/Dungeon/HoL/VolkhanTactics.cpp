// Volkhan bot tactics: dps stay on Volkhan and hold area damage so the brittle golems don't shatter into the group.

#include "ChooseTargetActions.h"
#include "HoLActions.h"
#include "HoLMultipliers.h"
#include "HoLPolicy.h"
#include "HoLTriggers.h"
#include "Playerbots.h"

bool VolkhanTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "volkhan") &&
           HallsOfLightning::AttacksVolkhan(PlayerbotAI::IsTank(bot), PlayerbotAI::IsHeal(bot));
}

bool VolkhanTargetAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "volkhan");
    if (!boss || AI_VALUE(Unit*, "current target") == boss)
        return false;

    return Attack(boss);
}

float VolkhanMultiplier::GetValue(Action* action)
{
    if (!AI_VALUE2(Unit*, "find target", "volkhan"))
        return 1.0f;

    bool tank = PlayerbotAI::IsTank(bot);
    bool healer = PlayerbotAI::IsHeal(bot);
    bool dpsAssist = dynamic_cast<DpsAssistAction*>(action) != nullptr;
    bool aoe = action->getThreatType() == Action::ActionThreatType::Aoe;
    return HallsOfLightning::VolkhanAllows(tank, healer, dpsAssist, aoe) ? 1.0f : 0.0f;
}
