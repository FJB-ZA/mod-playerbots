// Anomalus bot tactics: kill the Chaotic Rift while his Rift Shield is up.

#include "ChooseTargetActions.h"
#include "NexActions.h"
#include "NexMultipliers.h"
#include "NexPolicy.h"
#include "NexTriggers.h"
#include "Playerbots.h"

bool ChaoticRiftTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anomalus");
    return boss && boss->HasAura(BUFF_RIFT_SHIELD);
}

bool ChaoticRiftTargetAction::isUseful() { return Nexus::KillsRift(PlayerbotAI::IsHeal(bot)); }

bool ChaoticRiftTargetAction::Execute(Event /*event*/)
{
    // The rift isn't on the threat table, so find target can't see it
    Unit* chaoticRift = nullptr;
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetName() == "Chaotic Rift")
        {
            chaoticRift = unit;
            break;
        }
    }

    if (!chaoticRift || AI_VALUE(Unit*, "current target") == chaoticRift)
        return false;
    return Attack(chaoticRift);
}

float AnomalusMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "anomalus");
    bool shield = boss && boss->HasAura(BUFF_RIFT_SHIELD);
    return Nexus::AnomalusAllows(shield, dynamic_cast<DpsAssistAction*>(action) != nullptr) ? 1.0f : 0.0f;
}
