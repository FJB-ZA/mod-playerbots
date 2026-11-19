/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Every sub-class path's combat strategy, one folder per path under Ai/Subclass, as Ai/Dungeon does per dungeon. A
// bot follows one path, so the path strategies are siblings: adding one drops the others. A path's own talent tree
// has a strategy of its own beside the path's (a Ranger on Windrunner fights with both "ranger" and "windrunner"),
// so the tree strategies sit in a context without siblings.

#ifndef PLAYERBOTS_SUBCLASSSTRATEGYCONTEXT_H
#define PLAYERBOTS_SUBCLASSSTRATEGYCONTEXT_H

#include "GladiatorStrategy.h"
#include "NamedObjectContext.h"
#include "NecromancerStrategy.h"
#include "RangerStrategy.h"
#include "Strategy.h"
#include "WindrunnerStrategy.h"

class SubclassStrategyContext : public NamedObjectContext<Strategy>
{
public:
    SubclassStrategyContext() : NamedObjectContext<Strategy>(false, true)
    {
        creators["gladiator"] = &SubclassStrategyContext::gladiator;  // warrior, path 1
        creators["ranger"] = &SubclassStrategyContext::ranger;        // hunter, path 3
    }

private:
    static Strategy* gladiator(PlayerbotAI* botAI) { return new GladiatorStrategy(botAI); }
    static Strategy* ranger(PlayerbotAI* botAI) { return new RangerStrategy(botAI); }
};

class SubclassTreeStrategyContext : public NamedObjectContext<Strategy>
{
public:
    SubclassTreeStrategyContext() : NamedObjectContext<Strategy>(false, false)
    {
        creators["blight"] = &SubclassTreeStrategyContext::blight;          // warlock, path 2 (Necromancer)
        creators["windrunner"] = &SubclassTreeStrategyContext::windrunner;  // hunter, path 3 (Ranger)
    }

private:
    static Strategy* blight(PlayerbotAI* botAI) { return new NecromancerStrategy(botAI); }
    static Strategy* windrunner(PlayerbotAI* botAI) { return new WindrunnerStrategy(botAI); }
};

#endif
