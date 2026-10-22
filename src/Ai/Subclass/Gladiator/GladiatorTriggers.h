/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// When the Gladiator sub-class path's spells are worth casting.

#ifndef PLAYERBOTS_GLADIATORTRIGGERS_H
#define PLAYERBOTS_GLADIATORTRIGGERS_H

#include "AiObject.h"
#include "GenericTriggers.h"

HAS_AURA_TRIGGER(UltimatumTrigger, "ultimatum");
CAN_CAST_TRIGGER(ShieldSlamTrigger, "shield slam");
CD_TRIGGER(DragonRoarTrigger, "dragon roar");
BOOST_TRIGGER(AvatarTrigger, "avatar");
BOOST_TRIGGER(BloodbathTrigger, "bloodbath");
DEFLECT_TRIGGER(MassSpellReflectionTrigger, "mass spell reflection");
INTERRUPT_TRIGGER(StormBoltInterruptSpellTrigger, "storm bolt");
INTERRUPT_HEALER_TRIGGER(StormBoltInterruptEnemyHealerSpellTrigger, "storm bolt");

// The stance can't be entered in combat, so it's taken out of combat by a warrior who knows it.
class GladiatorStanceTrigger : public BuffTrigger
{
public:
    GladiatorStanceTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "gladiator stance") {}

    bool IsActive() override;
};

// Shield Charge's buff is down, the charge is ready and the target is within its 10 yards.
class ShieldChargeTrigger : public BuffTrigger
{
public:
    ShieldChargeTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "shield charge") {}

    bool IsActive() override;
};

#endif
