/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Hexcraft tree's action and trigger names.

#ifndef PLAYERBOTS_HEXCRAFTCONTEXTS_H
#define PLAYERBOTS_HEXCRAFTCONTEXTS_H

#include "Action.h"
#include "HexcraftActions.h"
#include "HexcraftTriggers.h"
#include "NamedObjectContext.h"
#include "Trigger.h"

class SubclassHexcraftActionContext : public NamedObjectContext<Action>
{
public:
    SubclassHexcraftActionContext()
    {
        creators["shade puppet"] = &SubclassHexcraftActionContext::shade_puppet;
        creators["loa's spite"] = &SubclassHexcraftActionContext::loas_spite;
        creators["witchfire"] = &SubclassHexcraftActionContext::witchfire;
        creators["echo ward"] = &SubclassHexcraftActionContext::echo_ward;
        creators["spirit shroud"] = &SubclassHexcraftActionContext::spirit_shroud;
        creators["choking jinx"] = &SubclassHexcraftActionContext::choking_jinx;
        creators["loa's ride"] = &SubclassHexcraftActionContext::loas_ride;
        creators["mojo golem"] = &SubclassHexcraftActionContext::mojo_golem;
        creators["marionette dance"] = &SubclassHexcraftActionContext::marionette_dance;
    }

private:
    static Action* shade_puppet(PlayerbotAI* botAI) { return new CastShadePuppetAction(botAI); }
    static Action* loas_spite(PlayerbotAI* botAI) { return new CastLoasSpiteAction(botAI); }
    static Action* witchfire(PlayerbotAI* botAI) { return new CastWitchfireAction(botAI); }
    static Action* echo_ward(PlayerbotAI* botAI) { return new CastEchoWardAction(botAI); }
    static Action* spirit_shroud(PlayerbotAI* botAI) { return new CastSpiritShroudAction(botAI); }
    static Action* choking_jinx(PlayerbotAI* botAI) { return new CastChokingJinxAction(botAI); }
    static Action* loas_ride(PlayerbotAI* botAI) { return new CastLoasRideAction(botAI); }
    static Action* mojo_golem(PlayerbotAI* botAI) { return new CastMojoGolemAction(botAI); }
    static Action* marionette_dance(PlayerbotAI* botAI) { return new CastMarionetteDanceAction(botAI); }
};

class SubclassHexcraftTriggerContext : public NamedObjectContext<Trigger>
{
public:
    SubclassHexcraftTriggerContext()
    {
        creators["shade puppet"] = &SubclassHexcraftTriggerContext::shade_puppet;
        creators["loa's spite"] = &SubclassHexcraftTriggerContext::loas_spite;
        creators["witchfire"] = &SubclassHexcraftTriggerContext::witchfire;
        creators["echo ward"] = &SubclassHexcraftTriggerContext::echo_ward;
        creators["spirit shroud"] = &SubclassHexcraftTriggerContext::spirit_shroud;
        creators["choking jinx"] = &SubclassHexcraftTriggerContext::choking_jinx;
        creators["loa's ride"] = &SubclassHexcraftTriggerContext::loas_ride;
        creators["mojo golem"] = &SubclassHexcraftTriggerContext::mojo_golem;
        creators["marionette dance"] = &SubclassHexcraftTriggerContext::marionette_dance;
    }

private:
    static Trigger* shade_puppet(PlayerbotAI* botAI) { return new ShadePuppetTrigger(botAI); }
    static Trigger* loas_spite(PlayerbotAI* botAI) { return new LoasSpiteTrigger(botAI); }
    static Trigger* witchfire(PlayerbotAI* botAI) { return new WitchfireTrigger(botAI); }
    static Trigger* echo_ward(PlayerbotAI* botAI) { return new EchoWardTrigger(botAI); }
    static Trigger* spirit_shroud(PlayerbotAI* botAI) { return new SpiritShroudTrigger(botAI); }
    static Trigger* choking_jinx(PlayerbotAI* botAI) { return new ChokingJinxTrigger(botAI); }
    static Trigger* loas_ride(PlayerbotAI* botAI) { return new LoasRideTrigger(botAI); }
    static Trigger* mojo_golem(PlayerbotAI* botAI) { return new MojoGolemTrigger(botAI); }
    static Trigger* marionette_dance(PlayerbotAI* botAI) { return new MarionetteDanceTrigger(botAI); }
};

#endif
