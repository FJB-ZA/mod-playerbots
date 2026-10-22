/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Varos Cloudstrider bot trigger: fires while he is on the threat table; no strategy uses it yet.

#include "OCTriggers.h"
#include "Playerbots.h"

bool VarosCloudstriderTrigger::IsActive() { return AI_VALUE2(Unit*, "find target", "varos cloudstrider"); }
