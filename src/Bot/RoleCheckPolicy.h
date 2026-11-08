/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The role check's verdict (solo-wow): what a bot group did in an instance, counted by the .rolecheck harness, against
// what each role should do. Pure rules, unit tested in solo-wow's tests/role-check-policy.cpp.

#ifndef _PLAYERBOT_ROLE_CHECK_POLICY_H
#define _PLAYERBOT_ROLE_CHECK_POLICY_H

#include <cstdint>
#include <string>
#include <vector>

namespace RoleCheck
{
// Each count is of samples taken twice a second in a fight, after its first seconds (the tank's time to pick up)
struct Tally
{
    uint32_t fights = 0;
    uint32_t pulls = 0;         // fights a group member opened (in a raid the player opens them)
    uint32_t tankPulls = 0;     // of those, the tank's
    uint32_t mobSamples = 0;    // a mob fighting the group
    uint32_t onTank = 0;        // ...hitting a tank
    uint32_t facedSamples = 0;  // a mob on the tank, in melee reach of it and standing (one moving has no facing to keep)
    uint32_t facedAtGroup = 0;  // ...with someone other than a tank in front of it
    uint32_t dpsSamples = 0;    // a damage dealer, while the tank had a target
    uint32_t dpsOnFocus = 0;    // ...on the tank's target or the skull
    uint32_t healerSamples = 0;
    uint32_t healerAtRange = 0;  // ...with no mob in the fight within HealerRange
    uint32_t hurtFights = 0;     // fights the group took damage in
    uint32_t healedFights = 0;   // ...that a healer healed someone in
    uint32_t deaths = 0;
    uint32_t kills = 0;
    uint32_t bosses = 0;
};

inline constexpr float HealerRange = 10.0f;     // yards from the nearest mob a healer counts as standing back
inline constexpr float HealReach = 38.0f;       // a healer further from the tank than this, or out of sight, cannot heal it
inline constexpr float FrontReach = 10.0f;     // yards in front of a mob a cleave or breath reaches
inline constexpr uint32_t GraceMs = 5000;       // from a fight's start: the tank picking the pull up
inline constexpr uint32_t MinFights = 3;        // fewer is too little to judge

inline constexpr double TankAggroPct = 75.0;    // of mob samples on a tank
inline constexpr double TankPullPct = 75.0;     // of the pulls (dungeons, where the tank leads)
inline constexpr double FacingPct = 20.0;       // at most, of the tank's mobs facing someone else
inline constexpr double DpsFocusPct = 60.0;     // of damage dealer samples on the tank's target
inline constexpr double HealerRangePct = 70.0;  // of healer samples standing back
inline constexpr double HealedPct = 80.0;       // of the hurt fights
inline constexpr float HurtHealthPct = 70.0f;   // a member under this much health makes a fight a hurt one

struct Line
{
    std::string what;
    double pct = 0.0;   // the measured share, -1 when there was nothing to measure
    double bound = 0.0;
    bool atMost = false;  // the share must stay at or under the bound, not reach it
    bool pass = false;
};

inline double Pct(uint32_t part, uint32_t whole) { return whole ? 100.0 * double(part) / double(whole) : -1.0; }

inline Line Judge(std::string what, uint32_t part, uint32_t whole, double bound, bool atMost = false)
{
    Line line{std::move(what), Pct(part, whole), bound, atMost, false};
    // Nothing to measure is no failure of the role: no healer sample with no healer, no facing sample with no melee
    line.pass = line.pct < 0.0 || (atMost ? line.pct <= bound : line.pct >= bound);
    return line;
}

// The lines a run is judged on; `leads` when the tank is to lead the pulls (a dungeon, not a raid the player pulls)
inline std::vector<Line> Judge(Tally const& t, bool leads)
{
    std::vector<Line> lines;
    lines.push_back(Line{"fights", double(t.fights), double(MinFights), false, t.fights >= MinFights});
    if (leads)
        lines.push_back(Judge("tank opens the pulls", t.tankPulls, t.pulls, TankPullPct));
    lines.push_back(Judge("mobs on the tank", t.onTank, t.mobSamples, TankAggroPct));
    lines.push_back(Judge("tank's mobs facing the group", t.facedAtGroup, t.facedSamples, FacingPct, true));
    lines.push_back(Judge("damage dealers on the tank's target", t.dpsOnFocus, t.dpsSamples, DpsFocusPct));
    lines.push_back(Judge("healer standing back", t.healerAtRange, t.healerSamples, HealerRangePct));
    lines.push_back(Judge("hurt fights healed", t.healedFights, t.hurtFights, HealedPct));
    return lines;
}

inline bool Passed(std::vector<Line> const& lines)
{
    for (Line const& line : lines)
        if (!line.pass)
            return false;
    return true;
}
}  // namespace RoleCheck

#endif
