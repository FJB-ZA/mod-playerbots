// Arathi Basin bot tactics: start spots, waypoints and node objectives.

#include "BattleGroundTactics.h"

#include <algorithm>

#include "Battleground.h"
#include "BattlegroundAB.h"
#include "GameObject.h"
#include "Playerbots.h"
#include "PositionValue.h"

Position const AB_WAITING_POS_HORDE = {702.884f, 703.045f, -16.115f, 0.77f};
Position const AB_WAITING_POS_ALLIANCE = {1286.054f, 1282.500f, -15.697f, 3.95f};
Position const AB_GY_CAMPING_HORDE = {723.513f, 725.924f, -28.265f, 3.99f};
Position const AB_GY_CAMPING_ALLIANCE = {1262.627f, 1256.341f, -27.289f, 0.64f};

std::vector<uint32> const vFlagsAB = {
    BG_AB_OBJECTID_BANNER_A,
    BG_AB_OBJECTID_BANNER_CONT_A,
    BG_AB_OBJECTID_BANNER_H,
    BG_AB_OBJECTID_BANNER_CONT_H,
    BG_AB_OBJECTID_NODE_BANNER_0,
    BG_AB_OBJECTID_NODE_BANNER_1,
    BG_AB_OBJECTID_NODE_BANNER_2,
    BG_AB_OBJECTID_NODE_BANNER_3,
    BG_AB_OBJECTID_NODE_BANNER_4
};

// Alliance Base to Stables
BattleBotPath vPath_AB_AllianceBase_to_Stables = {
    {1285.67f, 1282.14f, -15.8466f, nullptr}, {1272.52f, 1267.83f, -21.7811f, nullptr},
    {1250.44f, 1248.09f, -33.3028f, nullptr}, {1232.56f, 1233.05f, -41.5241f, nullptr},
    {1213.25f, 1224.93f, -47.5513f, nullptr}, {1189.29f, 1219.49f, -53.119f, nullptr},
    {1177.17f, 1210.21f, -56.4593f, nullptr}, {1167.98f, 1202.9f, -56.4743f, nullptr},
};

// Blacksmith to Lumber Mill
BattleBotPath vPath_AB_Blacksmith_to_LumberMill = {
    {967.04f, 1039.03f, -45.091f, nullptr},  {933.67f, 1016.49f, -50.5154f, nullptr},
    {904.02f, 996.63f, -62.3461f, nullptr},  {841.74f, 985.23f, -58.8920f, nullptr},
    {796.25f, 1009.93f, -44.3286f, nullptr}, {781.29f, 1034.49f, -32.887f, nullptr},
    {793.17f, 1107.21f, 5.5663f, nullptr},   {848.98f, 1155.9f, 11.3453f, nullptr},
};

// Blacksmith to GoldMine
BattleBotPath vPath_AB_Blacksmith_to_GoldMine = {
    {1035.98f, 1015.66f, -46.0278f, nullptr}, {1096.86f, 1002.05f, -60.8013f, nullptr},
    {1159.93f, 1003.69f, -63.8378f, nullptr}, {1198.03f, 1064.09f, -65.8385f, nullptr},
    {1218.58f, 1016.96f, -76.9848f, nullptr}, {1192.83f, 956.25f, -93.6974f, nullptr},
    {1162.93f, 908.92f, -108.6703f, nullptr}, {1144.94f, 860.09f, -111.2100f, nullptr},
};

// Farm to Stables
BattleBotPath vPath_AB_Farm_to_Stable = {
    {749.88f, 878.23f, -55.1523f, nullptr},   {819.77f, 931.13f, -57.5882f, nullptr},
    {842.34f, 984.76f, -59.0333f, nullptr},   {863.03f, 1051.47f, -58.0495f, nullptr},
    {899.28f, 1098.27f, -57.4149f, nullptr},  {949.22f, 1153.27f, -54.4464f, nullptr},
    {999.07f, 1189.47f, -49.9125f, nullptr},  {1063.11f, 1211.55f, -53.4164f, nullptr},
    {1098.45f, 1225.47f, -53.1301f, nullptr}, {1146.02f, 1226.34f, -53.8979f, nullptr},
    {1167.10f, 1204.31f, -56.55f, nullptr},
};

