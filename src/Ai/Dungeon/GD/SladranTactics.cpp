// Slad'ran bot tactics: out of Poison Nova, the closest dps frees each snake wrap, everyone else stacks on the tank.

#include <algorithm>
#include <list>
#include <utility>
#include <vector>

#include "AttackersValue.h"
#include "ChooseTargetActions.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "GDActions.h"
#include "GDMultipliers.h"
#include "GDPolicy.h"
#include "GDTriggers.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"

namespace
{
bool IsClosestSnakeWrapFreer(Player* bot, Unit* snakeWrap, float distance)
{
    Group* group = bot->GetGroup();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->IsInCombat() || !GET_PLAYERBOT_AI(member) ||
            member->GetMap() != bot->GetMap() ||
            !Gundrak::CanFreeSnakeWrap(PlayerbotAI::IsDps(member), member->HasAura(SPELL_SNAKE_WRAP)))
            continue;

        if (!Gundrak::BeatsToSnakeWrap(member->GetExactDist2d(snakeWrap), distance, member->GetGUID() < bot->GetGUID()))
            continue;

        if (member->IsWithinLOSInMap(snakeWrap))
            return false;
    }
    return true;
}

bool CastingPoisonNova(Player* bot, Unit* boss) { return boss->FindCurrentSpellBySpellId(SPELL_POISON_NOVA); }
}  // namespace

namespace GundrakSladran
{
bool IsAdd(Unit* unit)
{
    if (!unit)
        return false;

    uint32 entry = unit->GetEntry();
    return entry == NPC_SLADRAN_VIPER || entry == NPC_SLADRAN_CONSTRICTOR;
}

ObjectGuid CalculateAssignedSnakeWrap(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsAlive() || !bot->IsInCombat() || !bot->GetGroup() ||
        !Gundrak::CanFreeSnakeWrap(PlayerbotAI::IsDps(bot), bot->HasAura(SPELL_SNAKE_WRAP)))
        return ObjectGuid::Empty;

    std::list<Creature*> wraps;
    bot->GetCreatureListWithEntryInGrid(wraps, NPC_SNAKE_WRAP, sPlayerbotAIConfig.SightDistance);

    std::vector<std::pair<float, Creature*>> candidates;
    candidates.reserve(wraps.size());
    for (Creature* snakeWrap : wraps)
    {
        if (snakeWrap->IsAlive() && AttackersValue::IsPossibleTarget(snakeWrap, bot))
            candidates.emplace_back(bot->GetExactDist2d(snakeWrap), snakeWrap);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](std::pair<float, Creature*> const& lhs, std::pair<float, Creature*> const& rhs)
              { return lhs.first < rhs.first; });

    for (auto const& [distance, snakeWrap] : candidates)
    {
        if (bot->IsWithinLOSInMap(snakeWrap) && IsClosestSnakeWrapFreer(bot, snakeWrap, distance))
            return snakeWrap->GetGUID();
    }
    return ObjectGuid::Empty;
}

Unit* GetAssignedSnakeWrap(PlayerbotAI* botAI)
{
    ObjectGuid guid = botAI->GetAiObjectContext()->GetValue<ObjectGuid>("slad'ran snake wrap target")->Get();
    if (guid.IsEmpty())
        return nullptr;

    Unit* snakeWrap = botAI->GetUnit(guid);
    if (!snakeWrap || !snakeWrap->IsAlive() || snakeWrap->GetEntry() != NPC_SNAKE_WRAP)
        return nullptr;
    return snakeWrap;
}

Player* GetStackTank(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsAlive() || PlayerbotAI::IsTank(bot))
        return nullptr;

    AiObjectContext* context = botAI->GetAiObjectContext();
    if (!context->GetValue<Unit*>("find target", "slad'ran")->Get())
        return nullptr;

    Unit* currentTarget = context->GetValue<Unit*>("current target")->Get();
    bool freeingWrap = GetAssignedSnakeWrap(botAI) ||
                       (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == NPC_SNAKE_WRAP);
    if (freeingWrap)
        return nullptr;

    Player* tank = EncounterHelpers::GetGroupMainTank(bot);
    if (!tank || tank == bot || tank->GetMap() != bot->GetMap())
        return nullptr;

    return Gundrak::StacksOnTank(false, false, bot->GetExactDist2d(tank)) ? tank : nullptr;
}

