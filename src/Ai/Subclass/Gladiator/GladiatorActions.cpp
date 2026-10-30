/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Gladiator's actions that need more than a spell cast.

#include "GladiatorActions.h"

#include "Playerbots.h"

bool CastShieldChargeAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    // A charge moves the bot, which a staying bot mustn't do; in melee range it only buffs.
    if (!bot->IsWithinMeleeRange(target) && botAI->HasStrategy("stay", botAI->GetState()))
        return false;

    return CastSpellAction::isUseful();
}

// Out of range makes it impossible rather than useless, so the node falls back to Charge and then to walking.
bool CastShieldChargeAction::isPossible()
{
    Unit* target = GetTarget();
    return target && bot->IsWithinDistInMap(target, range) && CastSpellAction::isPossible();
}
