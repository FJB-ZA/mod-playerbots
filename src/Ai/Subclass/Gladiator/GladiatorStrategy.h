/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Gladiator sub-class path: a shield-and-one-hander damage dealer built on Shield Charge and Shield Slam.

#ifndef PLAYERBOTS_GLADIATORSTRATEGY_H
#define PLAYERBOTS_GLADIATORSTRATEGY_H

#include "GenericWarriorStrategy.h"

class PlayerbotAI;

class GladiatorStrategy : public GenericWarriorStrategy
{
public:
    GladiatorStrategy(PlayerbotAI* botAI);

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "gladiator"; }
    std::vector<NextAction> getDefaultActions() override;
    uint32 GetType() const override { return STRATEGY_TYPE_DPS | STRATEGY_TYPE_MELEE; }
};

#endif
