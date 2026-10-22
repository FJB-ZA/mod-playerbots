// Pure rules for Sethekk Halls: tremor totems, Syth's elementals, Anzu's spirits and Spell Bomb, Ikiss's pillar.

#ifndef _PLAYERBOTS_SETH_POLICY_H
#define _PLAYERBOTS_SETH_POLICY_H

#include <algorithm>
#include <array>
#include <cmath>

#include "DungeonTacticsPolicy.h"

namespace SethekkHalls
{
// Sethekk Prophet

constexpr bool DropsTremorTotem(bool shaman, bool prophetUp, bool hasTremor)
{
    return shaman && prophetUp && !hasTremor;
}

// Any other earth totem would replace the tremor totem while the prophet can still fear.
constexpr bool ProphetAllows(bool prophetUp, bool otherEarthTotem) { return !(prophetUp && otherEarthTotem); }

// Darkweaver Syth

// Below the burn threshold his last elementals are ignored and the boss is finished.
constexpr bool MarksElementals(bool tracker, bool burning) { return tracker && !burning; }

// Anzu

enum class AnzuMark
{
    Skull,
    Moon
};

// While banished he takes no damage: the moon moves the skull off him so the broods die first.
constexpr AnzuMark AnzuMarkFor(bool banished) { return banished ? AnzuMark::Moon : AnzuMark::Skull; }

enum class BirdSpirit
{
    Falcon,  // haste
    Hawk,    // damage reduction
    Eagle    // damage to all enemies
};

constexpr std::array<BirdSpirit, 3> SpiritHotOrder = {BirdSpirit::Falcon, BirdSpirit::Hawk, BirdSpirit::Eagle};

constexpr bool HotsSpirits(bool druid, bool healer) { return druid && healer; }

// A healer keeps healing through Spell Bomb only once the main tank drops to this
constexpr float SpellBombTankHealthPct = 50.0f;

struct SpellBombCaster
{
    bool manaUser = false;
    bool tank = false;
    bool dps = false;
    bool bombed = false;
    bool hasMainTank = false;
    float mainTankHealthPct = 100.0f;
};

// Spell Bomb hurts its target for every spell it casts, so mana users hold their casts.
constexpr bool SpellBombAllows(SpellBombCaster caster, bool spellCast)
{
    if (!spellCast || !caster.manaUser || caster.tank || !caster.bombed)
        return true;
    if (caster.dps)
        return false;
    return !caster.hasMainTank || caster.mainTankHealthPct <= SpellBombTankHealthPct;
}

// Talon King Ikiss

// Lust waits until the tank has him at the pillar and the fight has properly started.
constexpr bool IkissAllowsBloodlust(bool lust, bool engaged) { return !lust || engaged; }

// The tank runs back to the pillar on every new pull.
constexpr bool ResetsPillarRun(bool engaged) { return !engaged; }

constexpr float PillarArrivalDistance = 2.0f;

constexpr bool StaysNearVictim(bool ranged, bool bubble, bool inLos) { return ranged && !bubble && inLos; }

constexpr bool ReturnsToLos(bool bubble, bool inLos) { return !bubble && !inLos; }

// Ranged stay this close to his victim so wherever he blinks, the pillar is a short run.
constexpr float VictimStayDistance = 10.0f;
constexpr float VictimStayTolerance = 5.0f;

constexpr bool ClosesOnVictim(float distanceToVictim)
{
    return distanceToVictim > VictimStayDistance + VictimStayTolerance;
}

// Ring round the pillar where it blocks line of sight to Arcane Explosion
constexpr float PillarMinRadius = 10.0f;
constexpr float PillarMaxRadius = 13.0f;

constexpr bool OnPillarRing(float distanceToPillar)
{
    return distanceToPillar >= PillarMinRadius && distanceToPillar <= PillarMaxRadius;
}

constexpr float PillarRingRadius(float distanceToPillar) { return distanceToPillar < PillarMinRadius ? 11.0f : 12.0f; }

constexpr float PillarAngularStep = DungeonTactics::Pi / 8.0f;
constexpr float PillarAngularDeadzone = 0.105f;  // ~6 degrees

// Signed turn from one angle round the pillar to another, in (-pi, pi].
inline float PillarTurn(float fromAngle, float toAngle)
{
    float delta = DungeonTactics::NormalizeAngle(toAngle - fromAngle);
    if (delta > DungeonTactics::Pi)
        delta -= 2.0f * DungeonTactics::Pi;
    return delta;
}

inline bool PillarTurnDone(float turn) { return std::fabs(turn) < PillarAngularDeadzone; }

// One step along the ring towards the wanted angle, never past it.
inline float PillarStepAngle(float fromAngle, float turn)
{
    float const direction = turn > 0.0f ? 1.0f : -1.0f;
    return fromAngle + direction * std::min(PillarAngularStep, std::fabs(turn));
}

// Hide on the far side of the pillar from him.
inline float HidingAngle(float pillarToIkiss) { return pillarToIkiss + DungeonTactics::Pi; }

struct IkissMove
{
    bool movement = false;       // movement actions and reach-target spells
    bool alwaysBlocked = false;  // formation moves, flee, blink back, disengage
    bool exempt = false;         // tank facing, getting behind, hiding behind the pillar
};

// Ikiss blinks to a random player: free movement scatters the group away from the pillar.
constexpr bool IkissAllowsMove(bool bubble, IkissMove action)
{
    if (action.exempt)
        return true;
    if (action.alwaysBlocked)
        return false;
    return !(action.movement && bubble);
}
}  // namespace SethekkHalls

#endif
