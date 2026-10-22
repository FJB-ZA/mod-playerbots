/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Pure rules for the Hellfire Ramparts bosses: Gargolmar's watchers, Omor's treachery, Vazruden and Nazan.

#ifndef _PLAYERBOTS_RAMP_POLICY_H
#define _PLAYERBOTS_RAMP_POLICY_H

namespace HellfireRamparts
{
// Omor the Unscarred

// Treacherous Aura and Bane of Treachery hit allies within 15 yards
constexpr float TreacherySafeDistance = 20.0f;

// The main tank keeps Omor with the aura; anyone else runs from the group.
constexpr bool FleesWithTreachery(bool omorUp, bool hasAura, bool mainTank) { return omorUp && hasAura && !mainTank; }

constexpr bool FleesFromTank(bool omorUp, bool tankHasAura, bool isTank) { return omorUp && tankHasAura && !isTank; }

constexpr bool NeedsToLeaveTank(float distanceToTank) { return distanceToTank < TreacherySafeDistance; }

// Ranged stay out of each other's aura range all fight, since it lands on a random player.
constexpr bool SpreadsFromOthers(bool omorUp, bool ranged, bool playerTooClose)
{
    return omorUp && ranged && playerTooClose;
}

struct OmorMoment
{
    bool mainTank = false;
    bool ranged = false;
    bool hasAura = false;        // this bot carries the aura
    bool tankHasAura = false;    // the group's main tank carries it
    bool victimHasAura = false;  // whoever Omor is hitting carries it
};

struct OmorAction
{
    bool treacheryMove = false;  // the spread or flee moves
    bool mover = false;          // a movement action or a cast that walks into range
};

// Melee may not walk in while they, the tank or Omor's target carry the aura; tanks and ranged move freely.
constexpr bool OmorAllows(OmorMoment fight, OmorAction action)
{
    if (action.treacheryMove || !action.mover)
        return true;
    if (fight.mainTank || fight.ranged)
        return true;
    return !fight.hasAura && !fight.tankHasAura && !fight.victimHasAura;
}

// Vazruden the Herald and Nazan

// Middle of the platform, where Nazan lands
constexpr float VazrudenTankX = -1407.405f;
constexpr float VazrudenTankY = 1744.521f;
constexpr float VazrudenTankZ = 81.075f;
constexpr float VazrudenArrivalDistance = 10.0f;
// Below this the tank stops dragging Vazruden and just holds him
constexpr float VazrudenDragMinHealthPct = 25.0f;

constexpr bool NazanLanded(bool nazanUp, bool flying) { return nazanUp && !flying; }

// The tank drags Vazruden to the middle until Nazan lands there.
constexpr bool PositionsVazruden(bool mainTank, bool vazrudenUp, bool nazanLanded)
{
    return mainTank && vazrudenUp && !nazanLanded;
}

constexpr bool DragsVazruden(bool holdsBoss, bool inMelee, float healthPct, float distanceToMiddle)
{
    return holdsBoss && inMelee && healthPct > VazrudenDragMinHealthPct && distanceToMiddle > VazrudenArrivalDistance;
}

// Once Nazan lands her Cone of Fire and fireballs make fire resistance the totem or aura to hold.
constexpr bool SetsFireResistance(bool rightClass, bool nazanLanded, bool alreadySet)
{
    return rightClass && nazanLanded && !alreadySet;
}

// The other buffs of the same totem element or aura slot would replace the fire resistance one.
constexpr bool NazanBuffAllows(bool inCombat, bool rightClass, bool replacesFireResistance, bool nazanLanded)
{
    return !(inCombat && rightClass && replacesFireResistance && nazanLanded);
}

// Nazan (heroic)

// Bellowing Roar (39427) fears the group on heroic only: a shaman holds Tremor Totem while she is down.
constexpr bool SetsTremorTotem(bool heroic, bool shaman, bool nazanLanded, bool hasTremor)
{
    return heroic && shaman && nazanLanded && !hasTremor;
}

// The other earth totems would replace Tremor Totem; on normal there is no roar and no tremor to protect.
constexpr bool NazanTremorAllows(bool heroic, bool inCombat, bool shaman, bool replacesTremor, bool nazanLanded)
{
    return !(heroic && inCombat && shaman && replacesTremor && nazanLanded);
}
}  // namespace HellfireRamparts

#endif
