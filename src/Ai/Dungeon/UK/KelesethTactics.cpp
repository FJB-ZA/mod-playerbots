// Prince Keleseth bot tactics: break a Frost Tomb before going back to the boss.

#include "ChooseTargetActions.h"
#include "Playerbots.h"
#include "UKActions.h"
#include "UKMultipliers.h"
#include "UKPolicy.h"
#include "UKTriggers.h"

namespace
{
bool AnyoneTombed(PlayerbotAI* botAI)
{
    GuidVector members = botAI->GetAiObjectContext()->GetValue<GuidVector>("group members")->Get();
    for (ObjectGuid const& member : members)
    {
        Unit* unit = botAI->GetUnit(member);
        if (unit && unit->HasAura(SPELL_FROST_TOMB))
            return true;
    }
    return false;
}
}  // namespace

bool KelesethFrostTombTrigger::IsActive() { return AnyoneTombed(botAI); }

bool AttackFrostTombAction::isUseful() { return UtgardeKeep::AttacksFrostTomb(PlayerbotAI::IsHeal(bot)); }

bool AttackFrostTombAction::Execute(Event /*event*/)
{
    // The tomb isn't on the threat table, so find target can't see it
    Unit* frostTomb = nullptr;
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NPC_FROST_TOMB)
        {
            frostTomb = unit;
            break;
        }
    }

    if (!frostTomb || AI_VALUE(Unit*, "current target") == frostTomb)
        return false;
    return Attack(frostTomb);
}

float PrinceKelesethMultiplier::GetValue(Action* action)
{
    if (!AI_VALUE2(Unit*, "find target", "prince keleseth") || !dynamic_cast<DpsAssistAction*>(action))
        return 1.0f;

    return UtgardeKeep::KelesethAllows(AnyoneTombed(botAI), true) ? 1.0f : 0.0f;
}
