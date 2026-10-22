/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Ingvar the Plunderer bot tactics: the tank runs through him to dodge Smash, nobody starts a long cast into a roar.

#include "GenericSpellActions.h"
#include "Playerbots.h"
#include "UKActions.h"
#include "UKMultipliers.h"
#include "UKPolicy.h"
#include "UKTriggers.h"

namespace
{
bool CastingSmash(Player* bot, Unit* boss)
{
    return boss->FindCurrentSpellBySpellId(SPELL_SMASH) || boss->FindCurrentSpellBySpellId(SPELL_DARK_SMASH);
}

bool CastingRoar(Player* bot, Unit* boss)
{
    return boss->FindCurrentSpellBySpellId(SPELL_STAGGERING_ROAR) ||
           boss->FindCurrentSpellBySpellId(SPELL_DREADFUL_ROAR);
}
}  // namespace

bool IngvarDreadfulRoarTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ingvar the plunderer");
    return boss && boss->FindCurrentSpellBySpellId(SPELL_DREADFUL_ROAR);
}

bool IngvarSmashTankTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ingvar the plunderer");
    return boss && PlayerbotAI::IsTank(bot) && CastingSmash(bot, boss);
}

// Ignores his casting state: he sometimes chains a roar after a smash, and the tank must still get back in front
bool IngvarSmashTankReturnTrigger::IsActive()
{
    return PlayerbotAI::IsTank(bot) && AI_VALUE2(Unit*, "find target", "ingvar the plunderer");
}

bool NotBehindIngvarTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ingvar the plunderer");
    if (!boss)
        return false;

    // The tank runs through him on every Smash and he turns to follow, so wait for both to settle
    Unit* victim = boss->GetVictim();
    bool settled = !boss->HasUnitState(UNIT_STATE_CASTING) && victim && !victim->isMoving();
    return UtgardeKeep::GetsBehindIngvar(PlayerbotAI::IsTank(bot), PlayerbotAI::IsMelee(bot),
                                         AI_VALUE2(bool, "behind", "current target"), settled);
}

bool IngvarDodgeSmashAction::isUseful() { return !AI_VALUE2(bool, "behind", "current target"); }

bool IngvarDodgeSmashAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ingvar the plunderer");
    if (!boss)
        return false;

    return Move(bot->GetAngle(boss), UtgardeKeep::SmashDodgeStep(bot->GetExactDist2d(boss->GetPosition())));
}

bool IngvarSmashReturnAction::isUseful() { return AI_VALUE2(bool, "behind", "current target"); }

bool IngvarSmashReturnAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ingvar the plunderer");
    if (!boss)
        return false;

    return Move(bot->GetAngle(boss), bot->GetExactDist2d(boss->GetPosition()) + bot->GetMeleeReach());
}

float IngvarThePlundererMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ingvar the plunderer");
    if (!boss)
        return 1.0f;

    UtgardeKeep::IngvarMoment fight;
    fight.tank = PlayerbotAI::IsTank(bot);
    fight.tankMoving = bot->isMoving();
    bool casting = boss->HasUnitState(UNIT_STATE_CASTING);
    fight.roar = casting && CastingRoar(bot, boss);
    fight.smash = casting && CastingSmash(bot, boss);

    UtgardeKeep::IngvarAction shape;
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.dodgeSmash = dynamic_cast<IngvarDodgeSmashAction*>(action) != nullptr;
    if (fight.roar && dynamic_cast<CastSpellAction*>(action))
    {
        uint32 spellId = AI_VALUE2(uint32, "spell id", action->getName());
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId))
            shape.castWithCastTime = spellInfo->CalcCastTime(bot) != 0;
    }

    return UtgardeKeep::IngvarAllows(fight, shape) ? 1.0f : 0.0f;
}
