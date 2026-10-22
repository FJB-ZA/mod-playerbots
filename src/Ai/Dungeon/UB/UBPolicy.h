// Pure rules for the Underbog: Hungarfen's mushrooms and Foul Spores, and staying out of Underbat lashes.

#ifndef _PLAYERBOTS_UB_POLICY_H
#define _PLAYERBOTS_UB_POLICY_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "DungeonTacticsPolicy.h"

namespace Underbog
{
// Hungarfen

constexpr float SporeCloudRadiusFallback = 8.0f;
constexpr float FoulSporesRadiusFallback = 20.0f;
constexpr float SporeCloudMargin = 1.5f;
constexpr float FoulSporesMargin = 2.0f;

// A mushroom casts Spore Cloud on its 10th Grow: from 8 stacks bots start walking off
constexpr uint8_t GrowStacksDanger = 8;

constexpr float MushroomScanRange = 40.0f;
constexpr uint32_t MushroomScanInterval = 1000;
constexpr uint8_t MushroomIdleScanSkips = 9;
constexpr uint32_t MushroomLingerTime = 30000;

constexpr bool MushroomDangerous(bool alive, bool sporeCloud, uint8_t growStacks)
{
    return alive && (sporeCloud || growStacks >= GrowStacksDanger);
}

constexpr float MushroomDangerRange(float sporeCloudRadius, float combatReach)
{
    return sporeCloudRadius + combatReach + SporeCloudMargin;
}

// The tank holding Hungarfen backs straight off so he doesn't turn; everyone else flees freely.
constexpr bool TankBacksOffMushroom(bool tank, bool tankingBoss) { return tank && tankingBoss; }

constexpr float MushroomBackOffStep(float dangerRange, float distanceToMushroom)
{
    return dangerRange - distanceToMushroom + 2.0f;
}

constexpr float FoulSporesSafeDistance(float foulSporesRadius) { return foulSporesRadius + FoulSporesMargin; }

constexpr bool RetreatsFromFoulSpores(float distanceToBoss, float safeDistance)
{
    return distanceToBoss < safeDistance;
}

constexpr float FoulSporesRetreatStep(float distanceToBoss, float safeDistance)
{
    return safeDistance - distanceToBoss + 1.0f;
}

// Straight away from him first, then fanning out to either side up to a right angle.
constexpr float Pi = DungeonTactics::Pi;
constexpr std::array<float, 9> FoulSporesRetreatAngles = {
    0.0f, Pi / 8.0f, -Pi / 8.0f, Pi / 4.0f, -Pi / 4.0f, 3.0f * Pi / 8.0f, -3.0f * Pi / 8.0f, Pi / 2.0f, -Pi / 2.0f};

// Distance from a point to the segment a-b, in 2d.
inline float PointSegmentDistance(float px, float py, float ax, float ay, float bx, float by)
{
    float const dx = bx - ax;
    float const dy = by - ay;
    float const lengthSquared = dx * dx + dy * dy;
    float t = (lengthSquared < 1e-6f) ? 0.0f : ((px - ax) * dx + (py - ay) * dy) / lengthSquared;
    t = std::max(0.0f, std::min(1.0f, t));
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

// A retreat may not end near any mushroom, nor cross a dangerous one.
constexpr bool RetreatCrossesMushroom(bool dangerous, float destinationDistance, float pathDistance, float dangerRange)
{
    return destinationDistance < dangerRange || (dangerous && pathDistance < dangerRange);
}

// While he channels Foul Spores only the escapes and attacks may move a bot, or it walks back into the drain.
constexpr bool FoulSporesAllows(bool foulSpores, bool movement, bool escape)
{
    return !(foulSpores && movement && !escape);
}

struct MushroomAction
{
    bool aoe = false;
    bool heal = false;
    bool attackAnything = false;
    bool grindTargetIsMushroom = false;
};

// Mushrooms are not worth hitting: no dps AoE while any is up, and nobody grinds one down.
constexpr bool MushroomsAllow(bool mushroomsUp, bool dps, MushroomAction action)
{
    if (!action.aoe && !action.attackAnything)
        return true;
    if (action.aoe && (action.heal || !dps))
        return true;
    if (!mushroomsUp)
        return true;
    if (!action.aoe)
        return !action.grindTargetIsMushroom;
    return false;
}

// Underbat

// Tentacle Lash hits a unit within 5 yards that is not in front of the bat
constexpr float UnderbatLashRange = 5.0f;
constexpr float UnderbatLashMargin = 1.5f;
constexpr float UnderbatSafeArc = 2.6f;
constexpr uint32_t UnderbatRepositionCooldown = 750;
constexpr float UnderbatRallyTolerance = 3.0f;
constexpr float UnderbatRallyMaxRange = 20.0f;

constexpr float UnderbatLashWindow() { return UnderbatLashRange + UnderbatLashMargin; }

constexpr bool WatchesUnderbats(bool tank, bool batInLashRange) { return !tank && batInLashRange; }

// Getting behind a bat is exactly where the lash lands.
constexpr bool UnderbatAllowsGetBehind(bool batInLashRange) { return !batInLashRange; }

// Melee rally on the tank, so every bat faces them; a throttled melee waits instead of stepping away.
constexpr bool WaitsToRally(bool melee, bool hasRally, bool parked, bool throttled)
{
    return melee && hasRally && !parked && throttled;
}

constexpr bool TriesRally(bool melee, bool hasRally, bool parked, bool rallySpotClear)
{
    return melee && hasRally && !parked && rallySpotClear;
}

struct UnderbatStep
{
    bool melee = false;
    bool lashing = false;  // a bat in range has this bot outside its front arc
    bool casting = false;
    bool throttled = false;
    float gap = 0.0f;
};

// Ranged only break a cast for a bat that can actually lash them.
constexpr bool StepsAwayFromUnderbat(UnderbatStep step)
{
    if (step.melee && !step.lashing)
        return false;
    if (!step.melee && step.casting && !step.lashing)
        return false;
    return !step.throttled && step.gap < UnderbatLashWindow();
}

constexpr float UnderbatStepDistance(float gap) { return UnderbatLashWindow() - gap + 1.0f; }
}  // namespace Underbog

#endif
