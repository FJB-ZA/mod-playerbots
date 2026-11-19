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

// An attacker other than `current` that Contagion's copies can reach: alive, none of the bot's diseases yet, within
// 30 yards and in sight. The group's attackers include enemies other members fight far away, out of Contagion's reach.
Unit* FindSpreadTarget(PlayerbotAI* botAI, Unit* current)
{
    Player* bot = botAI->GetBot();
    for (ObjectGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit != current && unit->IsAlive() && !HasOwnDisease(botAI, unit) &&
            bot->IsWithinDistInMap(unit, 30.0f) && bot->IsWithinLOSInMap(unit))
            return unit;
    }
    return nullptr;
}
}  // namespace

// Worth it only with another enemy in reach to infect, and while the bot carries no copies yet.
bool CastContagionAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target || !HasOwnDisease(botAI, target) || botAI->HasAura("contagion", bot))
        return false;
    return FindSpreadTarget(botAI, target) && CastSpellAction::isUseful();
}

Unit* CastContagionSpreadAction::SpreadTarget() { return FindSpreadTarget(botAI, AI_VALUE(Unit*, "current target")); }

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
