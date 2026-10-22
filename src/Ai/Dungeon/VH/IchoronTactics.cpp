// Ichoron bot tactics: the tank holds him while his bubble is up; everyone kills the globules once it bursts.

#include "ChooseTargetActions.h"
#include "Playerbots.h"
#include "VHActions.h"
#include "VHMultipliers.h"
#include "VHPolicy.h"
#include "VHTriggers.h"

bool IchoronTargetTrigger::IsActive()
{
    return !PlayerbotAI::IsHeal(bot) && AI_VALUE2(Unit*, "find target", "ichoron");
}

bool AttackIchorGlobuleAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "ichoron");
    if (!boss)
        return false;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (VioletHold::TankStaysOnIchoron(PlayerbotAI::IsTank(bot), boss->HasAura(SPELL_DRAINED)))
        return currentTarget != boss && Attack(boss);

    // Globules aren't on the threat table, so find target can't see them
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit->GetEntry() != NPC_ICHOR_GLOBULE)
            continue;

        // By entry, not pointer: once on a globule, finish it rather than hop between them
        if (currentTarget && currentTarget->GetEntry() == NPC_ICHOR_GLOBULE)
            return false;
        return Attack(unit);
    }

    // No globules left: back to the boss
    return currentTarget != boss && Attack(boss);
}

float IchoronMultiplier::GetValue(Action* action)
{
    if (!AI_VALUE2(Unit*, "find target", "ichoron"))
        return 1.0f;

    bool retarget = dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
                    dynamic_cast<DropTargetAction*>(action);
    return VioletHold::IchoronAllows(retarget) ? 1.0f : 0.0f;
}
