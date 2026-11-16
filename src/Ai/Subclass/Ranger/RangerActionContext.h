/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Ranger sub-class path's action names.

#ifndef PLAYERBOTS_RANGERACTIONCONTEXT_H
#define PLAYERBOTS_RANGERACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "RangerActions.h"

class SubclassRangerActionContext : public NamedObjectContext<Action>
{
public:
    SubclassRangerActionContext() { creators["field dressing"] = &SubclassRangerActionContext::field_dressing; }

private:
    static Action* field_dressing(PlayerbotAI* botAI) { return new CastFieldDressingAction(botAI); }
};

#endif
