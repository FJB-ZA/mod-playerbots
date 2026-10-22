/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Sethekk Prophet bot tactics: shamans keep a Tremor Totem down against its Fear.

#include "Playerbots.h"
#include "SethActions.h"
#include "SethMultipliers.h"
#include "SethPolicy.h"
#include "SethShared.h"
#include "SethTriggers.h"
#include "ShamanActions.h"

using namespace SethShared;

bool SethekkProphetCastsFearTrigger::IsActive()
{
    return SethekkHalls::DropsTremorTotem(bot->getClass() == CLASS_SHAMAN,
                                          AI_VALUE2(Unit*, "find target", "sethekk prophet"),
                                          AI_VALUE2(bool, "has totem", "tremor totem"));
}

bool SethekkProphetSetTremorTotemAction::Execute(Event /*event*/)
{
    return botAI->CanCastSpell(Id(SethSpells::SPELL_TREMOR_TOTEM), bot) &&
           botAI->CastSpell(Id(SethSpells::SPELL_TREMOR_TOTEM), bot);
}

float SethekkProphetSetTremorTotemMultiplier::GetValue(Action* action)
{
    if (botAI->GetState() == BOT_STATE_NON_COMBAT || bot->getClass() != CLASS_SHAMAN)
        return 1.0f;

    bool const otherEarthTotem =
        dynamic_cast<CastStrengthOfEarthTotemAction*>(action) || dynamic_cast<CastStoneskinTotemAction*>(action) ||
        dynamic_cast<CastStoneclawTotemAction*>(action) || dynamic_cast<CastEarthbindTotemAction*>(action);
    if (!otherEarthTotem)
        return 1.0f;

    return SethekkHalls::ProphetAllows(AI_VALUE2(Unit*, "find target", "sethekk prophet"), true) ? 1.0f : 0.0f;
}
