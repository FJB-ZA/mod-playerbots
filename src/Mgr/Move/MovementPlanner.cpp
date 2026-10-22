/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Movement planner glue: measures navmesh paths, hands the numbers to the movement policies, and jumps like a client.

#include "MovementPlanner.h"

#include "Group.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "Opcodes.h"
#include "PathGenerator.h"
#include "Playerbots.h"
#include "World.h"
#include "WorldPacket.h"

namespace
{
constexpr std::size_t MaxLinksTried = 2;

struct Measured
{
    bool usable = false;
    bool noMesh = false;
    bool startsOnMesh = false;
    BotMove::Point end;
    BotMove::PathShape shape;
};

BotMove::Point PointOf(WorldObject const* object)
{
    return {object->GetPositionX(), object->GetPositionY(), object->GetPositionZ()};
}

Measured Measure(Player* bot, BotMove::Point from, BotMove::Point to)
{
    Measured result;
    PathGenerator path(bot);
    path.CalculatePath(from.x, from.y, from.z, to.x, to.y, to.z, false);
    PathType type = path.GetPathType();
    result.noMesh = type & PATHFIND_NOT_USING_PATH;
    result.usable = !(type & PATHFIND_NOPATH) && !path.GetPath().empty();
    if (!result.usable)
        return result;

    G3D::Vector3 const& start = path.GetPath().front();
    result.startsOnMesh = BotMove::Distance({start.x, start.y, start.z}, from) <= BotMove::ArriveSlack;
    G3D::Vector3 const& end = path.GetPath().back();
    result.end = {end.x, end.y, end.z};
    result.shape.endToGoal = BotMove::Distance(result.end, to);
    result.shape.reachable = !(type & PATHFIND_INCOMPLETE) && result.shape.endToGoal <= BotMove::ArriveSlack;
    result.shape.length = path.getPathLength();
    result.shape.straight = BotMove::Distance(from, to);
    return result;
}

bool ClearArc(Player* bot, BotMove::Point takeoff, BotMove::Point landing)
{
    float head = bot->GetCollisionHeight();
    BotMove::Point top{(takeoff.x + landing.x) / 2.0f, (takeoff.y + landing.y) / 2.0f,
                       std::max(takeoff.z, landing.z) + BotMove::JumpApex};
    Map* map = bot->GetMap();
    uint32 phase = bot->GetPhaseMask();
    return map->isInLineOfSight(takeoff.x, takeoff.y, takeoff.z + head, top.x, top.y, top.z + head, phase,
                                LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing) &&
           map->isInLineOfSight(top.x, top.y, top.z + head, landing.x, landing.y, landing.z + head, phase,
                                LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing);
}

bool TakesDrop(PlayerbotAI* botAI, BotMove::Point takeoff, BotMove::Point landing, bool fatalForLeader)
{
    Player* bot = botAI->GetBot();
    Player* leader = botAI->GetMaster() ? botAI->GetMaster() : botAI->GetGroupLeader();

    BotMove::DropMoment drop;
    drop.height = takeoff.z - landing.z;
    drop.safeFall = static_cast<float>(bot->GetTotalAuraModifier(SPELL_AURA_SAFE_FALL));
    drop.featherFall = bot->HasFeatherFallAura() || bot->HasHoverAura();
    drop.healthPct = bot->GetHealthPct();
    drop.fatalForLeader = fatalForLeader;
    drop.wiping = BotMove::IsWipe(leader && leader != bot && !leader->IsAlive(), bot->IsInCombat());
    drop.rate = sWorld->getRate(RATE_DAMAGE_FALL);
    return BotMove::TakesDrop(drop, sPlayerbotAIConfig.MovementPlannerMaxFallDamage);
}

// The bot itself must be able to make the jump: links also record knockbacks, blinks and teleports.
bool CanMakeJump(Player* bot, BotMove::Point takeoff, BotMove::Point landing)
{
    BotMove::GapKind gap =
        BotMove::ClassifyGap(BotMove::Distance2d(takeoff, landing), landing.z - takeoff.z, bot->GetSpeed(MOVE_RUN));
    return BotMove::CanCross(gap) && ClearArc(bot, takeoff, landing);
}

void CacheWalk(MovePlanState& state, BotMove::Point goal, bool searched, uint32 now)
{
    state.walkCached = true;
    state.cachedAfterSearch = searched;
    state.cachedGoal = goal;
    state.cachedMs = now;
}

void PlanJump(MovePlanState& state, BotMove::Point takeoff, BotMove::Point landing, BotMove::Point goal, uint32 now)
{
    state.jumpPlanned = true;
    state.takeoff = takeoff;
    state.landing = landing;
    state.plannedGoal = goal;
    state.plannedMs = now;
    state.walkCached = false;
}

// A jump the leader made that gets the bot to the goal much faster than walking.
bool TakeLearnedJump(PlayerbotAI* botAI, BotMove::Point here, BotMove::Point goal, BotMove::PathShape walk, uint32 now)
{
    Player* bot = botAI->GetBot();
    BotMove::JumpLink links[MaxLinksTried];
    std::size_t count = botAI->jumpTrail.Fresh(bot->GetMapId(), now, links, MaxLinksTried);
    if (!count)
        return false;

    BotMove::RouteCost costs[MaxLinksTried];
    for (std::size_t i = 0; i < count; ++i)
    {
        BotMove::JumpLink const& link = links[i];
        float straightLine = BotMove::Distance(here, link.takeoff) + BotMove::Distance(link.takeoff, link.landing) +
                             BotMove::Distance(link.landing, goal);
        if (BotMove::Distance(here, link.takeoff) > BotMove::LinkSearchRange ||
            BotMove::Distance(goal, link.landing) > BotMove::LinkSearchRange ||
            !BotMove::RouteCouldBeat(straightLine, walk) || !TakesDrop(botAI, link.takeoff, link.landing, link.fatal) ||
            !CanMakeJump(bot, link.takeoff, link.landing))
            continue;

        Measured toTakeoff = Measure(bot, here, link.takeoff);
        Measured fromLanding = Measure(bot, link.landing, goal);
        if (!toTakeoff.usable || !fromLanding.usable || BotMove::StopsShort(toTakeoff.shape) ||
            BotMove::StopsShort(fromLanding.shape))
            continue;

        costs[i].usable = true;
        costs[i].cost =
            toTakeoff.shape.length + BotMove::Distance(link.takeoff, link.landing) + fromLanding.shape.length;
    }

    int chosen = BotMove::ChooseRoute(walk, costs, count);
    if (chosen < 0)
        return false;

    PlanJump(botAI->movePlan, links[chosen].takeoff, links[chosen].landing, goal, now);
    return true;
}

// The closest pair of points across the split in the navmesh, if a jump or drop the bot survives can cross it.
bool FindGapJump(PlayerbotAI* botAI, BotMove::Point here, BotMove::Point goal, Measured const& walk, uint32 now)
{
    if (!walk.usable)
        return false;

    Player* bot = botAI->GetBot();
    BotMove::Point takeoff = walk.end;
    // A goal off the navmesh (a gameobject bridge, a crate) would snap to the floor below it
    Measured back = Measure(bot, goal, takeoff);
    if (!back.usable || !back.startsOnMesh || back.shape.reachable)
        return false;
    BotMove::Point landing = back.end;

    // One refinement pass pulls both ends toward the narrowest point of the gap.
    Measured outAgain = Measure(bot, here, landing);
    if (outAgain.usable)
    {
        Measured backAgain = Measure(bot, goal, outAgain.end);
        if (backAgain.usable && BotMove::Distance(outAgain.end, backAgain.end) < BotMove::Distance(takeoff, landing))
        {
            takeoff = outAgain.end;
            landing = backAgain.end;
        }
    }

    BotMove::GapKind gap =
        BotMove::ClassifyGap(BotMove::Distance2d(takeoff, landing), landing.z - takeoff.z, bot->GetSpeed(MOVE_RUN));
    if (gap == BotMove::GapKind::Seam || !BotMove::CanCross(gap))
        return false;
    if (gap == BotMove::GapKind::Drop && !TakesDrop(botAI, takeoff, landing, false))
        return false;
    if (!ClearArc(bot, takeoff, landing))
        return false;

    PlanJump(botAI->movePlan, takeoff, landing, goal, now);
    return true;
}

MovePlan StepFor(MovePlanState const& state, BotMove::Point here)
{
    MovePlan plan;
    // The take-off can sit right above or below the bot on another floor.
    constexpr float TakeoffHeightSlack = 3.0f;
    bool atTakeoff = BotMove::NextJumpStep(BotMove::Distance2d(here, state.takeoff)) == BotMove::JumpStep::Jump &&
                     std::fabs(here.z - state.takeoff.z) <= TakeoffHeightSlack;
    plan.kind = atTakeoff ? MovePlanKind::Jump : MovePlanKind::WalkToTakeoff;
    plan.point = atTakeoff ? state.landing : state.takeoff;
    return plan;
}

void SendMovement(Player* bot, uint16 opcode)
{
    WorldPacket data(opcode, 64);
    data << bot->GetPackGUID();
    bot->BuildMovementPacket(&data);
    bot->SendMessageToSet(&data, false);
}

void ClearJumpFlags(Player* bot, MovePlanState& state)
{
    state.airborne = false;
    bot->RemoveUnitMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR | MOVEMENTFLAG_FORWARD);
    bot->m_movementInfo.SetFallTime(state.jumpFlightMs);
    bot->m_movementInfo.jump.Reset();
}

