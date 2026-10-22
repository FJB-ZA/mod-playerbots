/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Loken bot tactics: ranged stack on him against Pulsing Shockwave and everyone runs out of Lightning Nova.

#include "HoLActions.h"
#include "HoLMultipliers.h"
#include "HoLPolicy.h"
#include "HoLTriggers.h"
#include "Playerbots.h"
#include "WarriorActions.h"

namespace
{
bool CastingNova(Player* bot, Unit* boss) { return boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA); }
}  // namespace

bool LokenRangedTrigger::IsActive()
{
    return HallsOfLightning::StacksOnLoken(PlayerbotAI::IsMelee(bot)) && AI_VALUE2(Unit*, "find target", "loken");
}

bool LokenLightningNovaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "loken");
    return boss && boss->HasUnitState(UNIT_STATE_CASTING) && CastingNova(bot, boss);
}

bool LokenStackAction::isUseful()
{
    float stackDistance = HallsOfLightning::LokenStackDistance(bot->getClass() == CLASS_HUNTER);
    return AI_VALUE2(float, "distance", "current target") > stackDistance;
}

bool LokenStackAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "loken");
    if (!boss || boss->HasUnitState(UNIT_STATE_CASTING))
        return false;

    float step = HallsOfLightning::LokenStackStep(bot->GetExactDist2d(boss), bot->getClass() == CLASS_HUNTER);
    return Move(bot->GetAngle(boss), step);
}

bool AvoidLightningNovaAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "loken");
    if (!boss)
        return false;

    float step = HallsOfLightning::LightningNovaStepAway(bot->GetExactDist2d(boss->GetPosition()));
    return step > 0.0f && MoveAway(boss, step);
}

float LokenMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "loken");
    if (!boss)
        return 1.0f;

    HallsOfLightning::LokenAction shape;
    shape.flee = dynamic_cast<FleeAction*>(action) != nullptr;
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.avoidNova = dynamic_cast<AvoidLightningNovaAction*>(action) != nullptr;
    shape.charge = dynamic_cast<CastChargeAction*>(action) != nullptr;

    return HallsOfLightning::LokenAllows(CastingNova(bot, boss), shape) ? 1.0f : 0.0f;
}
