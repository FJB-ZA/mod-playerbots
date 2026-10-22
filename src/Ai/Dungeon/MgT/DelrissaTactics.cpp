// Priestess Delrissa bot tactics: her party dies healers first, bots split the interrupts, a shaman holds Tremor.

#include <algorithm>
#include <utility>

#include "AttackersValue.h"
#include "Group.h"
#include "MgTActions.h"
#include "MgTMultipliers.h"
#include "MgTShared.h"
#include "MgTTriggers.h"
#include "Playerbots.h"
#include "ShamanActions.h"

namespace MagistersTerrace
{
namespace
{
bool RetinueEngaged(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

    auto const& attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const guid : attackers)
    {
        Unit* attacker = botAI->GetUnit(guid);
        if (attacker && RetinueFocusRank(attacker->GetEntry()))
            return true;
    }

    return false;
}

// How many living bots in the group sort before this one.
int InterruptSlot(Player* bot)
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

        if (!GET_PLAYERBOT_AI(member))
            continue;

        if (member->GetGUID() < bot->GetGUID())
            ++slot;
    }

    return slot;
}
}  // namespace

uint8 GetRetinueInterruptUrgency(Unit* caster)
{
    return caster ? BestCastPriority(caster, &RetinueInterruptPriority) : 0;
}

bool ShouldHoldTremorTotem(Player* bot)
{
    if (bot->getClass() != CLASS_SHAMAN || !bot->HasSpell(SPELL_TREMOR_TOTEM))
        return false;

    return RetinueEngaged(bot);
}

Unit* GetDelrissaFocusTarget(Player* bot, ObjectGuid& latched)
{
    std::vector<Unit*> sheep;
    if (RetinueEngaged(bot))
    {
        std::vector<Unit*> found;
        CollectCreaturesByEntry(bot, {NPC_HIGH_EXPLOSIVE_SHEEP}, FocusScan, found);

        for (Unit* mob : found)
        {
            if (AttackersValue::IsValidTarget(mob, bot))
                sheep.push_back(mob);
        }
    }

    return PickRankedFocus(bot, &RetinueFocusRank, sheep, latched);
}

void CollectDelrissaInterruptPreference(Player* bot, std::vector<Unit*>& out)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

    std::vector<std::pair<uint8, Unit*>> ranked;
    auto const& attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const guid : attackers)
    {
        Unit* caster = botAI->GetUnit(guid);
        if (!caster || bot->GetExactDist2d(caster) > InterruptScan)
            continue;

        if (!RetinueFocusRank(caster->GetEntry()))
            continue;

        if (uint8 const prio = BestCastPriority(caster, &RetinueInterruptPriority))
            ranked.push_back({prio, caster});
    }

    if (ranked.empty())
        return;

    std::sort(ranked.begin(), ranked.end(),
              [](auto const& a, auto const& b)
              {
                  if (a.first != b.first)
                      return a.first < b.first;

                  return a.second->GetGUID() < b.second->GetGUID();
              });

    size_t const index = RetinueInterruptPick(static_cast<size_t>(InterruptSlot(bot)), ranked.size());

    out.reserve(out.size() + ranked.size());
    out.push_back(ranked[index].second);

    for (size_t i = 0; i < ranked.size(); ++i)
    {
        if (i != index)
            out.push_back(ranked[i].second);
    }
}

void CollectDelrissaPets(Player* bot, std::vector<ObjectGuid>& out)
{
    std::vector<Unit*> pets;
    CollectAttackersByEntry(bot, {NPC_FIZZLE, NPC_SLIVER}, FocusScan, pets);

    out.reserve(out.size() + pets.size());
    for (Unit* pet : pets)
        out.push_back(pet->GetGUID());
}
}  // namespace MagistersTerrace

bool MgTDelrissaInterruptTrigger::IsActive()
{
    return !AI_VALUE_REF(GuidVector, "mgt delrissa interrupt order").empty();
}

bool MgTDelrissaFocusTargetTrigger::IsActive()
{
    if (!MagistersTerrace::TakesFocusOrder(bot->IsInCombat(), PlayerbotAI::IsDps(bot), PlayerbotAI::IsTank(bot)))
        return false;

    ObjectGuid const order = AI_VALUE(ObjectGuid, "mgt delrissa focus target");
    return !order.IsEmpty() && MagistersTerrace::ResolveFocusOrder(botAI) == order;
}

bool MgTDelrissaTremorTotemTrigger::IsActive()
{
    if (!AI_VALUE(bool, "mgt delrissa tremor totem"))
        return false;

    return !AI_VALUE2(bool, "has totem", "tremor totem");
}

bool MgTDelrissaInterruptAction::Execute(Event)
{
    auto const& order = AI_VALUE_REF(GuidVector, "mgt delrissa interrupt order");
    for (ObjectGuid const guid : order)
    {
        Unit* target = botAI->GetUnit(guid);
        if (!target)
            continue;

        uint8 const urgency = MagistersTerrace::GetRetinueInterruptUrgency(target);
        if (!urgency)
            continue;

        if (!MagistersTerrace::CastInterrupt(botAI, target, urgency).empty())
            return true;
    }

    return false;
}

bool MgTDelrissaTremorTotemAction::Execute(Event)
{
    return botAI->CanCastSpell("tremor totem", bot) && botAI->CastSpell("tremor totem", bot);
}

float MgTDelrissaTremorTotemMultiplier::GetValue(Action* action)
{
    bool const competing = dynamic_cast<CastStrengthOfEarthTotemAction*>(action) ||
                           dynamic_cast<CastStoneskinTotemAction*>(action) ||
                           dynamic_cast<CastEarthbindTotemAction*>(action);
    if (!competing)
        return 1.0f;

    return MagistersTerrace::DelrissaTremorTotemAllows(true, AI_VALUE(bool, "mgt delrissa tremor totem")) ? 1.0f : 0.0f;
}
