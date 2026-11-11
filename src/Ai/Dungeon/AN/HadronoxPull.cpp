/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HadronoxPull.h"

#include "ANActions.h"
#include "ANPolicy.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Group.h"
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

bool AzjolNerub::SkipHadronoxPull(Creature* mob)
{
    if (mob->GetMapId() != AzjolMap)
        return false;
    return HadronoxRunner(mob->GetEntry()) || (mob->GetEntry() == Hadronox && DoorsOpen(mob));
}

bool AzjolNerub::HadronoxTraffic(Player* bot, float x, float y, float z)
{
    if (bot->GetMapId() != AzjolMap || !InDoorTraffic(x, y, z))
        return false;
    InstanceScript* instance = bot->GetInstanceScript();
    return instance && instance->GetBossState(HadronoxEncounter) == IN_PROGRESS &&
           DoorsOpen(instance->GetCreature(HadronoxEncounter));
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

bool HadronoxTrafficTrigger::IsActive()
{
    return bot->IsAlive() && bot->IsInCombat() &&
           (PlayerbotAI::IsTank(bot) || PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsHeal(bot)) &&
           AzjolNerub::HadronoxTraffic(bot, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
}

bool HadronoxClearLaneAction::Execute(Event /*event*/)
{
    Unit* tank = AI_VALUE(Unit*, "group main tank");
    if (!tank || !tank->IsAlive() || tank->GetMap() != bot->GetMap() || !botAI->CanMove())
        return false;
    bool const lead = tank == bot;
    if (!lead && bot->IsNonMeleeSpellCast(false, false, true))
        return false;

    // Keep a tank's retreat in healing range. The back line follows to a point
    // from which it can heal or cast without standing in the pack.
    Unit* partner = lead ? nullptr : tank;
    if (lead && bot->GetGroup())
        for (GroupReference* ref = bot->GetGroup()->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member != bot && member->IsAlive() && member->GetMap() == bot->GetMap() &&
                PlayerbotAI::IsHeal(member))
            {
                partner = member;
                break;
            }
        }
    std::vector<Creature*> idle;
    for (ObjectGuid guid : AI_VALUE(GuidVector, "possible targets no los"))
        if (Creature* mob = botAI->GetCreature(guid);
            mob && mob->IsAlive() && !mob->IsInCombat() && mob->IsHostileTo(bot))
            idle.push_back(mob);

    // Ground on the crusher platform south of the two routes. Validate the floor,
    // path and aggro at the actual endpoint after clipping to walls.
    Position best;
    float nearest = 45.0f;
    for (float y : {555.0f, 549.0f, 543.0f, 537.0f})
        for (float x : {514.0f, 522.0f, 530.0f, 538.0f, 546.0f})
        {
            float sx = x, sy = y;
            float z = bot->GetMap()->GetHeight(bot->GetPhaseMask(), x, y, 746.0f, true, 24.0f);
            if (z < 722.0f || z > 742.0f || bot->GetExactDist(x, y, z) >= nearest ||
                !bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                                bot->GetPositionZ(), sx, sy, z) ||
                z < 722.0f || z > 742.0f || AzjolNerub::InDoorTraffic(sx, sy, z))
                continue;
            float const distance = bot->GetExactDist(sx, sy, z);
            if (distance >= nearest)
                continue;
            if (partner)
            {
                float const gap = partner->GetExactDist(sx, sy, z);
                if (gap > (lead ? 35.0f : 28.0f) || (!lead && gap < 12.0f) ||
                    !partner->IsWithinLOS(sx, sy, z + bot->GetCollisionHeight()))
                    continue;
            }
            if (std::any_of(idle.begin(), idle.end(),
                            [&](Creature* mob)
                            {
                                return mob->GetExactDist(sx, sy, z) < mob->GetAggroRange(bot) + 3.0f &&
                                       mob->IsWithinLOS(sx, sy, z + bot->GetCollisionHeight());
                            }))
                continue;
            best.Relocate(sx, sy, z);
            nearest = distance;
        }
    return nearest < 45.0f && MoveTo(bot->GetMapId(), best.GetPositionX(), best.GetPositionY(), best.GetPositionZ(),
                                     false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}