// Alliance Base to Gold Mine
BattleBotPath vPath_AB_AllianceBase_to_GoldMine = {
    {1285.67f, 1282.14f, -15.8466f, nullptr}, {1276.41f, 1267.11f, -20.775f, nullptr},
    {1261.34f, 1241.52f, -31.2971f, nullptr}, {1244.91f, 1219.03f, -41.9658f, nullptr},
    {1232.25f, 1184.41f, -50.3348f, nullptr}, {1226.89f, 1150.82f, -55.7935f, nullptr},
    {1224.09f, 1120.38f, -57.0633f, nullptr}, {1220.03f, 1092.72f, -59.1744f, nullptr},
    {1216.05f, 1060.86f, -67.2771f, nullptr}, {1213.77f, 1027.96f, -74.429f, nullptr},
    {1208.56f, 998.394f, -81.9493f, nullptr}, {1197.42f, 969.73f, -89.9385f, nullptr},
    {1185.23f, 944.531f, -97.2433f, nullptr}, {1166.29f, 913.945f, -107.214f, nullptr},
    {1153.17f, 887.863f, -112.34f, nullptr},  {1148.89f, 871.391f, -111.96f, nullptr},
    {1145.24f, 850.82f, -110.514f, nullptr},
};

// Alliance Base to Lumber Mill
BattleBotPath vPath_AB_AllianceBase_to_LumberMill = {
    {1285.67f, 1282.14f, -15.8466f, nullptr}, {1269.13f, 1267.89f, -22.7764f, nullptr},
    {1247.79f, 1249.77f, -33.2518f, nullptr}, {1226.29f, 1232.02f, -43.9193f, nullptr},
    {1196.68f, 1230.15f, -50.4644f, nullptr}, {1168.72f, 1228.98f, -53.9329f, nullptr},
    {1140.82f, 1226.7f, -53.6318f, nullptr},  {1126.85f, 1225.77f, -47.98f, nullptr},
    {1096.5f, 1226.57f, -53.1769f, nullptr},  {1054.52f, 1226.14f, -49.2011f, nullptr},
    {1033.52f, 1226.08f, -45.5968f, nullptr}, {1005.52f, 1226.08f, -43.2912f, nullptr},
    {977.53f, 1226.68f, -40.16f, nullptr},    {957.242f, 1227.94f, -34.1487f, nullptr},
    {930.689f, 1221.57f, -18.9588f, nullptr}, {918.202f, 1211.98f, -12.2494f, nullptr},
    {880.329f, 1192.63f, 7.61168f, nullptr},  {869.965f, 1178.52f, 10.9678f, nullptr},
    {864.74f, 1163.78f, 12.385f, nullptr},    {859.165f, 1148.84f, 11.5289f, nullptr},
};

// Stables to Blacksmith
BattleBotPath vPath_AB_Stables_to_Blacksmith = {
    {1169.52f, 1198.71f, -56.2742f, nullptr}, {1166.93f, 1185.2f, -56.3634f, nullptr},
    {1173.84f, 1170.6f, -56.4094f, nullptr},  {1186.7f, 1163.92f, -56.3961f, nullptr},
    {1189.7f, 1150.68f, -55.8664f, nullptr},  {1185.18f, 1129.31f, -58.1044f, nullptr},
    {1181.7f, 1108.6f, -62.1797f, nullptr},   {1177.92f, 1087.95f, -63.5768f, nullptr},
    {1174.52f, 1067.23f, -64.402f, nullptr},  {1171.27f, 1051.09f, -65.0833f, nullptr},
    {1163.22f, 1031.7f, -64.954f, nullptr},   {1154.25f, 1010.25f, -63.5299f, nullptr},
    {1141.07f, 999.479f, -63.3713f, nullptr}, {1127.12f, 1000.37f, -60.628f, nullptr},
    {1106.17f, 1001.66f, -61.7457f, nullptr}, {1085.64f, 1005.62f, -58.5932f, nullptr},
    {1064.88f, 1008.65f, -52.3547f, nullptr}, {1044.16f, 1011.96f, -47.2647f, nullptr},
    {1029.72f, 1014.88f, -45.3546f, nullptr}, {1013.94f, 1028.7f, -43.9786f, nullptr},
    {990.89f, 1039.3f, -42.7659f, nullptr},   {978.269f, 1043.84f, -44.4588f, nullptr},
};

