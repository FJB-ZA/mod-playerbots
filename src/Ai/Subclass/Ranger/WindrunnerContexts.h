/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Windrunner tree's action and trigger names.

#ifndef PLAYERBOTS_WINDRUNNERCONTEXTS_H
#define PLAYERBOTS_WINDRUNNERCONTEXTS_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "Trigger.h"
#include "WindrunnerActions.h"
#include "WindrunnerTriggers.h"

class SubclassWindrunnerActionContext : public NamedObjectContext<Action>
{
public:
    SubclassWindrunnerActionContext()
    {
        creators["serrated shot"] = &SubclassWindrunnerActionContext::serrated_shot;
        creators["skirmish"] = &SubclassWindrunnerActionContext::skirmish;
        creators["precision shot"] = &SubclassWindrunnerActionContext::precision_shot;
        creators["incendiary shot"] = &SubclassWindrunnerActionContext::incendiary_shot;
        creators["final arrow"] = &SubclassWindrunnerActionContext::final_arrow;
        creators["ricochet shot"] = &SubclassWindrunnerActionContext::ricochet_shot;
        creators["hushing arrow"] = &SubclassWindrunnerActionContext::hushing_arrow;
        creators["piercing volley"] = &SubclassWindrunnerActionContext::piercing_volley;
        creators["advantage arcane shot"] = &SubclassWindrunnerActionContext::advantage_arcane_shot;
    }

private:
    static Action* serrated_shot(PlayerbotAI* botAI) { return new CastSerratedShotAction(botAI); }
    static Action* skirmish(PlayerbotAI* botAI) { return new CastSkirmishAction(botAI); }
    static Action* precision_shot(PlayerbotAI* botAI) { return new CastPrecisionShotAction(botAI); }
    static Action* incendiary_shot(PlayerbotAI* botAI) { return new CastIncendiaryShotAction(botAI); }
    static Action* final_arrow(PlayerbotAI* botAI) { return new CastFinalArrowAction(botAI); }
    static Action* ricochet_shot(PlayerbotAI* botAI) { return new CastRicochetShotAction(botAI); }
    static Action* hushing_arrow(PlayerbotAI* botAI) { return new CastHushingArrowAction(botAI); }
    static Action* piercing_volley(PlayerbotAI* botAI) { return new CastPiercingVolleyAction(botAI); }
    static Action* advantage_arcane_shot(PlayerbotAI* botAI) { return new CastAdvantageArcaneShotAction(botAI); }
};

class SubclassWindrunnerTriggerContext : public NamedObjectContext<Trigger>
{
public:
    SubclassWindrunnerTriggerContext()
    {
        creators["serrated shot"] = &SubclassWindrunnerTriggerContext::serrated_shot;
        creators["skirmish"] = &SubclassWindrunnerTriggerContext::skirmish;
        creators["full advantage"] = &SubclassWindrunnerTriggerContext::full_advantage;
        creators["build advantage"] = &SubclassWindrunnerTriggerContext::build_advantage;
        creators["incendiary shot"] = &SubclassWindrunnerTriggerContext::incendiary_shot;
        creators["final arrow"] = &SubclassWindrunnerTriggerContext::final_arrow;
        creators["ricochet shot"] = &SubclassWindrunnerTriggerContext::ricochet_shot;
        creators["hushing arrow"] = &SubclassWindrunnerTriggerContext::hushing_arrow;
        creators["piercing volley"] = &SubclassWindrunnerTriggerContext::piercing_volley;
    }

private:
    static Trigger* serrated_shot(PlayerbotAI* botAI) { return new SerratedShotTrigger(botAI); }
    static Trigger* skirmish(PlayerbotAI* botAI) { return new SkirmishTrigger(botAI); }
    static Trigger* full_advantage(PlayerbotAI* botAI) { return new FullAdvantageTrigger(botAI); }
    static Trigger* build_advantage(PlayerbotAI* botAI) { return new BuildAdvantageTrigger(botAI); }
    static Trigger* incendiary_shot(PlayerbotAI* botAI) { return new IncendiaryShotTrigger(botAI); }
    static Trigger* final_arrow(PlayerbotAI* botAI) { return new FinalArrowTrigger(botAI); }
    static Trigger* ricochet_shot(PlayerbotAI* botAI) { return new RicochetShotTrigger(botAI); }
    static Trigger* hushing_arrow(PlayerbotAI* botAI) { return new HushingArrowTrigger(botAI); }
    static Trigger* piercing_volley(PlayerbotAI* botAI) { return new PiercingVolleyTrigger(botAI); }
};

#endif
