/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Necromancer's actions that need more than a spell cast.

#include "NecromancerActions.h"

#include "Playerbots.h"

namespace
{
bool HasOwnDisease(PlayerbotAI* botAI, Unit* unit)
{
    return botAI->HasAura("grave worms", unit, false, true) || botAI->HasAura("plague of the risen", unit, false, true);
}
}  // namespace

// Worth it only with another enemy to infect, and while the bot carries no copies yet.
bool CastContagionAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target || !HasOwnDisease(botAI, target) || botAI->HasAura("contagion", bot))
        return false;
    return AI_VALUE(GuidVector, "attackers").size() > 1 && CastSpellAction::isUseful();
}

Unit* CastContagionSpreadAction::SpreadTarget()
{
    Unit* current = AI_VALUE(Unit*, "current target");
    for (ObjectGuid const guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit != current && unit->IsAlive() && !HasOwnDisease(botAI, unit) &&
            bot->IsWithinDistInMap(unit, 30.0f) && bot->IsWithinLOSInMap(unit))
            return unit;
    }
    return nullptr;
}

bool CastContagionSpreadAction::isUseful() { return botAI->HasAura("contagion", bot); }

bool CastContagionSpreadAction::isPossible()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", spell);
    return spellId && !bot->HasSpellCooldown(spellId) && SpreadTarget();
}

bool CastContagionSpreadAction::Execute(Event /*event*/)
{
    Unit* target = SpreadTarget();
    return target && botAI->CastSpell(spell, target);
}
