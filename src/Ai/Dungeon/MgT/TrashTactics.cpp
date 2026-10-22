// Magisters' Terrace trash bot tactics: leave dampening fields and novas, kick the worst casts, kill healers first.

#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "MgTActions.h"
#include "MgTMultipliers.h"
#include "MgTShared.h"
#include "MgTTriggers.h"
#include "MovementActions.h"
#include "Playerbots.h"

using namespace EncounterHelpers;

namespace MagistersTerrace
{
void CollectDampeningEscapes(Player* bot, EscapeSpots& out)
{
    if (!bot->IsInCombat())
        return;

    std::vector<Hazard> fields;
    for (Position const& pos : GetDynamicObjectPositions(bot, DampeningScan, SPELL_MAGIC_DAMPENING_FIELD))
        fields.push_back({pos.GetPositionX(), pos.GetPositionY(), DampeningStand, DampeningClear});

    CollectHazardEscapes(bot, fields, PlayerbotAI::IsTank(bot) ? TankRejoinGap : 0.0f, out);
}

Unit* GetGlaiveThrowingMageGuard(Player* bot)
{
    std::vector<Unit*> guards;
    CollectAttackersByEntry(bot, {NPC_SUNBLADE_MAGE_GUARD}, GlaiveMaxRange, guards);

    for (Unit* guard : guards)
    {
        if (ThrowsGlaiveAt(guard->GetVictim() == bot, bot->GetExactDist2d(guard)))
            return guard;
    }

    return nullptr;
}

void CollectNovaEscapes(Player* bot, EscapeSpots& out)
{
    if (!DodgesArcaneNova(PlayerbotAI::IsMelee(bot)))
        return;

    std::vector<Unit*> magisters;
    CollectAttackersByEntry(bot, {NPC_SUNBLADE_MAGISTER}, NovaClear, magisters);

    std::vector<Hazard> hazards;
    hazards.reserve(magisters.size());
    for (Unit* magister : magisters)
        hazards.push_back({magister->GetPositionX(), magister->GetPositionY(), NovaClear, NovaClear + NovaClearSlack});

    CollectHazardEscapes(bot, hazards, 0.0f, out);
}

uint8 GetInterruptUrgency(Unit* caster) { return caster ? BestCastPriority(caster, &InterruptPriority) : 0; }

Unit* GetInterruptTarget(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

    Unit* best = nullptr;
    uint8 bestPrio = 0;
    float bestDist = 0.0f;
    auto const& attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const guid : attackers)
    {
        Unit* caster = botAI->GetUnit(guid);
        if (!caster)
            continue;

        float const dist = bot->GetExactDist2d(caster);
        if (dist > InterruptScan)
            continue;

        uint8 const prio = BestCastPriority(caster, &InterruptPriority);
        if (!prio)
            continue;

        if (OutranksBest(prio, dist, best != nullptr, bestPrio, bestDist))
        {
            best = caster;
            bestPrio = prio;
            bestDist = dist;
        }
    }

    return best;
}

Unit* GetFocusTarget(Player* bot, ObjectGuid& latched) { return PickRankedFocus(bot, &TrashFocusRank, {}, latched); }

Unit* GetEnragedWretched(Player* bot)
{
    std::vector<Unit*> wretched;
    CollectAttackersByEntry(bot, {NPC_WRETCHED_SKULKER, NPC_WRETCHED_BRUISER, NPC_WRETCHED_HUSK}, WretchedScan,
                            wretched);

    Unit* best = nullptr;
    float bestDist = WretchedScan;
    for (Unit* mob : wretched)
    {
        Unit* victim = mob->GetVictim();
        if (!TauntsWretched(mob->HasAura(SPELL_DRINK_FEL_INFUSION), victim != nullptr, victim == bot))
            continue;

        float const dist = bot->GetExactDist2d(mob);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = mob;
        }
    }

    return best;
}
}  // namespace MagistersTerrace

bool MgTInDampeningFieldTrigger::IsActive()
{
    return !AI_VALUE_REF(MagistersTerrace::EscapeSpots, "mgt dampening escape").empty();
}

bool MgTMageGuardAtRangeTrigger::IsActive() { return !AI_VALUE(ObjectGuid, "mgt mage guard target").IsEmpty(); }

bool MgTArcaneNovaRangeTrigger::IsActive()
{
    return !AI_VALUE_REF(MagistersTerrace::EscapeSpots, "mgt nova escape").empty();
}

bool MgTPriorityInterruptTrigger::IsActive() { return !AI_VALUE(ObjectGuid, "mgt interrupt target").IsEmpty(); }

bool MgTFocusTargetTrigger::IsActive()
{
    if (!MagistersTerrace::TakesFocusOrder(bot->IsInCombat(), PlayerbotAI::IsDps(bot), PlayerbotAI::IsTank(bot)))
        return false;

    if (!AI_VALUE(ObjectGuid, "mgt crystal target").IsEmpty())
        return false;

    ObjectGuid const order = AI_VALUE(ObjectGuid, "mgt focus target");
    return !order.IsEmpty() && MagistersTerrace::ResolveFocusOrder(botAI) == order;
}

bool MgTEnragedWretchedTrigger::IsActive() { return !AI_VALUE(ObjectGuid, "mgt enraged wretched").IsEmpty(); }

bool MgTCloseOnMageGuardAction::Execute(Event)
{
    Unit* guard = botAI->GetUnit(AI_VALUE(ObjectGuid, "mgt mage guard target"));
    if (!guard)
        return false;

    return ReachCombatTo(guard, 0.0f);
}

bool MgTPriorityInterruptAction::Execute(Event)
{
    Unit* target = botAI->GetUnit(AI_VALUE(ObjectGuid, "mgt interrupt target"));
    if (!target)
        return false;

    uint8 const urgency = MagistersTerrace::GetInterruptUrgency(target);
    if (!urgency)
        return false;

    return !MagistersTerrace::CastInterrupt(botAI, target, urgency).empty();
}

bool MgTTauntEnragedWretchedAction::Execute(Event)
{
    Unit* wretched = botAI->GetUnit(AI_VALUE(ObjectGuid, "mgt enraged wretched"));
    if (!wretched)
        return false;

    for (std::string_view taunt : MagistersTerrace::TauntSpells)
    {
        std::string const spell(taunt);
        if (botAI->CanCastSpell(spell, wretched) && botAI->CastSpell(spell, wretched))
            return true;
    }

    return false;
}

float MgTDampeningFieldMultiplier::GetValue(Action* action)
{
    bool const competing = dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action) ||
                           dynamic_cast<AvoidAoeAction*>(action);
    if (!competing)
        return 1.0f;

    bool const escaping = !AI_VALUE_REF(MagistersTerrace::EscapeSpots, "mgt dampening escape").empty();
    return MagistersTerrace::DampeningFieldAllows(true, escaping) ? 1.0f : 0.0f;
}
