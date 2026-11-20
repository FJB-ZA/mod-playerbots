/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// When the Witch Doctor sub-class path's Hexcraft tree spells are worth casting. Voodoo Shock is the path's Flame
// Shock: the bot asks for "flame shock" and the sub-class aliases answer with the path's spell.

#ifndef PLAYERBOTS_HEXCRAFTTRIGGERS_H
#define PLAYERBOTS_HEXCRAFTTRIGGERS_H

#include "AiObject.h"
#include "GenericTriggers.h"

CD_TRIGGER(ShadePuppetTrigger, "shade puppet");
CD_TRIGGER(LoasSpiteTrigger, "loa's spite");
CD_TRIGGER(WitchfireTrigger, "witchfire");

// Echo Ward is ready and down (its 12 second aura on the shaman, spell 994003), and the target will outlive most of
// it: a boss, an elite, a player, a training dummy, or anything the group won't kill in the next 8 seconds
class EchoWardTrigger : public Trigger
{
public:
    EchoWardTrigger(PlayerbotAI* botAI) : Trigger(botAI, "echo ward") {}
    bool IsActive() override;
};

// A burst cooldown, named for its spell, that is ready while the bot fights a boss, an elite, a player or a training
// dummy (the balance harness fights one, as Elemental Mastery is used on it)
class HexcraftBurstTrigger : public Trigger
{
public:
    HexcraftBurstTrigger(PlayerbotAI* botAI, std::string const spell) : Trigger(botAI, spell) {}
    bool IsActive() override;
};

class SpiritShroudTrigger : public HexcraftBurstTrigger
{
public:
    SpiritShroudTrigger(PlayerbotAI* botAI) : HexcraftBurstTrigger(botAI, "spirit shroud") {}
};

// Loa's Ride bursts on each periodic tick, so the target holds the bot's Voodoo Shock or Shade Puppet
class LoasRideTrigger : public HexcraftBurstTrigger
{
public:
    LoasRideTrigger(PlayerbotAI* botAI) : HexcraftBurstTrigger(botAI, "loa's ride") {}
    bool IsActive() override;
};

// Marionette Dance roots the shaman for 8 seconds before its form comes, so no enemy may be in melee range and the
// target must not be nearly dead
class MarionetteDanceTrigger : public HexcraftBurstTrigger
{
public:
    MarionetteDanceTrigger(PlayerbotAI* botAI) : HexcraftBurstTrigger(botAI, "marionette dance") {}
    bool IsActive() override;
};

// Mojo Golem's absorb when the bot is below half health; with Wrathful Golem (its haste) also as a burst cooldown
class MojoGolemTrigger : public HexcraftBurstTrigger
{
public:
    MojoGolemTrigger(PlayerbotAI* botAI) : HexcraftBurstTrigger(botAI, "mojo golem") {}
    bool IsActive() override;
};

// The target is casting and holds no jinx yet. The jinx silences it on its next cast, so it goes out beside Wind
// Shear, which stops the cast under way.
class ChokingJinxTrigger : public InterruptSpellTrigger
{
public:
    ChokingJinxTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "choking jinx") {}
    bool IsActive() override;
};

#endif