// Horde Base to Farm
BattleBotPath vPath_AB_HordeBase_to_Farm = {
    {707.259f, 707.839f, -17.5318f, nullptr}, {712.063f, 712.928f, -20.1802f, nullptr},
    {725.941f, 728.682f, -29.7536f, nullptr}, {734.715f, 739.591f, -35.2144f, nullptr},
    {747.607f, 756.161f, -40.899f, nullptr},  {753.994f, 766.668f, -43.3049f, nullptr},
    {758.715f, 787.106f, -46.7014f, nullptr}, {762.077f, 807.831f, -48.4721f, nullptr},
    {764.132f, 821.68f, -49.656f, nullptr},   {767.947f, 839.274f, -50.8574f, nullptr},
    {773.745f, 852.013f, -52.6226f, nullptr}, {785.123f, 869.103f, -54.2089f, nullptr},
    {804.429f, 874.961f, -55.2691f, nullptr},
};

// Horde Base to Gold Mine
BattleBotPath vPath_AB_HordeBase_to_GoldMine = {
    {707.259f, 707.839f, -17.5318f, nullptr}, {717.935f, 716.874f, -23.3941f, nullptr},
    {739.195f, 732.483f, -34.5791f, nullptr}, {757.087f, 742.008f, -38.1123f, nullptr},
    {776.946f, 748.775f, -42.7346f, nullptr}, {797.138f, 754.539f, -46.3237f, nullptr},
    {817.37f, 760.167f, -48.9235f, nullptr},  {837.638f, 765.664f, -49.7374f, nullptr},
    {865.092f, 774.738f, -51.9831f, nullptr}, {878.86f, 777.149f, -47.2361f, nullptr},
    {903.911f, 780.212f, -53.1424f, nullptr}, {923.454f, 787.888f, -54.7937f, nullptr},
    {946.218f, 798.93f, -59.0904f, nullptr},  {978.1f, 813.321f, -66.7268f, nullptr},
    {1002.94f, 817.895f, -77.3119f, nullptr}, {1030.77f, 820.92f, -88.7717f, nullptr},
    {1058.61f, 823.889f, -94.1623f, nullptr}, {1081.6f, 828.32f, -99.4137f, nullptr},
    {1104.64f, 844.773f, -106.387f, nullptr}, {1117.56f, 853.686f, -110.716f, nullptr},
    {1144.9f, 850.049f, -110.522f, nullptr},
};

// Horde Base to Lumber Mill
BattleBotPath vPath_AB_HordeBase_to_LumberMill = {
    {707.259f, 707.839f, -17.5318f, nullptr}, {721.611f, 726.507f, -27.9646f, nullptr},
    {733.846f, 743.573f, -35.8633f, nullptr}, {746.201f, 760.547f, -40.838f, nullptr},
    {758.937f, 787.565f, -46.741f, nullptr},  {761.289f, 801.357f, -48.0037f, nullptr},
    {764.341f, 822.128f, -49.6908f, nullptr}, {769.766f, 842.244f, -51.1239f, nullptr},
    {775.322f, 855.093f, -53.1161f, nullptr}, {783.995f, 874.216f, -55.0822f, nullptr},
    {789.917f, 886.902f, -56.2935f, nullptr}, {798.03f, 906.259f, -57.1162f, nullptr},
    {803.183f, 919.266f, -57.6692f, nullptr}, {813.248f, 937.688f, -57.7106f, nullptr},
    {820.412f, 958.712f, -56.1492f, nullptr}, {814.247f, 973.692f, -50.4602f, nullptr},
    {807.697f, 985.502f, -47.2383f, nullptr}, {795.672f, 1002.69f, -44.9382f, nullptr},
    {784.653f, 1020.77f, -38.6278f, nullptr}, {784.826f, 1037.34f, -31.5719f, nullptr},
    {786.083f, 1051.28f, -24.0793f, nullptr}, {787.314f, 1065.23f, -16.8918f, nullptr},
    {788.892f, 1086.17f, -6.42608f, nullptr}, {792.077f, 1106.53f, 4.81124f, nullptr},
    {800.398f, 1119.48f, 8.5814f, nullptr},   {812.476f, 1131.1f, 10.439f, nullptr},
    {829.704f, 1142.52f, 10.738f, nullptr},   {842.646f, 1143.51f, 11.9984f, nullptr},
    {857.674f, 1146.16f, 11.529f, nullptr},
};

