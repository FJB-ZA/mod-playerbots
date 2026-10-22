/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Oculus drake tactics: bots take the seats the master leaves, fly in formation and run each drake's rotation.

#include "MovementActions.h"
#include "OCActions.h"
#include "OCMultipliers.h"
#include "OCPolicy.h"
#include "OCTriggers.h"
#include "Playerbots.h"

namespace
{
Oculus::Drake DrakeOf(Unit* vehicle)
{
    switch (vehicle->GetEntry())
    {
        case NPC_AMBER_DRAKE:
            return Oculus::Drake::Amber;
        case NPC_EMERALD_DRAKE:
            return Oculus::Drake::Emerald;
        case NPC_RUBY_DRAKE:
            return Oculus::Drake::Ruby;
        default:
            return Oculus::Drake::None;
    }
}

// The bot's place among the group's bots, in group order; -1 if it isn't one.
int BotRank(PlayerbotAI* botAI, Player* bot)
{
    int rank = 0;
    for (Player* player : botAI->GetAllPlayersInGroup())
    {
        if (!player || !player->IsInWorld() || player->IsDuringRemoveFromWorld())
            continue;

        WorldSession* session = player->GetSession();
        if (!session || !session->IsHeadless())
            continue;

        if (player == bot)
            return rank;
        ++rank;
    }
    return -1;
}

bool MasterOnDrake(PlayerbotAI* botAI)
{
    Player* master = botAI->GetMaster();
    return master && master->GetVehicleBase();
}
}  // namespace

bool DrakeMountTrigger::IsActive()
{
    return botAI->GetMaster() && Oculus::MountsDrake(MasterOnDrake(botAI), bot->GetVehicleBase());
}

bool DrakeDismountTrigger::IsActive()
{
    return botAI->GetMaster() && Oculus::DismountsDrake(MasterOnDrake(botAI), bot->GetVehicleBase());
}

bool GroupFlyingTrigger::IsActive()
{
    return botAI->GetMaster() && Oculus::FliesWithGroup(MasterOnDrake(botAI), bot->GetVehicleBase());
}

bool DrakeCombatTrigger::IsActive() { return !AI_VALUE(GuidVector, "possible targets").empty(); }

bool MountDrakeAction::isPossible() { return bot->GetMapId() == OCULUS_MAP_ID; }

bool MountDrakeAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();
    if (!master)
        return false;

    Unit* vehicle = master->GetVehicleBase();
    if (!vehicle)
        return false;

    Oculus::Drake drake = Oculus::AssignedDrake(DrakeOf(vehicle), BotRank(botAI, bot));
    if (drake == Oculus::Drake::None)
        return false;

    uint32 assignedItem = DRAKE_ITEMS[static_cast<int>(drake)];
    for (uint32 itemId : DRAKE_ITEMS)
    {
        Item* item = bot->GetItemByEntry(itemId);
        if (!item)
            continue;

        if (itemId == assignedItem)
            return UseItemAuto(item);

        // Seats changed since this essence was handed out
        uint32 count = 1;
        bot->DestroyItemCount(item, count, true);
        break;
    }

    bot->AddItem(assignedItem, 1);
    return false;
}

bool DismountDrakeAction::Execute(Event /*event*/)
{
    if (!bot->GetVehicle())
        return false;

    bot->ExitVehicle();
    return true;
}

