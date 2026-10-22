/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// A bot tank leads the dungeon: heads for the next boss, waits for health and mana, marks and pulls; healers call oom.

#ifndef _PLAYERBOTS_DUNGEON_LEAD_ACTIONS_H
#define _PLAYERBOTS_DUNGEON_LEAD_ACTIONS_H

#include "MovementActions.h"
#include "Multiplier.h"
#include "PullActions.h"
#include "Strategy.h"
#include "Trigger.h"

class DungeonLeadStrategy : public Strategy
{
public:
    DungeonLeadStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    std::string const getName() override { return "dungeon lead"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    void InitMultipliers(std::vector<Multiplier*>& multipliers) override;
};

class DungeonLeadTrigger : public Trigger
{
public:
    DungeonLeadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "dungeon lead") {}

    bool IsActive() override;
};

class LeadHealerTrigger : public Trigger
{
public:
    LeadHealerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lead healer") {}

    bool IsActive() override;
};

class LeadNeedsManaTrigger : public Trigger
{
public:
    LeadNeedsManaTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lead needs mana") {}

    bool IsActive() override;
};

// Followers stay at the gathering spot while the tank pulls, instead of walking after it into the pack.
class LeadHoldMultiplier : public Multiplier
{
public:
    LeadHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "lead hold") {}

    float GetValue(Action* action) override;
};

class LeadTalkAction : public Action
{
public:
    LeadTalkAction(PlayerbotAI* botAI) : Action(botAI, "lead talk") {}

    bool isUseful() override;
    bool Execute(Event event) override;
};

class LeadAdvanceAction : public MovementAction
{
public:
    LeadAdvanceAction(PlayerbotAI* botAI) : MovementAction(botAI, "lead advance") {}

    bool isUseful() override;
    bool Execute(Event event) override;
};

class LeadPullAction : public PullRequestAction
{
public:
    LeadPullAction(PlayerbotAI* botAI) : PullRequestAction(botAI, "lead pull") {}

    bool isUseful() override;
    bool Execute(Event event) override;

protected:
    Unit* GetPullTarget(Event event) override;

private:
    ObjectGuid _target;
};

class LeadMarksAction : public Action
{
public:
    LeadMarksAction(PlayerbotAI* botAI) : Action(botAI, "lead marks") {}

    bool isUseful() override;
    bool Execute(Event event) override;
};

class LeadOverpullAction : public Action
{
public:
    LeadOverpullAction(PlayerbotAI* botAI) : Action(botAI, "lead overpull") {}

    bool isUseful() override;
    bool Execute(Event event) override;
};

class LeadHealerCallAction : public Action
{
public:
    LeadHealerCallAction(PlayerbotAI* botAI) : Action(botAI, "lead healer call") {}

    bool Execute(Event event) override;
};

#endif