// Farm to Blacksmith
BattleBotPath vPath_AB_Farm_to_Blacksmith = {
    {803.826f, 874.909f, -55.2547f, nullptr}, {808.763f, 887.991f, -57.4437f, nullptr},
    {818.33f, 906.674f, -59.3554f, nullptr},  {828.634f, 924.972f, -60.5664f, nullptr},
    {835.255f, 937.308f, -60.2915f, nullptr}, {845.244f, 955.78f, -60.4208f, nullptr},
    {852.125f, 967.965f, -61.3135f, nullptr}, {863.232f, 983.109f, -62.6402f, nullptr},
    {875.413f, 989.245f, -61.2916f, nullptr}, {895.765f, 994.41f, -63.6287f, nullptr},
    {914.16f, 1001.09f, -58.37f, nullptr},    {932.418f, 1011.44f, -51.9225f, nullptr},
    {944.244f, 1018.92f, -49.1438f, nullptr}, {961.55f, 1030.81f, -45.814f, nullptr},
    {978.122f, 1043.87f, -44.4682f, nullptr},
};

// Stables to Gold Mine
BattleBotPath vPath_AB_Stables_to_GoldMine = {
    {1169.52f, 1198.71f, -56.2742f, nullptr}, {1166.72f, 1183.58f, -56.3633f, nullptr},
    {1172.14f, 1170.99f, -56.4735f, nullptr}, {1185.18f, 1164.02f, -56.4269f, nullptr},
    {1193.98f, 1155.85f, -55.924f, nullptr},  {1201.51f, 1145.65f, -56.4733f, nullptr},
    {1205.39f, 1134.81f, -56.2366f, nullptr}, {1207.57f, 1106.9f, -58.4748f, nullptr},
    {1209.4f, 1085.98f, -63.4022f, nullptr},  {1212.68f, 1065.25f, -66.514f, nullptr},
    {1216.42f, 1037.52f, -72.0457f, nullptr}, {1215.4f, 1011.56f, -78.3338f, nullptr},
    {1209.8f, 992.293f, -83.2433f, nullptr},  {1201.23f, 973.121f, -88.5661f, nullptr},
    {1192.16f, 954.183f, -94.2209f, nullptr}, {1181.88f, 935.894f, -99.5239f, nullptr},
    {1169.86f, 918.68f, -105.588f, nullptr},  {1159.36f, 900.497f, -110.461f, nullptr},
    {1149.32f, 874.429f, -112.142f, nullptr}, {1145.34f, 849.824f, -110.523f, nullptr},
};

// Stables to Lumber Mill
BattleBotPath vPath_AB_Stables_to_LumberMill = {
    {1169.52f, 1198.71f, -56.2742f, nullptr},  {1169.33f, 1203.43f, -56.5457f, nullptr},
    {1164.77f, 1208.73f, -56.1907f, nullptr},  {1141.52f, 1224.99f, -53.8204f, nullptr},
    {1127.54f, 1224.82f, -48.2081f, nullptr},  {1106.56f, 1225.58f, -50.5154f, nullptr},
    {1085.6f, 1226.54f, -53.1863f, nullptr},   {1064.6f, 1226.82f, -50.4381f, nullptr},
    {1043.6f, 1227.27f, -46.5439f, nullptr},   {1022.61f, 1227.72f, -44.7157f, nullptr},
    {1001.61f, 1227.62f, -42.6876f, nullptr},  {980.623f, 1226.93f, -40.4687f, nullptr},
    {959.628f, 1227.1f, -35.3838f, nullptr},   {938.776f, 1226.34f, -23.5399f, nullptr},
    {926.138f, 1217.21f, -16.2176f, nullptr},  {911.966f, 1205.99f, -9.69655f, nullptr},
    {895.135f, 1198.85f, -0.546275f, nullptr}, {873.419f, 1189.27f, 9.3466f, nullptr},
    {863.821f, 1181.72f, 9.76912f, nullptr},   {851.803f, 1166.3f, 10.4423f, nullptr},
    {853.921f, 1150.92f, 11.543f, nullptr},
};

