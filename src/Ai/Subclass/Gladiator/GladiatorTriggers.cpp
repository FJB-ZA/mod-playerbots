/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Gladiator's triggers that need more than an aura or cooldown check.

#include "GladiatorTriggers.h"

#include "Playerbots.h"

bool GladiatorStanceTrigger::IsActive()
{
    return !bot->IsInCombat() && AI_VALUE2(uint32, "spell id", spell) && BuffTrigger::IsActive();
}

bool ShieldChargeTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", spell);
    if (!spellId || bot->HasSpellCooldown(spellId))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && bot->IsWithinDistInMap(target, 10.0f) && BuffTrigger::IsActive();
}
