/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Gladiator sub-class path's spells, as bot actions.

#ifndef PLAYERBOTS_GLADIATORACTIONS_H
#define PLAYERBOTS_GLADIATORACTIONS_H

#include "AiObject.h"
#include "GenericSpellActions.h"

BUFF_ACTION(CastGladiatorStanceAction, "gladiator stance");
MELEE_ACTION(CastDragonRoarAction, "dragon roar");
BUFF_ACTION(CastAvatarAction, "avatar");
MELEE_ACTION(CastImpendingVictoryAction, "impending victory");
SPELL_ACTION(CastStormBoltAction, "storm bolt");
ENEMY_HEALER_ACTION(CastStormBoltOnEnemyHealerAction, "storm bolt");
BUFF_ACTION(CastBloodbathAction, "bloodbath");
BUFF_ACTION(CastShieldBarrierAction, "shield barrier");
BUFF_ACTION(CastDieByTheSwordAction, "die by the sword");
BUFF_ACTION(CastRallyingCryAction, "rallying cry");
BUFF_ACTION(CastMassSpellReflectionAction, "mass spell reflection");

// A 0-10 yard charge that also buffs Shield Slam, Revenge and Heroic Strike, so it's used in melee range too.
class CastShieldChargeAction : public CastSpellAction
{
public:
    CastShieldChargeAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "shield charge") { range = 10.0f; }

    bool isUseful() override;
    bool isPossible() override;
};

#endif
