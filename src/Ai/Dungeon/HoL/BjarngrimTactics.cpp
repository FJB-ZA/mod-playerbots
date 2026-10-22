/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// General Bjarngrim bot tactics: dps kill his Stormforged Lieutenants first and everyone leaves his Whirlwind.

#include "ChooseTargetActions.h"
#include "HoLActions.h"
#include "HoLMultipliers.h"
#include "HoLPolicy.h"
#include "HoLTriggers.h"
#include "Playerbots.h"

namespace
{
// The lieutenants don't always reach a dps bot's threat table, so find target can't see them
Unit* FindLieutenant(PlayerbotAI* botAI, std::string const& targetsValue, bool inCombatOnly)
{
    GuidVector targets = botAI->GetAiObjectContext()->GetValue<GuidVector>(targetsValue)->Get();
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NPC_STORMFORGED_LIEUTENANT && (!inCombatOnly || unit->IsInCombat()))
            return unit;
    }
    return nullptr;
}

bool CastingWhirlwind(Unit* boss)
{
    return boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_WHIRLWIND_BJARNGRIM);
}
}  // namespace

bool StormforgedLieutenantTrigger::IsActive()
{
    return HallsOfLightning::AttacksLieutenant(PlayerbotAI::IsDps(bot)) &&
           FindLieutenant(botAI, "possible targets", true);
}

bool BjarngrimWhirlwindTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "general bjarngrim");
    return boss && CastingWhirlwind(boss);
}

bool BjarngrimTargetAction::Execute(Event /*event*/)
{
    Unit* lieutenant = FindLieutenant(botAI, "possible targets", false);
    if (!lieutenant)
        return false;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    bool onLieutenant = currentTarget && currentTarget->GetEntry() == NPC_STORMFORGED_LIEUTENANT;
    if (!HallsOfLightning::SwitchesToLieutenant(onLieutenant))
        return false;

    return Attack(lieutenant);
}

bool AvoidWhirlwindAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "general bjarngrim");
    if (!boss)
        return false;

    float step = HallsOfLightning::WhirlwindStepAway(bot->GetExactDist2d(boss->GetPosition()));
    return step > 0.0f && MoveAway(boss, step);
}

float BjarngrimMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "general bjarngrim");
    if (!boss)
        return 1.0f;

    HallsOfLightning::BjarngrimMoment fight;
    fight.healer = PlayerbotAI::IsHeal(bot);
    if (fight.healer)
        return 1.0f;
    fight.tank = PlayerbotAI::IsTank(bot);
    fight.whirlwind = CastingWhirlwind(boss);
    fight.lieutenantUp = !fight.tank && FindLieutenant(botAI, "possible targets no los", false);

    HallsOfLightning::BjarngrimAction shape;
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.avoidWhirlwind = dynamic_cast<AvoidWhirlwindAction*>(action) != nullptr;
    shape.dpsAssist = dynamic_cast<DpsAssistAction*>(action) != nullptr;
    shape.aoe = action->getThreatType() == Action::ActionThreatType::Aoe;

    return HallsOfLightning::BjarngrimAllows(fight, shape) ? 1.0f : 0.0f;
}