// Farm to Gold Mine
BattleBotPath vPath_AB_Farm_to_GoldMine = {
    {803.826f, 874.909f, -55.2547f, nullptr}, {801.662f, 865.689f, -56.9445f, nullptr},
    {806.433f, 860.776f, -57.5899f, nullptr}, {816.236f, 857.397f, -57.7029f, nullptr},
    {826.717f, 855.846f, -57.9914f, nullptr}, {836.128f, 851.257f, -57.8321f, nullptr},
    {847.933f, 843.837f, -58.1296f, nullptr}, {855.08f, 832.688f, -57.7373f, nullptr},
    {864.513f, 813.663f, -57.574f, nullptr},  {864.229f, 797.762f, -54.2057f, nullptr},
    {862.967f, 787.372f, -53.0276f, nullptr}, {864.163f, 776.33f, -52.0372f, nullptr},
    {872.583f, 777.391f, -48.5342f, nullptr}, {893.575f, 777.922f, -49.1826f, nullptr},
    {915.941f, 783.534f, -53.6598f, nullptr}, {928.105f, 789.929f, -55.4802f, nullptr},
    {946.263f, 800.46f, -59.166f, nullptr},   {958.715f, 806.845f, -62.1494f, nullptr},
    {975.79f, 811.913f, -65.9648f, nullptr},  {989.468f, 814.883f, -71.3089f, nullptr},
    {1010.13f, 818.643f, -80.0817f, nullptr}, {1023.97f, 820.667f, -86.1114f, nullptr},
    {1044.84f, 823.011f, -92.0583f, nullptr}, {1058.77f, 824.482f, -94.1937f, nullptr},
    {1079.13f, 829.402f, -99.1207f, nullptr}, {1092.85f, 836.986f, -102.755f, nullptr},
    {1114.75f, 851.21f, -109.782f, nullptr},  {1128.22f, 851.928f, -111.078f, nullptr},
    {1145.14f, 849.895f, -110.523f, nullptr},
};

// Farm to Lumber Mill
BattleBotPath vPath_AB_Farm_to_LumberMill = {
    {803.826f, 874.909f, -55.2547f, nullptr}, {802.874f, 894.28f, -56.4661f, nullptr},
    {806.844f, 920.39f, -57.3157f, nullptr},  {814.003f, 934.161f, -57.6065f, nullptr},
    {824.594f, 958.47f, -58.4916f, nullptr},  {820.434f, 971.184f, -53.201f, nullptr},
    {808.339f, 987.79f, -47.5705f, nullptr},  {795.98f, 1004.76f, -44.9189f, nullptr},
    {785.497f, 1019.18f, -39.2806f, nullptr}, {783.94f, 1032.46f, -33.5692f, nullptr},
    {784.956f, 1053.41f, -22.8368f, nullptr}, {787.499f, 1074.25f, -12.4232f, nullptr},
    {789.406f, 1088.11f, -5.28606f, nullptr}, {794.617f, 1109.17f, 6.1966f, nullptr},
    {801.514f, 1120.77f, 8.81455f, nullptr},  {817.3f, 1134.59f, 10.6064f, nullptr},
    {828.961f, 1142.98f, 10.7354f, nullptr},  {841.63f, 1147.75f, 11.6916f, nullptr},
    {854.326f, 1150.55f, 11.537f, nullptr},
};

std::vector<BattleBotPath*> const vPaths_AB = {
    &vPath_AB_AllianceBase_to_Stables,  &vPath_AB_AllianceBase_to_GoldMine, &vPath_AB_AllianceBase_to_LumberMill,
    &vPath_AB_Stables_to_Blacksmith,    &vPath_AB_HordeBase_to_Farm,        &vPath_AB_HordeBase_to_GoldMine,
    &vPath_AB_HordeBase_to_LumberMill,  &vPath_AB_Farm_to_Blacksmith,       &vPath_AB_Stables_to_GoldMine,
    &vPath_AB_Stables_to_LumberMill,    &vPath_AB_Farm_to_GoldMine,         &vPath_AB_Farm_to_LumberMill,
    &vPath_AB_Blacksmith_to_LumberMill, &vPath_AB_Blacksmith_to_GoldMine,   &vPath_AB_Farm_to_Stable,
};

