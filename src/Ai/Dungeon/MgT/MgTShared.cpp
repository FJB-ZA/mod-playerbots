/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MgTShared.h"

#include <algorithm>
#include <iterator>
#include <list>
#include <string>

#include "AiObjectContext.h"
#include "AttackersValue.h"
#include "ChooseTargetActions.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "GenericSpellActions.h"
#include "Group.h"
#include "MgTActions.h"
#include "MgTMultipliers.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

using namespace EncounterHelpers;

namespace MagistersTerrace
{
namespace
{
constexpr char const* FOCUS_ORDER_KEYS[] = {"mgt kael focus target", "mgt delrissa focus target", "mgt focus target"};

bool PartyCentroid(Player* bot, float& cx, float& cy)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    float sx = 0.0f, sy = 0.0f;
    int n = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
            continue;
        if (bot->GetExactDist2d(member) > PartyCentroidRange)
            continue;

        sx += member->GetPositionX();
        sy += member->GetPositionY();
        ++n;
    }

    if (n == 0)
        return false;

    cx = sx / n;
    cy = sy / n;
    return true;
}

void KeepBestReachable(Player* bot, std::vector<ScoredSpot> const& scored, EscapeSpots& out)
{
    float const bz = bot->GetPositionZ();

    int kept = 0;
    for (ScoredSpot const& spot : scored)
    {
        float const z = bot->GetMapHeight(spot.x, spot.y, bz + GroundSearchUp, true, GroundSearchDown);
        if (!OnSameTier(z, bz))
            continue;

        if (!bot->IsWithinLOS(spot.x, spot.y, z + EscapeLosHeight))
            continue;

        out.push_back({spot.x, spot.y, z});

        if (++kept >= EscapeKeep)
            break;
    }
}

bool StopsACast(uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return false;

    if ((info->InterruptFlags & SPELL_INTERRUPT_FLAG_INTERRUPT) &&
        info->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
        return true;

    for (uint8 i = EFFECT_0; i <= EFFECT_2; ++i)
    {
        if (info->Effects[i].Effect == SPELL_EFFECT_INTERRUPT_CAST)
            return true;

        if (info->Effects[i].Effect != SPELL_EFFECT_APPLY_AURA)
            continue;

        if (info->Effects[i].ApplyAuraName == SPELL_AURA_MOD_SILENCE ||
            info->Effects[i].ApplyAuraName == SPELL_AURA_MOD_STUN)
            return true;
    }

    return false;
}
}  // namespace

void CollectCreaturesByEntry(Player* bot, std::vector<uint32> const& entries, float radius, std::vector<Unit*>& out)
{
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, entries, radius);

    out.reserve(out.size() + found.size());
    for (Creature* creature : found)
    {
        if (creature->IsAlive())
            out.push_back(creature);
    }
}

void CollectAttackersByEntry(Player* bot, std::vector<uint32> const& entries, float radius, std::vector<Unit*>& out)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

    auto const& attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const guid : attackers)
    {
        Unit* attacker = botAI->GetUnit(guid);
        if (!attacker)
            continue;

        if (std::find(entries.begin(), entries.end(), attacker->GetEntry()) == entries.end())
            continue;

        if (bot->GetExactDist2d(attacker) > radius)
            continue;

        out.push_back(attacker);
    }
}

void CollectHazardEscapes(Player* bot, std::vector<Hazard> const& hazards, float anchorGap, EscapeSpots& out)
{
    if (!InsideAnyHazard(hazards, bot->GetPositionX(), bot->GetPositionY()))
        return;

    float cx = 0.0f, cy = 0.0f;
    bool const haveParty = PartyCentroid(bot, cx, cy);

    KeepBestReachable(
        bot, RankHazardEscapes(bot->GetPositionX(), bot->GetPositionY(), hazards, haveParty, cx, cy, anchorGap), out);
}

