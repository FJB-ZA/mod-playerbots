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
        bool const parked = rally && bot->GetExactDist(rally) <= Underbog::UnderbatRallyTolerance;
        if (Underbog::WaitsToRally(true, rally != nullptr, parked, step.throttled))
            return false;

        if (rally && !parked)
        {
            float const rx = rally->GetPositionX();
            float const ry = rally->GetPositionY();
            float const rz = rally->GetPositionZ();
            if (Underbog::TriesRally(true, true, parked, SpotClearOfUnderbats(bot, attackers, rx, ry, rz)) &&
                MoveTo(bot->GetMapId(), rx, ry, rz, false, false, true, true, MovementPriority::MOVEMENT_COMBAT))
            {
                _lastReposition = getMSTime();
                return true;
            }
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
