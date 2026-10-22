/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// King Ymiron bot tactics: stop attacking while Bane is up.

#include "AttackAction.h"
#include "Playerbots.h"
#include "UPMultipliers.h"
#include "UPPolicy.h"
#include "UPTriggers.h"

namespace
{
bool BaneUp(Player* bot, Unit* boss)
{
    return boss->FindCurrentSpellBySpellId(SPELL_BANE) || boss->HasAura(SPELL_BANE);
}
}  // namespace

bool YmironBaneTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "king ymiron");
    return boss && BaneUp(bot, boss);
}

float YmironMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "king ymiron");
    if (!boss)
        return 1.0f;

    bool attack = dynamic_cast<AttackAction*>(action) != nullptr;
    return UtgardePinnacle::YmironAllows(BaneUp(bot, boss), attack) ? 1.0f : 0.0f;
}
