/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// When a damage dealer may open on a mob the tank holds: once the tank has held it a moment. Holding back at 80% of
// the tank's threat (ThreatMultiplier) cannot stop the first hit: at the pull the tank has a hundred or two threat, and
// one frostbolt or a rogue's opener is several times that. In the solo-wow role check (2026-11-06) most mobs that left
// the tank went to a damage dealer in the first five seconds, with the tank at 60-240 threat and the damage dealer at
// 500-1800.

#ifndef _PLAYERBOTS_THREAT_HOLD_POLICY_H
#define _PLAYERBOTS_THREAT_HOLD_POLICY_H

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace BotThreat
{
constexpr uint32_t TankHoldMs = 2500;  // a tank's first few swings and a thunder clap
constexpr size_t MaxClocks = 256;      // mobs remembered at once; a dead or gone mob's clock is dropped past this

// How long the tank has held each mob, as one damage dealer saw it
class TankHoldClock
{
public:
    // The milliseconds the tank has held the mob, now that it does (heldByTank) or not; nowMs from a wrapping clock
    uint32_t Held(uint64_t mob, bool heldByTank, uint32_t nowMs)
    {
        if (!heldByTank)
        {
            since_.erase(mob);
            return 0;
        }
        if (since_.size() >= MaxClocks && !since_.count(mob))
            since_.clear();
        auto const [it, fresh] = since_.try_emplace(mob, nowMs);
        return fresh ? 0 : nowMs - it->second;
    }

    // May a damage dealer open on the mob yet? Only a mob the tank holds is waited on
    bool MayOpen(uint64_t mob, bool heldByTank, uint32_t nowMs)
    {
        uint32_t const held = Held(mob, heldByTank, nowMs);
        return !heldByTank || held >= TankHoldMs;
    }

private:
    std::unordered_map<uint64_t, uint32_t> since_;
};
}  // namespace BotThreat

#endif
