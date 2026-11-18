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

// The bot's own disease is missing from its target. As with Corruption, the target's expected life doesn't matter.
class NecromancerDiseaseTrigger : public DebuffTrigger
{
public:
    NecromancerDiseaseTrigger(PlayerbotAI* botAI, std::string const spell)
        : DebuffTrigger(botAI, spell, 1, true, 0.5f)
    {
    }
    bool IsActive() override { return BuffTrigger::IsActive(); }
};

class GraveWormsTrigger : public NecromancerDiseaseTrigger
{
public:
    GraveWormsTrigger(PlayerbotAI* botAI) : NecromancerDiseaseTrigger(botAI, "grave worms") {}
};

class PlagueOfTheRisenTrigger : public NecromancerDiseaseTrigger
{
public:
    PlagueOfTheRisenTrigger(PlayerbotAI* botAI) : NecromancerDiseaseTrigger(botAI, "plague of the risen") {}
};

class WastingPlagueTrigger : public NecromancerDiseaseTrigger
{
public:
    WastingPlagueTrigger(PlayerbotAI* botAI) : NecromancerDiseaseTrigger(botAI, "wasting plague") {}
};
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