static uint32 AB_AttackObjectives[] = {
    BG_AB_NODE_STABLES,
    BG_AB_NODE_BLACKSMITH,
    BG_AB_NODE_FARM, BG_AB_NODE_LUMBER_MILL,
    BG_AB_NODE_GOLD_MINE
};

void BGTactics::moveToStartAB(Battleground* bg)
{
    if (bot->GetTeamId() == TEAM_HORDE)
        MoveTo(bg->GetMapId(), AB_WAITING_POS_HORDE.GetPositionX() + frand(-7.0f, 7.0f),
               AB_WAITING_POS_HORDE.GetPositionY() + frand(-2.0f, 2.0f), AB_WAITING_POS_HORDE.GetPositionZ());
    else
        MoveTo(bg->GetMapId(), AB_WAITING_POS_ALLIANCE.GetPositionX() + frand(-7.0f, 7.0f),
               AB_WAITING_POS_ALLIANCE.GetPositionY() + frand(-2.0f, 2.0f), AB_WAITING_POS_ALLIANCE.GetPositionZ());
}

bool BGTactics::selectObjectiveAB(Battleground* bg, PositionMap& posMap, PositionInfo& pos)
{
    WorldObject* BgObjective = nullptr;

    if (!bg)
        return false;

    BattlegroundAB* ab = static_cast<BattlegroundAB*>(bg);
    TeamId team = bot->GetTeamId();

    uint8 role = context->GetValue<uint32>("bg role")->Get();
    ABBotStrategy strategyHorde = static_cast<ABBotStrategy>(GetBotStrategyForTeam(bg, TEAM_HORDE));
    ABBotStrategy strategyAlliance = static_cast<ABBotStrategy>(GetBotStrategyForTeam(bg, TEAM_ALLIANCE));
    ABBotStrategy strategy = (team == TEAM_ALLIANCE) ? strategyAlliance : strategyHorde;
    ABBotStrategy enemyStrategy = (team == TEAM_ALLIANCE) ? strategyHorde : strategyAlliance;

    uint8 defendersProhab = 3;
    if (strategy == AB_STRATEGY_OFFENSIVE)
        defendersProhab = 1;
    else if (strategy == AB_STRATEGY_DEFENSIVE)
        defendersProhab = 6;

    if (enemyStrategy == AB_STRATEGY_DEFENSIVE)
        defendersProhab = 2;

    bool isDefender = role < defendersProhab;
    bool isSilly = slips(20);

    BgObjective = nullptr;

    // --- PRIORITY 1: Nearby enemy (rare aggressive impulse)
    if (slips(5))
    {
        if (Unit* enemy = AI_VALUE(Unit*, "enemy player target"))
        {
            if (bot->GetDistance(enemy) < 500.0f)
            {
                pos.Set(enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetPositionZ(), bot->GetMapId());
                posMap["bg objective"] = pos;
                return false;
            }
        }
    }

    // --- PRIORITY 2: No valid nodes? Camp or attack visible enemy
    bool hasValidTarget = false;
    for (uint32 nodeId : AB_AttackObjectives)
    {
        uint8 state = ab->GetCapturePointInfo(nodeId)._state;
        if (state == BG_AB_NODE_STATE_NEUTRAL ||
            (team == TEAM_ALLIANCE &&
             (state == BG_AB_NODE_STATE_HORDE_OCCUPIED || state == BG_AB_NODE_STATE_HORDE_CONTESTED)) ||
            (team == TEAM_HORDE &&
             (state == BG_AB_NODE_STATE_ALLY_OCCUPIED || state == BG_AB_NODE_STATE_ALLY_CONTESTED)))
        {
            hasValidTarget = true;
            break;
        }
    }

    // defenders keep guarding even when there is nothing left to attack
    if (!hasValidTarget && !isDefender)
    {
        if (Unit* enemy = AI_VALUE(Unit*, "enemy player target"))
        {
            if (bot->GetDistance(enemy) < 500.0f)
            {
                pos.Set(enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetPositionZ(), bot->GetMapId());
                posMap["bg objective"] = pos;
                return false;
            }
        }

        // Camp enemy GY fallback
        Position camp = (team == TEAM_ALLIANCE) ? AB_GY_CAMPING_HORDE : AB_GY_CAMPING_ALLIANCE;
        float rx, ry, rz;
        bot->GetRandomPoint(camp, 10.0f, rx, ry, rz);
        pos.Set(rx, ry, rz, bot->GetMapId());
        posMap["bg objective"] = pos;
        return false;
    }

    // --- PRIORITY 3: Defender logic ---
    if (isDefender && !slips(15))
    {
        float closestDist = FLT_MAX;
        for (uint32 nodeId : AB_AttackObjectives)
        {
            uint8 state = ab->GetCapturePointInfo(nodeId)._state;

            bool isContested = (team == TEAM_ALLIANCE && state == BG_AB_NODE_STATE_HORDE_CONTESTED) ||
                               (team == TEAM_HORDE && state == BG_AB_NODE_STATE_ALLY_CONTESTED);
            // our own assault counts: the enemy can click it back during the 60s
            bool isOwned = (team == TEAM_ALLIANCE && (state == BG_AB_NODE_STATE_ALLY_OCCUPIED ||
                                                      state == BG_AB_NODE_STATE_ALLY_CONTESTED)) ||
                           (team == TEAM_HORDE && (state == BG_AB_NODE_STATE_HORDE_OCCUPIED ||
                                                   state == BG_AB_NODE_STATE_HORDE_CONTESTED));

            if (!isContested && !isOwned)
                continue;

            GameObject* go = bg->GetBGObject(nodeId * BG_AB_OBJECTS_PER_NODE);
            if (!go)
                continue;

            float dist = bot->GetDistance(go);
            if (dist < closestDist)
            {
                closestDist = dist;
                BgObjective = go;
            }
        }
    }

    // --- PRIORITY 4: Attack objectives ---
    if (!BgObjective)
    {
        std::vector<GameObject*> objectivePool;

        for (uint8 i = 0; i < 3; ++i)
        {
            float bestDist = isSilly ? 0.0f : FLT_MAX;
            GameObject* chosen = nullptr;

            for (uint32 nodeId : AB_AttackObjectives)
            {
                uint8 state = ab->GetCapturePointInfo(nodeId)._state;

                bool isNeutral = state == BG_AB_NODE_STATE_NEUTRAL;
                bool isEnemyOccupied = (team == TEAM_ALLIANCE && state == BG_AB_NODE_STATE_HORDE_OCCUPIED) ||
                                       (team == TEAM_HORDE && state == BG_AB_NODE_STATE_ALLY_OCCUPIED);
                // an enemy assault can be clicked back; our own assault can't
                bool isEnemyContested =
                    (team == TEAM_ALLIANCE && state == BG_AB_NODE_STATE_HORDE_CONTESTED) ||
                    (team == TEAM_HORDE && state == BG_AB_NODE_STATE_ALLY_CONTESTED);

                if (!(isNeutral || isEnemyOccupied || isEnemyContested))
                    continue;

                GameObject* go = bg->GetBGObject(nodeId * BG_AB_OBJECTS_PER_NODE);
                if (!go || std::find(objectivePool.begin(), objectivePool.end(), go) != objectivePool.end())
                    continue;

                float dist = bot->GetDistance(go);
                if ((isSilly && dist > bestDist) || (!isSilly && dist < bestDist))
                {
                    bestDist = dist;
                    chosen = go;
                }
            }

            if (chosen)
                objectivePool.push_back(chosen);
        }

        if (!objectivePool.empty())
            BgObjective = objectivePool[urand(0, objectivePool.size() - 1)];
    }

    // --- Final move ---
    if (BgObjective)
    {
        float rx, ry, rz;
        Position objPos = BgObjective->GetPosition();
        bot->GetRandomPoint(objPos, frand(5.0f, 15.0f), rx, ry, rz);

        pos.Set(rx, ry, rz, BgObjective->GetMapId());
        posMap["bg objective"] = pos;
    }

    return true;
}
