// Pure rules for Magisters' Terrace: Selin's crystals, trash interrupts and kill order, Delrissa's party, Kael'thas.

#ifndef _PLAYERBOTS_MGT_POLICY_H
#define _PLAYERBOTS_MGT_POLICY_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string_view>
#include <vector>

#include "DungeonTacticsPolicy.h"

namespace MagistersTerrace
{
enum MgTNpc : uint32_t
{
    NPC_SELIN_FIREHEART = 24723,
    NPC_FEL_CRYSTAL = 24722,

    NPC_SUNBLADE_MAGE_GUARD = 24683,
    NPC_SUNBLADE_BLOOD_KNIGHT = 24684,
    NPC_SUNBLADE_MAGISTER = 24685,
    NPC_SUNBLADE_WARLOCK = 24686,
    NPC_SUNBLADE_PHYSICIAN = 24687,
    NPC_SUNBLADE_IMP = 24815,
    NPC_WRETCHED_SKULKER = 24688,
    NPC_WRETCHED_BRUISER = 24689,
    NPC_WRETCHED_HUSK = 24690,
    NPC_COILSKAR_WITCH = 24696,
    NPC_SISTER_OF_TORMENT = 24697,
    NPC_ETHEREUM_SMUGGLER = 24698,

    NPC_DELRISSA = 24560,
    NPC_APOKO = 24553,
    NPC_ELLRYS_DUSKHALLOW = 24558,
    NPC_YAZZAI = 24561,
    NPC_ZELFAN = 24556,
    NPC_GARAXXAS = 24555,
    NPC_KAGANI_NIGHTSTRIKE = 24557,
    NPC_WARLORD_SALARIS = 24559,
    NPC_ERAMAS_BRIGHTBLAZE = 24554,
    NPC_HIGH_EXPLOSIVE_SHEEP = 24715,
    NPC_FIZZLE = 24656,
    NPC_SLIVER = 24552,

    NPC_KAEL_THAS = 24664,
    NPC_FLAMESTRIKE_TRIGGER = 24666,
    NPC_PHOENIX = 24674,
    NPC_PHOENIX_EGG = 24675,
    NPC_ARCANE_SPHERE = 24708,
};

enum MgTSpell : uint32_t
{
    SPELL_MAGIC_DAMPENING_FIELD = 44475,
    SPELL_HOLY_LIGHT = 44479,
    SPELL_HOLY_LIGHT_H = 46029,
    SPELL_ARCANE_NOVA = 44644,
    SPELL_ARCANE_NOVA_H = 46036,
    SPELL_INCINERATE = 44519,
    SPELL_INCINERATE_H = 46043,
    SPELL_FROSTBOLT = 44606,
    SPELL_FROSTBOLT_H = 46035,
    SPELL_WRETCHED_FIREBALL = 44503,
    SPELL_WRETCHED_FROSTBOLT = 44504,
    SPELL_DEADLY_EMBRACE = 44547,
    SPELL_FORKED_LIGHTNING = 20299,
    SPELL_FORKED_LIGHTNING_H = 46150,
    SPELL_DRINK_FEL_INFUSION = 44505,

    SPELL_FLASH_HEAL = 17843,
    SPELL_LESSER_HEALING_WAVE = 44256,
    SPELL_LESSER_HEALING_WAVE_H = 46181,
    SPELL_SUMMON_IMP = 44163,
    SPELL_DELRISSA_FEAR = 38595,
    SPELL_POLYMORPH = 13323,
    SPELL_TREMOR_TOTEM = 8143,

