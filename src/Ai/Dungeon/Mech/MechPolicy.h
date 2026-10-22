/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for The Mechanar: Sepethrea's flames and fire, and Capacitus's heroic Polarity Shift spots.

#ifndef _PLAYERBOTS_MECH_POLICY_H
#define _PLAYERBOTS_MECH_POLICY_H

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include "DungeonTacticsPolicy.h"

namespace Mechanar
{
struct Point
{
    float x = 0.0f;
    float y = 0.0f;
};

// A Raging Flames fire patch (35278) on the floor
struct TrailPatch
{
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
};

// Nethermancer Sepethrea

// Inferno (35283) pulses this far round a flame
constexpr float InfernoRadius = 10.0f;
constexpr float InfernoAvoidRange = 10.0f;
constexpr float InfernoAvoidClear = 19.0f;
// A fixated kiter keeps this berth; a bot that is not chased only clears MeleeFlameClear
constexpr float InfernoSafeDist = 14.0f;
constexpr float MeleeFlameClear = 12.0f;
constexpr float KiteThreshold = 16.0f;
constexpr float KiteStep = 14.0f;
constexpr std::array<float, 3> KiteStepLengths = {KiteStep, 10.0f, 7.0f};
constexpr std::array<float, 18> KiteFanOffsets = {0.0f,  0.35f,  -0.35f, 0.70f,  -0.70f, 1.05f,  -1.05f, 1.40f,  -1.40f,
                                                  1.75f, -1.75f, 2.10f,  -2.10f, 2.45f,  -2.45f, 2.80f,  -2.80f, 3.14f};
constexpr float KiteLongAxisWeight = 7.0f;
constexpr float KiteGroupClearance = 16.0f;

constexpr float KiteTurnRunway = 16.0f;
constexpr float KiteTurnThreshold = 24.0f;
constexpr float KiteTurnPassBehind = 20.0f;
constexpr float KiteTurnWallInset = 4.0f;
constexpr float KiteTurnExitPast = 3.0f;
constexpr float KiteTurnClearWeight = 8.0f;
constexpr float KiteTurnAbortClear = -2.0f;
constexpr float KiteTurnLaneEndMargin = 3.0f;
constexpr float KiteTurnWallReached = 2.0f;

constexpr float HealerFireBailPct = 50.0f;
constexpr float HealerKiteLeash = 34.0f;
constexpr float HealerKiteLeashMax = 38.0f;
constexpr float HealerKiteLeashWeight = 3.0f;
constexpr float HealerKiteFlamePathWeight = 2.5f;

constexpr float TrailPatchRadius = 5.0f;
constexpr float TrailDangerMargin = 2.5f;
constexpr float TrailScan = 18.0f;
constexpr float TrailClearStep = 10.0f;

constexpr float FlameSearchStart = 4.0f;
constexpr float FlameSearchStep = 3.0f;
constexpr float FlameSearchEnd = InfernoAvoidClear + 8.0f;
constexpr int FlameSearchDirections = 16;

// Sepethrea's room: a box along the y axis, so kiting runs up and down its length
constexpr Point RoomAnchor = {300.0f, 6.0f};
constexpr float RoomXMin = 285.0f;
constexpr float RoomXMax = 308.0f;
constexpr float RoomYMin = -26.0f;
constexpr float RoomYMax = 54.0f;
constexpr std::array<Point, 4> RoomCorners = {Point{RoomXMin, RoomYMin}, Point{RoomXMax, RoomYMin},
                                              Point{RoomXMax, RoomYMax}, Point{RoomXMin, RoomYMax}};

inline float Dist2d(Point a, Point b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

inline bool HasDirection(Point v) { return std::sqrt(v.x * v.x + v.y * v.y) >= 0.01f; }

inline bool Normalize(Point& v)
{
    float const length = std::sqrt(v.x * v.x + v.y * v.y);
    if (length < 0.01f)
        return false;
    v.x /= length;
    v.y /= length;
    return true;
}

inline Point ClosestOnSegment(Point p, Point a, Point b)
{
    float const dx = b.x - a.x;
    float const dy = b.y - a.y;
    float const length2 = dx * dx + dy * dy;
    float t = (length2 < 1e-6f) ? 0.0f : ((p.x - a.x) * dx + (p.y - a.y) * dy) / length2;
    t = std::clamp(t, 0.0f, 1.0f);
    return {a.x + t * dx, a.y + t * dy};
}

inline float MinSegDist(Point a, Point b, Point p) { return Dist2d(ClosestOnSegment(p, a, b), p); }

inline bool InRoom(Point p)
{
    bool inside = false;
    std::size_t j = RoomCorners.size() - 1;
    for (std::size_t i = 0; i < RoomCorners.size(); ++i)
    {
        Point const ci = RoomCorners[i];
        Point const cj = RoomCorners[j];
        if (((ci.y > p.y) != (cj.y > p.y)) && (p.x < (cj.x - ci.x) * (p.y - ci.y) / (cj.y - ci.y + 1e-6f) + ci.x))
            inside = !inside;
        j = i;
    }
    return inside;
}

inline bool SegmentInRoom(Point a, Point b)
{
    return InRoom(a) && InRoom(b) && InRoom({(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f});
}

// Nearest point on the room's edge, nudged half a yard inwards.
inline Point ClampIntoRoom(Point p)
{
    if (InRoom(p))
        return p;

    float best = 1e18f;
    Point result = p;
    std::size_t j = RoomCorners.size() - 1;
    for (std::size_t i = 0; i < RoomCorners.size(); ++i)
    {
        Point const c = ClosestOnSegment(p, RoomCorners[j], RoomCorners[i]);
        float const d = (c.x - p.x) * (c.x - p.x) + (c.y - p.y) * (c.y - p.y);
        if (d < best)
        {
            best = d;
            result = c;
        }
        j = i;
    }

    Point inward = {RoomAnchor.x - result.x, RoomAnchor.y - result.y};
    float const length = std::sqrt(inward.x * inward.x + inward.y * inward.y);
    if (length > 1e-3f)
    {
        result.x += inward.x / length * 0.5f;
        result.y += inward.y / length * 0.5f;
    }
    return result;
}

inline bool InTrailDanger(Point bot, std::vector<TrailPatch> const& patches)
{
    for (TrailPatch const& patch : patches)
        if (Dist2d(bot, {patch.x, patch.y}) < patch.radius + TrailDangerMargin)
            return true;
    return false;
}

inline bool SpotSafe(std::vector<Point> const& flames, std::vector<TrailPatch> const& patches, Point p,
                     float flameClear = InfernoSafeDist)
{
    for (Point const& flame : flames)
    {
        float const dx = p.x - flame.x;
        float const dy = p.y - flame.y;
        if (dx * dx + dy * dy < flameClear * flameClear)
            return false;
    }
    for (TrailPatch const& patch : patches)
    {
        float const dx = p.x - patch.x;
        float const dy = p.y - patch.y;
        float const r = patch.radius + TrailDangerMargin;
        if (dx * dx + dy * dy < r * r)
            return false;
    }
    return true;
}

// How far a spot is outside the nearest hazard; negative = inside one.
inline float HazardClearance(std::vector<Point> const& flames, std::vector<TrailPatch> const& patches, Point p,
                             float flameClear = InfernoSafeDist)
{
    float clear = 1e9f;
    for (Point const& flame : flames)
        clear = std::min(clear, Dist2d(p, flame) - flameClear);
    for (TrailPatch const& patch : patches)
        clear = std::min(clear, Dist2d(p, {patch.x, patch.y}) - (patch.radius + TrailDangerMargin));
    return clear;
}

inline int SegSamples(Point a, Point b) { return std::max(5, static_cast<int>(Dist2d(a, b) / 3.0f) + 1); }

inline Point Lerp(Point a, Point b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

// The start is skipped: a kiter already standing near a hazard may still walk away from it.
inline bool SegSafe(std::vector<Point> const& flames, std::vector<TrailPatch> const& patches, Point a, Point b)
{
    int const samples = SegSamples(a, b);
    for (int k = 1; k <= samples; ++k)
        if (!SpotSafe(flames, patches, Lerp(a, b, static_cast<float>(k) / samples)))
            return false;
    return true;
}

inline float SegClearance(std::vector<Point> const& flames, std::vector<TrailPatch> const& patches, Point a, Point b)
{
    int const samples = SegSamples(a, b);
    float clear = 1e9f;
    for (int k = 0; k <= samples; ++k)
        clear = std::min(clear, HazardClearance(flames, patches, Lerp(a, b, static_cast<float>(k) / samples)));
    return clear;
}

// Healers keep healing through the fire while they have someone to heal and are above half health.
constexpr bool HealerMayHoldFire(bool healer, float healthPct) { return healer && healthPct > HealerFireBailPct; }

// A tank keeps Sepethrea even when fixated, unless another tank already has her.
constexpr bool TankMustGrabBoss(bool tank, bool bossUp, bool otherTankHasBoss)
{
    return tank && bossUp && !otherTankHasBoss;
}

constexpr bool KitesFlame(bool bossUp, bool fixated, bool tankMustGrabBoss)
{
    return bossUp && fixated && !tankMustGrabBoss;
}

constexpr bool AvoidsFlame(bool bossUp, bool fixated, bool flameNear, bool healerHoldsFire)
{
    return bossUp && !fixated && flameNear && !healerHoldsFire;
}

constexpr bool AvoidsTrail(bool bossUp, bool trailDanger, bool healerHoldsFire)
{
    return bossUp && trailDanger && !healerHoldsFire;
}

// Away from the flame; on top of it, away from the room's middle.
inline Point FleeDirection(Point bot, Point flame)
{
    Point away = {bot.x - flame.x, bot.y - flame.y};
    if (Normalize(away))
        return away;
    away = {bot.x - RoomAnchor.x, bot.y - RoomAnchor.y};
    if (Normalize(away))
        return away;
    return {1.0f, 0.0f};
}

// Bends the flee direction away from the group the closer the kiter is to it, so the flame isn't led through them.
inline Point BiasAwayFromGroup(Point direction, Point bot, Point group, float groupDist)
{
    Point fromGroup = {bot.x - group.x, bot.y - group.y};
    if (!Normalize(fromGroup))
        return direction;
    float const bias = std::max(0.0f, (KiteGroupClearance - groupDist) / KiteGroupClearance) * 1.5f;
    direction.x += bias * fromGroup.x;
    direction.y += bias * fromGroup.y;
    Normalize(direction);
    return direction;
}

// Far enough from the flame and the group, inside the healer's leash and out of every hazard: stand still.
constexpr bool KiterHoldsPosition(float flameDist, float groupDist, bool leashed, float tankDist, bool spotSafe)
{
    return flameDist >= KiteThreshold && groupDist >= KiteGroupClearance && (!leashed || tankDist <= HealerKiteLeash) &&
           spotSafe;
}

// A kiter that drifted out of the room steps back in.
inline std::optional<Point> ReturnIntoRoom(Point bot)
{
    if (InRoom(bot))
        return std::nullopt;
    Point const edge = ClampIntoRoom(bot);
    Point direction = {edge.x - bot.x, edge.y - bot.y};
    if (!Normalize(direction))
        return std::nullopt;
    return ClampIntoRoom({bot.x + direction.x * KiteStep, bot.y + direction.y * KiteStep});
}

// Turning at the end of the room: run to a side wall, then back up the length past the flame.

constexpr float TurnLaneX(float side)
{
    return side > 0.0f ? RoomXMax - KiteTurnWallInset : RoomXMin + KiteTurnWallInset;
}

constexpr float TurnLaneEndY(float flameY, float endSign)
{
    return std::clamp(flameY - endSign * KiteTurnPassBehind, RoomYMin + KiteTurnLaneEndMargin,
                      RoomYMax - KiteTurnLaneEndMargin);
}

// Healers on a leash never turn; others turn when running along the length into an end with the flame close.
inline bool StartsTurn(bool leashed, float fleeY, float flameDist, float endDist)
{
    return !leashed && std::fabs(fleeY) >= 0.5f && flameDist <= KiteTurnThreshold && endDist < KiteTurnRunway;
}

constexpr float TurnEndSign(float fleeY) { return fleeY > 0.0f ? 1.0f : -1.0f; }

constexpr float DistanceToRoomEnd(float botY, float endSign)
{
    return endSign > 0.0f ? RoomYMax - botY : botY - RoomYMin;
}

// Once back along the length, the turn ends when the flame is out of reach and the kiter is past it.
constexpr bool TurnPassedFlame(bool lateral, float flameDist, float botY, float flameY, float endSign)
{
    return !lateral && flameDist > KiteThreshold && (botY - flameY) * -endSign > KiteTurnExitPast;
}

inline float TurnLaneClearance(std::vector<Point> const& flames, std::vector<TrailPatch> const& patches, Point bot,
                               float side, float laneEndY, bool lateral)
{
    float const laneX = TurnLaneX(side);
    if (lateral)
        return std::min(SegClearance(flames, patches, bot, {laneX, bot.y}),
                        SegClearance(flames, patches, {laneX, bot.y}, {laneX, laneEndY}));
    return SegClearance(flames, patches, bot, {laneX, laneEndY});
}

struct TurnChoice
{
    float side = 1.0f;
    float laneClear = -1e18f;
};

// The side with the clearest lane, then the one farther from the flame.
inline TurnChoice ChooseTurnSide(std::vector<Point> const& flames, std::vector<TrailPatch> const& patches, Point bot,
                                 Point flame, float endSign)
{
    TurnChoice choice;
    float bestKey = -1e18f;
    for (float side : {1.0f, -1.0f})
    {
        float const laneClear = TurnLaneClearance(flames, patches, bot, side, TurnLaneEndY(flame.y, endSign), true);
        float const key = laneClear * KiteTurnClearWeight + std::fabs(TurnLaneX(side) - flame.x);
        if (key > bestKey)
        {
            bestKey = key;
            choice.side = side;
            choice.laneClear = laneClear;
        }
    }
    return choice;
}

struct KiteSearch
{
    Point bot;
    float baseAngle = 0.0f;
    bool leashed = false;  // a healer kiting within reach of the tank
    Point tank;
    float tankDist = 0.0f;
};

// Clear step on a fan round the flee direction, else the one with most clearance; nothing if none stays in the room.
inline std::optional<Point> ChooseKiteStep(KiteSearch const& search, std::vector<Point> const& flames,
                                           std::vector<TrailPatch> const& patches, std::vector<Point> const& chasers)
{
    std::optional<Point> best;
    float bestScore = -1e18f;
    std::optional<Point> fallback;
    float bestClear = -1e18f;
    for (float offset : KiteFanOffsets)
    {
        float const angle = search.baseAngle + offset;
        for (float length : KiteStepLengths)
        {
            Point const dest = {search.bot.x + std::cos(angle) * length, search.bot.y + std::sin(angle) * length};
            if (!SegmentInRoom(search.bot, dest))
                continue;

            float const clear = HazardClearance(flames, patches, dest);
            if (clear > bestClear)
            {
                bestClear = clear;
                fallback = dest;
            }

            if (!SegSafe(flames, patches, search.bot, dest))
                continue;

            float const destTankDist = search.leashed ? Dist2d(dest, search.tank) : 0.0f;
            if (search.leashed && destTankDist > HealerKiteLeashMax && destTankDist >= search.tankDist)
                continue;

            float chaserDist = 1e9f;
            for (Point const& chaser : chasers)
                chaserDist = std::min(chaserDist, Dist2d(dest, chaser));
            float const longAxis = search.leashed ? 0.0f : KiteLongAxisWeight;
            float score = chaserDist + longAxis * std::fabs(std::sin(angle)) + length * 0.1f;
            if (search.leashed)
            {
                if (destTankDist > HealerKiteLeash)
                    score -= HealerKiteLeashWeight * (destTankDist - HealerKiteLeash);
                float passDist = 1e9f;
                for (Point const& chaser : chasers)
                    passDist = std::min(passDist, MinSegDist(search.bot, dest, chaser));
                if (passDist < InfernoRadius)
                    score -= HealerKiteFlamePathWeight * (InfernoRadius - passDist);
            }
            if (score > bestScore)
            {
                bestScore = score;
                best = dest;
            }
        }
    }
    return best ? best : fallback;
}

// Nearest ring spot clear of every flame and patch, else the clearest spot found; nothing when already clear.
inline std::optional<Point> FindFlameClearSpot(Point bot, std::vector<Point> const& flames,
                                               std::vector<TrailPatch> const& patches)
{
    if (SpotSafe(flames, patches, bot, MeleeFlameClear))
        return std::nullopt;

    std::optional<Point> best;
    float bestClear = HazardClearance(flames, patches, bot, MeleeFlameClear);
    for (float radius = FlameSearchStart; radius <= FlameSearchEnd; radius += FlameSearchStep)
    {
        for (int i = 0; i < FlameSearchDirections; ++i)
        {
            float const angle = (i * 2.0f * DungeonTactics::Pi) / FlameSearchDirections;
            Point const spot = {bot.x + std::cos(angle) * radius, bot.y + std::sin(angle) * radius};
            if (!InRoom(spot))
                continue;
            if (SpotSafe(flames, patches, spot, MeleeFlameClear))
                return spot;
            float const clear = HazardClearance(flames, patches, spot, MeleeFlameClear);
            if (clear > bestClear)
            {
                bestClear = clear;
                best = spot;
            }
        }
    }
    return best;
}

// Sum of pushes out of every patch the bot stands in, deeper = stronger; nothing when it stands in none.
inline std::optional<Point> TrailPush(Point bot, std::vector<TrailPatch> const& patches)
{
    Point push;
    bool danger = false;
    for (TrailPatch const& patch : patches)
    {
        float const dx = bot.x - patch.x;
        float const dy = bot.y - patch.y;
        float const d = std::sqrt(dx * dx + dy * dy);
        float const reach = patch.radius + TrailDangerMargin;
        if (d >= reach)
            continue;
        danger = true;
        if (d < 0.1f)
            continue;
        float const weight = (reach - d) / reach;
        push.x += dx / d * weight;
        push.y += dy / d * weight;
    }
    if (!danger)
        return std::nullopt;
    return push;
}

// Out along the push; with no clear push, away from the nearest flame, then from the room's middle.
inline Point TrailEscapeSpot(Point bot, Point push, std::optional<Point> nearestFlame)
{
    Point direction = push;
    if (!Normalize(direction))
    {
        direction = nearestFlame ? Point{bot.x - nearestFlame->x, bot.y - nearestFlame->y} : push;
        if (!Normalize(direction))
        {
            direction = {bot.x - RoomAnchor.x, bot.y - RoomAnchor.y};
            if (!Normalize(direction))
                direction = {1.0f, 0.0f};
        }
    }
    Point const dest = {bot.x + direction.x * TrailClearStep, bot.y + direction.y * TrailClearStep};
    return SegmentInRoom(bot, dest) ? dest : ClampIntoRoom(dest);
}

struct SepethreaMoment
{
    bool kiting = false;  // fixated by a flame and not the tank who must keep her
    bool healerHoldsFire = false;
    bool hazardNear = false;  // in a trail patch or close to a flame
};

struct SepethreaAction
{
    bool flameMove = false;  // one of the flame kite, avoid or focus actions
    bool mover = false;      // a movement action or a cast that walks into range
};

// Only the flame moves may walk a kiter, or a bot near fire, anywhere.
constexpr bool SepethreaAllows(SepethreaMoment fight, SepethreaAction action)
{
    if (action.flameMove || !action.mover)
        return true;
    if (fight.kiting)
        return false;
    return fight.healerHoldsFire || !fight.hazardNear;
}

// Assist, debuffs on other attackers and dps AoE would pull bots onto the flames instead of Sepethrea.
constexpr bool LeavesSepethrea(bool assist, bool debuffOnAttacker, bool aoe, bool dps, bool heal)
{
    return assist || debuffOnAttacker || (aoe && dps && !heal);
}

constexpr bool SepethreaFocusAllows(bool bossUp, bool leavesBoss) { return !(bossUp && leavesBoss); }

// Mechano-Lord Capacitus (heroic)

enum class Polarity
{
    None,
    Positive,
    Negative
};

// Each charge pulses 10 yards (39090, 39093) and only hurts the opposite charge
constexpr float PolarityPulseRadius = 10.0f;
// Area spells reach a player's 1.5-yard combat reach past their radius
constexpr float PolarityPulseReach = PolarityPulseRadius + 1.5f;
// His 15-yard combat reach keeps both spots in melee range (about 17.8 yards from his centre)
constexpr float PolaritySpotOffset = 11.0f;
constexpr float PolaritySpotTolerance = 2.0f;
// Opposite charges each stopping short of their spot towards the other still stand out of each other's pulse
constexpr float PolarityWorstSeparation = 2.0f * (PolaritySpotOffset - PolaritySpotTolerance);

// The tank's charge stands in front with him, the other behind the boss; with no tank charge, positives in front.
constexpr bool PolarityStandsInFront(Polarity mine, Polarity tank)
{
    return tank == Polarity::None ? mine == Polarity::Positive : mine == tank;
}

// Along the boss's facing: +1 in front, -1 behind.
constexpr float PolaritySpotSign(bool front) { return front ? 1.0f : -1.0f; }

// Polarity Shift is heroic only; whoever holds the boss stays put and the others go to their charge's spot.
constexpr bool MovesForPolarity(bool heroic, bool holdsBoss, Polarity mine, float distanceToSpot)
{
    return heroic && !holdsBoss && mine != Polarity::None && distanceToSpot > PolaritySpotTolerance;
}

// A charged bot holds its spot: set-behind and flee would walk it off and the polarity tactic would walk it back.
constexpr bool CapacitusAllows(bool heroic, bool holdsBoss, Polarity mine, bool repositions)
{
    return !(heroic && !holdsBoss && mine != Polarity::None && repositions);
}
}  // namespace Mechanar

#endif
