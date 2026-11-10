/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ARGENTSOLDIERSVALUE_H
#define PLAYERBOTS_ARGENTSOLDIERSVALUE_H

#include "ObjectGuid.h"
#include "Value.h"

struct ToCSoldiers
{
    bool fighting = false;
    ObjectGuid nearest;
    ObjectGuid priestess;
    float botDistance = 1000.0f;
    float masterDistance = 1000.0f;
};

class ArgentSoldiersValue : public CalculatedValue<ToCSoldiers>
{
public:
    ArgentSoldiersValue(PlayerbotAI* ai) : CalculatedValue(ai, "toc argent soldiers", 500) {}
    ToCSoldiers Calculate() override;
};

#endif
