/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Ormorok the Tree-Shaper bot tactics: everyone stacks under him, where the crystal spikes can't reach.

#include "MovementActions.h"
#include "NexActions.h"
#include "NexMultipliers.h"
#include "NexPolicy.h"
#include "NexTriggers.h"
#include "Playerbots.h"

bool OrmorokSpikesTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot) || !AI_VALUE2(Unit*, "find target", "ormorok the tree-shaper"))
        return false;

    GuidVector objects = AI_VALUE(GuidVector, "closest game objects");
    for (ObjectGuid const& guid : objects)
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (go && go->GetEntry() == GO_CRYSTAL_SPIKE)
            return true;
    }
    return false;
}

bool OrmorokStackTrigger::IsActive()
{
    return !PlayerbotAI::IsTank(bot) && AI_VALUE2(Unit*, "find target", "ormorok the tree-shaper");
}

bool DodgeSpikesAction::isUseful()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ormorok the tree-shaper");
    return boss && Nexus::ShouldStackUnderOrmorok(bot->GetExactDist2d(boss));
}

bool DodgeSpikesAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ormorok the tree-shaper");
    if (!boss)
        return false;

    return Move(bot->GetAngle(boss), Nexus::StackStep(bot->GetExactDist2d(boss)));
}

float OrmorokMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ormorok the tree-shaper");
    if (!boss)
        return 1.0f;

    Nexus::OrmorokAction shape;
    shape.flee = dynamic_cast<FleeAction*>(action) != nullptr;
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.dodgeSpikes = dynamic_cast<DodgeSpikesAction*>(action) != nullptr;
    // Lets the tank close in and face him first
    bool tankEngaged =
        PlayerbotAI::IsTank(bot) && bot->IsWithinMeleeRange(boss) && AI_VALUE2(bool, "facing", "current target");
    return Nexus::OrmorokAllows(shape, tankEngaged) ? 1.0f : 0.0f;
}
