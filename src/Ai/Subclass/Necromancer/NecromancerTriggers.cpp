/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Necromancer's triggers that need more than an aura or cooldown check.

#include "NecromancerTriggers.h"

#include "Playerbots.h"

bool ContagionSpreadTrigger::IsActive() { return botAI->HasAura("contagion", bot); }
