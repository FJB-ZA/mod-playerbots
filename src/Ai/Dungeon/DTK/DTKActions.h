/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_DTKACTIONS_H
#define PLAYERBOTS_DTKACTIONS_H

#include "Action.h"
#include "AttackAction.h"
#include "DTKTriggers.h"
#include "GenericSpellActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

class CorpseExplodeSpreadAction : public MovementAction
{
public:
    CorpseExplodeSpreadAction(PlayerbotAI* botAI) : MovementAction(botAI, "corpse explode spread") {}
    bool Execute(Event event) override;
};

class AvoidArcaneFieldAction : public MovementAction
{
public:
    AvoidArcaneFieldAction(PlayerbotAI* botAI) : MovementAction(botAI, "avoid arcane field") {}
    bool Execute(Event event) override;
};

class NovosDefaultPositionAction : public MovementAction
{
public:
    NovosDefaultPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "novos default position") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class NovosTargetPriorityAction : public AttackAction
{
public:
    NovosTargetPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "novos target priority") {}
    bool Execute(Event event) override;
    // bool isUseful() override;
};

// Not a CastSpellAction: the Tharon'ja multiplier blocks every one of those in skeleton form
class TharonjaSkeletonAction : public Action
{
public:
    TharonjaSkeletonAction(PlayerbotAI* botAI) : Action(botAI, "tharonja skeleton") {}
    bool Execute(Event event) override;
};

#endif
