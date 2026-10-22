/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Nethermancer Sepethrea bot tactics: kite a fixating Raging Flames, keep clear of the others and their fire, burn her.

#include <cmath>
#include <optional>
#include <vector>

#include "ChooseTargetActions.h"
#include "Creature.h"
#include "GenericSpellActions.h"
#include "Group.h"
#include "MechActions.h"
#include "MechMultipliers.h"
#include "MechPolicy.h"
#include "MechShared.h"
#include "MechTriggers.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "ReachTargetActions.h"
#include "Timer.h"

namespace
{
constexpr float GroupScanRange = 60.0f;
constexpr float FlameFallbackScan = 100.0f;
constexpr float KiteTrailScan = 60.0f;
constexpr uint32 KiteTurnFailsafeMs = 12000;

bool CountsAsGroupMember(Player* bot, Player* member)
{
    return member && member != bot && member->IsAlive() && member->GetMapId() == bot->GetMapId();
}

bool PartyCentroid(Player* bot, Mechanar::Point& centroid)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    float sumX = 0.0f;
    float sumY = 0.0f;
    int count = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!CountsAsGroupMember(bot, member) || bot->GetExactDist2d(member) > GroupScanRange)
            continue;
        sumX += member->GetPositionX();
        sumY += member->GetPositionY();
        ++count;
    }
    if (count == 0)
        return false;
    centroid = {sumX / count, sumY / count};
    return true;
}

bool NearestTankPosition(Player* bot, Mechanar::Point& tank)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    float best = 1e18f;
    bool found = false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!CountsAsGroupMember(bot, member) || !PlayerbotAI::IsTank(member))
            continue;
        float const d = bot->GetExactDist2d(member);
        if (d < best)
        {
            best = d;
            tank = MechanarFlames::PositionOf(member);
            found = true;
        }
    }
    return found;
}

bool Fixated(Player* bot, Unit* boss) { return boss && MechanarFlames::GetFixatingFlame(bot); }
}  // namespace

bool SepethreaKiteFlameTrigger::IsActive()
{
    Unit* boss = MechanarFlames::GetSepethrea(bot);
    bool const fixated = Fixated(bot, boss);
    return Mechanar::KitesFlame(boss != nullptr, fixated, fixated && MechanarFlames::TankMustGrabBoss(bot));
}

bool SepethreaAvoidFlameTrigger::IsActive()
{
    Unit* boss = MechanarFlames::GetSepethrea(bot);
    bool const fixated = Fixated(bot, boss);
    bool const flameNear = boss && !fixated && MechanarFlames::IsFlameNearCached(bot);
    return Mechanar::AvoidsFlame(boss != nullptr, fixated, flameNear,
                                 flameNear && MechanarFlames::HealerHoldsFire(bot));
}

bool SepethreaTrailTrigger::IsActive()
{
    Unit* boss = MechanarFlames::GetSepethrea(bot);
    bool const trailDanger = boss && MechanarFlames::InTrailDangerCached(bot);
    return Mechanar::AvoidsTrail(boss != nullptr, trailDanger, trailDanger && MechanarFlames::HealerHoldsFire(bot));
}

bool SepethreaFocusBossTrigger::IsActive() { return MechanarFlames::GetSepethrea(bot) != nullptr; }

