/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Ranger sub-class path: a hunter without a pet. Its shots and stings are the Marksmanship or Survival
// rotation's, so this strategy only adds what the path brings: Field Dressing, Mend Pet's heal on the hunter.

#ifndef PLAYERBOTS_RANGERSTRATEGY_H
#define PLAYERBOTS_RANGERSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

class RangerStrategy : public Strategy
{
public:
    RangerStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}

    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "ranger"; }
};

#endif
