/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_THREATSTRATEGY_H
#define PLAYERBOTS_THREATSTRATEGY_H

#include "Action.h"
#include "Strategy.h"
#include "ThreatHoldPolicy.h"
#include "Trigger.h"

class PlayerbotAI;

class ThreatMultiplier : public Multiplier
{
public:
    ThreatMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "threat") {}

    float GetValue(Action* action) override;

private:
    BotThreat::TankHoldClock tankHold;  // how long the tank has held each mob, before this damage dealer opens on it
    BotThreat::UsualHit usualHit;       // what this damage dealer's hits come to, in threat
};

class ThreatStrategy : public Strategy
{
public:
    ThreatStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "threat"; }
};

// A melee damage dealer swinging at a mob on the tank with its threat near the tank's (ThreatHoldPolicy.h PauseSwings)
class SwingsNearTankThreatTrigger : public Trigger
{
public:
    SwingsNearTankThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "swings near tank threat") {}

    bool IsActive() override;
};

class HoldSwingsAction : public Action
{
public:
    HoldSwingsAction(PlayerbotAI* botAI) : Action(botAI, "hold swings") {}

    bool Execute(Event event) override;
};

class FocusMultiplier : public Multiplier
{
public:
    FocusMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "focus") {}

    float GetValue(Action* action) override;
};

class FocusStrategy : public Strategy
{
public:
    FocusStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    std::string const getName() override { return "focus"; }
};

#endif
