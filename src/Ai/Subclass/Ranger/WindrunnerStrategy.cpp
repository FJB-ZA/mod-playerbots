/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Hushing Arrow interrupts beside Silencing Shot (40). The finishers come above Marksmanship's Kill Shot (18.0):
// Skirmish, then Incendiary Shot on a pack, then Precision Shot. Final Arrow and Piercing Volley go out when ready;
// Serrated Shot is kept up just above Serpent Sting (17.0), and Ricochet Shot builds on cooldown.

#include "WindrunnerStrategy.h"

#include "Playerbots.h"

void WindrunnerStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("hushing arrow", { NextAction("hushing arrow", 40.5f) }));
    triggers.push_back(new TriggerNode("skirmish", { NextAction("skirmish", 22.0f) }));
    triggers.push_back(new TriggerNode("incendiary shot", { NextAction("incendiary shot", 21.5f) }));
    triggers.push_back(new TriggerNode("full advantage", { NextAction("precision shot", 21.0f) }));
    triggers.push_back(new TriggerNode("piercing volley", { NextAction("piercing volley", 20.0f) }));
    triggers.push_back(new TriggerNode("final arrow", { NextAction("final arrow", 19.0f) }));
    triggers.push_back(new TriggerNode("serrated shot", { NextAction("serrated shot", 17.2f) }));
    triggers.push_back(new TriggerNode("ricochet shot", { NextAction("ricochet shot", 16.0f) }));
}
