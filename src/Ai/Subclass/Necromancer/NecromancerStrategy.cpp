/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Blight's diseases go up just ahead of Affliction's Corruption (18.0). Contagion's copies are spent at once, inside
// their 10 seconds; Wasting Plague, Mutation and the gargoyle go out on cooldown.

#include "NecromancerStrategy.h"

#include "Playerbots.h"

void NecromancerStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("contagion spread", { NextAction("contagion spread", 25.0f) }));
    triggers.push_back(new TriggerNode("raise gargoyle", { NextAction("raise gargoyle", 22.0f) }));
    triggers.push_back(new TriggerNode("mutation", { NextAction("mutation", 21.0f) }));
    triggers.push_back(new TriggerNode("contagion", { NextAction("contagion", 18.8f) }));
    triggers.push_back(new TriggerNode("wasting plague", { NextAction("wasting plague", 18.6f) }));
    triggers.push_back(new TriggerNode("grave worms", { NextAction("grave worms", 18.4f) }));
    triggers.push_back(new TriggerNode("plague of the risen", { NextAction("plague of the risen", 18.2f) }));
}
