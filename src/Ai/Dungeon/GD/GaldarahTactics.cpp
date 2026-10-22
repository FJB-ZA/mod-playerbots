// Gal'darah bot tactics: everyone steps out of Whirling Slash, the tank twice as far since he chases her.

#include "GDActions.h"
#include "GDMultipliers.h"
#include "GDPolicy.h"
#include "GDTriggers.h"
#include "Playerbots.h"

namespace
{
bool Whirling(Player* bot, Unit* boss) { return boss->HasAura(SPELL_WHIRLING_SLASH); }
}  // namespace

bool GaldarahWhirlingSlashTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gal'darah");
    return boss && Whirling(bot, boss);
}

bool AvoidWhirlingSlashAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gal'darah");
    if (!boss)
        return false;

    float step = Gundrak::WhirlingSlashStepAway(bot->GetExactDist2d(boss->GetPosition()), PlayerbotAI::IsTank(bot));
    return step > 0.0f && MoveAway(boss, step);
}

float GaldarahMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gal'darah");
    if (!boss)
        return 1.0f;

    bool movement = dynamic_cast<MovementAction*>(action) != nullptr;
    bool avoidSlash = dynamic_cast<AvoidWhirlingSlashAction*>(action) != nullptr;
    return Gundrak::GaldarahAllows(Whirling(bot, boss), movement, avoidSlash) ? 1.0f : 0.0f;
}
