/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Ranger sub-class path's Windrunner tree: Advantage built by the Marksmanship rotation's shots, which the bot runs
// as well (AiFactory adds "mm" with this), and spent at 5 stacks on Skirmish, then Precision Shot (Incendiary Shot on a
// pack). It only adds the tree's spells, which a plain hunter never learns.

#ifndef PLAYERBOTS_WINDRUNNERSTRATEGY_H
#define PLAYERBOTS_WINDRUNNERSTRATEGY_H

#include "Strategy.h"

class PlayerbotAI;

class WindrunnerStrategy : public Strategy
{
public:
    WindrunnerStrategy(PlayerbotAI* botAI) : Strategy(botAI) {}
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::string const getName() override { return "windrunner"; }
};

#endif
