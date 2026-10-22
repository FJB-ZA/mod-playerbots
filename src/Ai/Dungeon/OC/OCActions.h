/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OCACTIONS_H
#define PLAYERBOTS_OCACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "PlayerbotAI.h"
#include "UseItemAction.h"

class AvoidUnstableSphereAction : public MovementAction
{
public:
    AvoidUnstableSphereAction(PlayerbotAI* botAI) : MovementAction(botAI, "avoid unstable sphere") {}
    bool Execute(Event event) override;
};

class MountDrakeAction : public UseItemAction
{
public:
    MountDrakeAction(PlayerbotAI* botAI) : UseItemAction(botAI, "mount drake") {}
    bool Execute(Event event) override;
    bool isPossible() override;
};

class DismountDrakeAction : public Action
{
public:
    DismountDrakeAction(PlayerbotAI* botAI) : Action(botAI, "dismount drake") {}
    bool Execute(Event event) override;
};

class OccFlyDrakeAction : public MovementAction
{
public:
    OccFlyDrakeAction(PlayerbotAI* botAI) : MovementAction(botAI, "occ fly drake") {}
    bool Execute(Event event) override;
};

class OccDrakeAttackAction : public Action
{
public:
    OccDrakeAttackAction(PlayerbotAI* botAI) : Action(botAI, "occ drake attack") {}
    bool Execute(Event event) override;

protected:
    Unit* vehicleBase;
    bool CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown);
    bool AmberDrakeAction(Unit* target);
    bool EmeraldDrakeAction(Unit* target);
    bool RubyDrakeAction(Unit* target);
};

class AvoidArcaneExplosionAction : public MovementAction
{
public:
    AvoidArcaneExplosionAction(PlayerbotAI* botAI) : MovementAction(botAI, "avoid arcane explosion") {}
    bool Execute(Event event) override;
};

class TimeBombSpreadAction : public MovementAction
{
public:
    TimeBombSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "time bomb spread") {}
    bool Execute(Event event) override;
};

#endif