    SPELL_FLAME_STRIKE = 44190,
    SPELL_FLAME_STRIKE_H = 46163,
    SPELL_PHOENIX_BURN = 44197,
    SPELL_SHOCK_BARRIER = 46165,
    SPELL_PYROBLAST = 36819,
    SPELL_KAEL_FIREBALL = 44189,
    SPELL_KAEL_FIREBALL_H = 46164,
    SPELL_GRAVITY_LAPSE_INITIAL = 44224,
    SPELL_GRAVITY_LAPSE_DOT = 44226,
    SPELL_GRAVITY_LAPSE_FLY = 44227,
    SPELL_POWER_FEEDBACK = 44233,
};

// Shared

struct EscapeSpot
{
    float x;
    float y;
    float z;
};

using EscapeSpots = std::vector<EscapeSpot>;

struct Hazard
{
    float x;
    float y;
    float stand;  // inside this the bot must leave
    float clear;  // a spot this far out is safe
};

struct ScoredSpot
{
    float score;
    float x;
    float y;
};

constexpr float GroundTierStep = 3.0f;
constexpr float GroundSearchUp = 5.0f;
constexpr float GroundSearchDown = 12.0f;
constexpr float EscapeLosHeight = 2.0f;
constexpr float PartyCentroidRange = 60.0f;

constexpr std::array<float, 4> EscapeRadii = {7.0f, 10.0f, 13.0f, 16.0f};
constexpr int EscapeBearings = 12;
constexpr float EscapeTravelWeight = 0.25f;
constexpr int EscapeKeep = 5;

// A spot more than one step up or down is another floor of the room.
inline bool OnSameTier(float z, float botZ) { return std::fabs(z - botZ) <= GroundTierStep; }

inline bool SpotClearsHazards(std::vector<Hazard> const& hazards, float x, float y)
{
    for (Hazard const& hazard : hazards)
    {
        float const dx = x - hazard.x;
        float const dy = y - hazard.y;
        if (dx * dx + dy * dy < hazard.clear * hazard.clear)
            return false;
    }
    return true;
}

inline bool InsideAnyHazard(std::vector<Hazard> const& hazards, float x, float y)
{
    for (Hazard const& hazard : hazards)
    {
        float const dx = x - hazard.x;
        float const dy = y - hazard.y;
        if (dx * dx + dy * dy < hazard.stand * hazard.stand)
            return true;
    }
    return false;
}

// Spots round the bot clear of every hazard, best first: short runs that end anchorGap from the party.
inline std::vector<ScoredSpot> RankHazardEscapes(float botX, float botY, std::vector<Hazard> const& hazards,
                                                 bool haveParty, float partyX, float partyY, float anchorGap)
{
    std::vector<ScoredSpot> scored;
    if (!InsideAnyHazard(hazards, botX, botY))
        return scored;

    for (float radius : EscapeRadii)
    {
        for (int i = 0; i < EscapeBearings; ++i)
        {
            float const angle =
                (2.0f * DungeonTactics::Pi * static_cast<float>(i)) / static_cast<float>(EscapeBearings);
            float const x = botX + std::cos(angle) * radius;
            float const y = botY + std::sin(angle) * radius;
            if (!SpotClearsHazards(hazards, x, y))
                continue;

            float score = radius * EscapeTravelWeight;
            if (haveParty)
                score += std::fabs(std::hypot(x - partyX, y - partyY) - anchorGap);

            scored.push_back({score, x, y});
        }
    }

    std::stable_sort(scored.begin(), scored.end(),
                     [](ScoredSpot const& a, ScoredSpot const& b) { return a.score < b.score; });
    return scored;
}

enum class InterruptKind
{
    Dedicated,
    Control
};

struct InterruptSpell
{
    std::string_view name;
    InterruptKind kind;
};

constexpr std::array<InterruptSpell, 18> InterruptSpells = {{
    {"kick", InterruptKind::Dedicated},
    {"pummel", InterruptKind::Dedicated},
    {"shield bash", InterruptKind::Dedicated},
    {"wind shear", InterruptKind::Dedicated},
    {"mind freeze", InterruptKind::Dedicated},
    {"counterspell", InterruptKind::Dedicated},
    {"spell lock", InterruptKind::Dedicated},
    {"silencing shot", InterruptKind::Dedicated},
    {"shockwave", InterruptKind::Control},
    {"shadowfury", InterruptKind::Control},
    {"concussion blow", InterruptKind::Control},
    {"hammer of justice", InterruptKind::Control},
    {"maim", InterruptKind::Control},
    {"kidney shot", InterruptKind::Control},
    {"silence", InterruptKind::Control},
    {"bash", InterruptKind::Control},
    {"strangulate", InterruptKind::Control},
    {"arcane torrent", InterruptKind::Control},
}};

// Urgency 1 is the most urgent; stuns and silences are spent only on urgency 1 and 2.
constexpr uint8_t ControlInterruptUrgency = 2;

constexpr bool AllowsControlInterrupt(uint8_t urgency) { return urgency && urgency <= ControlInterruptUrgency; }

constexpr bool UsesInterrupt(InterruptKind kind, uint8_t urgency)
{
    return kind == InterruptKind::Dedicated || AllowsControlInterrupt(urgency);
}

// Lower rank wins, distance breaks ties; rank 0 means not a candidate.
constexpr bool OutranksBest(uint8_t rank, float dist, bool haveBest, uint8_t bestRank, float bestDist)
{
    return !haveBest || rank < bestRank || (rank == bestRank && dist < bestDist);
}

// A latched focus is kept until something strictly more urgent shows up, so dps don't flip between equals.
constexpr bool KeepsLatchedFocus(bool held, uint8_t heldRank, uint8_t bestRank) { return held && heldRank <= bestRank; }

enum class FocusOrder
{
    None,
    Kael,
    Delrissa,
    Trash
};

constexpr FocusOrder FocusOrderOwner(bool kaelOrder, bool delrissaOrder, bool trashOrder)
{
    if (kaelOrder)
        return FocusOrder::Kael;
    if (delrissaOrder)
        return FocusOrder::Delrissa;
    if (trashOrder)
        return FocusOrder::Trash;
    return FocusOrder::None;
}

// Sheep explode and eggs hatch or break within seconds: never worth a DoT.
constexpr bool IsTransientFocus(uint32_t entry)
{
    return entry == NPC_HIGH_EXPLOSIVE_SHEEP || entry == NPC_PHOENIX_EGG;
}

constexpr bool TakesFocusOrder(bool inCombat, bool dps, bool tank) { return inCombat && dps && !tank; }

// Assist, attacker DoTs and dps area spells all spread damage away from the one target that matters.
constexpr bool PullsOffFocus(bool assistOrAttackerDebuff, bool aoe, bool dps, bool healSpell)
{
    return assistOrAttackerDebuff || (aoe && dps && !healSpell);
}

struct FocusOrderMoment
{
    bool inCombat = false;
    bool healer = false;
    bool tank = false;
    bool dps = false;
    bool crystalUp = false;
    bool haveOrder = false;
    bool onLiveOrder = false;  // current target is the focus, alive and attackable
};

struct FocusOrderAction
{
    bool picker = false;  // dps assist, dps aoe or tank assist
    bool drop = false;    // drop target
};

constexpr bool FollowsFocusOrder(bool inCombat, bool healer, bool tank, bool dps)
{
    return inCombat && !healer && !tank && dps;
}

constexpr bool FocusOrderAllows(FocusOrderMoment fight, FocusOrderAction action)
{
    if (!action.picker && !action.drop)
        return true;
    if (!FollowsFocusOrder(fight.inCombat, fight.healer, fight.tank, fight.dps))
        return true;
    // The crystal multiplier owns target picks while a crystal is up
    if (fight.crystalUp || !fight.haveOrder)
        return true;
    if (action.picker)
        return false;
    return !fight.onLiveOrder;
}

constexpr bool IsDpsAreaSpell(bool aoe, bool healSpell, bool healer, bool dps)
{
    return aoe && !healSpell && !healer && dps;
}

// Bosses' adds die one by one: dps area spells would only feed the healers.
constexpr bool FocusBurstAllows(bool dpsAreaSpell, FocusOrder owner)
{
    return !(dpsAreaSpell && (owner == FocusOrder::Kael || owner == FocusOrder::Delrissa));
}

// Selin Fireheart

// He only attacks units past x 216; the room's walkable floor starts a little further in
constexpr float SelinLeashX = 216.0f;
constexpr float SelinSafeX = SelinLeashX + 4.0f;
constexpr float SelinRegroupX = 232.0f;
constexpr float SelinFleeSlack = 6.0f;

constexpr float RoomXMin = 218.0f;
constexpr float RoomXMax = 260.0f;
constexpr float RoomYMin = -40.0f;
constexpr float RoomYMax = 40.0f;

constexpr float CrystalScanRange = 60.0f;

constexpr float ReturnArrived = 1.0f;
constexpr float ReturnStepMax = 8.0f;

constexpr bool OutsideSelinRoom(float x) { return x < SelinSafeX; }

constexpr void ClampIntoRoom(float& x, float& y)
{
    x = std::clamp(x, RoomXMin, RoomXMax);
    y = std::clamp(y, RoomYMin, RoomYMax);
}

constexpr float ReturnStep(float distance) { return std::min(ReturnStepMax, distance); }

// Everyone but healers drops Selin for the crystal he is draining.
constexpr bool CrystalFocusAllows(bool pullsOffFocus, bool healer, bool crystalUp)
{
    return !(pullsOffFocus && !healer && crystalUp);
}

// Fleeing past the leash line takes the bot out of his reach and resets the fight.
constexpr bool RoomLeashAllows(bool flight, float x, bool selinUp)
{
    return !(flight && x <= SelinSafeX + SelinFleeSlack && selinUp);
}

// Sunblade and Wretched trash

// Magic Dampening Field (44475) is a 5-yard pool
constexpr float DampeningRadius = 5.0f;
constexpr float DampeningStand = DampeningRadius + 1.5f;
constexpr float DampeningClear = DampeningRadius + 4.0f;
constexpr float DampeningScan = 30.0f;

constexpr float TankRejoinGap = 5.0f;

// Mage guards throw glaives at anyone past melee range
constexpr float GlaiveMinRange = 12.0f;
constexpr float GlaiveMaxRange = 60.0f;

// Arcane Nova (44644/46036) knocks back everyone round the magister
constexpr float NovaClear = 16.0f;
constexpr float NovaClearSlack = 2.0f;

constexpr float InterruptScan = 30.0f;
constexpr float WretchedScan = 40.0f;
constexpr float FocusScan = 40.0f;

constexpr bool ThrowsGlaiveAt(bool targetsBot, float distance) { return targetsBot && distance > GlaiveMinRange; }

constexpr bool DodgesArcaneNova(bool melee) { return !melee; }

// A Wretched that drank Fel Infusion hits hard: the tank takes it back unless it is already on him.
constexpr bool TauntsWretched(bool infused, bool hasVictim, bool victimIsBot)
{
    return infused && hasVictim && !victimIsBot;
}

constexpr std::array<std::string_view, 4> TauntSpells = {"taunt", "growl", "dark command", "hand of reckoning"};

constexpr uint8_t InterruptPriority(uint32_t spellId)
{
    switch (spellId)
    {
        case SPELL_DEADLY_EMBRACE:
            return 1;
        case SPELL_HOLY_LIGHT:
        case SPELL_HOLY_LIGHT_H:
            return 2;
        case SPELL_ARCANE_NOVA:
        case SPELL_ARCANE_NOVA_H:
            return 3;
        case SPELL_FORKED_LIGHTNING:
        case SPELL_FORKED_LIGHTNING_H:
            return 4;
        case SPELL_INCINERATE:
        case SPELL_INCINERATE_H:
            return 5;
        case SPELL_FROSTBOLT:
        case SPELL_FROSTBOLT_H:
            return 6;
        case SPELL_WRETCHED_FIREBALL:
        case SPELL_WRETCHED_FROSTBOLT:
            return 7;
        default:
            return 0;
    }
}

// Healers first, then the hardest hitters; 0 = not a focus.
constexpr uint8_t TrashFocusRank(uint32_t entry)
{
    switch (entry)
    {
        case NPC_SUNBLADE_PHYSICIAN:
            return 1;
        case NPC_SUNBLADE_BLOOD_KNIGHT:
            return 2;
        case NPC_SISTER_OF_TORMENT:
            return 3;
        case NPC_SUNBLADE_WARLOCK:
            return 4;
        case NPC_SUNBLADE_MAGISTER:
            return 5;
        case NPC_ETHEREUM_SMUGGLER:
            return 6;
        case NPC_COILSKAR_WITCH:
            return 7;
        case NPC_SUNBLADE_IMP:
            return 8;
        case NPC_SUNBLADE_MAGE_GUARD:
            return 9;
        default:
            return 0;
    }
}

// Standing in the field drains mana: only the escape itself may move the bot.
constexpr bool DampeningFieldAllows(bool competingMove, bool escaping) { return !(competingMove && escaping); }

// Priestess Delrissa and her party

// The sheep runs at someone and explodes, then the healers; Delrissa herself last.
constexpr uint8_t RetinueFocusRank(uint32_t entry)
{
    switch (entry)
    {
        case NPC_HIGH_EXPLOSIVE_SHEEP:
            return 1;
        case NPC_APOKO:
            return 2;
        case NPC_ELLRYS_DUSKHALLOW:
            return 3;
        case NPC_YAZZAI:
            return 4;
        case NPC_ZELFAN:
            return 5;
        case NPC_WARLORD_SALARIS:
            return 6;
        case NPC_GARAXXAS:
            return 7;
        case NPC_KAGANI_NIGHTSTRIKE:
            return 8;
        case NPC_ERAMAS_BRIGHTBLAZE:
            return 9;
        case NPC_DELRISSA:
            return 10;
        default:
            return 0;
    }
}

constexpr uint8_t RetinueInterruptPriority(uint32_t spellId)
{
    switch (spellId)
    {
        case SPELL_DELRISSA_FEAR:
        case SPELL_POLYMORPH:
            return 1;
        case SPELL_FLASH_HEAL:
        case SPELL_LESSER_HEALING_WAVE:
        case SPELL_LESSER_HEALING_WAVE_H:
            return 2;
        case SPELL_SUMMON_IMP:
            return 3;
        default:
            return 0;
    }
}

// Each bot takes the cast at its own slot so two bots don't spend interrupts on one caster.
constexpr std::size_t RetinueInterruptPick(std::size_t slot, std::size_t casters) { return slot < casters ? slot : 0; }

// Ellrys fears and Yazzai polymorphs: a shaman keeps Tremor up instead of his other earth totems.
constexpr bool DelrissaTremorTotemAllows(bool competingEarthTotem, bool holdTremor)
{
    return !(competingEarthTotem && holdTremor);
}

// Kael'thas Sunstrider

// Flame Strike's pillar burns 10 yards out
constexpr float FlamestrikeStand = 7.5f;
constexpr float FlamestrikeClear = 10.0f;
constexpr float FlamestrikeScan = 40.0f;

// Phoenix Burn ticks on everyone within 8 yards
constexpr float BurnDamage = 8.0f;
constexpr float BurnStand = 9.0f;
constexpr float BurnClear = 12.0f;
constexpr float PhoenixScan = 40.0f;
constexpr float BurnMeleeStand = BurnDamage + 0.5f;
constexpr float BurnMeleeClear = BurnDamage + 2.5f;
constexpr std::array<float, 5> BurnMeleeRadii = {3.0f, 5.0f, 7.0f, 9.0f, 12.0f};
constexpr int BurnMeleeBearings = 16;

constexpr float KaelMeleeRing = 5.0f;

// Melee escape spots stay as close to Kael as the burn allows, so they keep hitting him.
inline std::vector<ScoredSpot> RankRingEscapes(float botX, float botY, std::vector<Hazard> const& hazards,
                                               float anchorX, float anchorY)
{
    std::vector<ScoredSpot> scored;
    if (!InsideAnyHazard(hazards, botX, botY))
        return scored;

    for (float radius : BurnMeleeRadii)
    {
        for (int i = 0; i < BurnMeleeBearings; ++i)
        {
            float const angle =
                (2.0f * DungeonTactics::Pi * static_cast<float>(i)) / static_cast<float>(BurnMeleeBearings);
            float const x = botX + std::cos(angle) * radius;
            float const y = botY + std::sin(angle) * radius;
            if (!SpotClearsHazards(hazards, x, y))
                continue;

            float const score = std::hypot(x - anchorX, y - anchorY) + radius * EscapeTravelWeight;
            scored.push_back({score, x, y});
        }
    }

    std::stable_sort(scored.begin(), scored.end(),
                     [](ScoredSpot const& a, ScoredSpot const& b) { return a.score < b.score; });
    return scored;
}

enum class PhoenixRing : uint8_t
{
    None,
    Threatens,  // a burning phoenix reaches part of Kael's melee ring
    Covers      // it reaches all of it and the bot is in it
};

// gap: from Kael to the nearest burning phoenix.
constexpr PhoenixRing PhoenixRingFor(float gap, float distanceToKael)
{
    if (gap >= KaelMeleeRing + BurnMeleeClear)
        return PhoenixRing::None;
    if (gap < BurnMeleeClear - KaelMeleeRing && distanceToKael <= KaelMeleeRing + BurnMeleeClear)
        return PhoenixRing::Covers;
    return PhoenixRing::Threatens;
}

// Pyroblast (heroic only) before Fireball; 0 = not worth an interrupt.
constexpr uint8_t KaelInterruptPriority(uint32_t spellId)
{
    switch (spellId)
    {
        case SPELL_PYROBLAST:
            return 1;
        case SPELL_KAEL_FIREBALL:
        case SPELL_KAEL_FIREBALL_H:
            return 2;
        default:
            return 0;
    }
}

// Melee lose too much time running after phoenixes; they stay on Kael.
constexpr bool HuntsPhoenix(bool melee) { return !melee; }

constexpr bool FlameStrikeAllows(bool competingMove, bool escaping) { return !(competingMove && escaping); }

struct PhoenixBurnMoment
{
    PhoenixRing ring = PhoenixRing::None;
    bool escaping = false;
};

struct PhoenixBurnAction
{
    bool competingMove = false;
    bool setBehind = false;
};

constexpr bool PhoenixBurnAllows(PhoenixBurnMoment fight, PhoenixBurnAction action)
{
    if (!action.competingMove)
        return true;
    // Circling Kael would walk the bot through the burn
    if (fight.ring != PhoenixRing::None && action.setBehind)
        return false;
    return !fight.escaping && fight.ring != PhoenixRing::Covers;
}

// During his intro he is immune: nothing but heals should be spent on him.
constexpr bool KaelUnattackableAllows(bool pullsOffFocus, bool healer, bool unattackable)
{
    return !(pullsOffFocus && !healer && unattackable);
}

// Gravity Lapse: everyone flies round the centre while three Arcane Spheres chase random players.

struct LapseSpot
{
    float x;
    float y;
};

struct Sphere
{
    float x;
    float y;
    bool chasesBot;
    bool heading;  // false when it sits on its victim: no direction to project
    float headingX;
    float headingY;
};

struct LapseWorld
{
    std::vector<Sphere> spheres;
    std::vector<LapseSpot> burns;
};

struct KiteState
{
    float turn = 0.0f;
    uint32_t lastMs = 0;
};

enum class LapseRole
{
    Tank,
    Healer,
    Melee,
    Ranged
};

constexpr LapseSpot KaelLapseCentre = {148.54f, 181.13f};
constexpr LapseSpot LapseTank = {148.54f, 177.53f};
constexpr std::array<LapseSpot, 2> LapseMelee = {{{151.30f, 178.82f}, {145.78f, 178.82f}}};
constexpr LapseSpot LapseHealer = {148.54f, 167.13f};
constexpr std::array<LapseSpot, 3> LapseRanged = {{{137.07f, 164.75f}, {148.54f, 155.13f}, {164.92f, 169.66f}}};

constexpr float SphereDamage = 5.0f;
constexpr float SphereClear = 8.0f;
constexpr float SphereScan = 40.0f;
constexpr float LapsePushMax = 8.0f;

// Where a sphere will be over the next few seconds: speed times lead
constexpr float SphereSpeed = 3.0f;
constexpr float SphereLead = 2.5f;

constexpr float SphereAlert = 9.0f;
constexpr float SphereTransit = 6.0f;
constexpr float SphereRouteSlack = 0.5f;

constexpr float SphereDodgeNearRadius = 4.0f;
constexpr std::array<float, 4> SphereDodgeRadii = {SphereDodgeNearRadius, 7.0f, 11.0f, 15.0f};
constexpr int SphereDodgeBearings = 16;
constexpr float SphereDodgeTravelCost = 0.25f;
constexpr float SphereDodgeMarginCap = 6.0f;
constexpr float SphereDodgeMarginWeight = 0.5f;
constexpr float SphereDodgeMinGain = 2.0f;

constexpr float KiteLeg = 12.0f;
constexpr std::array<float, 8> KiteTurnMagnitudes = {0.0f, 25.0f, 50.0f, 75.0f, 100.0f, 125.0f, 150.0f, 175.0f};
constexpr float KiteProbeStep = 2.0f;
constexpr float KiteProbeMax = 24.0f;
constexpr float KitePassClear = SphereDamage;
constexpr float KiteRunwayTiebreak = 0.5f;
constexpr float KiteCommitBonus = 3.0f;
constexpr float KiteCommitStraight = 10.0f;
constexpr uint32_t KiteCommitBreakMs = 1500;

// Bearings from the lapse centre, 0 = north: the north side is the entry stairs, the south bowl is open floor
constexpr float KiteNorthMin = 340.0f;
constexpr float KiteNorthMax = 20.0f;
constexpr float KiteBowlMin = 100.0f;
constexpr float KiteBowlMax = 260.0f;
constexpr float KiteBowlRadius = 22.0f;
constexpr float KiteSideRadius = 13.0f;

constexpr float LapseTolerance = 3.0f;
constexpr float LapseBurnCost = 6.0f;
constexpr float LapseLeg = 10.0f;
constexpr float LapseProgressMin = 1.0f;
constexpr std::array<float, 6> LapseDetourTurns = {0.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f};

constexpr LapseRole LapseRoleOf(bool tank, bool healer, bool melee)
{
    if (tank)
        return LapseRole::Tank;
    if (healer)
        return LapseRole::Healer;
    if (melee)
        return LapseRole::Melee;
    return LapseRole::Ranged;
}

constexpr bool IsLapseMeleeSlot(LapseRole role) { return role == LapseRole::Tank || role == LapseRole::Melee; }

// slot: how many living bots of the same role sort before this one.
constexpr LapseSpot LapseAnchor(LapseRole role, int slot)
{
    std::size_t const index = static_cast<std::size_t>(slot);
    switch (role)
    {
        case LapseRole::Tank:
            return LapseTank;
        case LapseRole::Healer:
            return LapseHealer;
        case LapseRole::Melee:
            return LapseMelee[index % LapseMelee.size()];
        default:
            return LapseRanged[index % LapseRanged.size()];
    }
}

inline float BearingFromKael(float x, float y)
{
    float degrees = std::atan2(x - KaelLapseCentre.x, y - KaelLapseCentre.y) * 180.0f / DungeonTactics::Pi;
    if (degrees < 0.0f)
        degrees += 360.0f;
    return degrees;
}

inline float PointToSegment(float px, float py, float ax, float ay, float bx, float by)
{
    float const dx = bx - ax;
    float const dy = by - ay;
    float const len2 = dx * dx + dy * dy;
    if (len2 < 0.0001f)
        return std::hypot(px - ax, py - ay);

    float t = ((px - ax) * dx + (py - ay) * dy) / len2;
    t = std::clamp(t, 0.0f, 1.0f);
    return std::hypot(px - (ax + dx * t), py - (ay + dy * t));
}

inline float SegmentToSegment(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy)
{
    auto side = [](float ox, float oy, float px, float py, float qx, float qy)
    { return (px - ox) * (qy - oy) - (py - oy) * (qx - ox); };

    float const d1 = side(cx, cy, dx, dy, ax, ay);
    float const d2 = side(cx, cy, dx, dy, bx, by);
    float const d3 = side(ax, ay, bx, by, cx, cy);
    float const d4 = side(ax, ay, bx, by, dx, dy);

    if (((d1 > 0.0f) != (d2 > 0.0f)) && ((d3 > 0.0f) != (d4 > 0.0f)))
        return 0.0f;

    return std::min({PointToSegment(ax, ay, cx, cy, dx, dy), PointToSegment(bx, by, cx, cy, dx, dy),
                     PointToSegment(cx, cy, ax, ay, bx, by), PointToSegment(dx, dy, ax, ay, bx, by)});
}

// Closest a sphere comes to a point over its next few seconds of travel.
inline float DistanceToSphereSweep(Sphere const& sphere, float x, float y)
{
    if (!sphere.heading)
        return std::hypot(x - sphere.x, y - sphere.y);

    float const reach = SphereSpeed * SphereLead;
    float t = (x - sphere.x) * sphere.headingX + (y - sphere.y) * sphere.headingY;
    t = std::clamp(t, 0.0f, reach);
    return std::hypot(x - (sphere.x + sphere.headingX * t), y - (sphere.y + sphere.headingY * t));
}

// Closest a walk from one point to another passes a sphere's sweep.
inline float WalkClearanceOfSphere(Sphere const& sphere, float fromX, float fromY, float toX, float toY)
{
    if (!sphere.heading)
        return PointToSegment(sphere.x, sphere.y, fromX, fromY, toX, toY);

    float const reach = SphereSpeed * SphereLead;
    return SegmentToSegment(fromX, fromY, toX, toY, sphere.x, sphere.y, sphere.x + sphere.headingX * reach,
                            sphere.y + sphere.headingY * reach);
}

inline float NearestSweep(LapseWorld const& world, float x, float y)
{
    float nearest = std::numeric_limits<float>::max();
    for (Sphere const& sphere : world.spheres)
        nearest = std::min(nearest, DistanceToSphereSweep(sphere, x, y));
    return nearest;
}

inline float NearestWalkClearance(LapseWorld const& world, float fromX, float fromY, float toX, float toY)
{
    float nearest = std::numeric_limits<float>::max();
    for (Sphere const& sphere : world.spheres)
        nearest = std::min(nearest, WalkClearanceOfSphere(sphere, fromX, fromY, toX, toY));
    return nearest;
}

// Healers keep to the south bowl so they stay in range of everyone.
inline bool InsideKiteArena(float x, float y, bool healer)
{
    float const bearing = BearingFromKael(x, y);
    if (bearing >= KiteNorthMin || bearing <= KiteNorthMax)
        return false;

    bool const bowl = bearing >= KiteBowlMin && bearing <= KiteBowlMax;
    if (healer && !bowl)
        return false;

    float const limit = bowl ? KiteBowlRadius : KiteSideRadius;
    return std::hypot(x - KaelLapseCentre.x, y - KaelLapseCentre.y) <= limit;
}

// How far the bot can run along a heading and stay in the arena; from outside, counted from where it enters.
inline float KiteRunway(float fromX, float fromY, float headingX, float headingY, bool healer, bool outside)
{
    int const steps = static_cast<int>(KiteProbeMax / KiteProbeStep);

    bool started = !outside;
    float runway = 0.0f;
    for (int i = 1; i <= steps; ++i)
    {
        float const step = KiteProbeStep * static_cast<float>(i);
        bool const clear = InsideKiteArena(fromX + headingX * step, fromY + headingY * step, healer);

        if (!started)
        {
            if (clear)
            {
                started = true;
                runway = step;
            }
            continue;
        }

        if (!clear)
            break;
        runway = step;
    }

    return started ? runway : 0.0f;
}

inline bool InsideAnyBurn(std::vector<LapseSpot> const& burns, float x, float y)
{
    for (LapseSpot const& burn : burns)
    {
        if (std::hypot(x - burn.x, y - burn.y) < BurnDamage)
            return true;
    }
    return false;
}

inline Sphere const* ChasingSphere(LapseWorld const& world)
{
    for (Sphere const& sphere : world.spheres)
    {
        if (sphere.chasesBot)
            return &sphere;
    }
    return nullptr;
}

// The bot a sphere chases runs legs away from it, turning as little as the arena allows and keeping its turn side.
inline bool LapseKite(LapseSpot here, bool healer, LapseWorld const& world, KiteState& state, uint32_t nowMs,
                      LapseSpot& out)
{
    Sphere const* chaser = ChasingSphere(world);
    if (!chaser)
    {
        state = KiteState{};
        return false;
    }

    if (state.lastMs && nowMs - state.lastMs > KiteCommitBreakMs)
        state = KiteState{};

    float awayX = here.x - chaser->x;
    float awayY = here.y - chaser->y;
    float away = std::hypot(awayX, awayY);
    if (away < 0.1f)
    {
        awayX = here.x - KaelLapseCentre.x;
        awayY = here.y - KaelLapseCentre.y;
        away = std::hypot(awayX, awayY);
        if (away < 0.1f)
            return false;
    }
    awayX /= away;
    awayY /= away;

    bool const outside = !InsideKiteArena(here.x, here.y, healer);
    float const passBar = std::min(KitePassClear, NearestSweep(world, here.x, here.y) - SphereRouteSlack);

    bool haveFallback = false;
    float fallbackClear = -1.0f;
    float fallbackTurn = 0.0f;
    LapseSpot fallback = here;

    for (int pass = 0; pass < 2; ++pass)
    {
        bool const requireFullLeg = pass == 0;

        for (float magnitude : KiteTurnMagnitudes)
        {
            bool found = false;
            float bestPreference = 0.0f;
            float bestTurn = 0.0f;
            LapseSpot best = here;

            for (int side = 0; side < 2; ++side)
            {
                float const turn = side ? -magnitude : magnitude;
                if (side && magnitude <= 0.0f)
                    continue;

                float const angle = turn * DungeonTactics::Pi / 180.0f;
                float const cosT = std::cos(angle);
                float const sinT = std::sin(angle);
                float const headingX = awayX * cosT - awayY * sinT;
                float const headingY = awayX * sinT + awayY * cosT;
                float const x = here.x + headingX * KiteLeg;
                float const y = here.y + headingY * KiteLeg;

                float const runway = KiteRunway(here.x, here.y, headingX, headingY, healer, outside);
                if (requireFullLeg ? runway < KiteLeg : !InsideKiteArena(x, y, healer))
                    continue;

                float const clearance = NearestWalkClearance(world, here.x, here.y, x, y);
                if (clearance < passBar)
                {
                    if (clearance > fallbackClear)
                    {
                        haveFallback = true;
                        fallbackClear = clearance;
                        fallbackTurn = turn;
                        fallback = {x, y};
                    }
                    continue;
                }

                float preference = clearance + runway * KiteRunwayTiebreak;
                if (std::fabs(turn) > KiteCommitStraight && std::fabs(state.turn) > KiteCommitStraight &&
                    (turn > 0.0f) == (state.turn > 0.0f))
                    preference += KiteCommitBonus;
                if (InsideAnyBurn(world.burns, x, y))
                    preference -= LapseBurnCost;

                if (!found || preference > bestPreference)
                {
                    found = true;
                    bestPreference = preference;
                    bestTurn = turn;
                    best = {x, y};
                }
            }

            if (found)
            {
                out = best;
                state.turn = bestTurn;
                state.lastMs = nowMs;
                return true;
            }
        }
    }

    if (!haveFallback)
        return false;

    out = fallback;
    state.turn = fallbackTurn;
    state.lastMs = nowMs;
    return true;
}

// A sphere passing close: step to the nearest spot clear of every sweep, close to the bot's lapse station.
inline bool LapseDodge(LapseSpot here, LapseSpot anchor, bool healer, LapseWorld const& world, LapseSpot& out)
{
    if (world.spheres.empty())
        return false;

    float const nearest = NearestSweep(world, here.x, here.y);
    if (nearest >= SphereAlert)
        return false;

    float const routeBar = std::min(SphereTransit, nearest - SphereRouteSlack);

    bool found = false;
    float best = 0.0f;
    bool haveFallback = false;
    float fallbackClearance = 0.0f;
    LapseSpot fallback = here;

    for (float radius : SphereDodgeRadii)
    {
        for (int i = 0; i < SphereDodgeBearings; ++i)
        {
            float const angle =
                (2.0f * DungeonTactics::Pi * static_cast<float>(i)) / static_cast<float>(SphereDodgeBearings);
            float const x = here.x + std::cos(angle) * radius;
            float const y = here.y + std::sin(angle) * radius;
            if (!InsideKiteArena(x, y, healer))
                continue;

            if (NearestWalkClearance(world, here.x, here.y, x, y) < routeBar)
                continue;

            float const clearance = NearestSweep(world, x, y);
            if (clearance < SphereClear)
            {
                if (clearance > nearest + SphereDodgeMinGain && clearance > fallbackClearance)
                {
                    haveFallback = true;
                    fallbackClearance = clearance;
                    fallback = {x, y};
                }
                continue;
            }

            float const margin = std::min(clearance - SphereClear, SphereDodgeMarginCap);
            float const score = std::hypot(x - anchor.x, y - anchor.y) + radius * SphereDodgeTravelCost -
                                margin * SphereDodgeMarginWeight +
                                (InsideAnyBurn(world.burns, x, y) ? LapseBurnCost : 0.0f);
            if (!found || score < best)
            {
                best = score;
                out = {x, y};
                found = true;
            }
        }
    }

    if (found)
        return true;
    if (!haveFallback)
        return false;

    out = fallback;
    return true;
}

// The bot's lapse station, pushed off any sphere sitting on it but never far or out of the arena.
inline LapseSpot LapseStation(LapseSpot anchor, bool healer, LapseWorld const& world)
{
    LapseSpot out = anchor;
    for (Sphere const& sphere : world.spheres)
    {
        float dx = out.x - sphere.x;
        float dy = out.y - sphere.y;
        float dist = std::hypot(dx, dy);
        if (dist >= SphereClear)
            continue;

        if (dist < 0.1f)
        {
            dx = out.x - KaelLapseCentre.x;
            dy = out.y - KaelLapseCentre.y;
            dist = std::hypot(dx, dy);
            if (dist < 0.1f)
                break;
        }

        out.x = sphere.x + (dx / dist) * SphereClear;
        out.y = sphere.y + (dy / dist) * SphereClear;
    }

    float const pushX = out.x - anchor.x;
    float const pushY = out.y - anchor.y;
    float const push = std::hypot(pushX, pushY);
    if (push > LapsePushMax)
    {
        out.x = anchor.x + (pushX / push) * LapsePushMax;
        out.y = anchor.y + (pushY / push) * LapsePushMax;
    }

    if (InsideKiteArena(anchor.x, anchor.y, healer) && !InsideKiteArena(out.x, out.y, healer))
        out = anchor;

    return out;
}

inline bool LapseThreatened(LapseSpot here, LapseWorld const& world)
{
    return NearestSweep(world, here.x, here.y) < SphereAlert;
}

// One leg towards the station, bending round sphere sweeps; a blocked leg is taken only when allowBlocked.
inline bool LapseApproach(LapseSpot here, LapseWorld const& world, LapseSpot station, bool allowBlocked, LapseSpot& out)
{
    float const gap = std::hypot(station.x - here.x, station.y - here.y);
    if (gap < 0.1f)
        return false;

    float const leg = std::min(LapseLeg, gap);
    float const progressBar = std::min(LapseProgressMin, leg * 0.5f);
    float const dirX = (station.x - here.x) / gap;
    float const dirY = (station.y - here.y) / gap;
    float const routeBar = std::min(SphereTransit, NearestSweep(world, here.x, here.y) - SphereRouteSlack);

    bool haveFallback = false;
    float fallbackRoute = -1.0f;
    LapseSpot fallback = here;

    for (float magnitude : LapseDetourTurns)
    {
        bool found = false;
        float bestRoute = 0.0f;
        LapseSpot best = here;

        for (int side = 0; side < 2; ++side)
        {
            if (side && magnitude <= 0.0f)
                continue;

            float const angle = (side ? -magnitude : magnitude) * DungeonTactics::Pi / 180.0f;
            float const cosT = std::cos(angle);
            float const sinT = std::sin(angle);
            float const x = here.x + (dirX * cosT - dirY * sinT) * leg;
            float const y = here.y + (dirX * sinT + dirY * cosT) * leg;

            if (gap - std::hypot(station.x - x, station.y - y) < progressBar)
                continue;

            float const route = NearestWalkClearance(world, here.x, here.y, x, y);
            if (route < routeBar)
            {
                if (route > fallbackRoute)
                {
                    haveFallback = true;
                    fallbackRoute = route;
                    fallback = {x, y};
                }
                continue;
            }

            if (!found || route > bestRoute)
            {
                found = true;
                bestRoute = route;
                best = {x, y};
            }
        }

        if (found)
        {
            out = best;
            return true;
        }
    }

    if (!allowBlocked || !haveFallback)
        return false;

    out = fallback;
    return true;
}

// Nothing may pull a bot back to its usual spot while it is placing itself for the lapse.
constexpr bool GravityLapseAllows(bool competingMove, bool lapse) { return !(competingMove && lapse); }

// Kael'thas Sunstrider (heroic)

// Shock Barrier blocks interrupts on the Pyroblast behind it: dps break the barrier so it can be kicked.
constexpr bool BreaksShockBarrier(bool heroic, bool barrierUp, bool attackable)
{
    return heroic && barrierUp && attackable;
}

constexpr bool KaelInterruptible(bool barrierUp) { return !barrierUp; }
}  // namespace MagistersTerrace

#endif
