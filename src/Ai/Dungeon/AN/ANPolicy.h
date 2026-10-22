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

// While watchers live no target swaps and no area damage: it pulls the packs still waiting by the boss.
constexpr bool KrikthirAllows(bool bossUp, bool watcherUp, bool dpsAssist, bool aoe)
{
    return !(bossUp && watcherUp && (dpsAssist || aoe));
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
