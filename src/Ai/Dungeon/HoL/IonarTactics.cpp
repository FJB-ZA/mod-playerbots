/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Ionar bot tactics: spread for Static Overload and Ball Lightning, tank him in place, run from his sparks.

#include "HoLActions.h"
#include "HoLMultipliers.h"
#include "HoLPolicy.h"
#include "HoLTriggers.h"
#include "Playerbots.h"
#include "WarriorActions.h"

namespace
{
bool CastingDisperse(Unit* boss) { return boss->FindCurrentSpellBySpellId(SPELL_DISPERSE); }

// He turns invisible while dispersed into sparks
bool Dispersed(Player* bot, Unit* boss) { return !bot->CanSeeOrDetect(boss); }
}  // namespace

bool IonarStaticOverloadTrigger::IsActive()
{
    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        Unit* unit = botAI->GetUnit(member);
        if (unit && unit->HasAura(SPELL_STATIC_OVERLOAD))
            return true;
    }
    return false;
}

bool IonarBallLightningTrigger::IsActive()
{
    if (!HallsOfLightning::SpreadsForBallLightning(PlayerbotAI::IsMelee(bot)))
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "ionar");
    return boss && boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_BALL_LIGHTNING);
}

bool IonarTankAggroTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot) || !AI_VALUE2(Unit*, "find target", "ionar"))
        return false;

    return AI_VALUE2(bool, "has aggro", "current target");
}

bool IonarDisperseTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ionar");
    return boss && (Dispersed(bot, boss) || CastingDisperse(boss));
}

bool StaticOverloadSpreadAction::Execute(Event /*event*/)
{
    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        if (bot->GetGUID() == member)
            continue;

        Unit* unit = botAI->GetUnit(member);
        if (!unit || !unit->HasAura(SPELL_STATIC_OVERLOAD))
            continue;

        float step = HallsOfLightning::StaticOverloadStepAway(bot->GetExactDist2d(unit));
        if (step > 0.0f)
            return MoveAway(unit, step);
    }
    return false;
}

bool BallLightningSpreadAction::Execute(Event /*event*/)
{
    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        if (bot->GetGUID() == member)
            continue;

        Unit* unit = botAI->GetUnit(member);
        if (!unit)
            continue;

        float step = HallsOfLightning::BallLightningStepAway(bot->GetExactDist2d(unit));
        if (step > 0.0f)
            return MoveAway(unit, step);
    }
    return false;
}

bool IonarTankPositionAction::isUseful()
{
    return bot->GetExactDist2d(IONAR_TANK_POSITION) > HallsOfLightning::IonarTankLeash;
}

bool IonarTankPositionAction::Execute(Event /*event*/)
{
    return MoveTo(bot->GetMapId(), IONAR_TANK_POSITION.GetPositionX(), IONAR_TANK_POSITION.GetPositionY(),
                  IONAR_TANK_POSITION.GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}

bool DispersePositionAction::isUseful()
{
    return bot->GetExactDist2d(DISPERSE_POSITION) > HallsOfLightning::DisperseLeash;
}

bool DispersePositionAction::Execute(Event /*event*/)
{
    return MoveTo(bot->GetMapId(), DISPERSE_POSITION.GetPositionX(), DISPERSE_POSITION.GetPositionY(),
                  DISPERSE_POSITION.GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}

float IonarMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ionar");
    if (!boss)
        return 1.0f;

    HallsOfLightning::IonarAction shape;
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.toDispersePosition = dynamic_cast<DispersePositionAction*>(action) != nullptr;
    shape.staticOverloadSpread = dynamic_cast<StaticOverloadSpreadAction*>(action) != nullptr;
    shape.charge = dynamic_cast<CastChargeAction*>(action) != nullptr;

    return HallsOfLightning::IonarAllows(Dispersed(bot, boss), CastingDisperse(boss), shape) ? 1.0f : 0.0f;
}
