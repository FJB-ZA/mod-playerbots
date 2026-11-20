/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Witch Doctor sub-class path's Hexcraft tree: the tree's spells woven into the Elemental rotation, which the bot
// runs as well (AiFactory adds "ele" with this), Lightning Bolt staying the filler. It only adds the tree's spells,
// which a plain shaman never learns.

#ifndef PLAYERBOTS_HEXCRAFTSTRATEGY_H
#define PLAYERBOTS_HEXCRAFTSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

class HexcraftStrategy : public Strategy
{
public:
    HexcraftStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "hexcraft"; }
};

#endif
