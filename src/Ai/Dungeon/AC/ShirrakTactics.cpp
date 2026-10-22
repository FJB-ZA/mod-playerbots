// Shirrak the Dead Watcher bot tactics: tank him up the stairs, ranged hold at range, everyone runs from Focus Fire.

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#include "ACActions.h"
#include "ACMultipliers.h"
#include "ACPolicy.h"
#include "ACTriggers.h"
#include "EncounterHelpers.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"

namespace
{
Creature* NearestFlare(Player* bot)
{
    return bot->FindNearestCreature(NPC_FOCUS_FIRE, AuchenaiCrypts::FlareSearchRadius);
}
}  // namespace

bool ShirrakTankPositionBossTrigger::IsActive()
{
    return PlayerbotAI::IsTank(bot) && AI_VALUE2(Unit*, "find target", "shirrak the dead watcher");
}

bool ShirrakFleeFocusFireTrigger::IsActive()
{
    if (!AI_VALUE2(Unit*, "find target", "shirrak the dead watcher"))
        return false;

    return NearestFlare(bot) != nullptr;
}

bool ShirrakRangedKeepDistanceTrigger::IsActive()
{
    return PlayerbotAI::IsRanged(bot) && AI_VALUE2(Unit*, "find target", "shirrak the dead watcher");
}

bool ShirrakTankPositionBossAction::Execute(Event /*event*/)
{
    Unit* shirrak = AI_VALUE2(Unit*, "find target", "shirrak the dead watcher");
    if (!shirrak)
        return false;

    if (bot->GetVictim() != shirrak)
        return Attack(shirrak);

    if (!AuchenaiCrypts::DragsShirrak(shirrak->GetVictim() == bot, bot->IsWithinMeleeRange(shirrak),
                                      bot->GetHealthPct()))
        return false;

    Position const spot(AuchenaiCrypts::TankX, AuchenaiCrypts::TankY, AuchenaiCrypts::TankZ);
    float moveX;
    float moveY;
    bool backwards;
    if (!EncounterHelpers::GetStepToPosition(bot, spot, AuchenaiCrypts::TankArrivalDistance, shirrak, moveX, moveY,
                                             backwards))
        return false;

    return MoveTo(bot->GetMapId(), moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, backwards);
}

bool ShirrakFleeFocusFireAction::Execute(Event /*event*/)
{
    Creature* flare = NearestFlare(bot);
    if (!flare)
        return false;

    float const step = AuchenaiCrypts::FocusFireStep(bot->GetExactDist2d(flare));
    if (step <= 0.0f)
        return false;

    bot->CastStop();
    return MoveAway(flare, step);
}

// Only while within spell range: past it the bot's own reach moves bring it back to the edge
bool ShirrakRangedKeepDistanceAction::Execute(Event /*event*/)
{
    Unit* shirrak = AI_VALUE2(Unit*, "find target", "shirrak the dead watcher");
    if (!shirrak)
        return false;

    if (bot->GetExactDist(shirrak) - shirrak->GetCombatReach() > sPlayerbotAIConfig.SpellDistance)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<Player*> rangedMembers;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && PlayerbotAI::IsRanged(member))
            rangedMembers.push_back(member);
    }

    auto found = std::find(rangedMembers.begin(), rangedMembers.end(), bot);
    std::size_t const index = found != rangedMembers.end() ? std::distance(rangedMembers.begin(), found) : 0;
    float const angle = AuchenaiCrypts::RangedSpotAngle(index, rangedMembers.size());

    float const spotX = AuchenaiCrypts::RangedX + std::cos(angle) * AuchenaiCrypts::RangedSpreadRadius;
    float const spotY = AuchenaiCrypts::RangedY + std::sin(angle) * AuchenaiCrypts::RangedSpreadRadius;
    float const distToSpot = bot->GetExactDist2d(spotX, spotY);
    if (!AuchenaiCrypts::MovesToRangedSpot(distToSpot))
        return false;

    float const step = AuchenaiCrypts::RangedStep(distToSpot);
    float const moveX = bot->GetPositionX() + (spotX - bot->GetPositionX()) / distToSpot * step;
    float const moveY = bot->GetPositionY() + (spotY - bot->GetPositionY()) / distToSpot * step;
    return MoveTo(bot->GetMapId(), moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, false);
}

float ShirrakFleeFocusFireMultiplier::GetValue(Action* action)
{
    AuchenaiCrypts::ShirrakAction shape;
    shape.attack = dynamic_cast<AttackAction*>(action) != nullptr;
    shape.fleeFocusFire = dynamic_cast<ShirrakFleeFocusFireAction*>(action) != nullptr;
    shape.movementSpell = dynamic_cast<CastReachTargetSpellAction*>(action) ||
                          dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action);
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    bool const restricted = !shape.attack && !shape.fleeFocusFire && (shape.movementSpell || shape.movement);
    Creature* flare =
        restricted && AI_VALUE2(Unit*, "find target", "shirrak the dead watcher") ? NearestFlare(bot) : nullptr;

    return AuchenaiCrypts::ShirrakAllows(shape, flare != nullptr, flare ? bot->GetExactDist2d(flare) : 0.0f) ? 1.0f
                                                                                                             : 0.0f;
}
