/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Hexcraft triggers that need more than a cooldown check.

#include "HexcraftTriggers.h"

#include <string_view>

#include "Playerbots.h"

namespace
{
constexpr uint32 SPELL_ECHO_WARD = 994003;       // also the shaman's aura while the ward stands
constexpr uint32 SPELL_WRATHFUL_GOLEM = 994145;  // the talent: Mojo Golem gives haste
constexpr float ECHO_WARD_LIFETIME = 8.0f;       // seconds of the ward's 12 the target should live through
constexpr int32 MOJO_GOLEM_HEALTH_PCT = 50;
constexpr int32 NEARLY_DEAD_PCT = 25;

bool Ready(PlayerbotAI* botAI, Player* bot, std::string const spell)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    return spellId && !bot->HasSpellCooldown(spellId);
}

// A fight worth a burst cooldown: a boss, an elite, a player, or a training dummy. The dummy (the balance harness
// fights one) never dies and reports no time to live, so it stands in for a boss.
bool BurstTarget(Unit* target)
{
    if (!target || !target->IsAlive())
        return false;
    if (target->IsPlayer())
        return true;
    Creature* creature = target->ToCreature();
    if (!creature)
        return false;
    if (creature->isElite() || creature->isWorldBoss() || creature->IsDungeonBoss())
        return true;
    return std::string_view(creature->GetName()).find("Dummy") != std::string_view::npos;
}
}  // namespace

bool EchoWardTrigger::IsActive()
{
    if (bot->HasAura(SPELL_ECHO_WARD) || !Ready(botAI, bot, "echo ward"))
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;
    if (BurstTarget(target))
        return true;
    return target->GetHealth() / AI_VALUE(float, "estimated group dps") >= ECHO_WARD_LIFETIME;
}

bool HexcraftBurstTrigger::IsActive()
{
    return Ready(botAI, bot, getName()) && BurstTarget(AI_VALUE(Unit*, "current target"));
}

bool LoasRideTrigger::IsActive()
{
    if (!HexcraftBurstTrigger::IsActive())
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    return botAI->HasAura("flame shock", target, false, true) || botAI->HasAura("shade puppet", target, false, true);
}

bool MarionetteDanceTrigger::IsActive()
{
    if (!HexcraftBurstTrigger::IsActive())
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target->HealthBelowPct(NEARLY_DEAD_PCT))
        return false;
    for (ObjectGuid const guid : AI_VALUE(GuidVector, "attackers"))
        if (Unit* unit = botAI->GetUnit(guid); unit && unit->IsAlive() && unit->IsWithinMeleeRange(bot))
            return false;
    return true;
}

bool MojoGolemTrigger::IsActive()
{
    if (!Ready(botAI, bot, getName()))
        return false;
    if (bot->HealthBelowPct(MOJO_GOLEM_HEALTH_PCT))
        return true;
    return bot->HasSpell(SPELL_WRATHFUL_GOLEM) && BurstTarget(AI_VALUE(Unit*, "current target"));
}

bool ChokingJinxTrigger::IsActive()
{
    Unit* target = GetTarget();
    return target && target->IsAlive() && target->IsNonMeleeSpellCast(true) && Ready(botAI, bot, spell) &&
           !botAI->HasAura(spell, target);
}
