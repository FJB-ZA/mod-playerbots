// Pure rules for the Drak'Tharon Keep bosses: Trollgore's corpses, Novos's field and adds, Tharon'ja's skeleton phase.

#ifndef _PLAYERBOTS_DTK_POLICY_H
#define _PLAYERBOTS_DTK_POLICY_H

#include <cstddef>
#include <vector>

namespace DrakTharon
{
// Trollgore

// Corpse Explode (49555) hits 5 yards round the infested corpse
constexpr float CorpseExplodeRadius = 5.0f;

constexpr bool InCorpseBlast(float distanceToCorpse) { return distanceToCorpse < CorpseExplodeRadius + 1.0f; }

// Steps a little past the trigger ring so the bot doesn't stop on its edge; 0 = clear already.
constexpr float CorpseStepAway(float distanceToCorpse)
{
    float clear = CorpseExplodeRadius + 2.0f;
    return distanceToCorpse < clear ? clear - distanceToCorpse : 0.0f;
}

// Novos the Summoner

// Arcane Field reaches 11 yards round him
constexpr float ArcaneFieldClearance = 12.0f;
// Bots drift back to the middle of the room once they are this far out and have nothing to kill
constexpr float NovosTether = 15.0f;
constexpr float NovosClusterDistance = 4.0f;
// The stairs dps only swaps to a nearer add of the same kind when it is this much closer
constexpr float StairsSwapMargin = 5.0f;

constexpr float ArcaneFieldStepAway(float distanceToBoss)
{
    return distanceToBoss < ArcaneFieldClearance ? ArcaneFieldClearance - distanceToBoss : 0.0f;
}

// Assist actions would pull a bot off the add it is killing while he channels the field.
constexpr bool NovosAllows(bool arcaneField, bool hasTarget, bool assist)
{
    return !(arcaneField && hasTarget && assist);
}

enum class NovosRole
{
    Tank,
    StairsDps,
    Dps,
    Other
};

enum class NovosAdd
{
    HulkingCorpse,
    CrystalHandler,
    RisenShadowcaster,
    FetidTrollCorpse,
    Other
};

// 0 = first choice, 1 = second, -1 = not this role's job.
constexpr int NovosPriority(NovosRole role, NovosAdd add)
{
    switch (role)
    {
        case NovosRole::Tank:
            return add == NovosAdd::HulkingCorpse ? 0 : add == NovosAdd::CrystalHandler ? 1 : -1;
        case NovosRole::StairsDps:
            return add == NovosAdd::RisenShadowcaster ? 0 : add == NovosAdd::FetidTrollCorpse ? 1 : -1;
        case NovosRole::Dps:
            return add == NovosAdd::CrystalHandler ? 0 : add == NovosAdd::HulkingCorpse ? 1 : -1;
        default:
            return -1;
    }
}

struct PartySlot
{
    bool dps = false;
    bool melee = false;
};

// The stairs adds go to the first melee dps, or the first ranged dps when there is no melee; -1 = no dps at all.
inline int StairsDpsIndex(std::vector<PartySlot> const& party)
{
    int ranged = -1;
    for (std::size_t i = 0; i < party.size(); ++i)
    {
        if (!party[i].dps)
            continue;
        if (party[i].melee)
            return static_cast<int>(i);
        if (ranged < 0)
            ranged = static_cast<int>(i);
    }
    return ranged;
}

// The Prophet Tharon'ja

enum class SkeletonSpell
{
    None,
    SlayingStrike,
    Taunt,
    BoneArmor,
    TouchOfLife
};

// Below this health a non-tank drains life back before striking again
constexpr float TouchOfLifeHealthPct = 70.0f;

struct SkeletonState
{
    bool tank = false;
    bool bossOnMe = false;
    bool boneArmorUp = false;
    float healthPct = 100.0f;
    bool slayingStrikeReady = false;
    bool tauntReady = false;
    bool boneArmorReady = false;
    bool touchOfLifeReady = false;
};

// The tank keeps Bone Armor up and taunts on cooldown; everyone else uses Slaying Strike and drains life when hurt.
constexpr SkeletonSpell NextSkeletonSpell(SkeletonState s)
{
    if ((s.tank || s.bossOnMe) && !s.boneArmorUp && s.boneArmorReady)
        return SkeletonSpell::BoneArmor;
    if (s.tank && s.tauntReady)
        return SkeletonSpell::Taunt;
    if ((s.tank || s.healthPct < TouchOfLifeHealthPct) && s.touchOfLifeReady)
        return SkeletonSpell::TouchOfLife;
    if (s.slayingStrikeReady)
        return SkeletonSpell::SlayingStrike;
    if (s.touchOfLifeReady)
        return SkeletonSpell::TouchOfLife;
    return SkeletonSpell::None;
}

// In skeleton form the class spells are gone: only the skeleton action casts, and nobody flees melee.
constexpr bool TharonjaAllows(bool skeleton, bool cast, bool flee) { return !skeleton || (!cast && !flee); }
}  // namespace DrakTharon

#endif
