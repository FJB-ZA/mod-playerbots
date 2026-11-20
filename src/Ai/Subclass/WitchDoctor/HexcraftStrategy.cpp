/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The cooldowns go out just below Elemental Mastery (29.0), and Choking Jinx just above Wind Shear (23.0). In the
// rotation Echo Ward goes up before the bolts it echoes and Shade Puppet goes out on cooldown, both above Elemental's
// Flame Shock (5.3, the path's Voodoo Shock); Loa's Spite and then Witchfire go out on cooldown between Lava Burst
// (5.2) and Lightning Bolt (5.0), the filler.

#include "HexcraftStrategy.h"

#include "Playerbots.h"

void HexcraftStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("spirit shroud", { NextAction("spirit shroud", 28.8f) }));
    triggers.push_back(new TriggerNode("loa's ride", { NextAction("loa's ride", 28.6f) }));
    triggers.push_back(new TriggerNode("marionette dance", { NextAction("marionette dance", 28.4f) }));
    triggers.push_back(new TriggerNode("mojo golem", { NextAction("mojo golem", 28.2f) }));
    triggers.push_back(new TriggerNode("choking jinx", { NextAction("choking jinx", 23.5f) }));
    triggers.push_back(new TriggerNode("echo ward", { NextAction("echo ward", 5.45f) }));
    triggers.push_back(new TriggerNode("shade puppet", { NextAction("shade puppet", 5.4f) }));
    triggers.push_back(new TriggerNode("loa's spite", { NextAction("loa's spite", 5.15f) }));
    triggers.push_back(new TriggerNode("witchfire", { NextAction("witchfire", 5.1f) }));
}
