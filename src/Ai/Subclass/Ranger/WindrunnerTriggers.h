/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// When the Ranger sub-class path's Windrunner tree spells are worth casting. Advantage is the buff of up to 5 stacks
// the Ranger's shots build (spell 993010; its hidden passive shares the name, so the triggers look it up by id).

#ifndef PLAYERBOTS_WINDRUNNERTRIGGERS_H
#define PLAYERBOTS_WINDRUNNERTRIGGERS_H

#include "AiObject.h"
#include "GenericTriggers.h"

// The bot's Serrated Shot is missing from its target, whatever the target's expected life
class SerratedShotTrigger : public DebuffTrigger
{
public:
    SerratedShotTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "serrated shot", 1, true, 0.5f) {}
    bool IsActive() override { return BuffTrigger::IsActive(); }
};
CD_TRIGGER(RicochetShotTrigger, "ricochet shot");
CD_TRIGGER(PiercingVolleyTrigger, "piercing volley");

// Advantage is full: time to spend it
class FullAdvantageTrigger : public Trigger
{
public:
    FullAdvantageTrigger(PlayerbotAI* botAI) : Trigger(botAI, "full advantage") {}
    bool IsActive() override;
};

// Advantage isn't full and Arcane Shot is ready: below level 50 (no Steady Shot) and between Steady Shots it is the
// builder the Marksmanship rotation leaves out
class BuildAdvantageTrigger : public Trigger
{
public:
    BuildAdvantageTrigger(PlayerbotAI* botAI) : Trigger(botAI, "build advantage") {}
    bool IsActive() override;
};

// Full Advantage and no Skirmish running: Skirmish first, it makes the finishers after it cheaper and surer
class SkirmishTrigger : public Trigger
{
public:
    SkirmishTrigger(PlayerbotAI* botAI) : Trigger(botAI, "skirmish") {}
    bool IsActive() override;
};

// Full Advantage with a pack around the target: Incendiary Shot bursts on all of them
class IncendiaryShotTrigger : public Trigger
{
public:
    IncendiaryShotTrigger(PlayerbotAI* botAI) : Trigger(botAI, "incendiary shot") {}
    bool IsActive() override;
};

// The target is below 20% health, or Telling Blow lets Final Arrow ignore that
class FinalArrowTrigger : public Trigger
{
public:
    FinalArrowTrigger(PlayerbotAI* botAI) : Trigger(botAI, "final arrow") {}
    bool IsActive() override;
};

// The target is casting something Hushing Arrow can stop, and the Ranger has Advantage to spend on it
class HushingArrowTrigger : public InterruptSpellTrigger
{
public:
    HushingArrowTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "hushing arrow") {}
    bool IsActive() override;
};

#endif
