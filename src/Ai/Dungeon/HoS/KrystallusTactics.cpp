// Krystallus bot tactics: once Ground Slam lands, everyone spreads so Shatter doesn't hit their neighbours.

#include "HoSActions.h"
#include "HoSMultipliers.h"
#include "HoSPolicy.h"
#include "HoSTriggers.h"
#include "Playerbots.h"

namespace
{
// The slam aura comes first and its periodic debuff later; neither lasts the whole slam
bool Slammed(Player* bot) { return bot->HasAura(SPELL_GROUND_SLAM) || bot->HasAura(DEBUFF_GROUND_SLAM); }
}  // namespace

bool KrystallusGroundSlamTrigger::IsActive() { return AI_VALUE2(Unit*, "find target", "krystallus") && Slammed(bot); }

bool ShatterSpreadAction::Execute(Event /*event*/)
{
    if (!AI_VALUE2(Unit*, "find target", "krystallus"))
        return false;

    Unit* closestMember = nullptr;
    GuidVector members = AI_VALUE(GuidVector, "group members");
    for (ObjectGuid const& member : members)
    {
        Unit* unit = botAI->GetUnit(member);
        if (!unit || bot->GetGUID() == member)
            continue;

        if (!closestMember || bot->GetExactDist2d(unit) < bot->GetExactDist2d(closestMember))
            closestMember = unit;
    }

    if (!closestMember)
        return false;

    float step = HallsOfStone::ShatterSpreadStep(bot->GetExactDist2d(closestMember));
    return step > 0.0f && MoveAway(closestMember, step);
}

float KrystallusMultiplier::GetValue(Action* action)
{
    if (!AI_VALUE2(Unit*, "find target", "krystallus"))
        return 1.0f;

    bool movement = dynamic_cast<MovementAction*>(action) != nullptr;
    bool spread = dynamic_cast<ShatterSpreadAction*>(action) != nullptr;
    return HallsOfStone::KrystallusAllows(Slammed(bot), movement, spread) ? 1.0f : 0.0f;
}
