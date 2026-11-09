/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ThreatStrategy.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "Map.h"
#include "Playerbots.h"
#include "Timer.h"

float ThreatMultiplier::GetValue(Action* action)
{
    if (AI_VALUE(bool, "neglect threat"))
    {
        return 1.0f;
    }

    // The swing that starts auto attack makes threat too, though it is no spell
    bool const melee = dynamic_cast<MeleeAction*>(action) != nullptr;
    if (!action || (action->getThreatType() == Action::ActionThreatType::None && !melee))
        return 1.0f;

    if (!AI_VALUE(bool, "group"))
        return 1.0f;

    // Hold off a mob until the tank has taken it and its first swings put some threat between them
    // (ThreatHoldPolicy.h)
    Unit* target = AI_VALUE(Unit*, "current target");
    // A mob that fixates where it likes is nobody's to hold: kill it (ThreatHoldPolicy.h TankKeeps)
    if (target && target->IsAlive() && !BotThreat::TankKeeps(target->GetEntry()))
        return 1.0f;
    if (target && target->IsAlive() && !PlayerbotAI::IsTank(bot))
    {
        Unit* tank = AI_VALUE(Unit*, "group main tank");
        bool const tankInFight = tank && tank != bot && tank->IsAlive() && tank->IsInCombat() &&
                                 tank->GetMap() == bot->GetMap() && tank->GetExactDist2d(bot) < 60.0f;
        Unit* victim = target->GetVictim();
        Player* holder = victim ? victim->ToPlayer() : nullptr;
        BotThreat::MobOn on = BotThreat::MobOn::Other;
        if (!tankInFight)
            on = BotThreat::MobOn::NoTankInFight;
        else if (victim == bot)
            on = BotThreat::MobOn::Self;
        // The tank's hold starts once the mob is at the tank, or stands (a caster): one still running in from the pull
        // has only the pull's threat on the tank, and a rogue that opened on it as it ran took it, 2000-5000 threat to
        // the tank's 20-300 at five seconds (solo-wow role check, Ramparts and Deadmines, 2026-11-07)
        else if (holder && PlayerbotAI::IsTank(holder) && holder->IsInSameGroupWith(bot) &&
                 (!target->isMoving() || target->IsWithinMeleeRange(holder)))
            on = BotThreat::MobOn::Tank;
        if (!tankHold.MayOpen(target->GetGUID().GetRawValue(), on, getMSTime()))
            return 0.0f;
        // Held long enough, but not with threat enough to take the next hit (ThreatHoldPolicy.h)
        float const own = target->GetThreatMgr().GetThreat(bot);
        usualHit.Saw(target->GetGUID().GetRawValue(), own);
        if (!melee && on == BotThreat::MobOn::Tank &&
            BotThreat::WouldTurnIt(target->GetThreatMgr().GetThreat(holder), own, usualHit.Threat()))
            return 0.0f;
        // Swings stopped near the tank's threat start again only once the tank has pulled clear
        if (melee && on == BotThreat::MobOn::Tank &&
            !BotThreat::MaySwing(target->GetThreatMgr().GetThreat(holder), own))
            return 0.0f;
    }

    if (melee)
        return 1.0f;

    if (action->getThreatType() == Action::ActionThreatType::Aoe)
    {
        uint8 threat = AI_VALUE2(uint8, "threat", "aoe");
        if (threat >= 50)
            return 0.0f;
    }

    uint8 threat = AI_VALUE2(uint8, "threat", "current target");
    if (threat >= 80)
        return 0.0f;

    return 1.0f;
}

void ThreatStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("swings near tank threat", {NextAction("hold swings", ACTION_HIGH + 9)}));
}

bool SwingsNearTankThreatTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot) || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING) || AI_VALUE(bool, "neglect threat"))
        return false;
    Unit* mob = bot->GetVictim();
    if (mob && !BotThreat::TankKeeps(mob->GetEntry()))
        return false;
    Unit* holder = mob && mob->IsAlive() ? mob->GetVictim() : nullptr;
    Player* tank = holder ? holder->ToPlayer() : nullptr;
    if (!tank || tank == bot || !PlayerbotAI::IsTank(tank) || !tank->IsInSameGroupWith(bot))
        return false;
    return BotThreat::PauseSwings(mob->GetThreatMgr().GetThreat(tank), mob->GetThreatMgr().GetThreat(bot));
}

// Stops the swings and keeps the target: its strikes go on as the threat hold allows, and "melee" starts the swings
// again once the tank has pulled clear (ThreatMultiplier)
bool HoldSwingsAction::Execute(Event /*event*/)
{
    bot->AttackStop();
    return true;
}

void ThreatStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new ThreatMultiplier(botAI));
}

float FocusMultiplier::GetValue(Action* action)
{
    if (!action)
    {
        return 1.0f;
    }
    if (action->getThreatType() == Action::ActionThreatType::Aoe && !dynamic_cast<CastHealingSpellAction*>(action))
    {
        return 0.0f;
    }
    if (dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
    {
        return 0.0f;
    }
    return 1.0f;
}

void FocusStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FocusMultiplier(botAI));
}
