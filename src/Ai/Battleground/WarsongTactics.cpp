// Warsong Gulch bot tactics: positions, paths, start spots, objectives and the cliff jump.

#include "BattleGroundTactics.h"

#include <span>

#include "BattlegroundWS.h"
#include "GameObject.h"
#include "IVMapMgr.h"
#include "LastMovementValue.h"
#include "LivingBattlegroundPolicy.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "PvpValues.h"
#include "ServerFacade.h"
#include "TargetedMovementGenerator.h"

// common bg positions
Position const WS_WAITING_POS_HORDE_1 = {944.981f, 1423.478f, 345.434f, 6.18f};
Position const WS_WAITING_POS_HORDE_2 = {948.488f, 1459.834f, 343.066f, 6.27f};
Position const WS_WAITING_POS_HORDE_3 = {933.484f, 1433.726f, 345.535f, 0.08f};
Position const WS_WAITING_POS_ALLIANCE_1 = {1510.502f, 1493.385f, 351.995f, 3.1f};
Position const WS_WAITING_POS_ALLIANCE_2 = {1496.578f, 1457.900f, 344.442f, 3.1f};
Position const WS_WAITING_POS_ALLIANCE_3 = {1521.235f, 1480.951f, 352.007f, 3.2f};
Position const WS_FLAG_POS_HORDE = {915.958f, 1433.925f, 346.193f, 0.0f};
Position const WS_FLAG_POS_ALLIANCE = {1539.219f, 1481.747f, 352.458f, 0.0f};
Position const WS_FLAG_HIDE_HORDE_1 = {926.142f, 1460.641f, 346.116f, 4.84f};
Position const WS_FLAG_HIDE_HORDE_2 = {925.166f, 1458.084f, 355.966f, 0.00f};
Position const WS_FLAG_HIDE_HORDE_3 = {924.922f, 1423.672f, 345.524f, 0.82f};
Position const WS_FLAG_HIDE_ALLIANCE_1 = {1529.249f, 1456.470f, 353.04f, 1.25f};
Position const WS_FLAG_HIDE_ALLIANCE_2 = {1540.286f, 1476.026f, 352.692f, 2.91f};
Position const WS_FLAG_HIDE_ALLIANCE_3 = {1495.807f, 1466.774f, 352.350f, 1.50f};
Position const WS_ROAM_POS = {1227.446f, 1476.235f, 307.484f, 1.50f};
std::vector<Position> const WS_FLAG_HIDE_HORDE = {WS_FLAG_HIDE_HORDE_1, WS_FLAG_HIDE_HORDE_2, WS_FLAG_HIDE_HORDE_3};
std::vector<Position> const WS_FLAG_HIDE_ALLIANCE = {WS_FLAG_HIDE_ALLIANCE_1, WS_FLAG_HIDE_ALLIANCE_2,
                                                     WS_FLAG_HIDE_ALLIANCE_3};

enum BattleBotWsgWaitSpot
{
    BB_WSG_WAIT_SPOT_SPAWN,
    BB_WSG_WAIT_SPOT_LEFT,
    BB_WSG_WAIT_SPOT_RIGHT
};

std::vector<uint32> const vFlagsWS = {BG_OBJECT_A_FLAG_WS_ENTRY, BG_OBJECT_H_FLAG_WS_ENTRY,
                                      BG_OBJECT_A_FLAG_GROUND_WS_ENTRY, BG_OBJECT_H_FLAG_GROUND_WS_ENTRY};

// BG Waypoints (vmangos)

// Horde Flag Room to Horde Graveyard
BattleBotPath vPath_WSG_HordeFlagRoom_to_HordeGraveyard = {
    {933.331f, 1433.72f, 345.536f, nullptr}, {944.859f, 1423.05f, 345.437f, nullptr},
    {966.691f, 1422.53f, 345.223f, nullptr}, {979.588f, 1422.84f, 345.46f, nullptr},
    {997.806f, 1422.52f, 344.623f, nullptr}, {1008.53f, 1417.02f, 343.206f, nullptr},
    {1016.42f, 1402.33f, 341.352f, nullptr}, {1029.14f, 1387.49f, 340.836f, nullptr},
};

// Horde Graveyard to Horde Tunnel
BattleBotPath vPath_WSG_HordeGraveyard_to_HordeTunnel = {
    {1029.14f, 1387.49f, 340.836f, nullptr}, {1034.95f, 1392.62f, 340.856f, nullptr},
    {1038.21f, 1406.43f, 341.562f, nullptr}, {1043.87f, 1426.9f, 339.197f, nullptr},
    {1054.53f, 1441.47f, 339.725f, nullptr}, {1056.33f, 1456.03f, 341.463f, nullptr},
    {1057.39f, 1469.98f, 342.148f, nullptr}, {1057.67f, 1487.55f, 342.537f, nullptr},
    {1048.7f, 1505.37f, 341.117f, nullptr},  {1042.19f, 1521.69f, 338.003f, nullptr},
    {1050.01f, 1538.22f, 332.43f, nullptr},  {1068.15f, 1548.1f, 321.446f, nullptr},
    {1088.14f, 1538.45f, 316.398f, nullptr}, {1101.26f, 1522.79f, 314.918f, nullptr},
    {1114.67f, 1503.18f, 312.947f, nullptr}, {1126.45f, 1487.4f, 314.136f, nullptr},
    {1124.37f, 1462.28f, 315.853f, nullptr},
};

// Horde Tunnel to Horde Flag Room
BattleBotPath vPath_WSG_HordeTunnel_to_HordeFlagRoom = {
    {1124.37f, 1462.28f, 315.853f, nullptr}, {1106.87f, 1462.13f, 316.558f, nullptr},
    {1089.44f, 1461.04f, 316.332f, nullptr}, {1072.07f, 1459.46f, 317.449f, nullptr},
    {1051.09f, 1459.89f, 323.126f, nullptr}, {1030.1f, 1459.58f, 330.204f, nullptr},
    {1010.76f, 1457.49f, 334.896f, nullptr}, {1005.47f, 1448.19f, 335.864f, nullptr},
    {999.974f, 1458.49f, 335.632f, nullptr}, {982.632f, 1459.18f, 336.127f, nullptr},
    {965.049f, 1459.15f, 338.076f, nullptr}, {944.526f, 1459.0f, 344.207f, nullptr},
    {937.479f, 1451.12f, 345.553f, nullptr}, {933.331f, 1433.72f, 345.536f, nullptr},
};

