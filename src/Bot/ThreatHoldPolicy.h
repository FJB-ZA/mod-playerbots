/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// When a damage dealer may open on a mob while a tank is in the fight: once the tank has held it a moment. Holding back
// at 80% of the tank's threat (ThreatMultiplier) cannot stop the first hit: at the pull the tank has a hundred or two
// threat, and one frostbolt or a rogue's opener is several times that. In the solo-wow role check (2026-11-06) most mobs
// that left the tank went to a damage dealer in the first five seconds, with the tank at 60-240 threat and the damage
// dealer at 500-1800.
//
// A mob the tank does not hold yet (still running in, or on someone else) is waited on too, for a while: the rerun with
// only the held mobs waited on still had 360 mobs go first to a damage dealer, most of them at five seconds and many with
// no threat at all on the tank. Past TankLateMs the tank has had its chance, and the damage dealers may take the mob off
// whoever it is on.

#ifndef _PLAYERBOTS_THREAT_HOLD_POLICY_H
#define _PLAYERBOTS_THREAT_HOLD_POLICY_H

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace BotThreat
{
constexpr uint32_t TankHoldMs = 2500;  // a tank's first few swings and a thunder clap
constexpr uint32_t TankLateMs = 6000;  // a mob the tank has not taken by then is the damage dealers' to take
constexpr size_t MaxClocks = 256;      // mobs remembered at once; a dead or gone mob's clock is dropped past this

// Who a mob is on, as one damage dealer sees it
enum class MobOn
{
    NoTankInFight,  // there is no tank in the fight to wait for
    Self,           // the damage dealer itself: it hits back
    Tank,           // a tank in the group
    Other           // no one yet, or another group member
};

// How long each mob has been on the tank, or off it, as one damage dealer saw it
class TankHoldClock
{
public:
    // The milliseconds the mob has been held by the tank (heldByTank) or off it (not), since that last changed; nowMs
    // from a wrapping clock
    uint32_t Held(uint64_t mob, bool heldByTank, uint32_t nowMs)
    {
        auto it = clocks_.find(mob);
        if (it == clocks_.end())
        {
            if (clocks_.size() >= MaxClocks)
                clocks_.clear();
            clocks_.emplace(mob, Clock{heldByTank, nowMs});
            return 0;
        }
        if (it->second.held != heldByTank)
        {
            it->second = Clock{heldByTank, nowMs};
            return 0;
        }
        return nowMs - it->second.since;
    }

    // May a damage dealer open on the mob yet?
    bool MayOpen(uint64_t mob, MobOn on, uint32_t nowMs)
    {
        switch (on)
        {
            case MobOn::NoTankInFight:
            case MobOn::Self:
                return true;
            case MobOn::Tank:
                return Held(mob, true, nowMs) >= TankHoldMs;
            case MobOn::Other:
                return Held(mob, false, nowMs) >= TankLateMs;
        }
        return true;
    }

private:
    struct Clock
    {
        bool held;
        uint32_t since;
    };
    std::unordered_map<uint64_t, Clock> clocks_;
};

// Past the hold, holding back at 80% of the tank's threat still lets through a hit bigger than the gap. At level 20 a
// tank has 8-25 threat at five seconds and one frostbolt is 60-170: the mob turned on the mage in fight after fight of
// the solo-wow role check (Deadmines, 2026-11-07). A damage dealer holds a hit that would carry its threat past the mark a
// mob turns at, judged by what its hits have come to.
constexpr float TurnsAt = 1.1f;    // a mob turns from its victim at 110% of its threat in melee, 130% at range
constexpr float HitWeight = 0.3f;  // how far each new hit moves the usual hit

// A damage dealer's usual hit, in threat, from what its threat on the mob it hits has risen by
class UsualHit
{
public:
    // Its threat on the mob it hits now
    void Saw(uint64_t mob, float threat)
    {
        if (mob == mob_ && threat > threat_)
        {
            float const gain = threat - threat_;
            usual_ = usual_ <= 0.0f ? gain : usual_ + HitWeight * (gain - usual_);
        }
        mob_ = mob;
        threat_ = threat;
    }

    float Threat() const { return usual_; }

private:
    uint64_t mob_ = 0;
    float threat_ = 0.0f;
    float usual_ = 0.0f;
};

// Would one more usual hit take the mob off the tank? With the tank at no threat, there is nothing to measure against.
inline bool WouldTurnIt(float tankThreat, float ownThreat, float hit)
{
    return tankThreat > 0.0f && ownThreat + hit >= tankThreat * TurnsAt;
}

// Mobs no tank keeps, whose target threat does not pick: holding back for the tank leaves them on whoever they chose.
// Krik'thir's Anub'ar Skirmishers fixate a random player as they enter combat (SmartAI 'Fixate Trigger'), and the
// damage dealers sent to kill them first stood waiting on a hold that never came: 21 deaths and 2 wipes at the
// watchers (role check, Azjol-Nerub, 2026-11-09).
constexpr uint32_t NPC_ANUBAR_SKIRMISHER = 28734;
constexpr uint32_t NPC_ANUBAR_SKIRMISHER_H = 31606;

constexpr bool TankKeeps(uint32_t entry)
{
    return entry != NPC_ANUBAR_SKIRMISHER && entry != NPC_ANUBAR_SKIRMISHER_H;
}

// Auto attack is no spell: past the hold nothing measured it, and a rogue's white swings carried it to 1.5-2 times the
// tank's threat, mob after mob off the tank (solo-wow role check, Underbog, 2026-11-08). A melee damage dealer stops
// swinging near the mark a mob turns at, and swings again once the tank has pulled clear: apart, so it does not flicker.
constexpr float SwingsPauseAt = 0.9f;    // of the tank's threat: the swing in the air still lands under TurnsAt
constexpr float SwingsResumeAt = 0.75f;  // of the tank's threat

inline bool PauseSwings(float tankThreat, float ownThreat)
{
    return tankThreat > 0.0f && ownThreat >= tankThreat * SwingsPauseAt;
}

inline bool MaySwing(float tankThreat, float ownThreat)
{
    return tankThreat <= 0.0f || ownThreat < tankThreat * SwingsResumeAt;
}
}  // namespace BotThreat

#endif
