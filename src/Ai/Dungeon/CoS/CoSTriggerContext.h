/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_COSTRIGGERCONTEXT_H
#define PLAYERBOTS_COSTRIGGERCONTEXT_H

#include "CoSTriggers.h"
#include "NamedObjectContext.h"

class WotlkDungeonCoSTriggerContext : public NamedObjectContext<Trigger>
{
    public:
        WotlkDungeonCoSTriggerContext()
        {
            creators["explode ghoul"] = &WotlkDungeonCoSTriggerContext::explode_ghoul;
            creators["salramm ghoul"] = &WotlkDungeonCoSTriggerContext::salramm_ghoul;
            creators["epoch ranged"] = &WotlkDungeonCoSTriggerContext::epoch_ranged;
            creators["infinite corruptor"] = &WotlkDungeonCoSTriggerContext::infinite_corruptor;
        }
    private:
        static Trigger* explode_ghoul(PlayerbotAI* botAI) { return new ExplodeGhoulTrigger(botAI); }
        static Trigger* salramm_ghoul(PlayerbotAI* botAI) { return new SalrammGhoulTrigger(botAI); }
        static Trigger* epoch_ranged(PlayerbotAI* botAI) { return new EpochRangedTrigger(botAI); }
        static Trigger* infinite_corruptor(PlayerbotAI* botAI) { return new InfiniteCorruptorTrigger(botAI); }
};

#endif
