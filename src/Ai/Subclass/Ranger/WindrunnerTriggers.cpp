/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Windrunner triggers that need more than an aura or cooldown check.

#include "WindrunnerTriggers.h"

#include "Playerbots.h"
#include "SpellAuras.h"

namespace
{
constexpr uint32 SpellAdvantage = 993010;
constexpr uint32 SpellSkirmish = 993002;
constexpr uint32 SpellTellingBlow = 993021;
constexpr uint32 FullStacks = 5;
constexpr float KindlingRange = 8.0f;  // Incendiary Shot's burst
constexpr uint32 IncendiaryPack = 3;

uint32 Advantage(Player* bot)
{
    Aura const* aura = bot->GetAura(SpellAdvantage);
    return aura ? aura->GetStackAmount() : 0;
}

bool Ready(PlayerbotAI* botAI, Player* bot, std::string const spell)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spell)->Get();
    return spellId && !bot->HasSpellCooldown(spellId);
}
}  // namespace

bool FullAdvantageTrigger::IsActive() { return Advantage(bot) >= FullStacks; }

bool SkirmishTrigger::IsActive()
{
    return Advantage(bot) >= FullStacks && !bot->HasAura(SpellSkirmish) && Ready(botAI, bot, "skirmish");
}

bool IncendiaryShotTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || Advantage(bot) < FullStacks || !Ready(botAI, bot, "incendiary shot"))
        return false;
    uint32 near = 0;
    for (ObjectGuid const guid : AI_VALUE(GuidVector, "attackers"))
        if (Unit* unit = botAI->GetUnit(guid); unit && unit->IsAlive() && unit->IsWithinDistInMap(target, KindlingRange))
            ++near;
    return near >= IncendiaryPack;
}

bool FinalArrowTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !Ready(botAI, bot, "final arrow"))
        return false;
    return target->HealthBelowPct(20) || bot->HasAura(SpellTellingBlow);
}

bool HushingArrowTrigger::IsActive() { return Advantage(bot) > 0 && InterruptSpellTrigger::IsActive(); }
