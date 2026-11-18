/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Every sub-class path's combat strategy, one folder per path under Ai/Subclass, as Ai/Dungeon does per dungeon.

#ifndef PLAYERBOTS_SUBCLASSSTRATEGYCONTEXT_H
#define PLAYERBOTS_SUBCLASSSTRATEGYCONTEXT_H

#include "GladiatorStrategy.h"
#include "NamedObjectContext.h"
#include "NecromancerStrategy.h"
#include "RangerStrategy.h"
#include "Strategy.h"

class SubclassStrategyContext : public NamedObjectContext<Strategy>
{
public:
    SubclassStrategyContext() : NamedObjectContext<Strategy>(false, true)
    {
        creators["gladiator"] = &SubclassStrategyContext::gladiator;  // warrior, path 1
        creators["blight"] = &SubclassStrategyContext::blight;        // warlock, path 2 (Necromancer)
        creators["ranger"] = &SubclassStrategyContext::ranger;        // hunter, path 3
    }

private:
    static Strategy* gladiator(PlayerbotAI* botAI) { return new GladiatorStrategy(botAI); }
    static Strategy* blight(PlayerbotAI* botAI) { return new NecromancerStrategy(botAI); }
    static Strategy* ranger(PlayerbotAI* botAI) { return new RangerStrategy(botAI); }
};

#endif
