/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Azjol-Nerub bosses: Krik'thir's watchers and web wraps, and dodging Anub'arak's Pound.

#ifndef _PLAYERBOTS_AN_POLICY_H
#define _PLAYERBOTS_AN_POLICY_H

namespace AzjolNerub
{
// Krik'thir the Gatewatcher

constexpr bool AttacksWebWrap(bool healer) { return !healer; }

enum class Watcher
{
    Skirmisher,
    Shadowcaster,
    Named,  // Silthik, Gashra or Narjil
    Warrior,
    None
};

// Skirmishers first: they drop aggro, ignore taunts and backstab whoever they pick; -1 = not a watcher.
constexpr int WatcherPriority(Watcher watcher)
{
    switch (watcher)
    {
        case Watcher::Skirmisher:
            return 0;
        case Watcher::Shadowcaster:
            return 1;
        case Watcher::Named:
            return 2;
        case Watcher::Warrior:
            return 3;
        default:
            return -1;
    }
}

// A damage dealer on a watcher keeps it while no other is strictly before it in the order; -1 = not on a watcher
constexpr bool KeepsWatcher(int currentPriority, int bestPriority)
{
    return currentPriority >= 0 && currentPriority <= bestPriority;
}

// While watchers live and the boss still waits, no target swaps and no area damage: it pulls the packs waiting by him.
// Once he is in the fight everything is pulled, and his swarms want area damage.
constexpr bool KrikthirAllows(bool bossWaiting, bool watcherUp, bool dpsAssist, bool aoe)
{
    return !(bossWaiting && watcherUp && (dpsAssist || aoe));
}

// The two kinds of add summoned by Krik'thir's Swarm.
constexpr bool SwarmAdd(unsigned entry) { return entry == 28735 || entry == 28736; }
constexpr unsigned LooseSwarm = 3;
constexpr float SwarmPickupRange = 8.0f;

constexpr bool CollectSwarmAdd(bool tank, bool chasesBot, bool chasesParty, bool inPickupRange)
{
    return tank ? (!chasesBot && chasesParty && inPickupRange) : chasesBot;
}

// Anub'arak

// Pound hits everyone in front of him within 15 yards; the slack covers his size, measured from his centre
constexpr float PoundReach = 15.0f + 3.0f;
// Run this far past his middle to clear the cone, not just level with him
constexpr float PoundDodgeOvershoot = 2.0f;

// Ranged further out are already clear: running through him would only pull them into melee.
constexpr bool DodgesPound(bool behind, float distanceToBoss) { return !behind && distanceToBoss <= PoundReach; }

constexpr float PoundDodgeStep(float distanceToBoss) { return distanceToBoss + PoundDodgeOvershoot; }
}  // namespace AzjolNerub

#endif
