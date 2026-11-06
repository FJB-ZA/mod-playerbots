/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ThreatValues.h"

#include <algorithm>

#include "Playerbots.h"
#include "ThreatManager.h"

uint8 ThreatValue::Calculate()
{
    if (qualifier == "aoe")
    {
        uint8 maxThreat = 0;
        GuidVector attackers = context->GetValue<GuidVector>("attackers")->Get();
        for (ObjectGuid const guid : attackers)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
                continue;

            uint8 threat = Calculate(unit);
            if (!maxThreat || threat > maxThreat)
                maxThreat = threat;
        }

        return maxThreat;
    }

    Unit* target = AI_VALUE(Unit*, qualifier);
    return Calculate(target);
}

uint8 ThreatValue::Calculate(Unit* target)
{
    if (!target)
        return 0;

    if (target->GetGUID().IsPlayer())
        return 0;

    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    // Already on us: holding back only stops us fighting back
    if (target->GetVictim() == bot)
        return 0;

    float botThreat = target->GetThreatMgr().GetThreat(bot);
    float maxThreat = -1.0f;
    bool hasTank = false;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player || !player->IsAlive() || player == bot)
            continue;
        // Only a tank here to take the mob: not one in a city, another zone or the far end of the instance
        if (!player->IsInWorld() || player->GetMap() != bot->GetMap() ||
            !player->IsWithinDistInMap(bot, sPlayerbotAIConfig.SightDistance))
            continue;

        if (PlayerbotAI::IsTank(player))
        {
            hasTank = true;
            float threat = target->GetThreatMgr().GetThreat(player);
            if (maxThreat < threat)
                maxThreat = threat;
        }
    }

    if (maxThreat <= 0 && !hasTank)
        return 0;

    // calculate normal threat for fleeing targets
    bool fleeing = target->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLEEING_MOTION_TYPE ||
                   target->GetMotionMaster()->GetCurrentMovementGeneratorType() == TIMED_FLEEING_MOTION_TYPE;

    // return high threat if tank has no threat
    if (target->IsInCombat() && maxThreat <= 0 && botThreat <= 0 && hasTank && !fleeing)
        return 100;

    // return low threat if mob if fleeing
    if (hasTank && fleeing)
        return 0;

    // The tank not on it yet: any threat of ours is the most it has (and the share below would divide by nothing)
    if (maxThreat <= 0)
        return botThreat > 0 ? 100 : 0;

    return uint8(std::min(100.0f, botThreat * 100 / maxThreat));
}
