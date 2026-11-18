/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Necromancer sub-class path's Blight tree spells, as bot actions.

#ifndef PLAYERBOTS_NECROMANCERACTIONS_H
#define PLAYERBOTS_NECROMANCERACTIONS_H

#include "AiObject.h"
#include "GenericSpellActions.h"

// A disease the bot keeps up on its target. Like the warlock's own DoTs it skips the target's time-to-live check, which
// a training dummy (little health, always healing) or a quick fight would otherwise fail.
class CastNecromancerDiseaseAction : public CastDebuffSpellAction
{
public:
    CastNecromancerDiseaseAction(PlayerbotAI* botAI, std::string const spell)
        : CastDebuffSpellAction(botAI, spell, true)
    {
    }
    bool isUseful() override { return CastAuraSpellAction::isUseful(); }
};

class CastGraveWormsAction : public CastNecromancerDiseaseAction
{
public:
    CastGraveWormsAction(PlayerbotAI* botAI) : CastNecromancerDiseaseAction(botAI, "grave worms") {}
};

class CastPlagueOfTheRisenAction : public CastNecromancerDiseaseAction
{
public:
    CastPlagueOfTheRisenAction(PlayerbotAI* botAI) : CastNecromancerDiseaseAction(botAI, "plague of the risen") {}
};

class CastWastingPlagueAction : public CastNecromancerDiseaseAction
{
public:
    CastWastingPlagueAction(PlayerbotAI* botAI) : CastNecromancerDiseaseAction(botAI, "wasting plague") {}
};
BUFF_ACTION(CastMutationAction, "mutation");
BUFF_ACTION(CastRaiseGargoyleAction, "raise gargoyle");

// Contagion's first cast: takes copies of the bot's diseases on its target to give to another enemy.
class CastContagionAction : public CastSpellAction
{
public:
    CastContagionAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "contagion") {}
    bool isUseful() override;
};

// Contagion's second cast: gives the carried copies to an attacker that has none of the bot's diseases yet.
class CastContagionSpreadAction : public CastSpellAction
{
public:
    CastContagionSpreadAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "contagion") {}
    std::string const getName() override { return "contagion spread"; }
    bool Execute(Event event) override;
    bool isUseful() override;
    bool isPossible() override;

private:
    Unit* SpreadTarget();
};

#endif
