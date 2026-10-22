/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Mechano-Lord Capacitus heroic bot tactics: after each Polarity Shift, stand with your own charge.

#include <cmath>

#include "MechActions.h"
#include "MechMultipliers.h"
#include "MechPolicy.h"
#include "MechTriggers.h"
#include "MovementActions.h"
#include "Playerbots.h"

namespace
{
Mechanar::Polarity PolarityOf(Unit* unit)
{
    if (!unit)
        return Mechanar::Polarity::None;
    if (unit->HasAura(static_cast<uint32>(MechanarIDs::SPELL_POSITIVE_POLARITY)))
        return Mechanar::Polarity::Positive;
    if (unit->HasAura(static_cast<uint32>(MechanarIDs::SPELL_NEGATIVE_POLARITY)))
        return Mechanar::Polarity::Negative;
    return Mechanar::Polarity::None;
}

Position PolaritySpot(Unit* boss, Mechanar::Polarity mine)
{
    bool const front = Mechanar::PolarityStandsInFront(mine, PolarityOf(boss->GetVictim()));
    float const offset = Mechanar::PolaritySpotSign(front) * Mechanar::PolaritySpotOffset;
    float const facing = boss->GetOrientation();
    return Position(boss->GetPositionX() + std::cos(facing) * offset, boss->GetPositionY() + std::sin(facing) * offset,
                    boss->GetPositionZ());
}
}  // namespace

bool CapacitusPolarityShiftTrigger::IsActive()
{
    if (!bot->GetMap()->IsHeroic())
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "mechano-lord capacitus");
    if (!boss)
        return false;

    Mechanar::Polarity const mine = PolarityOf(bot);
    if (mine == Mechanar::Polarity::None)
        return false;

    return Mechanar::MovesForPolarity(true, boss->GetVictim() == bot, mine,
                                      bot->GetExactDist2d(PolaritySpot(boss, mine)));
}

bool CapacitusPolaritySpotAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "mechano-lord capacitus");
    Mechanar::Polarity const mine = PolarityOf(bot);
    if (!boss || mine == Mechanar::Polarity::None)
        return false;

    Position const spot = PolaritySpot(boss, mine);
    return MoveTo(bot->GetMapId(), spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), false, false, false,
                  false, MovementPriority::MOVEMENT_COMBAT, true, false);
}

float CapacitusPolarityMultiplier::GetValue(Action* action)
{
    bool repositions = dynamic_cast<SetBehindTargetAction*>(action) || dynamic_cast<FleeAction*>(action);
    if (!repositions || !bot->GetMap()->IsHeroic())
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "mechano-lord capacitus");
    if (!boss)
        return 1.0f;
    return Mechanar::CapacitusAllows(true, boss->GetVictim() == bot, PolarityOf(bot), true) ? 1.0f : 0.0f;
}
