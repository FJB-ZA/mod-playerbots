/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Amanitar (heroic only) bot tactics: a shrunk dps kills a Healthy Mushroom beside it to shed Mini.

#include <algorithm>
#include <limits>
#include <vector>

#include "AKActions.h"
#include "AKMultipliers.h"
#include "AKPolicy.h"
#include "AKTriggers.h"
#include "ChooseTargetActions.h"
#include "MovementActions.h"
#include "Playerbots.h"

namespace
{
bool SeeksPotentFungus(Player* bot)
{
    return AhnKahet::SeeksPotentFungus(bot->GetMap()->IsHeroic(), bot->HasAura(SPELL_MINI), PlayerbotAI::IsTank(bot),
                                       PlayerbotAI::IsHeal(bot));
}

// Mushrooms are passive and never join the threat table, so find target can't see them
Unit* SafeHealthyMushroom(PlayerbotAI* botAI, Player* bot)
{
    std::vector<Unit*> healthy;
    std::vector<Unit*> poisonous;
    GuidVector targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;
        if (unit->GetEntry() == NPC_HEALTHY_MUSHROOM)
            healthy.push_back(unit);
        else if (unit->GetEntry() == NPC_POISONOUS_MUSHROOM)
            poisonous.push_back(unit);
    }

    Unit* best = nullptr;
    for (Unit* mushroom : healthy)
    {
        float nearestPoisonous = std::numeric_limits<float>::max();
        for (Unit* poison : poisonous)
            nearestPoisonous = std::min(nearestPoisonous, mushroom->GetExactDist2d(poison));

        float distance = bot->GetExactDist2d(mushroom);
        if (AhnKahet::SafeMushroom(distance, nearestPoisonous) && (!best || distance < bot->GetExactDist2d(best)))
            best = mushroom;
    }
    return best;
}
}  // namespace

bool AmanitarMiniTrigger::IsActive()
{
    return SeeksPotentFungus(bot) && AI_VALUE2(Unit*, "find target", "amanitar") && SafeHealthyMushroom(botAI, bot);
}

bool AmanitarPotentFungusAction::Execute(Event /*event*/)
{
    Unit* mushroom = SafeHealthyMushroom(botAI, bot);
    if (!mushroom)
        return false;

    // Potent Fungus only reaches players beside the mushroom, so ranged walk up before they shoot it
    float distance = bot->GetExactDist2d(mushroom);
    if (AhnKahet::ApproachesMushroom(distance))
        return Move(bot->GetAngle(mushroom), AhnKahet::MushroomStep(distance));

    if (AI_VALUE(Unit*, "current target") == mushroom)
        return false;
    return Attack(mushroom);
}

float AmanitarMultiplier::GetValue(Action* action)
{
    // Flee would step a ranged bot back out of the fungus radius every tick it stands beside the mushroom
    bool pullsAway = dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<FleeAction*>(action);
    if (!pullsAway || !SeeksPotentFungus(bot) || !AI_VALUE2(Unit*, "find target", "amanitar"))
        return 1.0f;

    return AhnKahet::AmanitarAllows(SafeHealthyMushroom(botAI, bot) != nullptr, true) ? 1.0f : 0.0f;
}
