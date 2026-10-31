/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Talon King Ikiss bot tactics: tank him at the pillar, ranged stay close, all hide behind it from Arcane Explosion.

#include <cmath>

#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "SethActions.h"
#include "SethMultipliers.h"
#include "SethPolicy.h"
#include "SethShared.h"
#include "SethTriggers.h"
#include "ShamanActions.h"

using namespace SethShared;

namespace
{
bool HasBubble(Unit* ikiss) { return ikiss->HasAura(Id(SethSpells::SPELL_ARCANE_BUBBLE)); }
}  // namespace

bool TalonKingIkissShouldBeTankedTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    return ikiss && ikiss->GetVictim() == bot && bot->IsWithinMeleeRange(ikiss);
}

bool TalonKingIkissRangedShouldStayNearTankTrigger::IsActive()
{
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    return ikiss && SethekkHalls::StaysNearVictim(true, HasBubble(ikiss), bot->IsWithinLOSInMap(ikiss));
}

// Arcane Bubble goes up 1s before Arcane Explosion starts and outlasts its cast
bool TalonKingIkissBossCastingArcaneExplosionTrigger::IsActive()
{
    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    return ikiss && HasBubble(ikiss);
}

bool TalonKingIkissOutOfLosTrigger::IsActive()
{
    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    return ikiss && SethekkHalls::ReturnsToLos(HasBubble(ikiss), bot->IsWithinLOSInMap(ikiss));
}

bool TalonKingIkissPositionTankNearPillarAction::Execute(Event /*event*/)
{
    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    if (!ikiss)
        return false;

    if (SethekkHalls::ResetsPillarRun(ikiss->GetHealthPct() <= EncounterHelpers::BOSS_ENGAGED_HEALTH_PCT))
        _hasReachedPillarPosition = false;

    if (_hasReachedPillarPosition)
        return false;

    Position const& position = PILLAR_POSITION;
    if (bot->GetExactDist2d(position) <= SethekkHalls::PillarArrivalDistance)
    {
        _hasReachedPillarPosition = true;
        return false;
    }

    float moveX;
    float moveY;
    bool backwards;
    if (!EncounterHelpers::GetStepToPosition(bot, position, SethekkHalls::PillarArrivalDistance, ikiss, moveX, moveY,
                                             backwards))
        return false;

    return MoveTo(SETH_MAP_ID, moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, backwards);
}

bool TalonKingIkissRangedMoveNearTankAction::Execute(Event /*event*/)
{
    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    if (!ikiss || !ikiss->GetVictim())
        return false;

    Player* victim = ikiss->GetVictim()->ToPlayer();
    if (!victim || !SethekkHalls::ClosesOnVictim(bot->GetExactDist2d(victim)))
        return false;

    float const angle = victim->GetAngle(bot);
    float const destX = victim->GetPositionX() + std::cos(angle) * SethekkHalls::VictimStayDistance;
    float const destY = victim->GetPositionY() + std::sin(angle) * SethekkHalls::VictimStayDistance;

    return MoveTo(SETH_MAP_ID, destX, destY, victim->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool TalonKingIkissLosArcaneExplosionAction::Execute(Event /*event*/)
{
    Position const& pillarCenter = PILLAR_CENTER;
    float const botAngle = pillarCenter.GetAngle(bot);
    float const distToPillar = bot->GetExactDist2d(pillarCenter);

    return MoveToPillar(pillarCenter, botAngle, distToPillar) || MoveAroundPillar(pillarCenter, distToPillar);
}

bool TalonKingIkissLosArcaneExplosionAction::MoveToPillar(Position const& pillarCenter, float botAngle,
                                                          float distToPillar)
{
    if (SethekkHalls::OnPillarRing(distToPillar))
        return false;

    float const targetRadius = SethekkHalls::PillarRingRadius(distToPillar);
    float const moveX = pillarCenter.GetPositionX() + targetRadius * std::cos(botAngle);
    float const moveY = pillarCenter.GetPositionY() + targetRadius * std::sin(botAngle);

    bot->CastStop();
    return MoveTo(SETH_MAP_ID, moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool TalonKingIkissLosArcaneExplosionAction::MoveAroundPillar(Position const& pillarCenter, float distToPillar)
{
    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    if (!ikiss)
        return false;

    float const botAngle = pillarCenter.GetAngle(bot);
    float const turn = SethekkHalls::PillarTurn(botAngle, SethekkHalls::HidingAngle(pillarCenter.GetAngle(ikiss)));
    if (SethekkHalls::PillarTurnDone(turn))
        return false;

    float const stepAngle = SethekkHalls::PillarStepAngle(botAngle, turn);
    float const moveX = pillarCenter.GetPositionX() + distToPillar * std::cos(stepAngle);
    float const moveY = pillarCenter.GetPositionY() + distToPillar * std::sin(stepAngle);

    bot->CastStop();
    return MoveTo(SETH_MAP_ID, moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool TalonKingIkissMoveWithinLosAction::Execute(Event /*event*/)
{
    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    if (!ikiss)
        return false;

    Position const& pillarCenter = PILLAR_CENTER;
    float const botAngle = pillarCenter.GetAngle(bot);
    float const turn = SethekkHalls::PillarTurn(botAngle, pillarCenter.GetAngle(ikiss));
    if (SethekkHalls::PillarTurnDone(turn))
        return false;

    float const radius = bot->GetExactDist2d(pillarCenter);
    float const stepAngle = SethekkHalls::PillarStepAngle(botAngle, turn);
    float const moveX = pillarCenter.GetPositionX() + radius * std::cos(stepAngle);
    float const moveY = pillarCenter.GetPositionY() + radius * std::sin(stepAngle);

    return MoveTo(SETH_MAP_ID, moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, false);
}

float TalonKingIkissDelayBloodlustAndHeroismMultiplier::GetValue(Action* action)
{
    if (botAI->GetState() == BOT_STATE_NON_COMBAT || bot->getClass() != CLASS_SHAMAN)
        return 1.0f;

    bool const lust = dynamic_cast<CastHeroismAction*>(action) || dynamic_cast<CastBloodlustAction*>(action);
    if (!lust)
        return 1.0f;

    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    if (!ikiss)
        return 1.0f;

    return SethekkHalls::IkissAllowsBloodlust(lust, ikiss->GetHealthPct() <= EncounterHelpers::BOSS_ENGAGED_HEALTH_PCT)
               ? 1.0f
               : 0.0f;
}

float TalonKingIkissControlMovementMultiplier::GetValue(Action* action)
{
    SethekkHalls::IkissMove shape;
    shape.alwaysBlocked = dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FleeAction*>(action) ||
                          dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action);
    shape.movement = dynamic_cast<MovementAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action);
    if (!shape.alwaysBlocked && !shape.movement)
        return 1.0f;

    shape.exempt = dynamic_cast<TankFaceAction*>(action) || dynamic_cast<SetBehindTargetAction*>(action) ||
                   dynamic_cast<TalonKingIkissLosArcaneExplosionAction*>(action);
    if (shape.exempt)
        return 1.0f;

    Unit* ikiss = AI_VALUE2(Unit*, "find target", "talon king ikiss");
    if (!ikiss)
        return 1.0f;

    return SethekkHalls::IkissAllowsMove(HasBubble(ikiss), shape) ? 1.0f : 0.0f;
}
