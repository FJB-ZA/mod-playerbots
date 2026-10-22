/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OCMULTIPLIERS_H
#define PLAYERBOTS_OCMULTIPLIERS_H

#include "Multiplier.h"
#include "Unit.h"

class MountingDrakeMultiplier : public Multiplier
{
    public:
        MountingDrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "mounting drake") {}

    public:
        float GetValue(Action* action) override;
};

class OccFlyingMultiplier : public Multiplier
{
    public:
        OccFlyingMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "occ flying drake") {}

    public:
        float GetValue(Action* action) override;
};

class UromMultiplier : public Multiplier
{
    public:
        UromMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "mage-lord urom") {}

    public:
        float GetValue(Action* action) override;
};

class EregosMultiplier : public Multiplier
{
    public:
        EregosMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "ley-guardian eregos") {}

    public:
        float GetValue(Action* action) override;
};

#endif