// Alliance Flag Room to Alliance Graveyard
BattleBotPath vPath_WSG_AllianceFlagRoom_to_AllianceGraveyard = {
    {1519.53f, 1481.87f, 352.024f, nullptr}, {1508.27f, 1493.17f, 352.005f, nullptr},
    {1490.78f, 1493.51f, 352.141f, nullptr}, {1469.79f, 1494.13f, 351.774f, nullptr},
    {1453.65f, 1494.39f, 350.614f, nullptr}, {1443.51f, 1501.75f, 348.317f, nullptr},
    {1443.33f, 1517.78f, 345.534f, nullptr}, {1443.55f, 1533.4f, 343.148f, nullptr},
    {1441.47f, 1548.12f, 342.752f, nullptr}, {1433.79f, 1552.67f, 342.763f, nullptr},
    {1422.88f, 1552.37f, 342.751f, nullptr}, {1415.33f, 1554.79f, 343.156f, nullptr},
};

// Alliance Graveyard to Alliance Tunnel
BattleBotPath vPath_WSG_AllianceGraveyard_to_AllianceTunnel = {
    {1415.33f, 1554.79f, 343.156f, nullptr}, {1428.29f, 1551.79f, 342.751f, nullptr},
    {1441.51f, 1545.79f, 342.757f, nullptr}, {1441.15f, 1530.35f, 343.712f, nullptr},
    {1435.53f, 1517.29f, 346.698f, nullptr}, {1424.81f, 1499.24f, 349.486f, nullptr},
    {1416.31f, 1483.94f, 348.536f, nullptr}, {1408.83f, 1468.4f, 347.648f, nullptr},
    {1404.64f, 1449.79f, 347.279f, nullptr}, {1405.34f, 1432.33f, 345.792f, nullptr},
    {1406.38f, 1416.18f, 344.755f, nullptr}, {1400.22f, 1401.87f, 340.496f, nullptr},
    {1385.96f, 1394.15f, 333.829f, nullptr}, {1372.38f, 1390.75f, 328.722f, nullptr},
    {1362.93f, 1390.02f, 327.034f, nullptr}, {1357.91f, 1398.07f, 325.674f, nullptr},
    {1354.17f, 1411.56f, 324.327f, nullptr}, {1351.44f, 1430.38f, 323.506f, nullptr},
    {1350.36f, 1444.43f, 323.388f, nullptr}, {1348.02f, 1461.06f, 323.167f, nullptr},
};

// Alliance Tunnel to Alliance Flag Room
BattleBotPath vPath_WSG_AllianceTunnel_to_AllianceFlagRoom = {
    {1348.02f, 1461.06f, 323.167f, nullptr}, {1359.8f, 1461.49f, 324.527f, nullptr},
    {1372.47f, 1461.61f, 324.354f, nullptr}, {1389.08f, 1461.12f, 325.913f, nullptr},
    {1406.57f, 1460.48f, 330.615f, nullptr}, {1424.04f, 1459.57f, 336.029f, nullptr},
    {1442.5f, 1459.7f, 342.024f, nullptr},   {1449.59f, 1469.14f, 342.65f, nullptr},
    {1458.03f, 1458.43f, 342.746f, nullptr}, {1469.4f, 1458.14f, 342.794f, nullptr},
    {1489.06f, 1457.86f, 342.794f, nullptr}, {1502.27f, 1457.52f, 347.589f, nullptr},
    {1512.87f, 1457.81f, 352.039f, nullptr}, {1517.53f, 1468.79f, 352.033f, nullptr},
    {1519.53f, 1481.87f, 352.024f, nullptr},
};

// Horde Tunnel to Horde Base Roof
BattleBotPath vPath_WSG_HordeTunnel_to_HordeBaseRoof = {
    {1124.37f, 1462.28f, 315.853f, nullptr}, {1106.87f, 1462.13f, 316.558f, nullptr},
    {1089.44f, 1461.04f, 316.332f, nullptr}, {1072.07f, 1459.46f, 317.449f, nullptr},
    {1051.09f, 1459.89f, 323.126f, nullptr}, {1030.1f, 1459.58f, 330.204f, nullptr},
    {1010.76f, 1457.49f, 334.896f, nullptr}, {981.948f, 1459.07f, 336.154f, nullptr},
    {981.768f, 1480.46f, 335.976f, nullptr}, {974.664f, 1495.9f, 340.837f, nullptr},
    {964.661f, 1510.21f, 347.509f, nullptr}, {951.188f, 1520.99f, 356.377f, nullptr},
    {937.37f, 1513.27f, 362.589f, nullptr},  {935.947f, 1499.58f, 364.199f, nullptr},
    {935.9f, 1482.08f, 366.396f, nullptr},   {937.564f, 1462.81f, 367.287f, nullptr},
    {945.871f, 1458.65f, 367.287f, nullptr}, {956.972f, 1459.48f, 367.291f, nullptr},
    {968.317f, 1459.71f, 367.291f, nullptr}, {979.934f, 1454.58f, 367.078f, nullptr},
    {979.99f, 1442.87f, 367.093f, nullptr},  {978.632f, 1430.71f, 367.125f, nullptr},
    {970.395f, 1422.32f, 367.289f, nullptr}, {956.338f, 1425.09f, 367.293f, nullptr},
    {952.778f, 1433.0f, 367.604f, nullptr},  {952.708f, 1445.01f, 367.604f, nullptr},
};

// Alliance Tunnel to Alliance Base Roof
BattleBotPath vPath_WSG_AllianceTunnel_to_AllianceBaseRoof = {
    {1348.02f, 1461.06f, 323.167f, nullptr}, {1359.8f, 1461.49f, 324.527f, nullptr},
    {1372.47f, 1461.61f, 324.354f, nullptr}, {1389.08f, 1461.12f, 325.913f, nullptr},
    {1406.57f, 1460.48f, 330.615f, nullptr}, {1424.04f, 1459.57f, 336.029f, nullptr},
    {1442.5f, 1459.7f, 342.024f, nullptr},   {1471.86f, 1456.65f, 342.794f, nullptr},
    {1470.93f, 1440.5f, 342.794f, nullptr},  {1472.24f, 1427.49f, 342.06f, nullptr},
    {1476.86f, 1412.46f, 341.426f, nullptr}, {1484.42f, 1396.69f, 346.117f, nullptr},
    {1490.7f, 1387.59f, 351.861f, nullptr},  {1500.79f, 1382.98f, 357.784f, nullptr},
    {1511.08f, 1391.29f, 364.444f, nullptr}, {1517.85f, 1403.18f, 370.336f, nullptr},
    {1517.99f, 1417.59f, 371.636f, nullptr}, {1517.07f, 1431.56f, 372.106f, nullptr},
    {1516.66f, 1445.55f, 372.939f, nullptr}, {1514.23f, 1457.37f, 373.689f, nullptr},
    {1503.73f, 1457.67f, 373.684f, nullptr}, {1486.24f, 1457.8f, 373.718f, nullptr},
    {1476.78f, 1460.35f, 373.711f, nullptr}, {1477.37f, 1470.83f, 373.709f, nullptr},
    {1477.5f, 1484.83f, 373.715f, nullptr},  {1480.53f, 1495.26f, 373.721f, nullptr},
    {1492.61f, 1494.72f, 373.721f, nullptr}, {1499.37f, 1489.02f, 373.718f, nullptr},
    {1500.63f, 1472.89f, 373.707f, nullptr},
};