bool OccFlyDrakeAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();
    if (!master)
        return false;

    Unit* masterVehicle = master->GetVehicleBase();
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || !masterVehicle)
        return false;

    MotionMaster* mm = vehicleBase->GetMotionMaster();
    Unit* boss = AI_VALUE2(Unit*, "find target", "ley-guardian eregos");
    if (Oculus::EngagesEregos(boss != nullptr, boss && boss->HasAura(SPELL_PLANAR_SHIFT)))
    {
        mm->Clear(false);
        if (vehicleBase->GetExactDist(boss) > Oculus::DrakeEngageRange)
        {
            // MoveForwards takes the distance from the target, not the length of the flight
            mm->MoveForwards(boss, Oculus::DrakeApproachDistance);
            vehicleBase->SendMovementFlagUpdate();
            return true;
        }

        vehicleBase->SetFacingToObject(boss);
        mm->MoveIdle();
        vehicleBase->SendMovementFlagUpdate();
        return false;
    }

    if (vehicleBase->GetExactDist(masterVehicle) <= Oculus::FormationRegroupDistance)
        return false;

    vehicleBase->SetCanFly(true);
    mm->MoveFollow(masterVehicle, Oculus::FormationRadius, Oculus::FormationAngle(botAI->GetGroupSlotIndex(bot)));
    vehicleBase->SendMovementFlagUpdate();
    return true;
}

bool OccDrakeAttackAction::Execute(Event /*event*/)
{
    vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
    {
        GuidVector npcs = AI_VALUE(GuidVector, "possible targets");
        for (ObjectGuid const& guid : npcs)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && unit->IsInCombat())
            {
                target = unit;
                break;
            }
        }
    }

    if (!target)
        return false;

    switch (DrakeOf(vehicleBase))
    {
        case Oculus::Drake::Amber:
            return AmberDrakeAction(target);
        case Oculus::Drake::Emerald:
            return EmeraldDrakeAction(target);
        case Oculus::Drake::Ruby:
            return RubyDrakeAction(target);
        default:
            return false;
    }
}

bool OccDrakeAttackAction::CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown)
{
    if (!botAI->CanCastVehicleSpell(spellId, target) || !botAI->CastVehicleSpell(spellId, target))
        return false;

    vehicleBase->AddSpellCooldown(spellId, 0, cooldown);
    return true;
}

bool OccDrakeAttackAction::AmberDrakeAction(Unit* target)
{
    Oculus::AmberState state;
    if (Aura* shockCharges = target->GetAura(SPELL_SHOCK_CHARGE, vehicleBase->GetGUID()))
        state.shockCharges = shockCharges->GetStackAmount();
    state.enraged = target->HasAura(SPELL_ENRAGED_ASSAULT);
    state.stopTimeOnTarget = target->HasAura(SPELL_STOP_TIME);
    state.stopTimeReady = !vehicleBase->HasSpellCooldown(SPELL_STOP_TIME);
    state.channelingRift = vehicleBase->FindCurrentSpellBySpellId(SPELL_TEMPORAL_RIFT) != nullptr;

    switch (Oculus::AmberNext(state))
    {
        case Oculus::AmberSpell::ShockLance:
            return CastDrakeSpellAction(target, SPELL_SHOCK_LANCE, 0);
        case Oculus::AmberSpell::StopTime:
            return CastDrakeSpellAction(target, SPELL_STOP_TIME, Oculus::StopTimeCooldownMs);
        case Oculus::AmberSpell::TemporalRift:
            return CastDrakeSpellAction(target, SPELL_TEMPORAL_RIFT, 0);
        default:
            return false;
    }
}

