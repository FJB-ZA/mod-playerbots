/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Zuramat the Obliterator bot tactics: a void-shifted bot kills the sentries; nobody attacks him through his shroud.

#include "ChooseTargetActions.h"
#include "Playerbots.h"
#include "VHActions.h"
#include "VHMultipliers.h"
#include "VHPolicy.h"
#include "VHTriggers.h"

bool VoidShiftTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "zuramat the obliterator") && bot->HasAura(SPELL_VOID_SHIFTED) &&
           !PlayerbotAI::IsHeal(bot);
}

bool ShroudOfDarknessTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "zuramat the obliterator");
    if (!boss)
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    bool onSentry = target && target->GetEntry() == NPC_VOID_SENTRY;
    return VioletHold::DropsTargetForShroud(boss->HasAura(SPELL_SHROUD_OF_DARKNESS), bot->HasAura(SPELL_VOID_SHIFTED),
                                            onSentry);
}

bool AttackVoidSentryAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "zuramat the obliterator");
    if (!boss)
        return false;

    // Sentries aren't on the threat table, so find target can't see them
    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit->GetEntry() != NPC_VOID_SENTRY)
            continue;

        // By entry, not pointer: once on a sentry, finish it rather than hop between them
        if (currentTarget && currentTarget->GetEntry() == NPC_VOID_SENTRY)
            return false;
        return Attack(unit);
    }

    // This action is exempt from the shroud block, so it must not hit the boss through the shroud itself
    if (boss->HasAura(SPELL_SHROUD_OF_DARKNESS))
        return false;
    return currentTarget != boss && Attack(boss);
}

float ZuramatMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "zuramat the obliterator");
    if (!boss)
        return 1.0f;

    VioletHold::ZuramatAction shape;
    shape.assist = dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action);
    shape.attack = dynamic_cast<AttackAction*>(action) != nullptr;
    shape.sentry = dynamic_cast<AttackVoidSentryAction*>(action) != nullptr;
    bool shroud = boss->HasAura(SPELL_SHROUD_OF_DARKNESS);
    return VioletHold::ZuramatAllows(bot->HasAura(SPELL_VOID_SHIFTED), shroud, shape) ? 1.0f : 0.0f;
}
