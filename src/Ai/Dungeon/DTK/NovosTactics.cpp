/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Novos the Summoner bot tactics: stay out of the Arcane Field, hold the middle of the room and split the adds by role.

#include "ChooseTargetActions.h"
#include "DTKActions.h"
#include "DTKMultipliers.h"
#include "DTKPolicy.h"
#include "DTKTriggers.h"
#include "Playerbots.h"

namespace
{
Position const NOVOS_PARTY_POSITION = Position(-378.852f, -760.349f, 28.587f);

DrakTharon::NovosAdd AddOf(uint32 entry)
{
    switch (entry)
    {
        case NPC_HULKING_CORPSE:
            return DrakTharon::NovosAdd::HulkingCorpse;
        case NPC_CRYSTAL_HANDLER:
            return DrakTharon::NovosAdd::CrystalHandler;
        case NPC_RISEN_SHADOWCASTER:
            return DrakTharon::NovosAdd::RisenShadowcaster;
        case NPC_FETID_TROLL_CORPSE:
            return DrakTharon::NovosAdd::FetidTrollCorpse;
        default:
            return DrakTharon::NovosAdd::Other;
    }
}
}  // namespace

bool ArcaneFieldTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "novos the summoner");
    return boss && boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_ARCANE_FIELD);
}

bool AvoidArcaneFieldAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "novos the summoner");
    if (!boss)
        return false;

    float step = DrakTharon::ArcaneFieldStepAway(bot->GetExactDist2d(boss));
    return step > 0.0f && MoveAway(boss, step);
}

bool NovosDefaultPositionAction::isUseful() { return bot->GetDistance(NOVOS_PARTY_POSITION) > DrakTharon::NovosTether; }

bool NovosDefaultPositionAction::Execute(Event /*event*/)
{
    // Only reposition while not killing anything
    if (bot->GetTarget())
        return false;

    return MoveNear(bot->GetMap()->GetId(), NOVOS_PARTY_POSITION.GetPositionX(), NOVOS_PARTY_POSITION.GetPositionY(),
                    NOVOS_PARTY_POSITION.GetPositionZ(), DrakTharon::NovosClusterDistance,
                    MovementPriority::MOVEMENT_NORMAL);
}

bool NovosTargetPriorityAction::Execute(Event /*event*/)
{
    std::vector<DrakTharon::PartySlot> party;
    std::vector<Player*> players;
    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        Player* player = botAI->GetPlayer(member);
        if (!player)
            continue;
        players.push_back(player);
        party.push_back({PlayerbotAI::IsDps(player), PlayerbotAI::IsMelee(player)});
    }

    int stairs = DrakTharon::StairsDpsIndex(party);
    DrakTharon::NovosRole role = DrakTharon::NovosRole::Other;
    if (PlayerbotAI::IsTank(bot))
        role = DrakTharon::NovosRole::Tank;
    else if (stairs >= 0 && players[stairs] == bot)
        role = DrakTharon::NovosRole::StairsDps;
    else if (PlayerbotAI::IsDps(bot))
        role = DrakTharon::NovosRole::Dps;

    // The adds aren't on the threat table, so find target can't see them
    Unit* selected[2] = {nullptr, nullptr};
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        int priority = DrakTharon::NovosPriority(role, AddOf(unit->GetEntry()));
        if (priority < 0)
            continue;

        Unit*& slot = selected[priority];
        if (role != DrakTharon::NovosRole::StairsDps || !slot ||
            bot->GetDistance(unit) < bot->GetDistance(slot) - DrakTharon::StairsSwapMargin)
            slot = unit;
    }

    for (Unit* target : selected)
    {
        if (!target)
            continue;
        // Already on the best target: nothing to send
        return AI_VALUE(Unit*, "current target") != target && Attack(target);
    }
    return false;
}

float NovosMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "novos the summoner");
    if (!boss)
        return 1.0f;

    bool assist = dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action);
    bool field = boss->FindCurrentSpellBySpellId(SPELL_ARCANE_FIELD) != nullptr;
    return DrakTharon::NovosAllows(field, bot->GetTarget() != ObjectGuid::Empty, assist) ? 1.0f : 0.0f;
}