// Alliance Tunnel to Horde Tunnel
BattleBotPath vPath_WSG_AllianceTunnel_to_HordeTunnel = {
    {1129.30f, 1461.03f, 315.206f, nullptr},
    {1149.55f, 1466.57f, 311.031f, nullptr},
    {1196.39f, 1477.25f, 305.697f, nullptr},
    {1227.73f, 1478.38f, 307.418f, nullptr},
    {1267.06f, 1463.40f, 312.227f, nullptr},
    {1303.42f, 1460.18f, 317.257f, nullptr},
};

// Alliance Graveyard (lower) to Horde Flag room
BattleBotPath vPath_WSG_AllianceGraveyardLower_to_HordeFlagRoom = {
    //{1370.71f, 1543.55f, 321.585f, nullptr},
    //{1339.41f, 1533.42f, 313.336f, nullptr},
    {1316.07f, 1533.53f, 315.700f, nullptr},
    {1276.17f, 1533.72f, 311.722f, nullptr},
    {1246.25f, 1533.86f, 307.072f, nullptr},
    {1206.84f, 1528.22f, 307.677f, nullptr},
    {1172.28f, 1523.28f, 301.958f, nullptr},
    {1135.93f, 1505.27f, 308.085f, nullptr},
    {1103.54f, 1521.89f, 314.583f, nullptr},
    {1073.49f, 1551.19f, 319.418f, nullptr},
    {1042.92f, 1530.49f, 336.667f, nullptr},
    {1052.11f, 1493.52f, 342.176f, nullptr},
    {1057.42f, 1452.75f, 341.131f, nullptr},
    {1037.96f, 1422.27f, 339.919f, nullptr},
    {966.01f, 1422.84f, 345.223f, nullptr},
    {942.74f, 1423.10f, 345.467f, nullptr},
    {929.39f, 1434.75f, 345.535f, nullptr},
};

// Horde Graveyard (lower) to Alliance Flag room
BattleBotPath vPath_WSG_HordeGraveyardLower_to_AllianceFlagRoom = {
    //{1096.59f, 1395.07f, 317.016f, nullptr},
    //{1134.38f, 1370.13f, 312.741f, nullptr},
    {1164.96f, 1356.91f, 313.884f, nullptr},
    {1208.32f, 1346.27f, 313.816f, nullptr},
    {1245.06f, 1346.12f, 312.112f, nullptr},
    {1291.43f, 1394.60f, 314.359f, nullptr},
    {1329.33f, 1411.13f, 318.399f, nullptr},
    {1361.57f, 1391.21f, 326.756f, nullptr},
    {1382.19f, 1381.03f, 332.314f, nullptr},
    {1408.06f, 1412.93f, 344.565f, nullptr},
    {1407.88f, 1458.76f, 347.346f, nullptr},
    {1430.40f, 1489.34f, 348.658f, nullptr},
    {1466.64f, 1493.50f, 351.869f, nullptr},
    {1511.51f, 1493.75f, 352.009f, nullptr},
    {1531.44f, 1481.79f, 351.959f, nullptr},
};

// Horde Graveyard (lower) to Horde Tunnel
BattleBotPath vPath_WSG_HordeGraveyardLower_to_HordeTunnel = {
    {1164.96f, 1356.91f, 313.884f, nullptr}, {1155.38f, 1378.91f, 311.360f, nullptr},
    {1148.72f, 1397.70f, 311.160f, nullptr}, {1144.07f, 1417.15f, 311.140f, nullptr},
    {1139.53f, 1432.42f, 311.060f, nullptr}, {1132.54f, 1446.81f, 313.040f, nullptr},
    {1124.37f, 1462.28f, 315.853f, nullptr},
};

// Alliance Graveyard (lower) to Alliance Tunnel
BattleBotPath vPath_WSG_AllianceGraveyardLower_to_AllianceTunnel = {
    {1316.07f, 1533.53f, 315.700f, nullptr}, {1322.52f, 1518.89f, 315.800f, nullptr},
    {1328.98f, 1504.25f, 316.630f, nullptr}, {1335.43f, 1489.61f, 322.550f, nullptr},
    {1341.89f, 1474.97f, 324.010f, nullptr}, {1348.02f, 1461.06f, 323.167f, nullptr},
};

// Alliance Graveyard Jump
BattleBotPath vPath_WSG_AllianceGraveyardJump = {
    {1420.08f, 1552.84f, 342.878f, nullptr},
    {1404.06f, 1551.37f, 342.850f, nullptr},
    {1386.24f, 1543.37f, 321.944f, nullptr},
};

// Horde Graveyard Jump
BattleBotPath vPath_WSG_HordeGraveyardJump = {
    {1045.70f, 1389.15f, 340.638f, nullptr},
    {1057.43f, 1390.12f, 339.869f, nullptr},
    {1074.59f, 1400.67f, 323.811f, nullptr},
    {1092.85f, 1399.38f, 317.429f, nullptr},
};

std::vector<BattleBotPath*> const vPaths_WS = {
    &vPath_WSG_HordeFlagRoom_to_HordeGraveyard,
    &vPath_WSG_HordeGraveyard_to_HordeTunnel,
    &vPath_WSG_HordeTunnel_to_HordeFlagRoom,
    &vPath_WSG_AllianceFlagRoom_to_AllianceGraveyard,
    &vPath_WSG_AllianceGraveyard_to_AllianceTunnel,
    &vPath_WSG_AllianceTunnel_to_AllianceFlagRoom,
    &vPath_WSG_HordeTunnel_to_HordeBaseRoof,
    &vPath_WSG_AllianceTunnel_to_AllianceBaseRoof,
    &vPath_WSG_AllianceTunnel_to_HordeTunnel,
    &vPath_WSG_AllianceGraveyardLower_to_HordeFlagRoom,
    &vPath_WSG_HordeGraveyardLower_to_AllianceFlagRoom,
    &vPath_WSG_HordeGraveyardLower_to_HordeTunnel,
    &vPath_WSG_AllianceGraveyardLower_to_AllianceTunnel,
    &vPath_WSG_AllianceGraveyardJump,
    &vPath_WSG_HordeGraveyardJump
};

std::vector<BattleBotPath*> const vPaths_OneWay_WS = {&vPath_WSG_AllianceGraveyardJump, &vPath_WSG_HordeGraveyardJump};

