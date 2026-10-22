// Drakos the Interrogator bot tactics: everyone but the tank steps away from parked Unstable Spheres.

#include "OCActions.h"
#include "OCPolicy.h"
#include "OCTriggers.h"
#include "Playerbots.h"

bool DrakosUnstableSphereTrigger::IsActive()
{
    if (!Oculus::AvoidsSpheres(PlayerbotAI::IsTank(bot)))
        return false;

    GuidVector targets = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NPC_UNSTABLE_SPHERE)
            return true;
    }
    return false;
}

bool AvoidUnstableSphereAction::Execute(Event /*event*/)
{
    if (!AI_VALUE2(Unit*, "find target", "drakos the interrogator"))
        return false;

    // A moving sphere is still wandering to its spot; only parked ones pulse
    Unit* closestSphere = nullptr;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NPC_UNSTABLE_SPHERE && !unit->isMoving() &&
            (!closestSphere || bot->GetExactDist2d(unit) < bot->GetExactDist2d(closestSphere)))
            closestSphere = unit;
    }

    if (!closestSphere)
        return false;

    float step = Oculus::SphereStepAway(bot->GetExactDist2d(closestSphere));
    return step != 0.0f && MoveAway(closestSphere, step);
}
