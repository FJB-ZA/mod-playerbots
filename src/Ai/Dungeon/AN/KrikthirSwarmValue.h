/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_KRIKTHIRSWARMVALUE_H
#define PLAYERBOTS_KRIKTHIRSWARMVALUE_H

#include "Value.h"

class KrikthirSwarmValue : public CalculatedValue<bool>
{
public:
    KrikthirSwarmValue(PlayerbotAI* ai) : CalculatedValue(ai, "krik'thir loose swarm", 500) {}
    bool Calculate() override;
};

#endif
