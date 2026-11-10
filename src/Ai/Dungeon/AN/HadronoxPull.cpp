/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HadronoxPull.h"

#include "ANPolicy.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "InstanceScript.h"
#include "Player.h"

namespace
{
constexpr uint32 AzjolMap = 601;
constexpr uint32 Hadronox = 28921;
constexpr uint32 Crusher = 28922;
constexpr uint32 HadronoxEncounter = 1;

// The core exposes the door state through its Hadronox Denied achievement query.
bool DoorsOpen(Creature* boss)
{
    return boss && boss->IsAlive() && boss->AI() && boss->AI()->GetData(Hadronox) != 0;
}
}

bool AzjolNerub::SkipHadronoxPull(Player* tank, Creature* mob)
{
    if (tank->GetMapId() != AzjolMap)
        return false;
    return HadronoxRunner(mob->GetEntry()) || (mob->GetEntry() == Hadronox && DoorsOpen(mob));
}

bool AzjolNerub::WaitForHadronox(Player* tank)
{
    if (tank->GetMapId() != AzjolMap || tank->GetExactDist(530.42f, 560.00f, 733.22f) > 55.0f)
        return false;
    InstanceScript* instance = tank->GetInstanceScript();
    if (!instance || instance->GetBossState(HadronoxEncounter) != IN_PROGRESS)
        return false;
    Creature* boss = instance->GetCreature(HadronoxEncounter);
    // His final climb waits for all three crushers. A living one still needs a pull.
    return DoorsOpen(boss) && !boss->FindNearestCreature(Crusher, 180.0f, true);
}
