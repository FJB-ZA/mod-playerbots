/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Mythic+ Explosive Orbs; Quaking and the other ground circles are dynobject auras that "avoid aoe" already leaves.

#include "MythicPlusTactics.h"

#include "ChooseTargetActions.h"
#include "MythicPlusBotPolicy.h"
#include "Playerbots.h"

namespace
{
// Tanks hold the pack and healers heal; the rest kill the orb
bool KillsOrbs(Player* bot)
{
    return bot->IsInCombat() && !PlayerbotAI::IsTank(bot) && !PlayerbotAI::IsHeal(bot);
}

// The orb never attacks, so it isn't on the threat table and only the cached possible targets list it
Unit* NearestOrb(PlayerbotAI* botAI, Player* bot)
{
    Unit* nearest = nullptr;
    GuidVector const targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != MythicPlusBots::ExplosiveOrbEntry)
            continue;
        if (!nearest || bot->GetExactDist2dSq(unit) < bot->GetExactDist2dSq(nearest))
            nearest = unit;
    }
    return nearest;
}
}  // namespace

void MythicPlusStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("explosive orb", {NextAction("attack explosive orb", ACTION_RAID + 3)}));
}

void MythicPlusStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new ExplosiveOrbMultiplier(botAI));
}

bool ExplosiveOrbTrigger::IsActive() { return KillsOrbs(bot) && NearestOrb(botAI, bot); }

bool AttackExplosiveOrbAction::Execute(Event /*event*/)
{
    Unit* orb = NearestOrb(botAI, bot);
    if (!orb || AI_VALUE(Unit*, "current target") == orb)
        return false;
    return Attack(orb);
}

float ExplosiveOrbMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<DpsAssistAction*>(action) || !KillsOrbs(bot))
        return 1.0f;
    return NearestOrb(botAI, bot) ? 0.0f : 1.0f;
}
