/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Every sub-class path's action and trigger names; a new path adds its two contexts here and nothing in the engine.

#ifndef PLAYERBOTS_SUBCLASSCONTEXTS_H
#define PLAYERBOTS_SUBCLASSCONTEXTS_H

#include "GladiatorActionContext.h"
#include "GladiatorTriggerContext.h"
#include "NamedObjectContext.h"

inline void AddSubclassActionContexts(SharedNamedObjectContextList<Action>& actionContexts)
{
    actionContexts.Add(new SubclassGladiatorActionContext());
}

inline void AddSubclassTriggerContexts(SharedNamedObjectContextList<Trigger>& triggerContexts)
{
    triggerContexts.Add(new SubclassGladiatorTriggerContext());
}

#endif
