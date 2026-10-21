// Arena playbook strategy for bots: target calls, PvP trinket, interrupts, peels, crowd control, kiting and line of sight.

#ifndef PLAYERBOTS_LIVING_ARENA_ACTIONS_H
#define PLAYERBOTS_LIVING_ARENA_ACTIONS_H

#include <unordered_map>

#include "LivingArenaPolicy.h"
#include "MovementActions.h"
#include "Strategy.h"
#include "Trigger.h"

class LivingArenaTrigger : public Trigger
{
public:
    LivingArenaTrigger(PlayerbotAI* ai) : Trigger(ai, "living arena") {}
    bool IsActive() override;
};

// Shared helpers; every playbook decision is rolled against the bot's skill so weaker bots miss more.
class LivingArenaAction : public MovementAction
{
public:
    LivingArenaAction(PlayerbotAI* ai, std::string const name) : MovementAction(ai, name) {}

protected:
    bool InFight();
    std::vector<Player*> Enemies();
    std::vector<Player*> Allies();
    uint32 Skill();
    LivingArena::Tier Tier() { return LivingArena::TierFor(Skill()); }
    bool Roll(uint32 skill) { return urand(1, 100) <= LivingArena::Adherence(skill); }
    bool TryCast(std::vector<char const*> const& spells, Unit* target);
    static bool IsControlled(Unit* unit);
    static bool IsImmune(Unit* unit);
};

class LivingArenaCallAction : public LivingArenaAction
{
public:
    LivingArenaCallAction(PlayerbotAI* ai) : LivingArenaAction(ai, "living arena call") {}
    bool isUseful() override;
    bool Execute(Event event) override;
};

class LivingArenaTrinketAction : public LivingArenaAction
{
public:
    LivingArenaTrinketAction(PlayerbotAI* ai) : LivingArenaAction(ai, "living arena trinket") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    uint64 _decided = 0;
    bool _willing = false;
};

class LivingArenaInterruptAction : public LivingArenaAction
{
public:
    LivingArenaInterruptAction(PlayerbotAI* ai) : LivingArenaAction(ai, "living arena interrupt") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    std::unordered_map<uintptr_t, bool> _rolled;
};

class LivingArenaPeelAction : public LivingArenaAction
{
public:
    LivingArenaPeelAction(PlayerbotAI* ai) : LivingArenaAction(ai, "living arena peel") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    uint32 _lastTry = 0;
};

class LivingArenaCcAction : public LivingArenaAction
{
public:
    LivingArenaCcAction(PlayerbotAI* ai) : LivingArenaAction(ai, "living arena cc") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    uint32 _lastTry = 0;
};

class LivingArenaLosAction : public LivingArenaAction
{
public:
    LivingArenaLosAction(PlayerbotAI* ai) : LivingArenaAction(ai, "living arena los") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    uint32 _lastTry = 0;
};

class LivingArenaKiteAction : public LivingArenaAction
{
public:
    LivingArenaKiteAction(PlayerbotAI* ai) : LivingArenaAction(ai, "living arena kite") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    uint32 _lastTry = 0;
};

class LivingArenaStealthAction : public LivingArenaAction
{
public:
    LivingArenaStealthAction(PlayerbotAI* ai) : LivingArenaAction(ai, "living arena stealth") {}
    bool isUseful() override;
    bool Execute(Event event) override;
};

// "arena skill <0-100>" from the master pins this bot's playbook skill until restart; no number reports it.
class LivingArenaSkillAction : public Action
{
public:
    LivingArenaSkillAction(PlayerbotAI* ai) : Action(ai, "arena skill") {}
    bool Execute(Event event) override;
};

class LivingArenaStrategy : public Strategy
{
public:
    LivingArenaStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "living arena"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