uint8 BGTactics::GetWsgDefenderCount(Battleground* bg, TeamId teamId)
{
    if (!bg)
        return WSG_DEFENDER_ROLES_BALANCED;

    WSBotStrategy teamStrategy = static_cast<WSBotStrategy>(GetBotStrategyForTeam(bg, teamId));
    WSBotStrategy enemyStrategy = static_cast<WSBotStrategy>(GetBotStrategyForTeam(bg, bg->GetOtherTeamId(teamId)));

    uint8 defendersCount = WSG_DEFENDER_ROLES_BALANCED;
    switch (teamStrategy)
    {
        case WS_STRATEGY_BALANCED:
            defendersCount = WSG_DEFENDER_ROLES_BALANCED;
            break;
        case WS_STRATEGY_OFFENSIVE:
            defendersCount = WSG_DEFENDER_ROLES_OFFENSIVE;
            break;
        case WS_STRATEGY_DEFENSIVE:
            defendersCount = WSG_DEFENDER_ROLES_DEFENSIVE;
            break;
        default:
            defendersCount = WSG_DEFENDER_ROLES_BALANCED;
            break;
    }

    if (enemyStrategy == WS_STRATEGY_DEFENSIVE)
        defendersCount = WSG_DEFENDER_ROLES_VS_DEFENSIVE;

    return defendersCount;
}

BGTactics::WsgJumpResult BGTactics::WsJumpDown()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetBgTypeID(true) != BATTLEGROUND_WS)
        return WsgJumpResult::NotApplicable;

    PositionInfo goal = context->GetValue<PositionMap&>("position")->Get()["bg objective"];
    if (!goal.isSet() || goal.mapId != bot->GetMapId())
        return WsgJumpResult::NotApplicable;

    TeamId team = bot->GetTeamId();
    Position const& ownBase = team == TEAM_HORDE ? WS_FLAG_POS_HORDE : WS_FLAG_POS_ALLIANCE;
    Position const& enemyBase = team == TEAM_HORDE ? WS_FLAG_POS_ALLIANCE : WS_FLAG_POS_HORDE;
    BattleBotPath const& jumpPath = team == TEAM_HORDE ? vPath_WSG_HordeGraveyardJump : vPath_WSG_AllianceGraveyardJump;
    constexpr float descentHeight = 10.0f;
    bool towardEnemyBase = enemyBase.GetExactDist2dSq(goal.x, goal.y) < ownBase.GetExactDist2dSq(goal.x, goal.y);
    bool belowLedge = goal.z + descentHeight < jumpPath.at(1).z;
    // A resurrection departure owns its exit; live home-plateau approaches do not.
    if (!_wsgGraveyardDeparture && !_wsgJumpNextPoint && !towardEnemyBase && !belowLedge)
        return WsgJumpResult::NotApplicable;

    Position const hordeJumpPositions[] = {
        {1038.220f, 1420.197f, 340.099f}, {1029.242f, 1387.024f, 340.866f}, {1045.764f, 1389.831f, 340.825f},
        {1057.076f, 1393.081f, 339.505f}, {1075.233f, 1398.645f, 323.669f}, {1096.590f, 1395.070f, 317.016f},
        {1134.380f, 1370.130f, 312.741f},
    };
    Position const allianceJumpPositions[] = {
        {1415.548f, 1554.538f, 343.164f}, {1407.234f, 1551.658f, 343.432f}, {1385.325f, 1544.592f, 322.047f},
        {1370.710f, 1543.550f, 321.585f}, {1339.410f, 1533.420f, 313.336f},
    };
    std::span<Position const> positions = team == TEAM_HORDE ? std::span<Position const>(hordeJumpPositions)
                                                             : std::span<Position const>(allianceJumpPositions);
    constexpr uint32 hordeJumpPoint = 4;
    constexpr uint32 allianceJumpPoint = 2;
    uint32 jumpPoint = team == TEAM_HORDE ? hordeJumpPoint : allianceJumpPoint;
    constexpr float departureRadius = 4.0f;
    constexpr float graveyardRadius = 12.0f;
    if (!_wsgJumpNextPoint)
    {
        float closestDistance = _wsgGraveyardDeparture ? graveyardRadius : departureRadius;
        for (uint32 i = 0; i + 1 < positions.size(); ++i)
        {
            float distance = bot->GetDistance(positions[i]);
            if (distance < closestDistance)
            {
                closestDistance = distance;
                _wsgJumpNextPoint = i + 1;
            }
        }
        _wsgGraveyardDeparture = false;
        if (!_wsgJumpNextPoint)
            return WsgJumpResult::NotApplicable;
    }

    if (bot->isMoving() || IsWaitingForLastMove(MovementPriority::MOVEMENT_NORMAL))
        return WsgJumpResult::Blocked;
    if (_wsgJumpNextPoint >= positions.size())
    {
        ClearWsgJump();
        return WsgJumpResult::NotApplicable;
    }
    if (bot->GetDistance(positions[_wsgJumpNextPoint]) <= departureRadius)
        ++_wsgJumpNextPoint;
    if (_wsgJumpNextPoint >= positions.size())
    {
        ClearWsgJump();
        return WsgJumpResult::NotApplicable;
    }

    Position const& target = positions[_wsgJumpNextPoint];
    constexpr float departureResumeRadius = 90.0f;
    if (bot->GetDistance(target) > departureResumeRadius ||
        IsUnsafeWsgCliffApproach(target.GetPositionX(), target.GetPositionY(), target.GetPositionZ()) ||
        (_wsgJumpNextPoint == jumpPoint && bot->GetDistance(positions[jumpPoint - 1]) > departureRadius))
    {
        ClearWsgJump();
        return WsgJumpResult::NotApplicable;
    }
    bool moved = _wsgJumpNextPoint == jumpPoint
                     ? JumpTo(bg->GetMapId(), target.GetPositionX(), target.GetPositionY(), target.GetPositionZ())
                     : MoveTo(bg->GetMapId(), target.GetPositionX(), target.GetPositionY(), target.GetPositionZ());
    return moved ? WsgJumpResult::Moved : WsgJumpResult::Blocked;
}

