// Warsong Gulch bot tactics: positions, paths, start spots, objectives and the cliff jump.

#include "BattleGroundTactics.h"

#include "BattlegroundWS.h"
#include "LivingBattlegroundPolicy.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "ServerFacade.h"

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
Position const WS_GY_CAMPING_HORDE = {1039.819, 1388.759f, 340.703f, 0.0f};
Position const WS_GY_CAMPING_ALLIANCE = {1422.320f, 1551.978f, 342.834f, 0.0f};
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
    &vPath_WSG_AllianceGraveyardJump,
    &vPath_WSG_HordeGraveyardJump
};

std::vector<BattleBotPath*> const vPaths_OneWay_WS = {&vPath_WSG_AllianceGraveyardJump, &vPath_WSG_HordeGraveyardJump};

bool BGTactics::wsJumpDown()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    TeamId team = bot->GetTeamId();
    uint32 mapId = bg->GetMapId();

    if (team == TEAM_HORDE)
    {
        if (bot->GetDistance({1038.220f, 1420.197f, 340.099f}) < 4.0f)
        {
            MoveTo(mapId, 1029.242f, 1387.024f, 340.866f);
            return true;
        }
        if (bot->GetDistance({1029.242f, 1387.024f, 340.866f}) < 4.0f)
        {
            MoveTo(mapId, 1045.764f, 1389.831f, 340.825f);
            return true;
        }
        if (bot->GetDistance({1045.764f, 1389.831f, 340.825f}) < 4.0f)
        {
            MoveTo(mapId, 1057.076f, 1393.081f, 339.505f);
            return true;
        }
        if (bot->GetDistance({1057.076f, 1393.081f, 339.505f}) < 4.0f)
        {
            JumpTo(mapId, 1075.233f, 1398.645f, 323.669f);
            return true;
        }
        if (bot->GetDistance({1075.233f, 1398.645f, 323.669f}) < 4.0f)
        {
            MoveTo(mapId, 1096.590f, 1395.070f, 317.016f);
            return true;
        }
        if (bot->GetDistance({1096.590f, 1395.070f, 317.016f}) < 4.0f)
        {
            MoveTo(mapId, 1134.380f, 1370.130f, 312.741f);
            return true;
        }
    }
    else if (team == TEAM_ALLIANCE)
    {
        if (bot->GetDistance({1415.548f, 1554.538f, 343.164f}) < 4.0f)
        {
            MoveTo(mapId, 1407.234f, 1551.658f, 343.432f);
            return true;
        }
        if (bot->GetDistance({1407.234f, 1551.658f, 343.432f}) < 4.0f)
        {
            JumpTo(mapId, 1385.325f, 1544.592f, 322.047f);
            return true;
        }
        if (bot->GetDistance({1385.325f, 1544.592f, 322.047f}) < 4.0f)
        {
            MoveTo(mapId, 1370.710f, 1543.550f, 321.585f);
            return true;
        }
        if (bot->GetDistance({1370.710f, 1543.550f, 321.585f}) < 4.0f)
        {
            MoveTo(mapId, 1339.410f, 1533.420f, 313.336f);
            return true;
        }
    }

    return false;
}

