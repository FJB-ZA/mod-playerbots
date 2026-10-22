// Infinite Corruptor bot tactics (heroic only): skull him so the whole group burns him before the clock runs out.

#include "CoSActions.h"
#include "CoSPolicy.h"
#include "CoSTriggers.h"
#include "Group.h"
#include "Playerbots.h"

bool InfiniteCorruptorTrigger::IsActive()
{
    if (!bot->GetMap()->IsHeroic())
        return false;

    // His pull skips the zone-wide combat, so only bots already fighting him see him here
    Unit* boss = AI_VALUE2(Unit*, "find target", "infinite corruptor");
    Group* group = bot->GetGroup();
    return boss && group && CullingOfStratholme::CorruptorNeedsSkull(true, group->GetTargetIcon(7) == boss->GetGUID());
}

bool MarkInfiniteCorruptorAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "infinite corruptor");
    Group* group = bot->GetGroup();
    if (boss && group && group->GetTargetIcon(7) != boss->GetGUID())
        group->SetTargetIcon(7, bot->GetGUID(), boss->GetGUID());
    return false;
}
