// Jedoga Shadowseeker bot tactics: kill the Twilight Volunteer before it reaches her.

#include "AKActions.h"
#include "AKMultipliers.h"
#include "AKPolicy.h"
#include "AKTriggers.h"
#include "ChooseTargetActions.h"
#include "Playerbots.h"

namespace
{
// The volunteer isn't on the threat table, so find target can't see it
Unit* Volunteer(PlayerbotAI* botAI)
{
    GuidVector targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NPC_TWILIGHT_VOLUNTEER)
            return unit;
    }
    return nullptr;
}
}  // namespace

bool JedogaVolunteerTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "jedoga shadowseeker") && Volunteer(botAI);
}

bool AttackJedogaVolunteerAction::Execute(Event /*event*/)
{
    Unit* target = Volunteer(botAI);
    if (!target || AI_VALUE(Unit*, "current target") == target)
        return false;
    return Attack(target);
}

float JedogaShadowseekerMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<DpsAssistAction*>(action) || !AI_VALUE2(Unit*, "find target", "jedoga shadowseeker"))
        return 1.0f;

    return AhnKahet::JedogaAllows(Volunteer(botAI) != nullptr, true) ? 1.0f : 0.0f;
}
