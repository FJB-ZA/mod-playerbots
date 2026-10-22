// Eck the Ferocious bot tactics (heroic only): everyone but his victim sidesteps out of the Eck Spit cone.

#include <cmath>

#include "DungeonTactics.h"
#include "GDActions.h"
#include "GDMultipliers.h"
#include "GDPolicy.h"
#include "GDTriggers.h"
#include "Playerbots.h"

namespace
{
struct SpitOffset
{
    float forward;
    float lateral;  // signed: positive on his left
};

SpitOffset OffsetFrom(Unit* eck, Player* bot)
{
    float distance = eck->GetExactDist2d(bot);
    float angle = DungeonTactics::RelativeAngle(eck->GetAngle(bot), eck->GetOrientation());
    return {distance * std::cos(angle), distance * std::sin(angle)};
}

Unit* SpittingEck(PlayerbotAI* botAI, Player* bot)
{
    if (!bot->GetMap()->IsHeroic())
        return nullptr;

    Unit* eck = botAI->GetAiObjectContext()->GetValue<Unit*>("find target", "eck the ferocious")->Get();
    return eck && eck->FindCurrentSpellBySpellId(SPELL_ECK_SPIT) ? eck : nullptr;
}
}  // namespace

bool EckSpitTrigger::IsActive()
{
    Unit* eck = SpittingEck(botAI, bot);
    if (!eck)
        return false;

    SpitOffset offset = OffsetFrom(eck, bot);
    return Gundrak::DodgesEckSpit(eck->GetVictim() == bot, offset.forward, std::fabs(offset.lateral));
}

bool DodgeEckSpitAction::Execute(Event /*event*/)
{
    Unit* eck = SpittingEck(botAI, bot);
    if (!eck)
        return false;

    SpitOffset offset = OffsetFrom(eck, bot);
    float step = Gundrak::EckSpitSidestep(offset.forward, std::fabs(offset.lateral));
    if (step <= 0.0f)
        return false;

    float side = offset.lateral >= 0.0f ? 1.0f : -1.0f;
    return Move(eck->GetOrientation() + side * DungeonTactics::Pi / 2.0f, step);
}

float EckMultiplier::GetValue(Action* action)
{
    Unit* eck = SpittingEck(botAI, bot);
    if (!eck)
        return 1.0f;

    bool repositioning = DungeonTactics::IsRepositioning(DungeonTactics::ShapeOf(action));
    bool dodge = dynamic_cast<DodgeEckSpitAction*>(action) != nullptr;
    return Gundrak::EckAllows(true, eck->GetVictim() == bot, repositioning, dodge) ? 1.0f : 0.0f;
}
