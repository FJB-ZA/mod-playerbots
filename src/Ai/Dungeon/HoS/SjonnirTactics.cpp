// Sjonnir the Ironshaper bot tactics: step out of Lightning Ring and hold still until the channel ends.

#include "HoSActions.h"
#include "HoSMultipliers.h"
#include "HoSPolicy.h"
#include "HoSTriggers.h"
#include "Playerbots.h"

namespace
{
bool ChannelingRing(Player* bot, Unit* boss)
{
    return boss->HasUnitState(UNIT_STATE_CASTING) && (boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_RING) ||
                                                      boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_RING_5S));
}
}  // namespace

bool SjonnirLightningRingTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sjonnir the ironshaper");
    return boss && ChannelingRing(bot, boss);
}

bool AvoidLightningRingAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sjonnir the ironshaper");
    if (!boss)
        return false;

    float step = HallsOfStone::LightningRingStepAway(bot->GetExactDist2d(boss->GetPosition()));
    return step > 0.0f && MoveAway(boss, step);
}

// Blocks all other movement during the channel, which can hold bots off adds for its duration
float SjonnirMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sjonnir the ironshaper");
    if (!boss)
        return 1.0f;

    bool movement = dynamic_cast<MovementAction*>(action) != nullptr;
    bool avoidRing = dynamic_cast<AvoidLightningRingAction*>(action) != nullptr;
    return HallsOfStone::SjonnirAllows(ChannelingRing(bot, boss), movement, avoidRing) ? 1.0f : 0.0f;
}
