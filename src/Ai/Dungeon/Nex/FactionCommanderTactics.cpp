// Faction commander bot tactics: get out of the whirlwind and stay out until it ends.

#include "MovementActions.h"
#include "NexActions.h"
#include "NexMultipliers.h"
#include "NexPolicy.h"
#include "NexTriggers.h"
#include "Playerbots.h"

namespace
{
Unit* Commander(PlayerbotAI* botAI, Player* bot)
{
    Difficulty difficulty = bot->GetMap()->GetDifficulty();
    if (difficulty != DUNGEON_DIFFICULTY_NORMAL && difficulty != DUNGEON_DIFFICULTY_HEROIC)
        return nullptr;

    std::string name(Nexus::CommanderName(bot->GetTeamId() == TEAM_ALLIANCE, difficulty == DUNGEON_DIFFICULTY_HEROIC));
    return botAI->GetAiObjectContext()->GetValue<Unit*>("find target", name)->Get();
}

bool Whirlwinding(Unit* boss)
{
    return boss && boss->HasUnitState(UNIT_STATE_CASTING) && boss->FindCurrentSpellBySpellId(SPELL_WHIRLWIND);
}
}  // namespace

bool FactionCommanderWhirlwindTrigger::IsActive() { return Whirlwinding(Commander(botAI, bot)); }

bool MoveFromWhirlwindAction::Execute(Event /*event*/)
{
    Unit* boss = Commander(botAI, bot);
    if (!boss)
        return false;

    float step = Nexus::WhirlwindStepAway(bot->GetExactDist2d(boss->GetPosition()));
    return step > 0.0f && MoveAway(boss, step);
}

float FactionCommanderMultiplier::GetValue(Action* action)
{
    bool movement = dynamic_cast<MovementAction*>(action) != nullptr;
    bool stepOut = dynamic_cast<MoveFromWhirlwindAction*>(action) != nullptr;
    if (!movement || stepOut)
        return 1.0f;

    return Nexus::CommanderAllows(Whirlwinding(Commander(botAI, bot)), movement, stepOut) ? 1.0f : 0.0f;
}
