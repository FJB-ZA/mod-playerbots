/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Field Dressing in and out of combat. It's an instant heal over time, so the bot casts it as soon as it's hurt
// and the last one has run out, whatever is next to it. A plain hunter never learns it, so the strategy does
// nothing for one.

#include "RangerStrategy.h"

void RangerStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("medium health", { NextAction("field dressing", ACTION_MEDIUM_HEAL) }));
    triggers.push_back(new TriggerNode("low health", { NextAction("field dressing", ACTION_CRITICAL_HEAL) }));
}