void Land(Player* bot, MovePlanState& state)
{
    bot->UpdatePosition(state.jumpTo.x, state.jumpTo.y, state.jumpTo.z, bot->GetOrientation());
    ClearJumpFlags(bot, state);
    SendMovement(bot, MSG_MOVE_FALL_LAND);

    // Fall damage the way a client's landing deals it
    MovementInfo landing = bot->m_movementInfo;
    landing.pos.Relocate(state.jumpTo.x, state.jumpTo.y, state.jumpTo.z);
    bot->HandleFall(landing);
    bot->SetFallInformation(0, state.jumpTo.z);
}
}  // namespace

bool MovementPlanner::IsActiveFor(Player* player)
{
    switch (sPlayerbotAIConfig.MovementPlanner)
    {
        case 1:
            return player->GetMap() && player->GetMap()->IsDungeon();
        case 2:
            return true;
        default:
            return false;
    }
}

MovePlan MovementPlanner::Plan(PlayerbotAI* botAI, float x, float y, float z)
{
    MovePlan walkOn;
    Player* bot = botAI->GetBot();
    MovePlanState& state = botAI->movePlan;
    if (state.airborne || !IsActiveFor(bot) || bot->GetVehicle() || bot->GetTransport() || bot->IsFlying() ||
        bot->isSwimming())
        return walkOn;

    uint32 now = getMSTime();
    BotMove::Point here = PointOf(bot);
    BotMove::Point goal{x, y, z};
    BotMove::TrackProgress(state.progress, here.x, here.y, here.z, now);

    // Checked first, so a bot summoned or knocked next to its goal drops an old jump plan.
    if (BotMove::IsNearMove(BotMove::Distance(here, goal), goal.z - here.z))
    {
        state.jumpPlanned = false;
        return walkOn;
    }

    if (state.jumpPlanned)
    {
        if (BotMove::PlanStillValid(BotMove::Distance(goal, state.plannedGoal), getMSTimeDiff(state.plannedMs, now)))
            return StepFor(state, here);
        state.jumpPlanned = false;
    }

    if (state.walkCached && BotMove::CacheStillValid(BotMove::Distance(goal, state.cachedGoal),
                                                     getMSTimeDiff(state.cachedMs, now), state.cachedAfterSearch))
        return walkOn;

    Measured walk = Measure(bot, here, goal);
    bool searched = !walk.noMesh && BotMove::LooksForJump(walk.shape);
    if (searched && (TakeLearnedJump(botAI, here, goal, walk.shape, now) ||
                     (BotMove::StopsShort(walk.shape) && FindGapJump(botAI, here, goal, walk, now))))
        return StepFor(state, here);

    CacheWalk(state, goal, searched, now);
    return walkOn;
}