Unit* GetTankHoldTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsAlive() || !PlayerbotAI::IsTank(bot))
        return nullptr;

    AiObjectContext* context = botAI->GetAiObjectContext();
    Unit* boss = context->GetValue<Unit*>("find target", "slad'ran")->Get();
    if (!boss || !boss->IsAlive())
        return nullptr;

    Unit* currentTarget = context->GetValue<Unit*>("current target")->Get();
    if (!currentTarget || !currentTarget->IsAlive())
        return boss;

    bool onAdd = IsAdd(currentTarget);
    float distanceToAdd = onAdd ? bot->GetExactDist2d(currentTarget) : 0.0f;
    return Gundrak::TankReturnsToSladran(currentTarget == boss, onAdd, distanceToAdd) ? boss : nullptr;
}
}  // namespace GundrakSladran

bool SladranPoisonNovaTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "slad'ran");
    return boss && CastingPoisonNova(bot, boss);
}

bool SladranSnakeWrapTrigger::IsActive() { return GundrakSladran::GetAssignedSnakeWrap(botAI) != nullptr; }

bool SladranStackOnTankTrigger::IsActive() { return GundrakSladran::GetStackTank(botAI) != nullptr; }

bool SladranTankHoldTrigger::IsActive() { return GundrakSladran::GetTankHoldTarget(botAI) != nullptr; }

bool AvoidPoisonNovaAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "slad'ran");
    if (!boss)
        return false;

    float step = Gundrak::PoisonNovaStepAway(bot->GetExactDist2d(boss->GetPosition()));
    return step > 0.0f && MoveAway(boss, step);
}

bool AttackSnakeWrapAction::Execute(Event /*event*/)
{
    Unit* snakeWrap = GundrakSladran::GetAssignedSnakeWrap(botAI);
    if (!snakeWrap || AI_VALUE(Unit*, "current target") == snakeWrap)
        return false;

    return Attack(snakeWrap);
}

bool SladranStackOnTankAction::Execute(Event /*event*/)
{
    Player* tank = GundrakSladran::GetStackTank(botAI);
    if (!tank)
        return false;

    return MoveTo(tank, Gundrak::StackCloseTo, MovementPriority::MOVEMENT_COMBAT);
}

bool SladranTankHoldAction::Execute(Event /*event*/)
{
    Unit* boss = GundrakSladran::GetTankHoldTarget(botAI);
    if (!boss)
        return false;

    return Attack(boss);
}

float SladranMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "slad'ran");
    if (!boss)
        return 1.0f;

    Gundrak::SladranMoment fight;
    fight.poisonNova = CastingPoisonNova(bot, boss);
    fight.tank = PlayerbotAI::IsTank(bot);

    Gundrak::SladranAction shape;
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.avoidNova = dynamic_cast<AvoidPoisonNovaAction*>(action) != nullptr;
    shape.flee = dynamic_cast<FleeAction*>(action) || dynamic_cast<RunAwayAction*>(action) ||
                 dynamic_cast<MoveRandomAction*>(action) || dynamic_cast<MoveFromGroupAction*>(action);
    if (fight.tank)
    {
        Unit* chased = nullptr;
        if (dynamic_cast<TankAssistAction*>(action))
            chased = AI_VALUE(Unit*, "tank target");
        else if (dynamic_cast<ReachTargetAction*>(action))
            chased = AI_VALUE(Unit*, "current target");
        shape.chaseFarAdd =
            chased && GundrakSladran::IsAdd(chased) && bot->GetExactDist2d(chased) > Gundrak::TankPickupRange;
    }
    else
    {
        shape.formationMove =
            dynamic_cast<CombatFormationMoveAction*>(action) && !dynamic_cast<SetBehindTargetAction*>(action);
        shape.dpsAssist = dynamic_cast<DpsAssistAction*>(action) != nullptr;
        fight.freeingWrap = shape.dpsAssist && GundrakSladran::GetAssignedSnakeWrap(botAI);
    }

    return Gundrak::SladranAllows(fight, shape) ? 1.0f : 0.0f;
}
