/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Hungarfen bot tactics: walk off ripening mushrooms, leave Foul Spores range, and don't waste damage on mushrooms.

#include <cmath>

#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "UBActions.h"
#include "UBMultipliers.h"
#include "UBPolicy.h"
#include "UBShared.h"
#include "UBTriggers.h"

using namespace UnderbogHungarfen;

bool UBFoulSporesTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "hungarfen");
    return boss && boss->HasAura(SPELL_FOUL_SPORES);
}

bool UBSporeCloudDangerTrigger::IsActive()
{
    auto const& mushrooms = AI_VALUE_REF(GuidVector, "ub mushrooms");
    return GetNearestDangerousMushroom(bot, mushrooms, MushroomDangerRange(bot)) != nullptr;
}

bool UBRetreatFromFoulSporesAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "hungarfen");
    if (!boss)
        return false;

    float const safeDistance =
        Underbog::FoulSporesSafeDistance(MaxEffectRadius(SPELL_FOUL_SPORES, Underbog::FoulSporesRadiusFallback));
    float const currentDistance = bot->GetDistance2d(boss);
    if (!Underbog::RetreatsFromFoulSpores(currentDistance, safeDistance))
        return false;

    float const moveDist = Underbog::FoulSporesRetreatStep(currentDistance, safeDistance);
    float const awayAngle = boss->GetAngle(bot);

    auto const& mushrooms = AI_VALUE_REF(GuidVector, "ub mushrooms");

    for (float delta : Underbog::FoulSporesRetreatAngles)
    {
        float const angle = awayAngle + delta;
        float dx = bot->GetPositionX() + std::cos(angle) * moveDist;
        float dy = bot->GetPositionY() + std::sin(angle) * moveDist;
        float dz = bot->GetPositionZ();
        if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                            bot->GetPositionZ(), dx, dy, dz))
            continue;

        if (RetreatPathUnsafe(bot, mushrooms, dx, dy))
            continue;

        if (MoveTo(bot->GetMapId(), dx, dy, dz, false, false, true, true, MovementPriority::MOVEMENT_COMBAT))
            return true;
    }

    return MoveAway(boss, moveDist);
}

bool UBVacateSporeCloudAction::Execute(Event /*event*/)
{
    float const dangerRange = MushroomDangerRange(bot);
    auto const& mushrooms = AI_VALUE_REF(GuidVector, "ub mushrooms");
    Creature* mushroom = GetNearestDangerousMushroom(bot, mushrooms, dangerRange);
    if (!mushroom)
        return false;

    Unit* boss = PlayerbotAI::IsTank(bot) ? AI_VALUE2(Unit*, "find target", "hungarfen") : nullptr;
    if (Underbog::TankBacksOffMushroom(PlayerbotAI::IsTank(bot), boss && boss->GetVictim() == bot))
        return MoveAway(mushroom, Underbog::MushroomBackOffStep(dangerRange, bot->GetDistance2d(mushroom)));

    return FleePosition(mushroom->GetPosition(), dangerRange);
}

float HungarfenFoulSporesMultiplier::GetValue(Action* action)
{
    bool const movement = dynamic_cast<MovementAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action);
    if (!movement)
        return 1.0f;

    bool const escape = dynamic_cast<UBRetreatFromFoulSporesAction*>(action) ||
                        dynamic_cast<UBVacateSporeCloudAction*>(action) || dynamic_cast<AttackAction*>(action);
    if (escape)
        return 1.0f;

    Unit* boss = AI_VALUE2(Unit*, "find target", "hungarfen");
    if (!boss)
        return 1.0f;

    return Underbog::FoulSporesAllows(boss->HasAura(SPELL_FOUL_SPORES), movement, escape) ? 1.0f : 0.0f;
}

float HungarfenMushroomIgnoreMultiplier::GetValue(Action* action)
{
    Underbog::MushroomAction shape;
    shape.aoe = action->getThreatType() == Action::ActionThreatType::Aoe;
    shape.attackAnything = dynamic_cast<AttackAnythingAction*>(action) != nullptr;
    if (!shape.aoe && !shape.attackAnything)
        return 1.0f;

    shape.heal = dynamic_cast<CastHealingSpellAction*>(action) != nullptr;
    bool const dps = PlayerbotAI::IsDps(bot);
    if (shape.aoe && (shape.heal || !dps))
        return 1.0f;

    auto const& mushrooms = AI_VALUE_REF(GuidVector, "ub mushrooms");
    bool const mushroomsUp = AnyMushroomAlive(bot, mushrooms);
    if (mushroomsUp && !shape.aoe)
        shape.grindTargetIsMushroom = IsMushroom(AI_VALUE(Unit*, "grind target"));

    return Underbog::MushroomsAllow(mushroomsUp, dps, shape) ? 1.0f : 0.0f;
}
