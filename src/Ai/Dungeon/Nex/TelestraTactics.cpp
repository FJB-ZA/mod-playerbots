/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Grand Magus Telestra bot tactics: ranged spread for Firebomb, and the images die in a fixed order.

#include "ChooseTargetActions.h"
#include "NexActions.h"
#include "NexMultipliers.h"
#include "NexPolicy.h"
#include "NexTriggers.h"
#include "Playerbots.h"

namespace
{
Nexus::TelestraImage ImageOf(uint32 entry)
{
    switch (entry)
    {
        case NPC_ARCANE_MAGUS:
            return Nexus::TelestraImage::Arcane;
        case NPC_FROST_MAGUS:
            return Nexus::TelestraImage::Frost;
        case NPC_FIRE_MAGUS:
            return Nexus::TelestraImage::Fire;
        default:
            return Nexus::TelestraImage::None;
    }
}

bool ImagesUp(PlayerbotAI* botAI)
{
    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && ImageOf(unit->GetEntry()) != Nexus::TelestraImage::None)
            return true;
    }
    return false;
}
}  // namespace

bool TelestraFirebombTrigger::IsActive()
{
    bool telestraUp = AI_VALUE2(Unit*, "find target", "grand magus telestra") != nullptr;
    return Nexus::SpreadsForFirebomb(PlayerbotAI::IsMelee(bot), telestraUp, telestraUp && ImagesUp(botAI));
}

bool TelestraSplitPhaseTrigger::IsActive() { return ImagesUp(botAI); }

bool FirebombSpreadAction::Execute(Event /*event*/)
{
    if (!AI_VALUE2(Unit*, "find target", "grand magus telestra"))
        return false;

    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        Unit* unit = botAI->GetUnit(member);
        if (unit && member != bot->GetGUID() && bot->GetExactDist2d(unit) < Nexus::FirebombSpacing)
            return MoveAway(unit, Nexus::FirebombSpacing);
    }
    return false;
}

bool TelestraSplitTargetAction::isUseful() { return !PlayerbotAI::IsHeal(bot); }

bool TelestraSplitTargetAction::Execute(Event /*event*/)
{
    Unit* best = nullptr;
    int bestPriority = -1;
    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const& guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        int priority = Nexus::ImagePriority(ImageOf(unit->GetEntry()));
        if (priority >= 0 && (!best || priority <= bestPriority))
        {
            best = unit;
            bestPriority = priority;
        }
    }

    // Already on the best image: nothing to send
    return best && AI_VALUE(Unit*, "current target") != best && Attack(best);
}

float TelestraMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<DpsAssistAction*>(action))
        return 1.0f;
    return Nexus::TelestraAllows(ImagesUp(botAI), true) ? 1.0f : 0.0f;
}
