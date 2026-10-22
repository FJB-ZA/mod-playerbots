/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Kael'thas Sunstrider bot tactics: leave Flame Strike and phoenix burns, kill eggs, kick Fireball, fly the lapse.

#include <algorithm>
#include <cmath>
#include <limits>

#include "AttackersValue.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "Group.h"
#include "MgTActions.h"
#include "MgTMultipliers.h"
#include "MgTShared.h"
#include "MgTTriggers.h"
#include "MovementActions.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "Timer.h"

using namespace EncounterHelpers;

namespace MagistersTerrace
{
namespace
{
bool LapseActive(Player* bot, Unit* kael)
{
    if (!kael || kael->HasAura(SPELL_POWER_FEEDBACK))
        return false;

    if (bot->HasAura(SPELL_GRAVITY_LAPSE_DOT) || bot->HasAura(SPELL_GRAVITY_LAPSE_FLY))
        return true;

    for (CurrentSpellTypes slot : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
    {
        Spell* spell = kael->GetCurrentSpell(slot);
        if (spell && spell->m_spellInfo && spell->m_spellInfo->Id == SPELL_GRAVITY_LAPSE_INITIAL)
            return true;
    }

    return false;
}

float BurningPhoenixKaelGap(Player* bot, Unit* kael)
{
    std::vector<Unit*> phoenixes;
    CollectCreaturesByEntry(bot, {NPC_PHOENIX}, PhoenixScan, phoenixes);

    float gap = std::numeric_limits<float>::max();
    for (Unit* phoenix : phoenixes)
    {
        if (phoenix->HasAura(SPELL_PHOENIX_BURN))
            gap = std::min(gap, phoenix->GetExactDist2d(kael));
    }

    return gap;
}

LapseRole RoleOf(Player* member)
{
    return LapseRoleOf(PlayerbotAI::IsTank(member), PlayerbotAI::IsHeal(member), PlayerbotAI::IsMelee(member));
}

// How many living bots of the same role sort before this one.
int LapseSlot(Player* bot, LapseRole role)
{
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    int slot = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;

        if (!GET_PLAYERBOT_AI(member) || RoleOf(member) != role)
            continue;

        if (member->GetGUID() < bot->GetGUID())
            ++slot;
    }

    return slot;
}

Sphere SphereOf(Player* bot, Unit* unit)
{
    Sphere sphere{unit->GetPositionX(), unit->GetPositionY(), false, true, 0.0f, 0.0f};
    if (Unit* victim = unit->GetVictim())
    {
        sphere.chasesBot = victim == bot;
        float const dx = victim->GetPositionX() - sphere.x;
        float const dy = victim->GetPositionY() - sphere.y;
        float const len = std::hypot(dx, dy);
        if (len < 0.1f)
        {
            sphere.heading = false;
            return sphere;
        }

        sphere.headingX = dx / len;
        sphere.headingY = dy / len;
        return sphere;
    }

    sphere.headingX = std::cos(unit->GetOrientation());
    sphere.headingY = std::sin(unit->GetOrientation());
    return sphere;
}

void CollectLapseWorld(Player* bot, LapseWorld& out)
{
    std::vector<Unit*> units;
    CollectCreaturesByEntry(bot, {NPC_ARCANE_SPHERE, NPC_PHOENIX}, std::max(SphereScan, PhoenixScan), units);

    for (Unit* unit : units)
    {
        if (unit->GetEntry() == NPC_ARCANE_SPHERE && bot->GetExactDist2d(unit) <= SphereScan)
            out.spheres.push_back(SphereOf(bot, unit));
        else if (unit->GetEntry() == NPC_PHOENIX && unit->HasAura(SPELL_PHOENIX_BURN) &&
                 bot->GetExactDist2d(unit) <= PhoenixScan)
            out.burns.push_back({unit->GetPositionX(), unit->GetPositionY()});
    }
}

// Melee count as placed once they can hit Kael; everyone else once on their spot.
bool OnLapseStation(Player* bot, LapseRole role, Unit* kael, LapseSpot const& spot)
{
    if (!IsLapseMeleeSlot(role) || !kael)
        return bot->GetExactDist2d(spot.x, spot.y) <= LapseTolerance;

    return bot->IsWithinCombatRange(kael, sPlayerbotAIConfig.MeleeDistance + CONTACT_DISTANCE);
}
}  // namespace

Unit* GetKaelthas(Player* bot)
{
    return GET_PLAYERBOT_AI(bot)->GetAiObjectContext()->GetValue<Unit*>("find target", "kael'thas sunstrider")->Get();
}

Unit* GetKaelInterruptTarget(Player* bot)
{
    Unit* kael = GetKaelthas(bot);
    if (!kael || !KaelInterruptible(IsKaelShielded(kael)))
        return nullptr;

    return BestCastPriority(kael, &KaelInterruptPriority) ? kael : nullptr;
}

bool IsKaelUnattackable(Player* bot)
{
    Unit* kael = GetKaelthas(bot);
    return kael && kael->HasUnitFlag(UNIT_FLAG_IMMUNE_TO_PC);
}

void CollectFlameStrikeEscapes(Player* bot, EscapeSpots& out)
{
    if (!GetKaelthas(bot))
        return;

    std::vector<Hazard> hazards;

    std::vector<Unit*> triggers;
    CollectCreaturesByEntry(bot, {NPC_FLAMESTRIKE_TRIGGER}, FlamestrikeScan, triggers);
    for (Unit* trigger : triggers)
        hazards.push_back({trigger->GetPositionX(), trigger->GetPositionY(), FlamestrikeStand, FlamestrikeClear});

    for (uint32 spellId : {SPELL_FLAME_STRIKE, SPELL_FLAME_STRIKE_H})
    {
        for (Position const& pos : GetDynamicObjectPositions(bot, FlamestrikeScan, spellId))
            hazards.push_back({pos.GetPositionX(), pos.GetPositionY(), FlamestrikeStand, FlamestrikeClear});
    }

    CollectHazardEscapes(bot, hazards, 0.0f, out);
}

void CollectPhoenixEscapes(Player* bot, EscapeSpots& out)
{
    Unit* kael = GetKaelthas(bot);
    if (!kael || LapseActive(bot, kael))
        return;

    bool const melee = PlayerbotAI::IsMelee(bot);
    float const stand = melee ? BurnMeleeStand : BurnStand;
    float const clear = melee ? BurnMeleeClear : BurnClear;

    std::vector<Unit*> phoenixes;
    CollectCreaturesByEntry(bot, {NPC_PHOENIX}, PhoenixScan, phoenixes);

    std::vector<Hazard> hazards;
    for (Unit* phoenix : phoenixes)
    {
        if (phoenix->HasAura(SPELL_PHOENIX_BURN))
            hazards.push_back({phoenix->GetPositionX(), phoenix->GetPositionY(), stand, clear});
    }

    if (melee)
        CollectRingEscapes(bot, kael, hazards, out);
    else
        CollectHazardEscapes(bot, hazards, 0.0f, out);
}

PhoenixRing GetPhoenixRing(Player* bot)
{
    if (!PlayerbotAI::IsMelee(bot))
        return PhoenixRing::None;

    Unit* kael = GetKaelthas(bot);
    if (!kael || LapseActive(bot, kael))
        return PhoenixRing::None;

    return PhoenixRingFor(BurningPhoenixKaelGap(bot, kael), bot->GetExactDist2d(kael));
}

Unit* GetKaelFocusTarget(Player* bot)
{
    Unit* kael = GetKaelthas(bot);
    if (!kael)
        return nullptr;

    if (Unit* shielded = GetShockBarrierKael(bot, kael))
        return shielded;

    std::vector<Unit*> kin;
    CollectCreaturesByEntry(bot, {NPC_PHOENIX_EGG, NPC_PHOENIX}, PhoenixScan, kin);

    Unit* best = nullptr;
    float bestDist = PhoenixScan;
    for (Unit* egg : kin)
    {
        if (egg->GetEntry() != NPC_PHOENIX_EGG || !AttackersValue::IsValidTarget(egg, bot))
            continue;

        float const dist = bot->GetExactDist2d(egg);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = egg;
        }
    }

