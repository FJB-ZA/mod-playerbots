/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Ranger sub-class path's spells, as bot actions.

#ifndef PLAYERBOTS_RANGERACTIONS_H
#define PLAYERBOTS_RANGERACTIONS_H

#include "AiObject.h"
#include "GenericSpellActions.h"

// Mend Pet's heal on the hunter, cast on itself while the heal isn't already running
BUFF_ACTION(CastFieldDressingAction, "field dressing");

#endif
