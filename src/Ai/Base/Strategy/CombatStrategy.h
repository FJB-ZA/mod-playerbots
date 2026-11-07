/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_COMBATSTRATEGY_H
#define PLAYERBOTS_COMBATSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

class CombatStrategy : public Strategy
{
public:
    CombatStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    uint32 GetType() const override { return STRATEGY_TYPE_COMBAT; }
};

class AvoidAoeStrategy : public Strategy
{
public:
    explicit AvoidAoeStrategy(PlayerbotAI* botAI);
    const std::string getName() override { return "avoid aoe"; }
    std::vector<NextAction> getDefaultActions() override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

class TankFaceStrategy : public Strategy
{
public:
    explicit TankFaceStrategy(PlayerbotAI* botAI);
    const std::string getName() override { return "tank face"; }
    std::vector<NextAction> getDefaultActions() override;
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

class CombatFormationStrategy : public Strategy
{
public:
    CombatFormationStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    const std::string getName() override { return "formation"; }
    std::vector<NextAction> getDefaultActions() override;
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

// Out of combat, the back line's step to the tank once the tank is fighting. A healer is in combat only once something
// hits it or it heals, so it stayed where the pull left it, often round a corner from the tank: in the solo-wow role
// check (2026-11-07) a healer out of the tank's sight at 8-20 yards was not in combat, held still by the lead's hold
class BackLineStrategy : public Strategy
{
public:
    BackLineStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    const std::string getName() override { return "back line"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