bool OccDrakeAttackAction::EmeraldDrakeAction(Unit* target)
{
    Oculus::EmeraldState state;
    if (Aura* poison = target->GetAura(SPELL_LEECHING_POISON, vehicleBase->GetGUID()))
    {
        state.poisonUp = true;
        state.poisonStacks = poison->GetStackAmount();
        state.poisonMsLeft = poison->GetDuration();
    }
    state.touchReady = !vehicleBase->HasSpellCooldown(SPELL_TOUCH_THE_NIGHTMARE);
    state.touchOnTarget = target->HasAura(SPELL_TOUCH_THE_NIGHTMARE);
    state.healthAbove90 = vehicleBase->HealthAbovePct(90);

    Unit* healingTarget = nullptr;
    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        Unit* unit = botAI->GetUnit(member);
        if (!unit || member == bot->GetGUID())
            continue;

        Unit* drake = unit->GetVehicleBase();
        if (!drake || drake->IsFullHealth())
            continue;

        if (Oculus::PrefersHealTarget(healingTarget != nullptr, drake->GetHealthPct(),
                                      healingTarget ? healingTarget->GetHealthPct() : 0.0f))
            healingTarget = drake;
    }

    Spell* funnel = vehicleBase->FindCurrentSpellBySpellId(SPELL_DREAM_FUNNEL);
    state.funneling = funnel != nullptr;
    if (healingTarget)
    {
        state.healTarget = true;
        state.funnelOnHealTarget = funnel && funnel->m_targets.GetUnitTarget() == healingTarget;
        state.healTargetInRange = vehicleBase->GetExactDist(healingTarget) <= Oculus::DrakeEngageRange;
    }

    switch (Oculus::EmeraldNext(state))
    {
        case Oculus::EmeraldSpell::LeechingPoison:
            return CastDrakeSpellAction(target, SPELL_LEECHING_POISON, 0);
        case Oculus::EmeraldSpell::TouchTheNightmare:
            return CastDrakeSpellAction(target, SPELL_TOUCH_THE_NIGHTMARE, Oculus::TouchTheNightmareCooldownMs);
        case Oculus::EmeraldSpell::DreamFunnel:
            return CastDrakeSpellAction(healingTarget, SPELL_DREAM_FUNNEL, 0);
        case Oculus::EmeraldSpell::FlyToHealTarget:
        {
            MotionMaster* mm = vehicleBase->GetMotionMaster();
            mm->Clear(false);
            mm->MoveForwards(healingTarget, Oculus::DrakeApproachDistance);
            vehicleBase->SendMovementFlagUpdate();
            return false;
        }
        default:
            return false;
    }
}

bool OccDrakeAttackAction::RubyDrakeAction(Unit* target)
{
    Oculus::RubyState state;
    if (Aura* evasiveCharges = vehicleBase->GetAura(SPELL_EVASIVE_CHARGES))
        state.evasiveCharges = evasiveCharges->GetStackAmount();
    if (Aura* evasiveManeuvers = vehicleBase->GetAura(SPELL_EVASIVE_MANEUVERS))
    {
        state.maneuversUp = true;
        state.maneuversMsLeft = evasiveManeuvers->GetDuration();
    }
    state.martyrReady = !vehicleBase->HasSpellCooldown(SPELL_MARTYR);
    state.maneuversReady = !vehicleBase->HasSpellCooldown(SPELL_EVASIVE_MANEUVERS);

    switch (Oculus::RubyNext(state))
    {
        case Oculus::RubySpell::Martyr:
            return CastDrakeSpellAction(vehicleBase, SPELL_MARTYR, Oculus::MartyrCooldownMs);
        case Oculus::RubySpell::EvasiveManeuvers:
            return CastDrakeSpellAction(vehicleBase, SPELL_EVASIVE_MANEUVERS, Oculus::EvasiveManeuversCooldownMs);
        default:
            return CastDrakeSpellAction(target, SPELL_SEARING_WRATH, 0);
    }
}

float MountingDrakeMultiplier::GetValue(Action* action)
{
    if (!botAI->GetMaster())
        return 1.0f;

    bool allowed = Oculus::MountingAllows(bot->GetMapId() == OCULUS_MAP_ID, MasterOnDrake(botAI), bot->GetVehicleBase(),
                                          dynamic_cast<MountDrakeAction*>(action));
    return allowed ? 1.0f : 0.0f;
}

float OccFlyingMultiplier::GetValue(Action* action)
{
    bool allowed =
        Oculus::FlyingAllows(bot->GetMapId() == OCULUS_MAP_ID, bot->GetVehicleBase(),
                             dynamic_cast<MovementAction*>(action), dynamic_cast<OccFlyDrakeAction*>(action));
    return allowed ? 1.0f : 0.0f;
}
