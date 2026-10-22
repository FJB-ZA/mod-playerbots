/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Anzu bot tactics: skull or moon him as his banish comes and goes, HoT the bird spirits, hold casts under Spell Bomb.

#include "EncounterHelpers.h"
#include "GenericSpellActions.h"
#include "Playerbots.h"
#include "SethActions.h"
#include "SethMultipliers.h"
#include "SethPolicy.h"
#include "SethShared.h"
#include "SethTriggers.h"

using namespace SethShared;

namespace
{
uint32 SpiritEntry(SethekkHalls::BirdSpirit spirit)
{
    switch (spirit)
    {
        case SethekkHalls::BirdSpirit::Falcon:
            return Id(SethNpcs::NPC_FALCON_SPIRIT);
        case SethekkHalls::BirdSpirit::Hawk:
            return Id(SethNpcs::NPC_HAWK_SPIRIT);
        default:
            return Id(SethNpcs::NPC_EAGLE_SPIRIT);
    }
}
}  // namespace

bool AnzuEncounterHasTwoPhasesTrigger::IsActive() { return AI_VALUE2(Unit*, "find target", "anzu"); }

bool AnzuBirdSpiritsProvideBuffsTrigger::IsActive()
{
    return SethekkHalls::HotsSpirits(bot->getClass() == CLASS_DRUID, PlayerbotAI::IsHeal(bot)) &&
           AI_VALUE2(Unit*, "find target", "anzu");
}

bool AnzuAlternateMarksOnBossAction::Execute(Event /*event*/)
{
    Unit* anzu = AI_VALUE2(Unit*, "find target", "anzu");
    if (!anzu)
        return false;

    if (SethekkHalls::AnzuMarkFor(anzu->HasAura(Id(SethSpells::SPELL_BANISH_ANZU))) == SethekkHalls::AnzuMark::Moon)
        return EncounterHelpers::MarkTargetWithMoon(bot, anzu);

    return EncounterHelpers::MarkTargetWithSkull(bot, anzu);
}

bool AnzuCastHealOverTimeSpellOnBirdSpiritAction::Execute(Event /*event*/)
{
    constexpr float searchRadius = 60.0f;
    Creature* targetSpirit = nullptr;

    for (SethekkHalls::BirdSpirit kind : SethekkHalls::SpiritHotOrder)
    {
        Creature* spirit = bot->FindNearestCreature(SpiritEntry(kind), searchRadius);
        // The spirit's script only wakes for a druid HoT carrying Rejuvenation's icon
        if (spirit &&
            !spirit->GetAuraEffect(SPELL_AURA_PERIODIC_HEAL, SPELLFAMILY_DRUID, REJUVENATION_SPELL_ICON_ID, 0))
        {
            targetSpirit = spirit;
            break;
        }
    }

    if (!targetSpirit)
        return false;

    if (!botAI->CanCastSpell(Id(SethSpells::SPELL_REJUVENATION_RANK_1), targetSpirit))
        return false;

    return botAI->CastSpell(Id(SethSpells::SPELL_REJUVENATION_RANK_1), targetSpirit);
}

float AnzuControlSpellCastingWithSpellBombMultiplier::GetValue(Action* action)
{
    bool const spellCast = dynamic_cast<CastSpellAction*>(action) != nullptr;
    if (!spellCast)
        return 1.0f;

    SethekkHalls::SpellBombCaster caster;
    caster.manaUser = bot->getPowerType() == POWER_MANA;
    caster.tank = PlayerbotAI::IsTank(bot);
    caster.dps = PlayerbotAI::IsDps(bot);
    caster.bombed = bot->HasAura(Id(SethSpells::SPELL_SPELL_BOMB));
    if (caster.bombed && caster.manaUser && !caster.tank && !caster.dps)
    {
        if (Player* mainTank = EncounterHelpers::GetGroupMainTank(bot))
        {
            caster.hasMainTank = true;
            caster.mainTankHealthPct = mainTank->GetHealthPct();
        }
    }

    return SethekkHalls::SpellBombAllows(caster, spellCast) ? 1.0f : 0.0f;
}