void BGTactics::moveToStartWS(Battleground* bg)
{
    uint32 role = context->GetValue<uint32>("bg role")->Get();

    int startSpot = role < 4 ? BB_WSG_WAIT_SPOT_LEFT : role > 6 ? BB_WSG_WAIT_SPOT_RIGHT : BB_WSG_WAIT_SPOT_SPAWN;
    if (bg->GetStatus() == STATUS_WAIT_JOIN)
    {
        WsgTeamAssignment assignment = context->GetValue<WsgTeamAssignment>("wsg team assignment")->Get();
        constexpr uint32 startSpotCount = 3;
        startSpot = assignment.Role % startSpotCount;
    }
    if (startSpot == BB_WSG_WAIT_SPOT_RIGHT)
    {
        if (bot->GetTeamId() == TEAM_HORDE)
            MoveTo(bg->GetMapId(), WS_WAITING_POS_HORDE_1.GetPositionX() + frand(-4.0f, 4.0f),
                   WS_WAITING_POS_HORDE_1.GetPositionY() + frand(-4.0f, 4.0f),
                   WS_WAITING_POS_HORDE_1.GetPositionZ());
        else
            MoveTo(bg->GetMapId(), WS_WAITING_POS_ALLIANCE_1.GetPositionX() + frand(-4.0f, 4.0f),
                   WS_WAITING_POS_ALLIANCE_1.GetPositionY() + frand(-4.0f, 4.0f),
                   WS_WAITING_POS_ALLIANCE_1.GetPositionZ());
    }
    else if (startSpot == BB_WSG_WAIT_SPOT_LEFT)
    {
        if (bot->GetTeamId() == TEAM_HORDE)
            MoveTo(bg->GetMapId(), WS_WAITING_POS_HORDE_2.GetPositionX() + frand(-4.0f, 4.0f),
                   WS_WAITING_POS_HORDE_2.GetPositionY() + frand(-4.0f, 4.0f),
                   WS_WAITING_POS_HORDE_2.GetPositionZ());
        else
            MoveTo(bg->GetMapId(), WS_WAITING_POS_ALLIANCE_2.GetPositionX() + frand(-4.0f, 4.0f),
                   WS_WAITING_POS_ALLIANCE_2.GetPositionY() + frand(-4.0f, 4.0f),
                   WS_WAITING_POS_ALLIANCE_2.GetPositionZ());
    }
    else  // BB_WSG_WAIT_SPOT_SPAWN
    {
        if (bot->GetTeamId() == TEAM_HORDE)
            MoveTo(bg->GetMapId(), WS_WAITING_POS_HORDE_3.GetPositionX() + frand(-10.0f, 10.0f),
                   WS_WAITING_POS_HORDE_3.GetPositionY() + frand(-10.0f, 10.0f),
                   WS_WAITING_POS_HORDE_3.GetPositionZ());
        else
            MoveTo(bg->GetMapId(), WS_WAITING_POS_ALLIANCE_3.GetPositionX() + frand(-10.0f, 10.0f),
                   WS_WAITING_POS_ALLIANCE_3.GetPositionY() + frand(-10.0f, 10.0f),
                   WS_WAITING_POS_ALLIANCE_3.GetPositionZ());
    }
}

