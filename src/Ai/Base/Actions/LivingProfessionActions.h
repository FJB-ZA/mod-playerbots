/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_LIVING_PROFESSION_ACTIONS_H
#define PLAYERBOTS_LIVING_PROFESSION_ACTIONS_H

#include <unordered_map>

#include "MovementActions.h"
#include "NonCombatStrategy.h"
#include "Trainer.h"
#include "Value.h"

class SpellInfo;
namespace LivingProfessions
{
bool CanWork(PlayerbotAI* ai);
// False for profession ranks and recipes beyond the current era (also used by the bot factory).
bool WithinEra(Trainer::Spell const& lesson);
bool IsProfessionRecipe(Player* player, SpellInfo const* spell);
bool NeedsProduct(Player* player, SpellInfo const* spell);
bool HasReagents(Player* player, SpellInfo const* spell);
// Wrath enchanting scrolls: a trader casts an enchant on vellum to make a scroll it can sell.
bool MakesScrolls(Player* player);
bool WantsScroll(Player* player, SpellInfo const* spell);
bool VellumFits(ItemTemplate const* vellum, SpellInfo const* spell);
Item* ScrollVellum(Player* player, SpellInfo const* spell);
}  // namespace LivingProfessions

struct LivingTrainingPlan
{
    ObjectGuid trainer;
    uint32 spell = 0;
};

// A lesson the bot could take at a profession trainer beyond its reach, on the same map.
struct LivingTrainingTrip
{
    bool go = false;
    WorldLocation destination;
    uint32 entry = 0;
    // Trainers reached that had nothing to teach (dead, hostile, or out of reach indoors), by entry, with
    // when; skipped for LivingWorld::FailedTrainerMemoryMs.
    std::unordered_map<uint32, uint32> failed;
};

struct LivingCraftPlan
{
    uint32 spell = 0;
    ObjectGuid item;
};

class LivingTrainingValue : public CalculatedValue<LivingTrainingPlan>
{
public:
    LivingTrainingValue(PlayerbotAI* botAI) : CalculatedValue(botAI, "living training", 60) {}
    LivingTrainingPlan Calculate() override;
};

class LivingTrainingTripValue : public CalculatedValue<LivingTrainingTrip>
{
public:
    LivingTrainingTripValue(PlayerbotAI* botAI) : CalculatedValue(botAI, "living training trip", 60000) {}
    LivingTrainingTrip Calculate() override;
};

class LivingCraftValue : public CalculatedValue<LivingCraftPlan>
{
public:
    LivingCraftValue(PlayerbotAI* botAI) : CalculatedValue(botAI, "living craft", 30) {}
    LivingCraftPlan Calculate() override;
};

class LivingTrainAction : public MovementAction
{
public:
    LivingTrainAction(PlayerbotAI* botAI) : MovementAction(botAI, "living train") {}
    bool isUseful() override;
    bool isPossible() override;
    bool Execute(Event event) override;
};

class LivingCraftAction : public Action
{
public:
    LivingCraftAction(PlayerbotAI* botAI) : Action(botAI, "living craft") {}
    bool isUseful() override;
    bool isPossible() override;
    bool Execute(Event event) override;
};

class LivingProfessionStrategy : public NonCombatStrategy
{
public:
    LivingProfessionStrategy(PlayerbotAI* botAI) : NonCombatStrategy(botAI) {}
    std::string const getName() override { return "living professions"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
};

#endif
