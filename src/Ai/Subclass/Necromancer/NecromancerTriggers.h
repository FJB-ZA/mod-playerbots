/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// When the Necromancer sub-class path's Blight tree spells are worth casting.

#ifndef PLAYERBOTS_NECROMANCERTRIGGERS_H
#define PLAYERBOTS_NECROMANCERTRIGGERS_H

#include "AiObject.h"
#include "GenericTriggers.h"

DEBUFF_CHECKISOWNER_TRIGGER(GraveWormsTrigger, "grave worms");
DEBUFF_CHECKISOWNER_TRIGGER(PlagueOfTheRisenTrigger, "plague of the risen");
DEBUFF_CHECKISOWNER_TRIGGER(WastingPlagueTrigger, "wasting plague");
BOOST_TRIGGER(MutationTrigger, "mutation");
BOOST_TRIGGER(RaiseGargoyleTrigger, "raise gargoyle");
CD_TRIGGER(ContagionTrigger, "contagion");

// The bot carries Contagion's copies (their aura has the spell's name) and has 10 seconds to give them away.
class ContagionSpreadTrigger : public Trigger
{
public:
    ContagionSpreadTrigger(PlayerbotAI* botAI) : Trigger(botAI, "contagion spread") {}
    bool IsActive() override;
};

#endif
