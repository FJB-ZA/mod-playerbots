/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "TOCStrategy.h"

#include "TOCMultipliers.h"

void WotlkDungeonToCStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("toc lance",
        { NextAction("toc lance", ACTION_RAID + 5) }));
    triggers.push_back(new TriggerNode("toc ue lance",
        { NextAction("toc ue lance", ACTION_RAID + 2) }));
    triggers.push_back(new TriggerNode("toc mount near",
        { NextAction("toc mount", ACTION_RAID + 4) }));
    triggers.push_back(new TriggerNode("toc mounted",
        { NextAction("toc mounted", ACTION_RAID + 6) }));
    triggers.push_back(new TriggerNode("toc eadric",
        { NextAction("toc eadric", ACTION_RAID + 3) }));
    // The Argent soldiers: keep off them while they wait, and the skull on a Priestess once they fight
    triggers.push_back(new TriggerNode("toc soldiers waiting", {NextAction("toc soldiers keep off", ACTION_RAID + 3)}));
    triggers.push_back(new TriggerNode("toc soldiers mark", {NextAction("toc soldiers mark", ACTION_RAID + 3)}));
}

void WotlkDungeonToCStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new ToCJoustMultiplier(botAI));
}
