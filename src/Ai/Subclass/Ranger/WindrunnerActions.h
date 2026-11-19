/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Ranger sub-class path's Windrunner tree spells, as bot actions.

#ifndef PLAYERBOTS_WINDRUNNERACTIONS_H
#define PLAYERBOTS_WINDRUNNERACTIONS_H

#include "AiObject.h"
#include "GenericSpellActions.h"

DEBUFF_CHECKISOWNER_ACTION(CastSerratedShotAction, "serrated shot");
BUFF_ACTION(CastSkirmishAction, "skirmish");

class CastPrecisionShotAction : public CastSpellAction
{
public:
    CastPrecisionShotAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "precision shot") {}
};

class CastIncendiaryShotAction : public CastSpellAction
{
public:
    CastIncendiaryShotAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "incendiary shot") {}
};

class CastFinalArrowAction : public CastSpellAction
{
public:
    CastFinalArrowAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "final arrow") {}
};

class CastRicochetShotAction : public CastSpellAction
{
public:
    CastRicochetShotAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "ricochet shot") {}
};

class CastHushingArrowAction : public CastSpellAction
{
public:
    CastHushingArrowAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "hushing arrow") {}
};

class CastPiercingVolleyAction : public CastSpellAction
{
public:
    CastPiercingVolleyAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "piercing volley") {}
};

#endif
