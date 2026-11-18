/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Necromancer sub-class path's action and trigger names.

#ifndef PLAYERBOTS_NECROMANCERCONTEXTS_H
#define PLAYERBOTS_NECROMANCERCONTEXTS_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "NecromancerActions.h"
#include "NecromancerTriggers.h"
#include "Trigger.h"

class SubclassNecromancerActionContext : public NamedObjectContext<Action>
{
public:
    SubclassNecromancerActionContext()
    {
        creators["grave worms"] = &SubclassNecromancerActionContext::grave_worms;
        creators["plague of the risen"] = &SubclassNecromancerActionContext::plague_of_the_risen;
        creators["wasting plague"] = &SubclassNecromancerActionContext::wasting_plague;
        creators["mutation"] = &SubclassNecromancerActionContext::mutation;
        creators["raise gargoyle"] = &SubclassNecromancerActionContext::raise_gargoyle;
        creators["contagion"] = &SubclassNecromancerActionContext::contagion;
        creators["contagion spread"] = &SubclassNecromancerActionContext::contagion_spread;
    }

private:
    static Action* grave_worms(PlayerbotAI* botAI) { return new CastGraveWormsAction(botAI); }
    static Action* plague_of_the_risen(PlayerbotAI* botAI) { return new CastPlagueOfTheRisenAction(botAI); }
    static Action* wasting_plague(PlayerbotAI* botAI) { return new CastWastingPlagueAction(botAI); }
    static Action* mutation(PlayerbotAI* botAI) { return new CastMutationAction(botAI); }
    static Action* raise_gargoyle(PlayerbotAI* botAI) { return new CastRaiseGargoyleAction(botAI); }
    static Action* contagion(PlayerbotAI* botAI) { return new CastContagionAction(botAI); }
    static Action* contagion_spread(PlayerbotAI* botAI) { return new CastContagionSpreadAction(botAI); }
};

class SubclassNecromancerTriggerContext : public NamedObjectContext<Trigger>
{
public:
    SubclassNecromancerTriggerContext()
    {
        creators["grave worms"] = &SubclassNecromancerTriggerContext::grave_worms;
        creators["plague of the risen"] = &SubclassNecromancerTriggerContext::plague_of_the_risen;
        creators["wasting plague"] = &SubclassNecromancerTriggerContext::wasting_plague;
        creators["mutation"] = &SubclassNecromancerTriggerContext::mutation;
        creators["raise gargoyle"] = &SubclassNecromancerTriggerContext::raise_gargoyle;
        creators["contagion"] = &SubclassNecromancerTriggerContext::contagion;
        creators["contagion spread"] = &SubclassNecromancerTriggerContext::contagion_spread;
    }

private:
    static Trigger* grave_worms(PlayerbotAI* botAI) { return new GraveWormsTrigger(botAI); }
    static Trigger* plague_of_the_risen(PlayerbotAI* botAI) { return new PlagueOfTheRisenTrigger(botAI); }
    static Trigger* wasting_plague(PlayerbotAI* botAI) { return new WastingPlagueTrigger(botAI); }
    static Trigger* mutation(PlayerbotAI* botAI) { return new MutationTrigger(botAI); }
    static Trigger* raise_gargoyle(PlayerbotAI* botAI) { return new RaiseGargoyleTrigger(botAI); }
    static Trigger* contagion(PlayerbotAI* botAI) { return new ContagionTrigger(botAI); }
    static Trigger* contagion_spread(PlayerbotAI* botAI) { return new ContagionSpreadTrigger(botAI); }
};

#endif
