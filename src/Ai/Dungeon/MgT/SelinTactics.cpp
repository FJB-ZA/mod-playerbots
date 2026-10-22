/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Selin Fireheart bot tactics: everyone kills the crystal he drains, and nobody leaves the room past his leash.

#include "AttackersValue.h"
#include "MgTActions.h"
#include "MgTMultipliers.h"
#include "MgTShared.h"
#include "MgTTriggers.h"
#include "MovementActions.h"
#include "Playerbots.h"

namespace MagistersTerrace
{
Unit* GetSelin(Player* bot)
{
    return GET_PLAYERBOT_AI(bot)->GetAiObjectContext()->GetValue<Unit*>("find target", "selin fireheart")->Get();
}

// Crystals are unselectable until he starts draining one, so the valid one is the active one.
Unit* GetActiveFelCrystal(Player* bot)
{
    if (!GetSelin(bot))
        return nullptr;

    std::vector<Unit*> crystals;
    CollectCreaturesByEntry(bot, {NPC_FEL_CRYSTAL}, CrystalScanRange, crystals);

    Unit* best = nullptr;
    float bestDist = CrystalScanRange;
    for (Unit* crystal : crystals)
    {
        if (!AttackersValue::IsValidTarget(crystal, bot))
            continue;

        float const dist = bot->GetExactDist2d(crystal);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = crystal;
        }
    }

    return best;
}
}  // namespace MagistersTerrace

bool MgTCrystalActiveTrigger::IsActive() { return !AI_VALUE(ObjectGuid, "mgt crystal target").IsEmpty(); }

bool MgTOutOfRoomTrigger::IsActive()
{
    if (!MagistersTerrace::OutsideSelinRoom(bot->GetPositionX()))
        return false;

    return MagistersTerrace::GetSelin(bot) != nullptr;
}

bool MgTKillCrystalAction::Execute(Event)
{
    Unit* crystal = botAI->GetUnit(AI_VALUE(ObjectGuid, "mgt crystal target"));
    if (!crystal || AI_VALUE(Unit*, "current target") == crystal)
        return false;

    return Attack(crystal);
}

bool MgTReturnToRoomAction::Execute(Event)
{
    float x = MagistersTerrace::SelinRegroupX;
    float y = bot->GetPositionY();
    MagistersTerrace::ClampIntoRoom(x, y);

    float const bx = bot->GetPositionX();
    float const by = bot->GetPositionY();
    float const bz = bot->GetPositionZ();

    float const dist = bot->GetExactDist2d(x, y);
    if (dist < MagistersTerrace::ReturnArrived)
        return false;

    float const step = MagistersTerrace::ReturnStep(dist);
    float const moveX = bx + ((x - bx) / dist) * step;
    float const moveY = by + ((y - by) / dist) * step;

    float moveZ = bot->GetMapHeight(moveX, moveY, bz + MagistersTerrace::GroundSearchUp, true,
                                    MagistersTerrace::GroundSearchDown);
    if (!MagistersTerrace::OnSameTier(moveZ, bz))
        moveZ = bz;

    return MoveTo(bot->GetMapId(), moveX, moveY, moveZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT,
                  true, false);
}

float MgTCrystalFocusMultiplier::GetValue(Action* action)
{
    if (dynamic_cast<MgTKillCrystalAction*>(action))
        return 1.0f;

    if (!MagistersTerrace::ActionPullsOffFocus(bot, action) || PlayerbotAI::IsHeal(bot))
        return 1.0f;

    bool const crystalUp = !AI_VALUE(ObjectGuid, "mgt crystal target").IsEmpty();
    return MagistersTerrace::CrystalFocusAllows(true, false, crystalUp) ? 1.0f : 0.0f;
}

float MgTRoomLeashMultiplier::GetValue(Action* action)
{
    bool const flight = dynamic_cast<FleeAction*>(action) || dynamic_cast<FleeWithPetAction*>(action) ||
                        dynamic_cast<RunAwayAction*>(action);
    if (!flight)
        return 1.0f;

    bool const selinUp = MagistersTerrace::GetSelin(bot) != nullptr;
    return MagistersTerrace::RoomLeashAllows(true, bot->GetPositionX(), selinUp) ? 1.0f : 0.0f;
}