bool BGTactics::selectObjectiveWS(Battleground* bg, PositionMap& posMap, PositionInfo& pos)
{
    Position target;
    TeamId team = bot->GetTeamId();

    BattlegroundWS* warsong = static_cast<BattlegroundWS*>(bg);
    TeamId enemyTeam = bg->GetOtherTeamId(team);
    uint8 ownFlagState = warsong->GetFlagState(team);
    uint8 enemyFlagState = warsong->GetFlagState(enemyTeam);
    bool carryingEnemyFlag = warsong->GetFlagPickerGUID(enemyTeam) == bot->GetGUID();
    WsgTeamAssignment assignment = context->GetValue<WsgTeamAssignment>("wsg team assignment")->Get();
    uint32 role = assignment.Valid ? assignment.Role : context->GetValue<uint32>("bg role")->Get();
    uint8 defendersCount = assignment.Valid ? assignment.Defenders : GetWsgDefenderCount(bg, team);
    constexpr uint32 fallbackEscortRoles = 2;
    uint32 escortRoles = assignment.Valid ? assignment.Escorts : fallbackEscortRoles;
    bool escortRole =
        assignment.Valid ? assignment.Escort : role >= defendersCount && role < defendersCount + escortRoles;
    bool baseDefender = assignment.Valid ? assignment.BaseDefender : role == 0 && defendersCount > 0;
    bool returnerRole = assignment.Valid ? assignment.Returner : role < defendersCount;

    auto ResolveCarrier = [&](TeamId flagTeam) -> Player*
    {
        ObjectGuid guid = warsong->GetFlagPickerGUID(flagTeam);
        Player* carrier = guid.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(bg->GetBgMap(), guid);
        return carrier && carrier->IsInWorld() && carrier->IsAlive() && carrier->GetMap() == bot->GetMap()
                   ? carrier
                   : nullptr;
    };

    Player* friendlyCarrier =
        enemyFlagState == BG_WS_FLAG_STATE_ON_PLAYER ? ResolveCarrier(enemyTeam) : nullptr;
    Player* enemyCarrier = ownFlagState == BG_WS_FLAG_STATE_ON_PLAYER ? ResolveCarrier(team) : nullptr;
    auto PatrolMidfield = [&]()
    {
        // Screen the enemy half rather than retreating the whole offense
        // behind the carrier. Keep a patrol leg until reached.
        PositionInfo& patrol = posMap["wsg midfield patrol"];
        constexpr float patrolArrivalRadius = 12.0f;
        if (!patrol.isSet() || patrol.mapId != bot->GetMapId() ||
            bot->GetDistance(patrol.x, patrol.y, patrol.z) < patrolArrivalRadius)
        {
            constexpr float forwardOffset = 35.0f;
            constexpr float laneSpacing = 45.0f;
            constexpr float patrolSpread = 25.0f;
            constexpr uint32 laneCount = 3;
            float direction = team == TEAM_ALLIANCE ? -1.0f : 1.0f;
            bool defenderRole = assignment.Valid ? assignment.Defender : role < defendersCount;
            if (defenderRole && !escortRole && !returnerRole)
                direction = -direction;
            float lane = static_cast<float>(role % laneCount) - 1.0f;
            Position center(WS_ROAM_POS.GetPositionX() + direction * forwardOffset,
                            WS_ROAM_POS.GetPositionY() + lane * laneSpacing, WS_ROAM_POS.GetPositionZ());
            Position point = bot->GetRandomPoint(center, patrolSpread);
            if (point.IsPositionValid())
                patrol.Set(point.GetPositionX(), point.GetPositionY(), point.GetPositionZ(), bot->GetMapId());
        }
        if (patrol.isSet())
            target.Relocate(patrol.x, patrol.y, patrol.z);
        else
            target.Relocate(WS_ROAM_POS);
    };
    bool enemyObserved = enemyCarrier && bot->CanSeeOrDetect(enemyCarrier);
    bool pursueEnemyCarrier = returnerRole;

    // Living battlegrounds: a loosely playing team's bots sometimes leave their role to wander midfield
    if (!carryingEnemyFlag && slips(10))
        PatrolMidfield();
    else if (carryingEnemyFlag)
    {
        // A carrier can score only when its own flag is home.
        GameObject* ownBaseFlag =
            bg->GetBGObject(team == TEAM_ALLIANCE ? BG_WS_OBJECT_A_FLAG : BG_WS_OBJECT_H_FLAG);
        if (ownFlagState == BG_WS_FLAG_STATE_ON_BASE && ownBaseFlag && ownBaseFlag->isSpawned() &&
            ownBaseFlag->GetGoState() == GO_STATE_READY)
            target.Relocate(team == TEAM_ALLIANCE ? WS_FLAG_POS_ALLIANCE : WS_FLAG_POS_HORDE);
        else
            target.Relocate(team == TEAM_ALLIANCE ? WS_FLAG_HIDE_ALLIANCE[0] : WS_FLAG_HIDE_HORDE[0]);
    }
    else if (ownFlagState == BG_WS_FLAG_STATE_ON_GROUND &&
             (assignment.Valid ? assignment.Defender : role < defendersCount) && !escortRole &&
             !assignment.CommittedAttack)
    {
        GameObject* droppedFlag = bg->GetBgMap()->GetGameObject(warsong->GetDroppedFlagGUID(team));
        if (droppedFlag && droppedFlag->isSpawned() && bot->CanSeeOrDetect(droppedFlag))
            target.Relocate(droppedFlag->GetPosition());
        else
            target.Relocate(team == TEAM_ALLIANCE ? WS_FLAG_POS_ALLIANCE : WS_FLAG_POS_HORDE);
    }
    else if (enemyObserved && pursueEnemyCarrier)
        target.Relocate(enemyCarrier->GetPosition());
    else if (ownFlagState == BG_WS_FLAG_STATE_ON_PLAYER && returnerRole && enemyCarrier)
    {
        // BG map positions expose carrier X/Y even without local detection.
        // Route to the marker, but require normal detection before attacking.
        float x = enemyCarrier->GetPositionX();
        float y = enemyCarrier->GetPositionY();
        Position const& enemyBase = team == TEAM_ALLIANCE ? WS_FLAG_POS_HORDE : WS_FLAG_POS_ALLIANCE;
        constexpr float flagRoomMarkerRadius = 80.0f;
        float sampleZ = bot->GetPositionZ();
        if (enemyBase.GetExactDist2d(x, y) < flagRoomMarkerRadius)
            sampleZ = enemyBase.GetPositionZ();
        // Use known terrain/flag-room height, not the hidden EFC's Z.
        float groundZ = bot->GetMap()->GetHeight(x, y, sampleZ);
        if (groundZ != VMAP_INVALID_HEIGHT_VALUE)
            target.Relocate(x, y, groundZ);
        else if (enemyFlagState == BG_WS_FLAG_STATE_ON_PLAYER)
            target.Relocate(enemyBase);
        else
            PatrolMidfield();
    }
    else if (enemyFlagState == BG_WS_FLAG_STATE_ON_GROUND && !baseDefender &&
             !(assignment.Valid ? assignment.Defender : role < defendersCount))
    {
        GameObject* droppedFlag = bg->GetBgMap()->GetGameObject(warsong->GetDroppedFlagGUID(enemyTeam));
        if (droppedFlag && droppedFlag->isSpawned() && bot->CanSeeOrDetect(droppedFlag))
            target.Relocate(droppedFlag->GetPosition());
        else
            PatrolMidfield();
    }
    // and an escort sticks with its carrier only when the team plays together
    else if (friendlyCarrier && friendlyCarrier != bot && escortRole && coordinated())
        target.Relocate(friendlyCarrier->GetPosition());
    else if (baseDefender && !assignment.CommittedAttack)
        target.Relocate(team == TEAM_ALLIANCE ? WS_FLAG_POS_ALLIANCE : WS_FLAG_POS_HORDE);
    else if ((assignment.Valid ? assignment.Defender : role < defendersCount) && !assignment.CommittedAttack)
    {
        Position const& home = team == TEAM_ALLIANCE ? WS_FLAG_POS_ALLIANCE : WS_FLAG_POS_HORDE;
        PositionInfo& defense = posMap["wsg home defense"];
        constexpr float defenseSpread = 15.0f;
        constexpr float defenseAreaRadius = 25.0f;
        if (!defense.isSet() || defense.mapId != bot->GetMapId() ||
            home.GetExactDist(defense.x, defense.y, defense.z) > defenseAreaRadius)
        {
            Position point = bot->GetRandomPoint(home, defenseSpread);
            if (point.IsPositionValid())
                defense.Set(point.GetPositionX(), point.GetPositionY(), point.GetPositionZ(), bot->GetMapId());
        }
        if (defense.isSet())
            target.Relocate(defense.x, defense.y, defense.z);
        else
            target.Relocate(home);
    }
    else if (enemyFlagState == BG_WS_FLAG_STATE_ON_BASE)
    {
        GameObject* baseFlag =
            bg->GetBGObject(enemyTeam == TEAM_ALLIANCE ? BG_WS_OBJECT_A_FLAG : BG_WS_OBJECT_H_FLAG);
        if (baseFlag && baseFlag->isSpawned() && baseFlag->GetGoState() == GO_STATE_READY)
            target.Relocate(team == TEAM_ALLIANCE ? WS_FLAG_POS_HORDE : WS_FLAG_POS_ALLIANCE);
        else
            PatrolMidfield();
    }
    else
    {
        // Unassigned attackers screen midfield, not the carrier's retreat.
        // This does not reveal a hidden carrier or dropped flag's position.
        PatrolMidfield();
    }

    if (!target.IsPositionValid())
        return false;

    pos.Set(target.GetPositionX(), target.GetPositionY(), target.GetPositionZ(), bot->GetMapId());
    posMap["bg objective"] = pos;
    return true;
}

bool BGTactics::flagTaken()
{
    BattlegroundWS* bg = (BattlegroundWS*)bot->GetBattleground();
    if (!bg)
        return false;

    return !bg->GetFlagPickerGUID(bg->GetOtherTeamId(bot->GetTeamId())).IsEmpty();
}

bool BGTactics::teamFlagTaken()
{
    BattlegroundWS* bg = (BattlegroundWS*)bot->GetBattleground();
    if (!bg)
        return false;

    return !bg->GetFlagPickerGUID(bot->GetTeamId()).IsEmpty();
}

bool BGTactics::ClearObsoleteWsgFollow()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetBgTypeID(true) != BATTLEGROUND_WS || bot->IsNonMeleeSpellCast(false))
        return false;
    Value<ObjectGuid>* ownership = context->GetValue<ObjectGuid>("wsg follow carrier");
    ObjectGuid guid = ownership->Get();
    if (guid.IsEmpty())
        return false;
    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
    {
        ownership->Set(ObjectGuid::Empty);
        return false;
    }
    auto* follow = static_cast<FollowMovementGenerator<Player>*>(bot->GetMotionMaster()->top());
    Unit* carrier = follow->GetTarget();
    if (!carrier || carrier->GetGUID() != guid)
    {
        ownership->Set(ObjectGuid::Empty);
        return false;
    }
    WsgTeamAssignment assignment = context->GetValue<WsgTeamAssignment>("wsg team assignment")->Get();
    bool stillCarrier = IsValidWsgCarrier(bot, carrier, true);
    bool stillSupport = assignment.Valid && assignment.Escort;
    if (stillCarrier && stillSupport)
        return false;
    bot->StopMoving();
    bot->GetMotionMaster()->Clear();
    ownership->Set(ObjectGuid::Empty);
    ClearWsgRoute();
    return true;
}