void CollectRingEscapes(Player* bot, Unit* anchor, std::vector<Hazard> const& hazards, EscapeSpots& out)
{
    KeepBestReachable(bot,
                      RankRingEscapes(bot->GetPositionX(), bot->GetPositionY(), hazards, anchor->GetPositionX(),
                                      anchor->GetPositionY()),
                      out);
}

uint8 BestCastPriority(Unit* caster, uint8 (*priority)(uint32))
{
    uint8 best = 0;
    for (CurrentSpellTypes slot : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
    {
        Spell* spell = caster->GetCurrentSpell(slot);
        if (!spell || !spell->m_spellInfo)
            continue;

        uint8 const prio = priority(spell->m_spellInfo->Id);
        if (prio && (!best || prio < best))
            best = prio;
    }

    return best;
}

std::string CastInterrupt(PlayerbotAI* botAI, Unit* target, uint8 urgency)
{
    if (!target->IsNonMeleeSpellCast(true))
        return {};

    for (InterruptSpell const& entry : InterruptSpells)
    {
        if (!UsesInterrupt(entry.kind, urgency))
            continue;

        std::string const name(entry.name);

        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", name)->Get();
        if (!spellId || !StopsACast(spellId))
            continue;

        if (!botAI->CanCastSpell(name, target))
            continue;

        if (botAI->CastSpell(name, target))
            return name;
    }

    return {};
}

Unit* PickRankedFocus(Player* bot, uint8 (*rank)(uint32), std::vector<Unit*> const& extra, ObjectGuid& latched)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

    Unit* held = nullptr;
    uint8 heldRank = 0;
    Unit* best = nullptr;
    uint8 bestRank = 0;
    float bestDist = 0.0f;

    auto consider = [&](Unit* candidate)
    {
        uint8 const candidateRank = rank(candidate->GetEntry());
        if (!candidateRank)
            return;

        float const dist = bot->GetExactDist2d(candidate);
        if (dist > FocusScan)
            return;

        if (!latched.IsEmpty() && candidate->GetGUID() == latched)
        {
            held = candidate;
            heldRank = candidateRank;
        }

        if (OutranksBest(candidateRank, dist, best != nullptr, bestRank, bestDist))
        {
            best = candidate;
            bestRank = candidateRank;
            bestDist = dist;
        }
    };

    auto const& attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const guid : attackers)
    {
        if (Unit* attacker = botAI->GetUnit(guid))
            consider(attacker);
    }

    for (Unit* candidate : extra)
        consider(candidate);

    if (KeepsLatchedFocus(held != nullptr, heldRank, bestRank))
        return held;

    latched = best ? best->GetGUID() : ObjectGuid::Empty;
    return best;
}

ObjectGuid ResolveFocusOrder(PlayerbotAI* botAI)
{
    AiObjectContext* context = botAI->GetAiObjectContext();

    for (char const* key : FOCUS_ORDER_KEYS)
    {
        ObjectGuid const order = context->GetValue<ObjectGuid>(key)->Get();
        if (!order.IsEmpty())
            return order;
    }

    return ObjectGuid::Empty;
}

FocusOrder ResolveFocusOrderOwner(PlayerbotAI* botAI)
{
    AiObjectContext* context = botAI->GetAiObjectContext();

    bool const kael = !context->GetValue<ObjectGuid>(FOCUS_ORDER_KEYS[0])->Get().IsEmpty();
    bool const delrissa = !kael && !context->GetValue<ObjectGuid>(FOCUS_ORDER_KEYS[1])->Get().IsEmpty();
    bool const trash = !kael && !delrissa && !context->GetValue<ObjectGuid>(FOCUS_ORDER_KEYS[2])->Get().IsEmpty();
    return FocusOrderOwner(kael, delrissa, trash);
}

void CollectFocusExclusions(Player* bot, std::vector<ObjectGuid>& out)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

    ObjectGuid const order = ResolveFocusOrder(botAI);
    if (order.IsEmpty())
        return;

    Unit* focus = botAI->GetUnit(order);
    bool const transient = !focus || IsTransientFocus(focus->GetEntry());

    auto const& attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    out.reserve(out.size() + attackers.size() + 1);
    for (ObjectGuid const guid : attackers)
    {
        if (guid != order)
            out.push_back(guid);
    }

    if (transient)
        out.push_back(order);
}

