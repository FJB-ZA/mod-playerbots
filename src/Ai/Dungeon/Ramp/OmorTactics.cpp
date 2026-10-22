/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Omor the Unscarred bot tactics: aura carriers run clear, melee hold off, ranged spread, hounds get skulled.

#include "EncounterHelpers.h"
#include "Playerbots.h"
#include "RampActions.h"
#include "RampMultipliers.h"
#include "RampPolicy.h"
#include "RampShared.h"
#include "RampTriggers.h"
#include "ReachTargetActions.h"

using HellfireRamparts::TreacherySafeDistance;

bool OmorTreacheryAuraTrigger::IsActive()
{
    bool const omorUp = AI_VALUE2(Unit*, "find target", "omor the unscarred") != nullptr;
    bool const hasAura = omorUp && helper.HasTreacheryAura(bot);
    return HellfireRamparts::FleesWithTreachery(omorUp, hasAura, hasAura && PlayerbotAI::IsMainTank(bot));
}

bool OmorTankHasTreacheryAuraTrigger::IsActive()
{
    if (!AI_VALUE2(Unit*, "find target", "omor the unscarred"))
        return false;

    Player* tank = EncounterHelpers::GetGroupMainTank(bot);
    if (!tank)
        return false;

    return HellfireRamparts::FleesFromTank(true, tank != bot && helper.HasTreacheryAura(tank), tank == bot);
}

bool OmorRangedSpreadTrigger::IsActive()
{
    bool const ranged = PlayerbotAI::IsRanged(bot);
    bool const tooClose = ranged && EncounterHelpers::GetNearestPlayerInRadius(bot, TreacherySafeDistance);
    bool const omorUp = tooClose && AI_VALUE2(Unit*, "find target", "omor the unscarred");
    return HellfireRamparts::SpreadsFromOthers(omorUp, ranged, tooClose);
}

bool OmorFiendishHoundIsActiveTrigger::IsActive()
{
    return EncounterHelpers::IsMechanicTrackerBot(bot, RampShared::RAMP_MAP_ID) &&
           AI_VALUE2(Unit*, "find target", "fiendish hound");
}

bool OmorTreacheryAuraFleeFromPlayersAction::Execute(Event /*event*/)
{
    if (!EncounterHelpers::GetNearestPlayerInRadius(bot, TreacherySafeDistance))
        return false;

    bot->CastStop();
    return MoveFromGroup(TreacherySafeDistance);
}

bool OmorTreacheryAuraFleeFromTankAction::Execute(Event /*event*/)
{
    if (!AI_VALUE2(Unit*, "find target", "omor the unscarred"))
        return false;

    Unit* tank = EncounterHelpers::GetGroupMainTank(bot);
    if (!tank || !HellfireRamparts::NeedsToLeaveTank(bot->GetExactDist2d(tank)))
        return false;

    bot->CastStop();
    return MoveAway(tank, TreacherySafeDistance);
}

bool OmorRangedSpreadAction::Execute(Event /*event*/)
{
    if (Unit* nearestPlayer = EncounterHelpers::GetNearestPlayerInRadius(bot, TreacherySafeDistance))
        return FleePosition(nearestPlayer->GetPosition(), TreacherySafeDistance);

    return false;
}

bool OmorMarkFiendishHoundAction::Execute(Event /*event*/)
{
    Unit* hound = AI_VALUE2(Unit*, "find target", "fiendish hound");
    if (!hound)
        return false;

    return EncounterHelpers::MarkTargetWithSkull(bot, hound);
}

float OmorTreacheryAuraFleeFromPlayersMultiplier::GetValue(Action* action)
{
    HellfireRamparts::OmorAction shape;
    shape.treacheryMove = dynamic_cast<OmorRangedSpreadAction*>(action) ||
                          dynamic_cast<OmorTreacheryAuraFleeFromPlayersAction*>(action) ||
                          dynamic_cast<OmorTreacheryAuraFleeFromTankAction*>(action);
    shape.mover = dynamic_cast<CastReachTargetSpellAction*>(action) || dynamic_cast<ReachTargetAction*>(action) ||
                  dynamic_cast<MovementAction*>(action);
    if (shape.treacheryMove || !shape.mover)
        return 1.0f;

    Unit* omor = AI_VALUE2(Unit*, "find target", "omor the unscarred");
    if (!omor)
        return 1.0f;

    HellfireRamparts::OmorMoment fight;
    fight.mainTank = PlayerbotAI::IsMainTank(bot);
    fight.ranged = PlayerbotAI::IsRanged(bot);
    if (!fight.mainTank && !fight.ranged)
    {
        fight.hasAura = helper.HasTreacheryAura(bot);
        Player* tank = EncounterHelpers::GetGroupMainTank(bot);
        fight.tankHasAura = tank && helper.HasTreacheryAura(tank);
        Player* victim = omor->GetVictim() ? omor->GetVictim()->ToPlayer() : nullptr;
        fight.victimHasAura = victim && helper.HasTreacheryAura(victim);
    }

    return HellfireRamparts::OmorAllows(fight, shape) ? 1.0f : 0.0f;
}