bool SepethreaKiteFlameAction::Execute(Event /*event*/)
{
    std::vector<Creature*> chasers;
    MechanarFlames::CollectChasingFlames(bot, chasers);
    if (chasers.empty())
        return false;

    Mechanar::Point const botPos = MechanarFlames::PositionOf(bot);

    Creature* flame = chasers.front();
    float dist = bot->GetDistance2d(flame);
    std::vector<Mechanar::Point> chaserPos;
    chaserPos.reserve(chasers.size());
    for (Creature* chaser : chasers)
    {
        chaserPos.push_back(MechanarFlames::PositionOf(chaser));
        float const d = bot->GetDistance2d(chaser);
        if (d < dist)
        {
            dist = d;
            flame = chaser;
        }
    }
    Mechanar::Point const flamePos = MechanarFlames::PositionOf(flame);
    Mechanar::Point flee = Mechanar::FleeDirection(botPos, flamePos);

    std::vector<Mechanar::Point> avoidFlames;
    MechanarFlames::CollectAvoidFlames(bot, avoidFlames);
    std::vector<Mechanar::TrailPatch> patches;
    MechanarFlames::CollectTrailPatches(bot, KiteTrailScan, patches);

    Mechanar::Point tank;
    bool const haveTank = NearestTankPosition(bot, tank);
    bool const leashed = PlayerbotAI::IsHeal(bot) && haveTank;
    float const tankDist = leashed ? Mechanar::Dist2d(botPos, tank) : 0.0f;

    Mechanar::Point group = tank;
    bool const haveGroup = haveTank || PartyCentroid(bot, group);
    float const groupDist = haveGroup ? Mechanar::Dist2d(botPos, group) : 1000.0f;

    uint32 const nowMs = getMSTime();
    bool turning = _turnUntilMs != 0 && nowMs < _turnUntilMs;
    if (turning && (leashed || flame->GetGUID() != _turnFlame))
        turning = false;
    if (turning && Mechanar::TurnPassedFlame(_turnLateral, dist, botPos.y, flamePos.y, _turnEnd))
        turning = false;
    if (!turning)
        _turnUntilMs = 0;

    float const endSign = Mechanar::TurnEndSign(flee.y);
    if (!turning && Mechanar::StartsTurn(leashed, flee.y, dist, Mechanar::DistanceToRoomEnd(botPos.y, endSign)))
    {
        Mechanar::TurnChoice const choice = Mechanar::ChooseTurnSide(avoidFlames, patches, botPos, flamePos, endSign);
        if (choice.laneClear > 0.0f)
        {
            _turnSide = choice.side;
            _turnEnd = endSign;
            _turnFlame = flame->GetGUID();
            _turnLateral = true;
            _turnUntilMs = nowMs + KiteTurnFailsafeMs;
            turning = true;
        }
    }
    if (turning)
    {
        float const laneX = Mechanar::TurnLaneX(_turnSide);
        if (_turnLateral && std::fabs(botPos.x - laneX) <= Mechanar::KiteTurnWallReached)
            _turnLateral = false;

        float const laneEndY = Mechanar::TurnLaneEndY(flamePos.y, _turnEnd);
        if (Mechanar::TurnLaneClearance(avoidFlames, patches, botPos, _turnSide, laneEndY, _turnLateral) <
            Mechanar::KiteTurnAbortClear)
            _turnUntilMs = 0;
        else
            return MoveTo(bot->GetMapId(), laneX, _turnLateral ? botPos.y : laneEndY, bot->GetPositionZ(), false, false,
                          false, false, MovementPriority::MOVEMENT_COMBAT, true, false);
    }

    if (Mechanar::KiterHoldsPosition(dist, groupDist, leashed, tankDist,
                                     Mechanar::SpotSafe(avoidFlames, patches, botPos)))
        return false;

    if (haveGroup)
        flee = Mechanar::BiasAwayFromGroup(flee, botPos, group, groupDist);

    if (std::optional<Mechanar::Point> back = Mechanar::ReturnIntoRoom(botPos))
        return MoveTo(bot->GetMapId(), back->x, back->y, bot->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT, true, false);

    Mechanar::KiteSearch search;
    search.bot = botPos;
    search.baseAngle = std::atan2(flee.y, flee.x);
    search.leashed = leashed;
    search.tank = tank;
    search.tankDist = tankDist;
    std::optional<Mechanar::Point> step = Mechanar::ChooseKiteStep(search, avoidFlames, patches, chaserPos);
    if (!step)
        return false;

    return MoveTo(bot->GetMapId(), step->x, step->y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool SepethreaAvoidFlameAction::Execute(Event /*event*/)
{
    std::vector<Mechanar::Point> avoidFlames;
    MechanarFlames::CollectAvoidFlames(bot, avoidFlames);
    std::vector<Mechanar::TrailPatch> patches;
    MechanarFlames::CollectTrailPatches(bot, Mechanar::TrailScan + Mechanar::InfernoAvoidClear, patches);

    std::optional<Mechanar::Point> spot =
        Mechanar::FindFlameClearSpot(MechanarFlames::PositionOf(bot), avoidFlames, patches);
    if (!spot)
        return false;

    MovementPriority const priority =
        PlayerbotAI::IsHeal(bot) ? MovementPriority::MOVEMENT_COMBAT : MovementPriority::MOVEMENT_FORCED;
    return MoveTo(bot->GetMapId(), spot->x, spot->y, bot->GetPositionZ(), false, false, false, false, priority, true,
                  false);
}

bool SepethreaAvoidTrailAction::Execute(Event /*event*/)
{
    std::vector<Mechanar::TrailPatch> patches;
    MechanarFlames::CollectTrailPatches(bot, Mechanar::TrailScan, patches);
    if (patches.empty())
        return false;

    Mechanar::Point const botPos = MechanarFlames::PositionOf(bot);
    std::optional<Mechanar::Point> push = Mechanar::TrailPush(botPos, patches);
    if (!push)
        return false;

    std::optional<Mechanar::Point> nearestFlame;
    if (!Mechanar::HasDirection(*push))
        if (Unit* flame = MechanarFlames::GetNearestFlame(bot, FlameFallbackScan))
            nearestFlame = MechanarFlames::PositionOf(flame);

    Mechanar::Point const dest = Mechanar::TrailEscapeSpot(botPos, *push, nearestFlame);
    MovementPriority const priority =
        PlayerbotAI::IsHeal(bot) ? MovementPriority::MOVEMENT_COMBAT : MovementPriority::MOVEMENT_FORCED;
    return MoveTo(bot->GetMapId(), dest.x, dest.y, bot->GetPositionZ(), false, false, false, false, priority, true,
                  false);
}

bool SepethreaFocusBossAction::Execute(Event /*event*/)
{
    Unit* boss = MechanarFlames::GetSepethrea(bot);
    if (!boss)
        return false;

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    return false;
}

float SepethreaKiteFlameMultiplier::GetValue(Action* action)
{
    Mechanar::SepethreaAction shape;
    shape.flameMove =
        dynamic_cast<SepethreaKiteFlameAction*>(action) || dynamic_cast<SepethreaAvoidTrailAction*>(action) ||
        dynamic_cast<SepethreaAvoidFlameAction*>(action) || dynamic_cast<SepethreaFocusBossAction*>(action);
    shape.mover = dynamic_cast<MovementAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action);

    Mechanar::SepethreaMoment fight;
    if (shape.mover && !shape.flameMove)
    {
        fight.kiting = MechanarFlames::GetFixatingFlame(bot) && !MechanarFlames::TankMustGrabBoss(bot);
        if (!fight.kiting)
            fight.healerHoldsFire = MechanarFlames::HealerHoldsFire(bot);
        if (!fight.kiting && !fight.healerHoldsFire)
            fight.hazardNear = MechanarFlames::InTrailDangerCached(bot) || MechanarFlames::IsFlameNearCached(bot);
    }

    return Mechanar::SepethreaAllows(fight, shape) ? 1.0f : 0.0f;
}

float SepethreaFocusBossMultiplier::GetValue(Action* action)
{
    bool const leavesBoss =
        Mechanar::LeavesSepethrea(dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action),
                                  dynamic_cast<CastDebuffSpellOnAttackerAction*>(action) != nullptr,
                                  action->getThreatType() == Action::ActionThreatType::Aoe, PlayerbotAI::IsDps(bot),
                                  dynamic_cast<CastHealingSpellAction*>(action) != nullptr);
    bool const bossUp = leavesBoss && MechanarFlames::GetSepethrea(bot);

    return Mechanar::SepethreaFocusAllows(bossUp, leavesBoss) ? 1.0f : 0.0f;
}
