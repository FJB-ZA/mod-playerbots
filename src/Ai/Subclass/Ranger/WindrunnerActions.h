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

// Serrated Shot, kept up on the target. Like Serpent Sting it skips the target's time-to-live check, which a training
// dummy (little health, always healing) or a quick fight would otherwise fail.
class CastSerratedShotAction : public CastDebuffSpellAction
{
public:
    CastSerratedShotAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "serrated shot", true) {}
    bool isUseful() override { return CastAuraSpellAction::isUseful(); }
};
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
