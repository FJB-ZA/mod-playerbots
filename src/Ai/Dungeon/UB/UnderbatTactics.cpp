/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Underbat bot tactics: melee gather on the tank and everyone stays out of the lash behind a bat.

#include "MovementActions.h"
#include "Playerbots.h"
#include "Timer.h"
#include "UBActions.h"
#include "UBMultipliers.h"
#include "UBPolicy.h"
#include "UBShared.h"
#include "UBTriggers.h"

using namespace UnderbogHungarfen;

bool UBUnderbatLashTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    auto const& attackers = AI_VALUE_REF(GuidVector, "attackers");
    return Underbog::WatchesUnderbats(false, GetNearestUnderbatInLashRange(bot, attackers) != nullptr);
}

bool UBClearUnderbatBackAction::Execute(Event /*event*/)
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    auto const& attackers = AI_VALUE_REF(GuidVector, "attackers");
    Creature* bat = GetNearestUnderbatInLashRange(bot, attackers);
    if (!bat)
        return false;

    Underbog::UnderbatStep step;
    step.throttled = _lastReposition && GetMSTimeDiffToNow(_lastReposition) < Underbog::UnderbatRepositionCooldown;
    step.melee = PlayerbotAI::IsMelee(bot);

    if (step.melee)
    {
        Unit* rally = UnderbatRallyUnit(bot, attackers);
        bool const parked =
            rally && Underbog::ParkedAtRally(bot->GetExactDist(rally),
                                             InPackFront(bot, attackers, bot->GetPositionX(), bot->GetPositionY()));
        if (Underbog::WaitsToRally(true, rally != nullptr, parked, step.throttled))
            return false;

        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
        if (rally && !parked &&
            Underbog::TriesRally(true, true, parked,
                                 UnderbatRallySpot(bot, rally, AI_VALUE(Unit*, "current target"), attackers, rx, ry, rz)) &&
            MoveTo(bot->GetMapId(), rx, ry, rz, false, false, true, true, MovementPriority::MOVEMENT_COMBAT))
        {
            _lastReposition = getMSTime();
            return true;
        }
    }
    else
        step.casting = bot->IsNonMeleeSpellCast(true);

    if (step.melee || step.casting)
        step.lashing = GetLashingUnderbat(bot, attackers) != nullptr;
    step.gap = bot->GetDistance2d(bat);
    if (!Underbog::StepsAwayFromUnderbat(step))
        return false;

    if (!MoveAway(bat, Underbog::UnderbatStepDistance(step.gap)))
        return false;

    _lastReposition = getMSTime();
    return true;
}

float UnderbatFacingMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<SetBehindTargetAction*>(action))
        return 1.0f;

    auto const& attackers = AI_VALUE_REF(GuidVector, "attackers");
    return Underbog::UnderbatAllowsGetBehind(AnyUnderbatInLashRange(bot, attackers)) ? 1.0f : 0.0f;
}
