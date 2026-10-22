/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The planner every bot move goes through: walk, take a jump the leader made, or jump a gap the navmesh cannot cross.

#ifndef _PLAYERBOTS_MOVEMENT_PLANNER_H
#define _PLAYERBOTS_MOVEMENT_PLANNER_H

#include "Define.h"
#include "FallPolicy.h"
#include "GapPolicy.h"
#include "JumpPhysics.h"
#include "JumpTrail.h"
#include "ProgressPolicy.h"
#include "RoutePolicy.h"

class Player;
class PlayerbotAI;

struct MovePlanState
{
    BotMove::Progress progress;

    bool jumpPlanned = false;
    BotMove::Point takeoff;
    BotMove::Point landing;
    BotMove::Point plannedGoal;
    uint32 plannedMs = 0;

    bool walkCached = false;
    bool cachedAfterSearch = false;
    BotMove::Point cachedGoal;
    uint32 cachedMs = 0;

    bool airborne = false;
    uint32 jumpMapId = 0;
    BotMove::Point jumpFrom;
    BotMove::Point jumpTo;
    float jumpAngle = 0.0f;
    float jumpSpeedXY = 0.0f;
    uint32 jumpStartMs = 0;
    uint32 jumpFlightMs = 0;
};

enum class MovePlanKind
{
    Walk,
    WalkToTakeoff,
    Jump
};

struct MovePlan
{
    MovePlanKind kind = MovePlanKind::Walk;
    BotMove::Point point;
};

class MovementPlanner
{
public:
    static bool IsActiveFor(Player* player);

    // The next leg toward the goal; Walk leaves the move as asked. Calling it again before the jump keeps the plan.
    static MovePlan Plan(PlayerbotAI* botAI, float x, float y, float z);

    // Jumps the way a client does: other players' clients play the jump from the packet.
    static bool Jump(PlayerbotAI* botAI, MovePlan const& plan);

    // Moves an airborne bot along its arc and lands it; one bool test when the bot is not jumping.
    static void UpdateAirborne(PlayerbotAI* botAI);

    // Hands a jump or drop to the bots in the jumper's group on the same map.
    static void ShareJump(Player* jumper, BotMove::Point takeoff, BotMove::Point landing, bool fatal);

    // How long the bot has asked to move without getting anywhere.
    static uint32 StuckFor(PlayerbotAI* botAI);

    static void Reset(PlayerbotAI* botAI);
};

#endif