bool BGTactics::IsUnsafeWsgCliffApproach(float x, float y, float z) const
{
    PerformanceData* cliffCheckData = nullptr;
    if (sPerfMonitor.IsEnabled())
    {
        static thread_local PerformanceData* data = sPerfMonitor.acquire(PERF_MON_VALUE, "wsg cliff check");
        cliffCheckData = data;
    }
    PerfMonitorScope cliffCheck(cliffCheckData);
    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetBgTypeID(true) != BATTLEGROUND_WS || !bot->IsAlive() ||
        z <= bot->GetPositionZ() + WSG_CLIFF_HEIGHT_TOLERANCE)
        return false;

    constexpr float landingRadius = 45.0f;
    constexpr float ledgeRadius = 20.0f;
    for (BattleBotPath const* cliff : {&vPath_WSG_HordeGraveyardJump, &vPath_WSG_AllianceGraveyardJump})
    {
        BattleBotWaypoint const& ledge = cliff->at(1);
        BattleBotWaypoint const& landing = cliff->back();
        if (bot->GetPositionZ() + WSG_CLIFF_HEIGHT_TOLERANCE >= ledge.z ||
            bot->GetExactDist2d(landing.x, landing.y) > landingRadius ||
            Position(x, y, z).GetExactDist2d(ledge.x, ledge.y) > ledgeRadius)
            continue;

        // A complete terrain path can take the ramp around this one-way jump.
        PerformanceData* cliffPathData = nullptr;
        if (sPerfMonitor.IsEnabled())
        {
            static thread_local PerformanceData* data = sPerfMonitor.acquire(PERF_MON_VALUE, "wsg cliff pathfind");
            cliffPathData = data;
        }
        PerfMonitorScope cliffPath(cliffPathData);
        PathGenerator path(bot);
        path.CalculatePath(x, y, z);
        return !(path.GetPathType() & PATHFIND_NORMAL) ||
               (path.GetPathType() & (PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH));
    }
    return false;
}

void BGTactics::UpdateWsgRouteLane()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetBgTypeID(true) != BATTLEGROUND_WS)
        return;

    PositionInfo goal = context->GetValue<PositionMap&>("position")->Get()["bg objective"];
    Position const& destinationBase =
        _wsgCarrying ? (bot->GetTeamId() == TEAM_ALLIANCE ? WS_FLAG_POS_ALLIANCE : WS_FLAG_POS_HORDE)
                     : (bot->GetTeamId() == TEAM_ALLIANCE ? WS_FLAG_POS_HORDE : WS_FLAG_POS_ALLIANCE);
    WsgTeamAssignment assignment = AI_VALUE(WsgTeamAssignment, "wsg team assignment");
    uint32 role = AI_VALUE(uint32, "bg role");
    uint8 defenders = assignment.Valid ? assignment.Defenders : GetWsgDefenderCount(bg, bot->GetTeamId());
    constexpr uint32 fallbackEscortRoles = 2;
    bool escort = assignment.Valid ? assignment.Escort : role >= defenders && role < defenders + fallbackEscortRoles;
    bool returner = assignment.Valid ? assignment.Returner : role < defenders;
    constexpr float flagRoomRadius = 80.0f;
    if (!goal.isSet() || goal.mapId != bot->GetMapId() ||
        destinationBase.GetExactDist(goal.x, goal.y, goal.z) > flagRoomRadius ||
        (!_wsgCarrying && (escort || returner)))
    {
        if (_wsgMidfieldPath)
            ClearWsgRoute();
        ClearWsgLane();
        return;
    }

    float direction = goal.x < WS_ROAM_POS.GetPositionX() ? -1.0f : 1.0f;
    if (_wsgMidfieldPath)
    {
        constexpr float laneArrivalRadius = 20.0f;
        if (bot->GetDistance(_wsgMidfieldGoal) <= laneArrivalRadius)
            _wsgMidfieldReached = true;
        // Abandon an unreached lane after external movement well into the far half.
        constexpr float farSideMargin = 80.0f;
        if (!_wsgMidfieldReached && (bot->GetPositionX() - _wsgMidfieldGoal.GetPositionX()) * direction > farSideMargin)
        {
            ClearWsgRoute();
            ClearWsgLane();
        }
        return;
    }

    // Only start a lane plan for a fresh cross-field trip, never a committed descent or corridor.
    constexpr float midfieldClearance = 80.0f;
    if (_wsgPath || _wsgGraveyardDeparture || _wsgJumpNextPoint ||
        (bot->GetPositionX() - WS_ROAM_POS.GetPositionX()) * direction > -midfieldClearance ||
        (goal.x - WS_ROAM_POS.GetPositionX()) * direction < midfieldClearance)
        return;

    struct MidfieldLane
    {
        BattleBotPath* Path;
        uint32 Point;
    };
    constexpr uint32 outerMidfieldPoint = 2;
    constexpr uint32 centralMidfieldPoint = 4;
    std::array<MidfieldLane, 3> const lanes = {
        {{&vPath_WSG_HordeGraveyardLower_to_AllianceFlagRoom, outerMidfieldPoint},
         {&vPath_WSG_AllianceTunnel_to_HordeTunnel, centralMidfieldPoint},
         {&vPath_WSG_AllianceGraveyardLower_to_HordeFlagRoom, outerMidfieldPoint}}};
    GuidVector observedEnemies;
    if (_wsgCarrying)
        observedEnemies = AI_VALUE(GuidVector, "nearest enemy players");
    uint32 now = getMSTime();
    constexpr uint32 avoidWindow = 20 * IN_MILLISECONDS;
    bool avoidPreviousPath = _wsgAvoidPath && now - _wsgAvoidMs < avoidWindow;
    MidfieldLane const* chosen = nullptr;
    uint32 lowestDanger = UINT32_MAX;
    uint32 equalChoices = 0;
    constexpr float dangerRadius = 60.0f;
    for (MidfieldLane const& lane : lanes)
    {
        if (avoidPreviousPath && lane.Path == _wsgAvoidPath)
            continue;
        BattleBotWaypoint const& point = lane.Path->at(lane.Point);
        uint32 danger = 0;
        for (ObjectGuid const& guid : observedEnemies)
        {
            Player* enemy = ObjectAccessor::GetPlayer(bot->GetMap(), guid);
            if (!enemy || !enemy->IsInWorld() || !enemy->IsAlive() || enemy->GetMap() != bot->GetMap() ||
                enemy->GetTeamId() == bot->GetTeamId() || !bot->CanSeeOrDetect(enemy))
                continue;
            if (enemy->GetDistance(point.x, point.y, point.z) < dangerRadius)
                ++danger;
        }
        if (danger < lowestDanger)
        {
            lowestDanger = danger;
            equalChoices = 1;
            chosen = &lane;
        }
        else if (danger == lowestDanger && urand(1, ++equalChoices) == 1)
            chosen = &lane;
    }
    if (chosen)
    {
        BattleBotWaypoint const& point = chosen->Path->at(chosen->Point);
        _wsgMidfieldPath = chosen->Path;
        _wsgMidfieldGoal.Relocate(point.x, point.y, point.z);
        _wsgMidfieldReached = false;
    }
}