void BGTactics::moveToStartWS(Battleground* bg)
{
    uint32 role = context->GetValue<uint32>("bg role")->Get();

    int startSpot = role < 4 ? BB_WSG_WAIT_SPOT_LEFT : role > 6 ? BB_WSG_WAIT_SPOT_RIGHT : BB_WSG_WAIT_SPOT_SPAWN;
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

    // Utility to safely relocate a position with optional random radius
    auto SetSafePos = [&](Position const& origin, float radius = 0.0f) -> void
    {
        float rx, ry, rz;
        if (radius > 0.0f)
        {
            bot->GetRandomPoint(origin, radius, rx, ry, rz);
            target.Relocate(rx, ry, rz);
        }
        else
        {
            target.Relocate(origin);
        }
    };

    // Check if the bot is carrying the flag
    bool hasFlag = bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG);

    // Retrieve role
    uint8 role = context->GetValue<uint32>("bg role")->Get();
    WSBotStrategy strategyHorde = static_cast<WSBotStrategy>(GetBotStrategyForTeam(bg, TEAM_HORDE));
    WSBotStrategy strategyAlliance = static_cast<WSBotStrategy>(GetBotStrategyForTeam(bg, TEAM_ALLIANCE));
    WSBotStrategy strategy = (team == TEAM_ALLIANCE) ? strategyAlliance : strategyHorde;
    WSBotStrategy enemyStrategy = (team == TEAM_ALLIANCE) ? strategyHorde : strategyAlliance;

    uint8 defendersProhab = 3;  // Default balanced

    switch (strategy)
    {
        case WS_STRATEGY_OFFENSIVE:
            defendersProhab = 1;
            break;
        case WS_STRATEGY_DEFENSIVE:
            defendersProhab = 6;
            break;
        default:
            break;
    }

    if (enemyStrategy == WS_STRATEGY_DEFENSIVE)
        defendersProhab = 2;

    // Role check
    bool isDefender = role < defendersProhab;

    // Retrieve flag carriers
    Unit* enemyFC = AI_VALUE(Unit*, "enemy flag carrier");
    Unit* teamFC = AI_VALUE(Unit*, "team flag carrier");

    // Retrieve current score
    uint8 allianceScore = bg->GetTeamScore(TEAM_ALLIANCE);
    uint8 hordeScore = bg->GetTeamScore(TEAM_HORDE);

    // Check if both teams currently have the flag
    bool bothFlagsTaken = enemyFC && teamFC;
    if (!hasFlag && bothFlagsTaken)
    {
        // If both flags taken: coordinated teams send some bots to escort their carrier, the rest hunt the enemy FC
        if (teamFC && coordinated() &&
            urand(0, 99) < LivingBattleground::EscortChance(sPlayerbotAIConfig.LivingWorldEnabled))
        {
            target.Relocate(teamFC->GetPositionX(), teamFC->GetPositionY(), teamFC->GetPositionZ());
            if (ServerFacade::instance().GetDistance2d(bot, teamFC) < 33.0f)
                Follow(teamFC);
        }
        else
            target.Relocate(enemyFC->GetPositionX(), enemyFC->GetPositionY(), enemyFC->GetPositionZ());
    }
    // Graveyard camping if in lead; defenders keep the flag room
    else if (!hasFlag && !isDefender && role < 8 && coordinated() &&
        ((team == TEAM_ALLIANCE && allianceScore == 2 && hordeScore == 0) ||
        (team == TEAM_HORDE && hordeScore == 2 && allianceScore == 0)))
    {
        if (team == TEAM_ALLIANCE)
            SetSafePos(WS_GY_CAMPING_HORDE, 10.0f);
        else
            SetSafePos(WS_GY_CAMPING_ALLIANCE, 10.0f);
    }
    else if (hasFlag)
    {
        // If carrying the flag, either hide or return to base
        if (team == TEAM_ALLIANCE)
            SetSafePos(teamFlagTaken() ? WS_FLAG_HIDE_ALLIANCE[urand(0, 2)] : WS_FLAG_POS_ALLIANCE);
        else
            SetSafePos(teamFlagTaken() ? WS_FLAG_HIDE_HORDE[urand(0, 2)] : WS_FLAG_POS_HORDE);
    }
    else
    {
        if (isDefender)
        {
            if (enemyFC)
            {
                // Defenders attack enemy FC if found
                target.Relocate(enemyFC->GetPositionX(), enemyFC->GetPositionY(), enemyFC->GetPositionZ());
            }
            else if (teamFlagTaken())
            {
                // our flag is gone and its carrier out of sight: cut it off on the way to their flag room
                SetSafePos(team == TEAM_ALLIANCE ? WS_FLAG_POS_HORDE : WS_FLAG_POS_ALLIANCE, 10.0f);
            }
            else if (teamFC && coordinated())
            {
                // support own FC
                target.Relocate(teamFC->GetPositionX(), teamFC->GetPositionY(), teamFC->GetPositionZ());
                if (ServerFacade::instance().GetDistance2d(bot, teamFC) < 33.0f)
                    Follow(teamFC);
            }
            else if (slips(15))
            {
                // Roam around central area
                SetSafePos(WS_ROAM_POS, 75.0f);
            }
            else
            {
                // Hold the flag room
                SetSafePos(team == TEAM_ALLIANCE ? WS_FLAG_HIDE_ALLIANCE[urand(0, 2)] : WS_FLAG_HIDE_HORDE[urand(0, 2)], 5.0f);
            }
        }
        else  // attacker logic
        {
            if (enemyFC && urand(0, 99) < 70)
            {
                // 70% chance to pursue enemy FC
                target.Relocate(enemyFC->GetPositionX(), enemyFC->GetPositionY(), enemyFC->GetPositionZ());
            }
            else if (teamFC && (coordinated() || ServerFacade::instance().GetDistance2d(bot, teamFC) < 40.0f))
            {
                // Assist own FC if not pursuing enemy FC; uncoordinated bots only when they're already close
                target.Relocate(teamFC->GetPositionX(), teamFC->GetPositionY(), teamFC->GetPositionZ());
                if (ServerFacade::instance().GetDistance2d(bot, teamFC) < 33.0f)
                    Follow(teamFC);
            }
            else if (slips(5))
            {
                // 5% chance to free roam
                SetSafePos(WS_ROAM_POS, 75.0f);
            }
            else
            {
                // Push toward enemy flag base
                SetSafePos(team == TEAM_ALLIANCE ? WS_FLAG_POS_HORDE : WS_FLAG_POS_ALLIANCE);
            }
        }
    }

    // Save the final target position
    if (target.IsPositionValid())
    {
        pos.Set(target.GetPositionX(), target.GetPositionY(), target.GetPositionZ(), bot->GetMapId());
        posMap["bg objective"] = pos;
        return true;
    }

    return false;
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
