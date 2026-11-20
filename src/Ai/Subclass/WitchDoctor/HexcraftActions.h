/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Witch Doctor sub-class path's Hexcraft tree spells, as bot actions.

#ifndef PLAYERBOTS_HEXCRAFTACTIONS_H
#define PLAYERBOTS_HEXCRAFTACTIONS_H

#include "AiObject.h"
#include "GenericSpellActions.h"

class CastShadePuppetAction : public CastSpellAction
{
public:
    CastShadePuppetAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "shade puppet") {}
};

class CastLoasSpiteAction : public CastSpellAction
{
public:
    CastLoasSpiteAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "loa's spite") {}
};

class CastWitchfireAction : public CastSpellAction
{
public:
    CastWitchfireAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "witchfire") {}
};

class CastChokingJinxAction : public CastSpellAction
{
public:
    CastChokingJinxAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "choking jinx") {}
};

// Echo Ward and the cooldowns are cast on the shaman: the ward and the golem stand beside it, and the rest are its own
// auras. Each leaves an aura of its name on the shaman while it lasts, so the buff check keeps it from being recast.
BUFF_ACTION(CastEchoWardAction, "echo ward");
BUFF_ACTION(CastSpiritShroudAction, "spirit shroud");
BUFF_ACTION(CastLoasRideAction, "loa's ride");
BUFF_ACTION(CastMojoGolemAction, "mojo golem");
BUFF_ACTION(CastMarionetteDanceAction, "marionette dance");

#endif