bool MovementPlanner::Jump(PlayerbotAI* botAI, MovePlan const& plan)
{
    Player* bot = botAI->GetBot();
    MovePlanState& state = botAI->movePlan;
    if (plan.kind != MovePlanKind::Jump)
        return false;

    state.jumpPlanned = false;
    BotMove::Point takeoff = PointOf(bot);
    float horizontal = BotMove::Distance2d(takeoff, plan.point);
    BotMove::ClientJump jump = BotMove::ClientJumpFor(horizontal, plan.point.z - takeoff.z);
    // The bot can reach the take-off lower than planned, leaving the landing above its jump apex
    if (jump.seconds <= 0.0f)
    {
        CacheWalk(state, state.plannedGoal, true, getMSTime());
        return false;
    }
    state.walkCached = false;

    float angle = std::atan2(plan.point.y - takeoff.y, plan.point.x - takeoff.x);
    bot->StopMoving();
    bot->GetMotionMaster()->Clear();
    bot->SetOrientation(angle);
    bot->SetFallInformation(0, takeoff.z);
    bot->m_movementInfo.SetFallTime(0);
    // A client sends its upward launch speed negated
    bot->m_movementInfo.jump.zspeed = -jump.zspeed;
    bot->m_movementInfo.jump.sinAngle = std::sin(angle);
    bot->m_movementInfo.jump.cosAngle = std::cos(angle);
    bot->m_movementInfo.jump.xyspeed = jump.xyspeed;
    bot->AddUnitMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FORWARD);
    SendMovement(bot, MSG_MOVE_JUMP);

    state.airborne = true;
    state.jumpMapId = bot->GetMapId();
    state.jumpFrom = takeoff;
    state.jumpTo = plan.point;
    state.jumpAngle = angle;
    state.jumpSpeedXY = jump.xyspeed;
    state.jumpStartMs = getMSTime();
    state.jumpFlightMs = static_cast<uint32>(jump.seconds * IN_MILLISECONDS);

    botAI->GetAiObjectContext()
        ->GetValue<LastMovement&>("last movement")
        ->Get()
        .Set(bot->GetMapId(), plan.point.x, plan.point.y, plan.point.z, angle, state.jumpFlightMs,
             MovementPriority::MOVEMENT_FORCED);
    ShareJump(bot, takeoff, plan.point, false);
    return true;
}