bool ActionPullsOffFocus(Player* bot, Action* action)
{
    bool const assistOrDebuff = dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
                                dynamic_cast<CastDebuffSpellOnAttackerAction*>(action);
    bool const aoe = !assistOrDebuff && action->getThreatType() == Action::ActionThreatType::Aoe;
    return PullsOffFocus(assistOrDebuff, aoe, aoe && PlayerbotAI::IsDps(bot),
                         aoe && dynamic_cast<CastHealingSpellAction*>(action) != nullptr);
}
}  // namespace MagistersTerrace

bool MgTEscapeAction::Execute(Event)
{
    if (IsWaitingForLastMove(MovementPriority::MOVEMENT_COMBAT))
        return false;

    auto const& spots = AI_VALUE_REF(MagistersTerrace::EscapeSpots, _value);
    for (MagistersTerrace::EscapeSpot const& spot : spots)
    {
        if (MoveTo(bot->GetMapId(), spot.x, spot.y, spot.z, false, false, false, false,
                   MovementPriority::MOVEMENT_COMBAT, true, false))
            return true;
    }

    return false;
}

bool MgTFocusAction::Execute(Event)
{
    Unit* target = botAI->GetUnit(AI_VALUE(ObjectGuid, _value));
    if (!target)
        return false;

    if (IsMechanicTrackerBot(bot, MagistersTerrace::MAP_MAGISTERS_TERRACE))
        MarkTargetWithSkull(bot, target);

    if (AI_VALUE(Unit*, "current target") == target)
        return false;

    return Attack(target);
}

float MgTFocusOrderMultiplier::GetValue(Action* action)
{
    MagistersTerrace::FocusOrderAction shape;
    shape.picker = dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<DpsAoeAction*>(action) ||
                   dynamic_cast<TankAssistAction*>(action);
    shape.drop = !shape.picker && dynamic_cast<DropTargetAction*>(action) != nullptr;
    if (!shape.picker && !shape.drop)
        return 1.0f;

    MagistersTerrace::FocusOrderMoment fight;
    fight.inCombat = bot->IsInCombat();
    fight.healer = PlayerbotAI::IsHeal(bot);
    fight.tank = PlayerbotAI::IsTank(bot);
    fight.dps = PlayerbotAI::IsDps(bot);
    if (!MagistersTerrace::FollowsFocusOrder(fight.inCombat, fight.healer, fight.tank, fight.dps))
        return 1.0f;

    fight.crystalUp = !AI_VALUE(ObjectGuid, "mgt crystal target").IsEmpty();
    if (fight.crystalUp)
        return 1.0f;

    ObjectGuid const order = MagistersTerrace::ResolveFocusOrder(botAI);
    fight.haveOrder = !order.IsEmpty();
    if (shape.drop && fight.haveOrder)
    {
        Unit* current = AI_VALUE(Unit*, "current target");
        fight.onLiveOrder =
            current && current->GetGUID() == order && current->IsAlive() && AttackersValue::IsValidTarget(current, bot);
    }

    return MagistersTerrace::FocusOrderAllows(fight, shape) ? 1.0f : 0.0f;
}

float MgTFocusBurstMultiplier::GetValue(Action* action)
{
    if (action->getThreatType() != Action::ActionThreatType::Aoe)
        return 1.0f;

    bool const dpsAreaSpell =
        MagistersTerrace::IsDpsAreaSpell(true, dynamic_cast<CastHealingSpellAction*>(action) != nullptr,
                                         PlayerbotAI::IsHeal(bot), PlayerbotAI::IsDps(bot));
    if (!dpsAreaSpell)
        return 1.0f;

    return MagistersTerrace::FocusBurstAllows(true, MagistersTerrace::ResolveFocusOrderOwner(botAI)) ? 1.0f : 0.0f;
}
