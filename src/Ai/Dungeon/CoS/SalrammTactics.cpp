/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Salramm the Fleshcrafter bot tactics: kill his ghouls first and keep ranged clear of them before he explodes one.

#include "CoSActions.h"
#include "CoSPolicy.h"
#include "CoSTriggers.h"
#include "Group.h"
#include "Playerbots.h"

namespace
{
// The nearest living ghoul this bot should stay clear of, with the step that takes it out of the blast.
Unit* GhoulTooClose(PlayerbotAI* botAI, Player* bot, float& step)
{
    GuidVector npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest hostile npcs")->Get();
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_GHOUL_MINION)
            continue;
        if (!CullingOfStratholme::KeepsClearOfGhoul(PlayerbotAI::IsMelee(bot), unit->GetVictim() == bot))
            continue;

        step = CullingOfStratholme::GhoulStepAway(bot->GetExactDist2d(unit));
        if (step > 0.0f)
            return unit;
    }
    return nullptr;
}
}  // namespace

bool ExplodeGhoulTrigger::IsActive()
{
    float step = 0.0f;
    return AI_VALUE2(Unit*, "find target", "salramm the fleshcrafter") && GhoulTooClose(botAI, bot, step);
}

bool ExplodeGhoulSpreadAction::Execute(Event /*event*/)
{
    float step = 0.0f;
    Unit* ghoul = GhoulTooClose(botAI, bot, step);
    return ghoul && MoveAway(ghoul, step);
}

namespace
{
constexpr uint8 SkullIcon = 7;

// Nearest Salramm rather than the bot, so every bot in the group picks the same ghoul.
Unit* GhoulNearestSalramm(PlayerbotAI* botAI, Unit* boss)
{
    Unit* best = nullptr;
    GuidVector npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest hostile npcs")->Get();
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->IsAlive() && unit->GetEntry() == NPC_GHOUL_MINION &&
            (!best || boss->GetExactDist2d(unit) < boss->GetExactDist2d(best)))
            best = unit;
    }
    return best;
}

bool SkullOnLivingGhoul(PlayerbotAI* botAI, Group* group)
{
    Unit* marked = botAI->GetUnit(group->GetTargetIcon(SkullIcon));
    return marked && marked->IsAlive() && marked->GetEntry() == NPC_GHOUL_MINION;
}
}  // namespace

bool SalrammGhoulTrigger::IsActive()
{
    Group* group = bot->GetGroup();
    if (!group || !PlayerbotAI::IsDps(bot))
        return false;
    Unit* boss = AI_VALUE2(Unit*, "find target", "salramm the fleshcrafter");
    if (!boss || SkullOnLivingGhoul(botAI, group))
        return false;
    return CullingOfStratholme::MarksGhoul(false, GhoulNearestSalramm(botAI, boss) != nullptr);
}

bool AttackGhoulMinionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "salramm the fleshcrafter");
    Group* group = bot->GetGroup();
    if (!boss || !group || SkullOnLivingGhoul(botAI, group))
        return false;

    // Skull the ghoul so the whole group switches to it; the next one gets the mark once it dies
    if (Unit* ghoul = GhoulNearestSalramm(botAI, boss))
        group->SetTargetIcon(SkullIcon, bot->GetGUID(), ghoul->GetGUID());
    return false;
}
