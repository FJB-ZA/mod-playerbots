// Mage-Lord Urom bot tactics: leave him alone on the platforms, hide from his explosion, carry a Time Bomb away.

#include "MovementActions.h"
#include "OCActions.h"
#include "OCMultipliers.h"
#include "OCPolicy.h"
#include "OCTriggers.h"
#include "Playerbots.h"

namespace
{
int PhaseAt(Unit* unit)
{
    std::array<float, 3> distances{};
    for (size_t i = 0; i < distances.size(); ++i)
    {
        Oculus::Point const& platform = Oculus::UromPlatforms[i];
        distances[i] = unit->GetDistance(platform.x, platform.y, platform.z);
    }
    return Oculus::UromPhase(distances);
}
}  // namespace

bool UromArcaneExplosionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "mage-lord urom");
    return boss && boss->FindCurrentSpellBySpellId(SPELL_EMPOWERED_ARCANE_EXPLOSION);
}

bool UromTimeBombTrigger::IsActive() { return bot->HasAura(SPELL_TIME_BOMB); }

bool AvoidArcaneExplosionAction::Execute(Event /*event*/)
{
    if (!AI_VALUE2(Unit*, "find target", "mage-lord urom"))
        return false;

    Oculus::Point const& spot =
        Oculus::UromSafeSpots[Oculus::ClosestSafeSpot(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ())];
    return MoveNear(bot->GetMapId(), spot.x, spot.y, spot.z, Oculus::SafeSpotTolerance,
                    MovementPriority::MOVEMENT_COMBAT);
}

bool TimeBombSpreadAction::Execute(Event /*event*/)
{
    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        if (member == bot->GetGUID())
            continue;

        Unit* unit = botAI->GetUnit(member);
        if (!unit)
            continue;

        float step = Oculus::TimeBombStepAway(bot->GetExactDist2d(unit));
        if (step != 0.0f)
            return MoveAway(unit, step);
    }
    return false;
}

float UromMultiplier::GetValue(Action* action)
{
    if (!Oculus::AttacksUromIn(PhaseAt(bot)))
    {
        Unit* target = action->GetTarget();
        if (target && target->GetEntry() == NPC_MAGE_LORD_UROM)
            return 0.0f;
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "mage-lord urom");
    if (!boss)
        return 1.0f;

    Oculus::UromMoment fight;
    fight.explosion =
        boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_EMPOWERED_ARCANE_EXPLOSION);
    fight.melee = PlayerbotAI::IsMelee(bot);
    fight.timeBomb = bot->HasAura(SPELL_TIME_BOMB);

    Oculus::UromAction shape;
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.avoidExplosion = dynamic_cast<AvoidArcaneExplosionAction*>(action) != nullptr;
    shape.avoidAoe = dynamic_cast<AvoidAoeAction*>(action) != nullptr;
    shape.timeBombSpread = dynamic_cast<TimeBombSpreadAction*>(action) != nullptr;

    return Oculus::UromAllows(fight, shape) ? 1.0f : 0.0f;
}
