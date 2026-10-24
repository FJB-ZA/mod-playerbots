/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Mythic+ affix tactics for any dungeon: damage dealers kill Explosive Orbs before they burst on the group.

#ifndef _PLAYERBOTS_MYTHIC_PLUS_TACTICS_H
#define _PLAYERBOTS_MYTHIC_PLUS_TACTICS_H

#include "AttackAction.h"
#include "Multiplier.h"
#include "Strategy.h"
#include "Trigger.h"

class MythicPlusStrategy : public Strategy
{
public:
    MythicPlusStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "mythic plus"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

class ExplosiveOrbTrigger : public Trigger
{
public:
    ExplosiveOrbTrigger(PlayerbotAI* botAI) : Trigger(botAI, "explosive orb") {}
    bool IsActive() override;
};

class AttackExplosiveOrbAction : public AttackAction
{
public:
    AttackExplosiveOrbAction(PlayerbotAI* botAI) : AttackAction(botAI, "attack explosive orb") {}
    bool Execute(Event event) override;
};

// Keeps damage dealers from assisting back onto the pack while an orb is up
class ExplosiveOrbMultiplier : public Multiplier
{
public:
    ExplosiveOrbMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "explosive orb") {}
    float GetValue(Action* action) override;
};

#endif