    if (best || !HuntsPhoenix(PlayerbotAI::IsMelee(bot)))
        return best;

    bestDist = PhoenixScan;
    for (Unit* phoenix : kin)
    {
        if (phoenix->GetEntry() != NPC_PHOENIX || !phoenix->IsInCombat() ||
            !AttackersValue::IsValidTarget(phoenix, bot))
            continue;

        float const dist = bot->GetExactDist2d(phoenix);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = phoenix;
        }
    }

    return best;
}

bool IsGravityLapseActive(Player* bot) { return LapseActive(bot, GetKaelthas(bot)); }

void CollectMeleePhoenixExclusions(Player* bot, std::vector<ObjectGuid>& out)
{
    if (!PlayerbotAI::IsMelee(bot) || !GetKaelthas(bot))
        return;

    std::vector<Unit*> phoenixes;
    CollectCreaturesByEntry(bot, {NPC_PHOENIX}, PhoenixScan, phoenixes);

    for (Unit* phoenix : phoenixes)
        out.push_back(phoenix->GetGUID());
}
}  // namespace MagistersTerrace

bool MgTFlameStrikeTrigger::IsActive()
{
    return !AI_VALUE_REF(MagistersTerrace::EscapeSpots, "mgt flame strike escape").empty();
}

bool MgTPhoenixBurnTrigger::IsActive()
{
    return !AI_VALUE_REF(MagistersTerrace::EscapeSpots, "mgt phoenix escape").empty();
}

