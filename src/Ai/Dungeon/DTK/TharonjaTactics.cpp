// The Prophet Tharon'ja bot tactics: in skeleton form use the skeleton spells by role and fight in melee.

#include "DTKActions.h"
#include "DTKMultipliers.h"
#include "DTKPolicy.h"
#include "DTKTriggers.h"
#include "MovementActions.h"
#include "Playerbots.h"

namespace
{
// By id: a warrior also knows a class Taunt of the same name, and name lookup can pick it
uint32 SpellIdOf(DrakTharon::SkeletonSpell spell)
{
    switch (spell)
    {
        case DrakTharon::SkeletonSpell::SlayingStrike:
            return SPELL_SKELETON_SLAYING_STRIKE;
        case DrakTharon::SkeletonSpell::Taunt:
            return SPELL_SKELETON_TAUNT;
        case DrakTharon::SkeletonSpell::BoneArmor:
            return SPELL_SKELETON_BONE_ARMOR;
        case DrakTharon::SkeletonSpell::TouchOfLife:
            return SPELL_SKELETON_TOUCH_OF_LIFE;
        default:
            return 0;
    }
}
}  // namespace

bool GiftOfTharonjaTrigger::IsActive() { return bot->HasAura(SPELL_GIFT_OF_THARONJA); }

bool TharonjaSkeletonAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "the prophet tharon'ja");
    if (!boss)
        return false;

    DrakTharon::SkeletonState state;
    state.tank = PlayerbotAI::IsTank(bot);
    state.bossOnMe = boss->GetVictim() == bot;
    state.boneArmorUp = bot->HasAura(SPELL_SKELETON_BONE_ARMOR);
    state.healthPct = bot->GetHealthPct();
    state.slayingStrikeReady = botAI->CanCastSpell(SPELL_SKELETON_SLAYING_STRIKE, boss);
    state.tauntReady = botAI->CanCastSpell(SPELL_SKELETON_TAUNT, boss);
    state.boneArmorReady = botAI->CanCastSpell(SPELL_SKELETON_BONE_ARMOR, bot);
    state.touchOfLifeReady = botAI->CanCastSpell(SPELL_SKELETON_TOUCH_OF_LIFE, boss);

    DrakTharon::SkeletonSpell spell = DrakTharon::NextSkeletonSpell(state);
    if (spell == DrakTharon::SkeletonSpell::None)
        return false;

    Unit* target = spell == DrakTharon::SkeletonSpell::BoneArmor ? static_cast<Unit*>(bot) : boss;
    return botAI->CastSpell(SpellIdOf(spell), target);
}

float TharonjaMultiplier::GetValue(Action* action)
{
    bool cast = dynamic_cast<CastSpellAction*>(action) != nullptr;
    bool flee = dynamic_cast<FleeAction*>(action) != nullptr;
    return DrakTharon::TharonjaAllows(bot->HasAura(SPELL_GIFT_OF_THARONJA), cast, flee) ? 1.0f : 0.0f;
}
