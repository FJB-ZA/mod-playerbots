// Chrono-Lord Epoch bot tactics: ranged stack on him instead of spreading out.

#include "CoSActions.h"
#include "CoSMultipliers.h"
#include "CoSPolicy.h"
#include "CoSTriggers.h"
#include "MovementActions.h"
#include "Playerbots.h"

bool EpochRangedTrigger::IsActive()
{
    return !PlayerbotAI::IsMelee(bot) && AI_VALUE2(Unit*, "find target", "chrono-lord epoch");
}

bool EpochStackAction::isUseful()
{
    return CullingOfStratholme::EpochStacks(bot->getClass() == CLASS_HUNTER,
                                            AI_VALUE2(float, "distance", "current target"));
}

bool EpochStackAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "chrono-lord epoch");
    if (!boss)
        return false;

    return Move(bot->GetAngle(boss), CullingOfStratholme::EpochStep(bot->GetExactDist2d(boss)));
}

float EpochMultiplier::GetValue(Action* action)
{
    if (!AI_VALUE2(Unit*, "find target", "chrono-lord epoch"))
        return 1.0f;

    bool flee = dynamic_cast<FleeAction*>(action) != nullptr;
    return CullingOfStratholme::EpochAllows(bot->getClass() == CLASS_HUNTER, flee) ? 1.0f : 0.0f;
}
