/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Argent soldiers bot tactics: keep off the three groups while they wait, so they come one at a time, and the skull on
// a Priestess once they fight (her Mind Control).

#include <list>

#include "ArgentSoldiersValue.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "TOCActions.h"
#include "TOCPolicy.h"
#include "TOCTriggers.h"

namespace Toc = TrialOfTheChampion;

ToCSoldiers ArgentSoldiersValue::Calculate()
{
    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance || instance->GetBossState(0) != DONE || instance->GetBossState(1) == DONE)
        return {};
    float priestessDistance = 1000.0f;
    Player* master = botAI->GetMaster();
    if (master && (master == bot || !master->IsInWorld() || master->GetMap() != bot->GetMap()))
        master = nullptr;

    ToCSoldiers soldiers;
    static std::vector<uint32> const entries{NPC_ARGENT_LIGHTWIELDER, NPC_ARGENT_MONK, NPC_PRIESTESS};
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, entries, 100.0f);
    for (Creature* soldier : found)
    {
        if (!soldier->IsAlive())
            continue;
        if (soldier->IsInCombat())
        {
            soldiers.fighting = true;
            float const distance = bot->GetExactDist2d(soldier);
            if (soldier->GetEntry() == NPC_PRIESTESS && distance < priestessDistance)
            {
                soldiers.priestess = soldier->GetGUID();
                priestessDistance = distance;
            }
            continue;
        }
        float const distance = bot->GetExactDist2d(soldier);
        if (distance < soldiers.botDistance)
        {
            soldiers.botDistance = distance;
            soldiers.nearest = soldier->GetGUID();
        }
        if (master)
            soldiers.masterDistance = std::min(soldiers.masterDistance, master->GetExactDist2d(soldier));
    }
    return soldiers;
}

bool ToCSoldiersWaitingTrigger::IsActive()
{
    if (bot->IsInCombat() || bot->GetVehicle() || !bot->IsAlive())
        return false;
    ToCSoldiers const soldiers = AI_VALUE(ToCSoldiers, "toc argent soldiers");
    return soldiers.nearest && Toc::KeepsOffSoldiers(soldiers.fighting, soldiers.botDistance, soldiers.masterDistance);
}

bool ToCSoldiersKeepOffAction::Execute(Event /*event*/)
{
    ToCSoldiers const soldiers = AI_VALUE(ToCSoldiers, "toc argent soldiers");
    if (!soldiers.nearest || soldiers.fighting || soldiers.botDistance >= Toc::SoldierKeepOff)
        return false;  // far enough back: the joust multiplier holds the follow that would walk it in again

    Unit* nearest = botAI->GetUnit(soldiers.nearest);
    if (!nearest || !nearest->IsAlive() || nearest->IsInCombat())
        return false;
    float x, y;
    Toc::SoldierWaitSpot(nearest->GetPositionX(), nearest->GetPositionY(), x, y);
    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);
    return MoveTo(bot->GetMapId(), x, y, z, false, false, false, true, MovementPriority::MOVEMENT_NORMAL) ||
           bot->isMoving();
}

bool ToCSoldiersMarkTrigger::IsActive()
{
    if (!bot->IsInCombat() || !bot->GetGroup() || !PlayerbotAI::IsTank(bot))
        return false;
    Unit* skull = botAI->GetUnit(bot->GetGroup()->GetTargetIcon(RtiTargetValue::skullIndex));
    bool const onPriestess = skull && skull->IsAlive() && skull->IsInCombat() && skull->GetEntry() == NPC_PRIESTESS;
    return Toc::MarksPriestess(true, !AI_VALUE(ToCSoldiers, "toc argent soldiers").priestess.IsEmpty(), onPriestess);
}

bool ToCSoldiersMarkAction::Execute(Event /*event*/)
{
    Unit* priestess = botAI->GetUnit(AI_VALUE(ToCSoldiers, "toc argent soldiers").priestess);
    Group* group = bot->GetGroup();
    if (!priestess || !priestess->IsAlive() || !priestess->IsInCombat() || !group)
        return false;
    group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), priestess->GetGUID());
    return true;
}