void MovementPlanner::UpdateAirborne(PlayerbotAI* botAI)
{
    MovePlanState& state = botAI->movePlan;
    if (!state.airborne)
        return;

    Player* bot = botAI->GetBot();
    // A teleport, knockback, grip or vehicle mid-air ends the jump where the bot now is
    constexpr float TeleportSlack = 10.0f;
    float arcLength = BotMove::Distance(state.jumpFrom, state.jumpTo) + BotMove::JumpApex;
    if (bot->GetMapId() != state.jumpMapId || !bot->movespline->Finalized() || bot->GetVehicle() ||
        BotMove::Distance(PointOf(bot), state.jumpFrom) > arcLength + TeleportSlack)
    {
        ClearJumpFlags(bot, state);
        return;
    }

    uint32 elapsed = getMSTimeDiff(state.jumpStartMs, getMSTime());
    if (!bot->IsAlive() || elapsed >= state.jumpFlightMs)
    {
        Land(bot, state);
        return;
    }

    // The server follows the same arc the clients draw, so range and line-of-sight checks stay true mid-air
    float seconds = elapsed / static_cast<float>(IN_MILLISECONDS);
    float travelled = state.jumpSpeedXY * seconds;
    bot->UpdatePosition(state.jumpFrom.x + std::cos(state.jumpAngle) * travelled,
                        state.jumpFrom.y + std::sin(state.jumpAngle) * travelled,
                        state.jumpFrom.z + BotMove::JumpHeightAt(seconds), state.jumpAngle);
}

void MovementPlanner::ShareJump(Player* jumper, BotMove::Point takeoff, BotMove::Point landing, bool fatal)
{
    if (!BotMove::WorthRemembering(BotMove::Distance2d(takeoff, landing), takeoff.z - landing.z))
        return;

    Group* group = jumper->GetGroup();
    if (!group)
        return;

    // Zero marks an empty trail slot.
    BotMove::JumpLink link{jumper->GetMapId(), takeoff, landing, std::max(getMSTime(), 1u), fatal};
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        // Same map, so the same map thread owns both players.
        if (!member || member == jumper || member->GetMap() != jumper->GetMap())
            continue;

        if (PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member))
            memberAI->jumpTrail.Add(link);
    }
}

uint32 MovementPlanner::StuckFor(PlayerbotAI* botAI)
{
    return BotMove::StuckFor(botAI->movePlan.progress, getMSTime());
}

void MovementPlanner::Reset(PlayerbotAI* botAI)
{
    if (botAI->movePlan.airborne)
        ClearJumpFlags(botAI->GetBot(), botAI->movePlan);
    botAI->movePlan = MovePlanState{};
}
