/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Necromancer sub-class path's Blight tree: diseases on top of the Affliction rotation, which the bot runs as
// well (AiFactory adds "affli" with this). It only adds the tree's spells, which a plain warlock never learns.

#ifndef PLAYERBOTS_NECROMANCERSTRATEGY_H
#define PLAYERBOTS_NECROMANCERSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

class NecromancerStrategy : public Strategy
{
public:
    NecromancerStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "blight"; }
};

#endif
