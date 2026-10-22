/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_RAMPSHARED_H
#define PLAYERBOTS_RAMPSHARED_H

#include "Common.h"

namespace RampShared
{

enum class RampSpells : uint32
{
    SPELL_TREACHEROUS_AURA  = 30695,
    SPELL_BANE_OF_TREACHERY = 37566,
};

inline constexpr uint32 RAMP_MAP_ID = 543;

}

#endif