bool BGTactics::MoveToWsgWaypoint()
{
    if (!_wsgPath || _wsgNextPoint >= _wsgPath->size())
    {
        ClearWsgRoute();
        return false;
    }

    BattleBotWaypoint const& next = _wsgPath->at(_wsgNextPoint);
    constexpr float waypointRadius = 8.0f;
    bool moving = bot->isMoving();
    if (moving && bot->GetDistance(next.x, next.y, next.z) > waypointRadius)
        return false;
    if (bot->GetDistance(next.x, next.y, next.z) <= waypointRadius)
    {
        bool atEnd = _wsgReverse ? _wsgNextPoint == 0 : _wsgNextPoint + 1 == _wsgPath->size();
        if (atEnd)
        {
            _wsgCompletedPath = _wsgPath;
            _wsgCompletedReverse = _wsgReverse;
            ClearWsgRoute();
            return false;
        }
        uint32 followingPoint = _wsgReverse ? _wsgNextPoint - 1 : _wsgNextPoint + 1;
        BattleBotWaypoint const& following = _wsgPath->at(followingPoint);
        if (moving && !bot->IsWithinLOS(following.x, following.y, following.z))
            return false;
        _wsgNextPoint = followingPoint;
        _wsgProgressMs = 0;
    }
    else if (moving)
        return false;

    // These legacy tunnel points are buff spurs, not required corridor corners.
    constexpr uint32 speedBuffPoint = 7;
    if (_wsgNextPoint == speedBuffPoint && (_wsgPath == &vPath_WSG_HordeTunnel_to_HordeFlagRoom ||
                                            _wsgPath == &vPath_WSG_AllianceTunnel_to_AllianceFlagRoom))
    {
        BattlegroundWS* warsong = static_cast<BattlegroundWS*>(bot->GetBattleground());
        GameObject* speedBuff = warsong->GetBGObject(
            _wsgPath == &vPath_WSG_HordeTunnel_to_HordeFlagRoom ? BG_WS_OBJECT_SPEEDBUFF_2 : BG_WS_OBJECT_SPEEDBUFF_1);
        if (_wsgCarrying || warsong->GetFlagState(bot->GetTeamId()) != BG_WS_FLAG_STATE_ON_BASE || !speedBuff ||
            !speedBuff->isSpawned() || speedBuff->getLootState() != GO_READY)
            _wsgNextPoint = _wsgReverse ? _wsgNextPoint - 1 : _wsgNextPoint + 1;
    }

    uint32 movePoint = _wsgNextPoint;
    constexpr uint32 maxLookaheadPoints = 3;
    constexpr float lookaheadRadius = 45.0f;
    constexpr float minimumAlignmentSquared = 0.96f;
    bool tunnelBuffPath = _wsgPath == &vPath_WSG_HordeTunnel_to_HordeFlagRoom ||
                          _wsgPath == &vPath_WSG_AllianceTunnel_to_AllianceFlagRoom;
    bool jumpPath = std::find(vPaths_OneWay_WS.begin(), vPaths_OneWay_WS.end(), _wsgPath) != vPaths_OneWay_WS.end();
    float fromX = bot->GetPositionX();
    float fromY = bot->GetPositionY();
    float fromZ = bot->GetPositionZ();
    for (uint32 i = 0; !jumpPath && i < maxLookaheadPoints; ++i)
    {
        bool atEnd = _wsgReverse ? movePoint == 0 : movePoint + 1 == _wsgPath->size();
        if (atEnd || (tunnelBuffPath && movePoint == speedBuffPoint))
            break;
        uint32 following = _wsgReverse ? movePoint - 1 : movePoint + 1;
        if (tunnelBuffPath && following == speedBuffPoint)
            break;
        BattleBotWaypoint const& current = _wsgPath->at(movePoint);
        BattleBotWaypoint const& candidate = _wsgPath->at(following);
        float ax = current.x - fromX;
        float ay = current.y - fromY;
        float az = current.z - fromZ;
        float bx = candidate.x - current.x;
        float by = candidate.y - current.y;
        float bz = candidate.z - current.z;
        float dot = ax * bx + ay * by + az * bz;
        float lengthProduct = (ax * ax + ay * ay + az * az) * (bx * bx + by * by + bz * bz);
        // Coalesce only nearly straight, LOS-clear segments. Keep corners, buff
        // spurs and one-way jumps explicit instead of randomly cutting across them.
        if (dot <= 0.0f || dot * dot < minimumAlignmentSquared * lengthProduct ||
            IsUnsafeWsgCliffApproach(candidate.x, candidate.y, candidate.z) ||
            bot->GetDistance(candidate.x, candidate.y, candidate.z) > lookaheadRadius ||
            !bot->IsWithinLOS(candidate.x, candidate.y, candidate.z))
            break;
        fromX = current.x;
        fromY = current.y;
        fromZ = current.z;
        movePoint = following;
    }

    BattleBotWaypoint const& waypoint = _wsgPath->at(movePoint);
    if (IsUnsafeWsgCliffApproach(waypoint.x, waypoint.y, waypoint.z))
    {
        ClearWsgRoute();
        return false;
    }
    constexpr float routeResumeRadius = 90.0f;
    if (bot->GetDistance(waypoint.x, waypoint.y, waypoint.z) > routeResumeRadius)
    {
        ClearWsgRoute();
        return false;
    }

    if (moving && !bot->IsWithinLOS(waypoint.x, waypoint.y, waypoint.z))
        return false;

    bool moved = MoveTo(bot->GetMapId(), waypoint.x, waypoint.y, waypoint.z);
    if (moved)
    {
        if (_wsgNextPoint != movePoint)
        {
            _wsgProgressMs = 0;
            _wsgStalledSamples = 0;
        }
        _wsgNextPoint = movePoint;
    }
    if (moved && !_wsgCarrying)
    {
        BattleBotWaypoint const& approachEnd = _wsgReverse ? _wsgPath->front() : _wsgPath->back();
        Position const& enemyBase = bot->GetTeamId() == TEAM_ALLIANCE ? WS_FLAG_POS_HORDE : WS_FLAG_POS_ALLIANCE;
        constexpr float flagRoomRadius = 70.0f;
        if (enemyBase.GetExactDist(approachEnd.x, approachEnd.y, approachEnd.z) < flagRoomRadius)
        {
            _wsgApproachPath = _wsgPath;
            _wsgApproachReverse = _wsgReverse;
            _wsgApproachMs = getMSTime();
        }
    }
    return moved;
}