bool MgTKaelFocusTargetTrigger::IsActive()
{
    if (!MagistersTerrace::TakesFocusOrder(bot->IsInCombat(), PlayerbotAI::IsDps(bot), PlayerbotAI::IsTank(bot)))
        return false;

    return !AI_VALUE(ObjectGuid, "mgt kael focus target").IsEmpty();
}

bool MgTKaelInterruptTrigger::IsActive() { return !AI_VALUE(ObjectGuid, "mgt kael interrupt target").IsEmpty(); }

bool MgTGravityLapseTrigger::IsActive() { return AI_VALUE(bool, "mgt gravity lapse"); }

bool MgTKaelInterruptAction::Execute(Event)
{
    Unit* target = MagistersTerrace::GetKaelInterruptTarget(bot);
    if (!target)
        return false;

    return !MagistersTerrace::CastInterrupt(botAI, target, MagistersTerrace::ControlInterruptUrgency).empty();
}

bool MgTTakeLapseSpotAction::Execute(Event)
{
    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_COMBAT))
        return false;

    MagistersTerrace::LapseWorld world;
    MagistersTerrace::CollectLapseWorld(bot, world);

    MagistersTerrace::LapseSpot const here = {bot->GetPositionX(), bot->GetPositionY()};
    MagistersTerrace::LapseSpot spot = here;
    Unit* kael = MagistersTerrace::GetKaelthas(bot);

    MagistersTerrace::LapseRole const role = MagistersTerrace::RoleOf(bot);
    bool const healer = role == MagistersTerrace::LapseRole::Healer;
    MagistersTerrace::LapseSpot const anchor =
        MagistersTerrace::LapseAnchor(role, MagistersTerrace::LapseSlot(bot, role));

    bool move = false;
    if (MagistersTerrace::LapseKite(here, healer, world, _kite, getMSTime(), spot) ||
        MagistersTerrace::LapseDodge(here, anchor, healer, world, spot))
    {
        move = true;
    }
    else
    {
        MagistersTerrace::LapseSpot const station = MagistersTerrace::LapseStation(anchor, healer, world);
        if (!MagistersTerrace::OnLapseStation(bot, role, kael, station))
            move = MagistersTerrace::LapseApproach(here, world, station, MagistersTerrace::LapseThreatened(here, world),
                                                   spot);
    }

    if (!move)
        return false;

    float z = bot->GetPositionZ();
    if (kael && MagistersTerrace::IsLapseMeleeSlot(role))
        z = kael->GetPositionZ();

    return MoveTo(bot->GetMapId(), spot.x, spot.y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT,
                  true, false);
}

float MgTFlameStrikeMultiplier::GetValue(Action* action)
{
    bool const competing = dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
                           dynamic_cast<AvoidAoeAction*>(action);
    if (!competing)
        return 1.0f;

    bool const escaping = !AI_VALUE_REF(MagistersTerrace::EscapeSpots, "mgt flame strike escape").empty();
    return MagistersTerrace::FlameStrikeAllows(true, escaping) ? 1.0f : 0.0f;
}

float MgTPhoenixBurnMultiplier::GetValue(Action* action)
{
    MagistersTerrace::PhoenixBurnAction shape;
    shape.competingMove = dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
                          dynamic_cast<AvoidAoeAction*>(action) || dynamic_cast<ReachTargetAction*>(action) ||
                          dynamic_cast<CastReachTargetSpellAction*>(action);
    if (!shape.competingMove)
        return 1.0f;

    shape.setBehind = dynamic_cast<SetBehindTargetAction*>(action) != nullptr;

    MagistersTerrace::PhoenixBurnMoment fight;
    fight.ring = AI_VALUE(MagistersTerrace::PhoenixRing, "mgt phoenix ring");
    fight.escaping = !AI_VALUE_REF(MagistersTerrace::EscapeSpots, "mgt phoenix escape").empty();

    return MagistersTerrace::PhoenixBurnAllows(fight, shape) ? 1.0f : 0.0f;
}

float MgTKaelUnattackableMultiplier::GetValue(Action* action)
{
    if (!MagistersTerrace::ActionPullsOffFocus(bot, action) || PlayerbotAI::IsHeal(bot))
        return 1.0f;

    return MagistersTerrace::KaelUnattackableAllows(true, false, AI_VALUE(bool, "mgt kael unattackable")) ? 1.0f : 0.0f;
}

float MgTGravityLapseMultiplier::GetValue(Action* action)
{
    bool const competing = dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
                           dynamic_cast<ReachTargetAction*>(action) ||
                           dynamic_cast<CastReachTargetSpellAction*>(action);
    if (!competing)
        return 1.0f;

    return MagistersTerrace::GravityLapseAllows(true, AI_VALUE(bool, "mgt gravity lapse")) ? 1.0f : 0.0f;
}
