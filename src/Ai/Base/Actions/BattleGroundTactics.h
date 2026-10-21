/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_BATTLEGROUNDTACTICS_H
#define PLAYERBOTS_BATTLEGROUNDTACTICS_H

#include "BattlegroundAV.h"
#include "MovementActions.h"
#include "PositionValue.h"

#include <mutex>

class ChatHandler;
class Battleground;
class PlayerbotAI;
struct Position;

#define SPELL_CAPTURE_BANNER 21651

enum WSBotStrategy : uint8
{
    WS_STRATEGY_BALANCED      = 0,
    WS_STRATEGY_OFFENSIVE     = 1,
    WS_STRATEGY_DEFENSIVE     = 2,
    WS_STRATEGY_MAX           = 3,
};

enum ABBotStrategy : uint8
{
    AB_STRATEGY_BALANCED      = 0,
    AB_STRATEGY_OFFENSIVE     = 1,
    AB_STRATEGY_DEFENSIVE     = 2,
    AB_STRATEGY_MAX           = 3,
};

enum AVBotStrategy : uint8
{
    AV_STRATEGY_BALANCED      = 0,
    AV_STRATEGY_OFFENSIVE     = 1,
    AV_STRATEGY_DEFENSIVE     = 2,
    AV_STRATEGY_MAX           = 3,
};

enum EYBotStrategy : uint8
{
    EY_STRATEGY_BALANCED      = 0,
    EY_STRATEGY_FRONT_FOCUS   = 1,
    EY_STRATEGY_BACK_FOCUS    = 2,
    EY_STRATEGY_FLAG_FOCUS    = 3,
    EY_STRATEGY_MAX           = 4
};

// Strand of the Ancients attack plans; the defense reads the same roll as how far forward it holds
enum SABotStrategy : uint8
{
    SA_STRATEGY_SINGLE_PUSH   = 0,  // the whole team down one lane
    SA_STRATEGY_MAIN_FEINT    = 1,  // one lane, a few bots pull defenders to the other
    SA_STRATEGY_SPLIT         = 2,  // both lanes at once
    SA_STRATEGY_MAX           = 3,
};

typedef void (*BattleBotWaypointFunc)();

// How well a team plays its battleground, rolled per team at gate open
enum BGTacticsTier : uint8
{
    BG_TIER_BASIC       = 0,
    BG_TIER_COORDINATED = 1,
    BG_TIER_PERFECT     = 2,
};

struct BGStrategyData
{
    uint8 allianceStrategy = 0;
    uint8 hordeStrategy = 0;
    uint8 allianceTier = BG_TIER_BASIC;
    uint8 hordeTier = BG_TIER_BASIC;
};

// Every battleground map thread reads and writes this map: hold bgStrategiesLock
extern std::unordered_map<uint32, BGStrategyData> bgStrategies;
extern std::mutex bgStrategiesLock;

struct BattleBotWaypoint
{
    BattleBotWaypoint(float x_, float y_, float z_, BattleBotWaypointFunc func) : x(x_), y(y_), z(z_), pFunc(func){};

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    BattleBotWaypointFunc pFunc = nullptr;
};

typedef std::vector<BattleBotWaypoint> BattleBotPath;

// Per battleground data, defined in src/Ai/Battleground/<Name>Tactics.cpp
extern std::vector<BattleBotPath*> const vPaths_WS;
extern std::vector<BattleBotPath*> const vPaths_AB;
extern std::vector<BattleBotPath*> const vPaths_AV;
extern std::vector<BattleBotPath*> const vPaths_EY;
extern std::vector<BattleBotPath*> const vPaths_IC;
extern std::vector<BattleBotPath*> const vPaths_SA;
// paths that are only walked start to end (jumps, drops)
extern std::vector<BattleBotPath*> const vPaths_OneWay_WS;
extern std::vector<BattleBotPath*> const vPaths_OneWay_IC;

extern std::vector<uint32> const vFlagsAV;
extern std::vector<uint32> const vFlagsAB;
extern std::vector<uint32> const vFlagsWS;
extern std::vector<uint32> const vFlagsEY;
extern std::vector<uint32> const vFlagsIC;
extern std::vector<uint32> const vFlagsSA;

class BGTactics : public MovementAction
{
public:
    static bool HandleConsoleCommand(ChatHandler* handler, char const* args);
    uint8 static GetBotStrategyForTeam(Battleground* bg, TeamId teamId);
    uint8 static GetTeamTier(Battleground* bg, TeamId teamId);
    static void RollTiers(Battleground* bg, BGStrategyData& data);

    BGTactics(PlayerbotAI* botAI, std::string const name = "bg tactics") : MovementAction(botAI, name) {}

    bool Execute(Event event) override;

private:
    static std::string const HandleConsoleCommandPrivate(WorldSession* session, char const* args);
    bool moveToStart(bool force = false);
    bool selectObjective(bool reset = false);
    bool moveToObjective(bool ignoreDist);
    bool selectObjectiveWp(std::vector<BattleBotPath*> const& vPaths);
    bool moveToObjectiveWp(BattleBotPath* const& currentPath, uint32 currentPoint, bool reverse = false);
    bool startNewPathBegin(std::vector<BattleBotPath*> const& vPaths);
    bool startNewPathFree(std::vector<BattleBotPath*> const& vPaths);
    bool resetObjective();
    bool atFlag(std::vector<BattleBotPath*> const& vPaths, std::vector<uint32> const& vFlagIds);
    bool protectFC();
    bool useBuff();
    uint32 getPlayersInArea(TeamId teamId, Position point, float range, bool combat = true);

    // Warsong Gulch
    void moveToStartWS(Battleground* bg);
    bool selectObjectiveWS(Battleground* bg, PositionMap& posMap, PositionInfo& pos);
    bool wsJumpDown();
    bool flagTaken();
    bool teamFlagTaken();

    // Arathi Basin
    void moveToStartAB(Battleground* bg);
    bool selectObjectiveAB(Battleground* bg, PositionMap& posMap, PositionInfo& pos);

    // Alterac Valley
    void moveToStartAV(Battleground* bg);
    bool selectObjectiveAV(Battleground* bg, PositionMap& posMap, PositionInfo& pos);
    bool avLeaveCave();

    // Eye of the Storm
    bool selectObjectiveEY(Battleground* bg, PositionMap& posMap, PositionInfo& pos);
    bool eyJumpDown();

    // Isle of Conquest
    void moveToStartIC(Battleground* bg);
    bool selectObjectiveIC(Battleground* bg, PositionMap& posMap, PositionInfo& pos);
    bool IsLockedInsideKeep();

    // Strand of the Ancients
    bool selectObjectiveSA(Battleground* bg, PositionMap& posMap, PositionInfo& pos);
    bool saLeaveBoat();
    bool saCanCapture(Battleground* bg, GameObject* banner);

    // Living battleground tiers, in src/Ai/Battleground/LivingBattlegroundTactics.cpp
    uint8 teamTier();
    bool coordinated();
    bool slips(uint32 basePercent);
};

class ArenaTactics : public MovementAction
{
public:
    ArenaTactics(PlayerbotAI* botAI, std::string const name = "arena tactics") : MovementAction(botAI, name) {}

    bool Execute(Event event) override;

private:
    bool moveToCenter(Battleground* bg);
};

#endif
