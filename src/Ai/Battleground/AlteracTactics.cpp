// Alterac Valley bot tactics: waypoints, node objectives and the per-bot objective choice.

#include "BattleGroundTactics.h"

#include "BattlegroundAV.h"
#include "Playerbots.h"
#include "PositionValue.h"

// the captains aren't the actual creatures but invisible trigger creatures - they still have correct death state and
// location (unless they move)
uint32 const AV_CREATURE_A_CAPTAIN = AV_CPLACE_TRIGGER16;
uint32 const AV_CREATURE_A_BOSS = AV_CPLACE_A_BOSS;
uint32 const AV_CREATURE_H_CAPTAIN = AV_CPLACE_TRIGGER18;
uint32 const AV_CREATURE_H_BOSS = AV_CPLACE_H_BOSS;

Position const AV_CAVE_SPAWN_ALLIANCE = {872.460f, -491.571f, 96.546f, 0.0f};
Position const AV_CAVE_SPAWN_HORDE = {-1437.127f, -608.382f, 51.185f, 0.0f};
Position const AV_WAITING_POS_ALLIANCE = {793.627f, -493.814f, 99.689f, 3.09f};
Position const AV_WAITING_POS_HORDE = {-1381.865f, -544.872f, 54.773f, 0.76f};
Position const AV_ICEBLOOD_GARRISON_WAITING_ALLIANCE = {-523.105f, -182.178f, 57.956f, 0.0f};
Position const AV_ICEBLOOD_GARRISON_ATTACKING_ALLIANCE = {-545.288f, -167.932f, 57.012f, 0.0f};
Position const AV_STONEHEARTH_WAITING_HORDE = {-36.399f, -306.403f, 15.565f, 0.0f};
Position const AV_STONEHEARTH_ATTACKING_HORDE = {-55.210f, -288.546f, 15.578f, 0.0f};
Position const AV_BOSS_WAIT_H = {689.210f, -20.173f, 50.620f, 0.0f};
Position const AV_BOSS_WAIT_A = {-1361.773f, -248.977f, 99.369f, 0.0f};
Position const AV_MINE_SOUTH_1 = {-860.159f, -12.780f, 70.817f, 0.0f};
Position const AV_MINE_SOUTH_2 = {-827.639f, -147.314f, 62.565f, 0.0f};
Position const AV_MINE_SOUTH_3 = {-920.228f, -134.594f, 61.478f, 0.0f};
Position const AV_MINE_NORTH_1 = {822.477f, -456.782f, 48.569f, 0.0f};
Position const AV_MINE_NORTH_2 = {966.362f, -446.570f, 56.641f, 0.0f};
Position const AV_MINE_NORTH_3 = {952.081f, -335.073f, 63.579f, 0.0f};

std::vector<uint32> const vFlagsAV = {
    BG_AV_OBJECTID_BANNER_H_B,      BG_AV_OBJECTID_BANNER_H,      BG_AV_OBJECTID_BANNER_A_B,
    BG_AV_OBJECTID_BANNER_A,        BG_AV_OBJECTID_BANNER_CONT_A, BG_AV_OBJECTID_BANNER_CONT_A_B,
    BG_AV_OBJECTID_BANNER_CONT_H_B, BG_AV_OBJECTID_BANNER_CONT_H, BG_AV_OBJECTID_BANNER_SNOWFALL_N};

struct AVNodePositionData
{
    Position pos;
    float maxRadius;
};

// Added to fix bot stuck at objectives; graveyards are their banner spots (BG_AV_ObjectPos), as a capture needs 5.5 yd
static std::unordered_map<uint8, AVNodePositionData> AVNodeMovementTargets = {
    {BG_AV_NODES_FIRSTAID_STATION, {Position(638.592f, -32.422f, 46.061f), 4.0f}},
    {BG_AV_NODES_STORMPIKE_GRAVE, {Position(669.007f, -294.078f, 30.291f), 4.0f}},
    {BG_AV_NODES_STONEHEART_GRAVE, {Position(77.801f, -404.700f, 46.755f), 4.0f}},
    {BG_AV_NODES_SNOWFALL_GRAVE, {Position(-202.581f, -112.730f, 78.488f), 4.0f}},
    {BG_AV_NODES_ICEBLOOD_GRAVE, {Position(-611.962f, -396.170f, 60.835f), 4.0f}},
    {BG_AV_NODES_FROSTWOLF_GRAVE, {Position(-1082.450f, -346.823f, 54.922f), 4.0f}},
    {BG_AV_NODES_FROSTWOLF_HUT, {Position(-1402.210f, -307.431f, 89.442f, 0.192f), 4.0f}},
    {BG_AV_NODES_DUNBALDAR_SOUTH, {Position(556.551f, -77.240f, 51.931f), 0.0f}},
    {BG_AV_NODES_DUNBALDAR_NORTH, {Position(670.664f, -142.031f, 63.666f), 0.0f}},
    {BG_AV_NODES_ICEWING_BUNKER, {Position(200.310f, -361.232f, 56.387f), 0.0f}},
    {BG_AV_NODES_STONEHEART_BUNKER, {Position(-156.302f, -440.032f, 40.403f), 0.0f}},
    {BG_AV_NODES_ICEBLOOD_TOWER, {Position(-569.702f, -265.362f, 75.009f), 0.0f}},
    {BG_AV_NODES_TOWER_POINT, {Position(-767.439f, -360.200f, 90.895f), 0.0f}},
    {BG_AV_NODES_FROSTWOLF_ETOWER, {Position(-1303.737f, -314.070f, 113.868f), 0.0f}},
    {BG_AV_NODES_FROSTWOLF_WTOWER, {Position(-1300.648f, -267.356f, 114.151f), 0.0f}},
};

//* ALLIANCE SIDE WAYPOINTS *//
BattleBotPath vPath_AV_AllianceSpawn_To_AllianceCrossroad1 = {
    {768.306f, -492.228f, 97.862f, nullptr}, {758.694f, -489.560f, 96.219f, nullptr},
    {718.949f, -473.185f, 75.285f, nullptr}, {701.759f, -457.199f, 64.398f, nullptr},
    {691.888f, -424.726f, 63.641f, nullptr}, {663.458f, -408.930f, 67.525f, nullptr},
    {650.696f, -400.027f, 67.948f, nullptr}, {621.638f, -383.551f, 67.511f, nullptr},
    {584.176f, -395.038f, 65.210f, nullptr}, {554.333f, -392.831f, 55.495f, nullptr},
    {518.020f, -415.663f, 43.470f, nullptr}, {444.974f, -429.032f, 27.276f, nullptr},
    {418.297f, -409.409f, 9.173f, nullptr},  {400.182f, -392.450f, -1.197f, nullptr}
};

BattleBotPath vPath_AV_AllianceFortress_To_AllianceCrossroad1 = {
    {654.750f, -32.779f, 48.611f, nullptr},  {639.731f, -46.509f, 44.072f, nullptr},
    {630.953f, -64.360f, 41.341f, nullptr},  {634.789f, -85.583f, 41.495f, nullptr},
    {628.271f, -100.943f, 40.328f, nullptr}, {621.330f, -129.900f, 33.687f, nullptr},
    {620.615f, -153.156f, 33.606f, nullptr}, {622.186f, -171.443f, 36.848f, nullptr},
    {624.456f, -188.047f, 38.569f, nullptr}, {629.185f, -222.637f, 38.362f, nullptr},
    {633.239f, -252.286f, 34.275f, nullptr}, {635.941f, -272.053f, 30.130f, nullptr},
    {632.884f, -294.946f, 30.267f, nullptr}, {625.348f, -322.407f, 30.133f, nullptr},
    {608.386f, -335.968f, 30.347f, nullptr}, {588.856f, -331.991f, 30.485f, nullptr},
    {560.140f, -327.872f, 17.396f, nullptr}, {530.523f, -324.481f, 1.718f, nullptr},
    {511.340f, -329.767f, -1.084f, nullptr}, {497.221f, -341.494f, -1.184f, nullptr},
    {465.281f, -365.577f, -1.243f, nullptr}, {437.839f, -376.473f, -1.243f, nullptr},
    {414.292f, -384.557f, -1.243f, nullptr}
};

BattleBotPath vPath_AV_AllianceCrossroad1_To_AllianceCrossroad2 = {
    {391.160f, -391.667f, -1.244f, nullptr}, {369.595f, -388.874f, -0.153f, nullptr},
    {334.986f, -384.282f, -0.726f, nullptr}, {308.373f, -383.026f, -0.336f, nullptr},
    {287.175f, -386.773f, 5.531f, nullptr},  {276.595f, -394.487f, 12.126f, nullptr},
    {264.248f, -405.109f, 23.509f, nullptr}, {253.142f, -412.069f, 31.665f, nullptr},
    {237.243f, -416.570f, 37.103f, nullptr}, {216.805f, -416.987f, 40.497f, nullptr},
    {195.369f, -407.750f, 42.876f, nullptr}, {179.619f, -402.642f, 42.793f, nullptr},
    {157.154f, -391.535f, 43.616f, nullptr}, {137.928f, -380.956f, 43.018f, nullptr},
    {117.990f, -380.243f, 43.571f, nullptr}
};

BattleBotPath vPath_AV_StoneheartGrave_To_AllianceCrossroad2 = {
    {73.141f, -481.863f, 48.479f, nullptr}, {72.974f, -461.923f, 48.498f, nullptr},
    {73.498f, -443.377f, 48.989f, nullptr}, {74.061f, -423.435f, 49.235f, nullptr},
    {83.086f, -403.871f, 47.412f, nullptr}, {102.003f, -388.023f, 45.095f, nullptr}
};

BattleBotPath vPath_AV_AllianceCrossroad2_To_StoneheartBunker = {
    {113.412f, -381.154f, 44.324f, nullptr},  {87.868f, -389.039f, 45.025f, nullptr},
    {62.170f, -388.790f, 45.012f, nullptr},   {30.645f, -390.873f, 45.644f, nullptr},
    {6.239f, -410.405f, 45.157f, nullptr},    {-11.185f, -423.994f, 45.222f, nullptr},
    {-19.595f, -437.877f, 46.383f, nullptr},  {-31.747f, -451.316f, 45.500f, nullptr},
    {-49.279f, -466.706f, 41.101f, nullptr},  {-78.427f, -459.899f, 27.648f, nullptr},
    {-103.623f, -467.766f, 24.806f, nullptr}, {-113.718f, -476.527f, 25.802f, nullptr},
    {-129.793f, -481.160f, 27.735f, nullptr}
};

BattleBotPath vPath_AV_AllianceCrossroad2_To_AllianceCaptain = {
    {117.948f, -363.836f, 43.485f, nullptr}, {110.096f, -342.635f, 41.331f, nullptr},
    {92.672f, -320.463f, 35.312f, nullptr},  {75.147f, -303.007f, 29.411f, nullptr},
    {59.694f, -293.137f, 24.670f, nullptr},  {41.342f, -293.374f, 17.193f, nullptr},
    {22.388f, -299.599f, 14.204f, nullptr}
};

BattleBotPath vPath_AV_AllianceCrossroads3_To_SnowfallGraveyard = {
    {-141.090f, -248.599f, 6.746f, nullptr},  {-147.551f, -233.354f, 9.675f, nullptr},
    {-153.353f, -219.665f, 17.267f, nullptr}, {-159.091f, -205.990f, 26.091f, nullptr},
    {-164.041f, -188.716f, 35.813f, nullptr}, {-170.634f, -166.838f, 51.540f, nullptr},
    {-183.338f, -154.159f, 64.252f, nullptr}, {-193.333f, -139.166f, 74.581f, nullptr},
    {-199.853f, -124.194f, 78.247f, nullptr}
};

BattleBotPath vPath_AV_AllianceCaptain_To_AllianceCrossroad3 = {
    {31.023f, -290.783f, 15.966f, nullptr},   {31.857f, -270.165f, 16.040f, nullptr},
    {26.531f, -242.488f, 14.158f, nullptr},   {3.448f, -241.318f, 11.900f, nullptr},
    {-24.158f, -233.744f, 9.802f, nullptr},   {-55.556f, -235.327f, 10.038f, nullptr},
    {-93.670f, -255.273f, 6.264f, nullptr},   {-117.164f, -263.636f, 6.363f, nullptr}
};

BattleBotPath vPath_AV_AllianceCaptain_To_HordeCrossroad3 = {
    {31.023f, -290.783f, 15.966f, nullptr}, {31.857f, -270.165f, 16.040f, nullptr},
    {26.531f, -242.488f, 14.158f, nullptr}, {3.448f, -241.318f, 11.900f, nullptr},
    {-24.158f, -233.744f, 9.802f, nullptr}, {-55.556f, -235.327f, 10.038f, nullptr},
    {-93.670f, -255.273f, 6.264f, nullptr}, {-117.164f, -263.636f, 6.363f, nullptr},
    {-154.433f, -272.428f, 8.016f, nullptr},  {-165.219f, -277.141f, 9.138f, nullptr},
    {-174.154f, -281.561f, 7.062f, nullptr},  {-189.765f, -290.755f, 6.668f, nullptr},
    {-213.121f, -303.227f, 6.668f, nullptr},  {-240.497f, -315.013f, 6.668f, nullptr},
    {-260.829f, -329.400f, 6.677f, nullptr},  {-280.652f, -344.530f, 6.668f, nullptr},
    {-305.708f, -363.655f, 6.668f, nullptr},  {-326.363f, -377.530f, 6.668f, nullptr},
    {-347.706f, -377.546f, 11.493f, nullptr}, {-361.965f, -373.012f, 13.904f, nullptr},
    {-379.646f, -367.389f, 16.907f, nullptr}, {-399.610f, -353.045f, 17.793f, nullptr},
    {-411.324f, -335.019f, 17.564f, nullptr}, {-421.800f, -316.128f, 17.843f, nullptr},
    {-435.855f, -293.863f, 19.553f, nullptr}, {-454.279f, -277.457f, 21.943f, nullptr},
    {-473.868f, -280.536f, 24.837f, nullptr}, {-492.305f, -289.846f, 29.787f, nullptr},
    {-504.724f, -313.745f, 31.938f, nullptr}, {-518.431f, -333.087f, 34.017f, nullptr}
};

BattleBotPath vPath_AV_StoneheartBunker_To_HordeCrossroad3 = {
    {-507.656f, -342.031f, 33.079f, nullptr}, {-490.580f, -348.193f, 29.170f, nullptr},
    {-458.194f, -343.101f, 31.685f, nullptr}, {-441.632f, -329.773f, 19.349f, nullptr},
    {-429.951f, -333.153f, 18.078f, nullptr}, {-415.858f, -341.834f, 17.865f, nullptr},
    {-406.698f, -361.322f, 17.764f, nullptr}, {-394.097f, -376.775f, 16.309f, nullptr},
    {-380.107f, -393.161f, 10.662f, nullptr}, {-359.474f, -405.379f, 10.437f, nullptr},
    {-343.166f, -415.036f, 10.177f, nullptr}, {-328.328f, -423.823f, 11.057f, nullptr},
    {-315.454f, -431.447f, 17.709f, nullptr}, {-296.180f, -449.228f, 22.316f, nullptr},
    {-272.700f, -459.303f, 28.764f, nullptr}, {-262.987f, -457.030f, 30.248f, nullptr},
    {-244.145f, -452.620f, 25.754f, nullptr}, {-221.311f, -448.195f, 25.166f, nullptr},
    {-201.762f, -457.441f, 27.413f, nullptr}, {-172.130f, -474.654f, 28.884f, nullptr},
    {-153.135f, -480.692f, 30.459f, nullptr}, {-137.077f, -478.766f, 28.798f, nullptr}
};

BattleBotPath vPath_AV_AllianceCrossroad1_To_AllianceMine = {
    {414.329f, -386.483f, -1.244f, nullptr}, {428.252f, -380.010f, -1.244f, nullptr},
    {446.203f, -372.263f, -1.244f, nullptr}, {468.352f, -360.936f, -1.244f, nullptr},
    {490.502f, -344.252f, -1.228f, nullptr}, {515.565f, -328.078f, -1.085f, nullptr},
    {531.431f, -323.435f, 1.906f, nullptr},  {548.612f, -324.795f, 10.162f, nullptr},
    {565.412f, -326.485f, 20.217f, nullptr}, {579.313f, -331.417f, 28.435f, nullptr},
    {593.026f, -336.281f, 30.215f, nullptr}, {614.580f, -331.387f, 30.236f, nullptr},
    {626.524f, -309.520f, 30.375f, nullptr}, {642.822f, -290.896f, 30.201f, nullptr},
    {661.428f, -285.810f, 29.862f, nullptr}, {670.488f, -281.637f, 27.951f, nullptr},
    {687.522f, -273.793f, 23.510f, nullptr}, {706.938f, -272.231f, 31.122f, nullptr},
    {725.715f, -281.845f, 40.529f, nullptr}, {733.208f, -296.224f, 47.418f, nullptr},
    {742.182f, -308.196f, 52.947f, nullptr}, {749.742f, -319.208f, 56.275f, nullptr},
    {760.992f, -335.660f, 60.279f, nullptr}, {783.734f, -342.468f, 61.410f, nullptr},
    {791.843f, -330.986f, 63.040f, nullptr}, {800.683f, -338.452f, 63.286f, nullptr},
    {812.383f, -337.218f, 64.698f, nullptr}, {826.326f, -331.789f, 64.492f, nullptr},
    {839.641f, -338.601f, 65.461f, nullptr}, {851.763f, -345.123f, 65.935f, nullptr},
    {871.511f, -345.464f, 64.947f, nullptr}, {882.320f, -340.819f, 66.864f, nullptr},
    {898.911f, -333.166f, 67.532f, nullptr}, {910.825f, -334.185f, 66.889f, nullptr},
    {922.109f, -336.746f, 66.120f, nullptr}, {929.915f, -353.314f, 65.902f, nullptr},
    {918.736f, -367.359f, 66.307f, nullptr}, {920.196f, -380.444f, 62.599f, nullptr},
    {920.625f, -400.310f, 59.454f, nullptr}, {920.521f, -410.085f, 57.002f, nullptr},
    {917.319f, -424.261f, 56.972f, nullptr}, {909.302f, -429.930f, 58.459f, nullptr},
    {893.164f, -430.083f, 55.782f, nullptr}, {873.846f, -425.108f, 51.387f, nullptr},
    {860.920f, -421.705f, 51.032f, nullptr}, {839.651f, -412.104f, 47.572f, nullptr},
    {831.515f, -410.477f, 47.778f, nullptr}
};
//* ALLIANCE SIDE WAYPOINTS *//

//* HORDE SIDE WAYPOINTS *//
BattleBotPath vPath_AV_HordeSpawn_To_MainRoad = {
    {-1366.182f, -532.170f, 53.354f, nullptr}, {-1325.007f, -516.055f, 51.554f, nullptr},
    {-1273.492f, -514.574f, 50.360f, nullptr}, {-1232.875f, -515.097f, 51.085f, nullptr},
    {-1201.299f, -500.807f, 51.665f, nullptr}, {-1161.199f, -476.367f, 54.956f, nullptr},
    {-1135.679f, -469.551f, 56.934f, nullptr}, {-1113.439f, -458.271f, 52.196f, nullptr},
    {-1086.750f, -444.735f, 52.903f, nullptr}, {-1061.950f, -434.380f, 51.396f, nullptr},
    {-1031.777f, -424.596f, 51.262f, nullptr}, {-967.556f, -399.110f, 49.213f, nullptr}
};

BattleBotPath vPath_AV_SnowfallGraveyard_To_HordeCaptain = {
    {-213.992f, -103.451f, 79.389f, nullptr}, {-222.690f, -95.820f, 77.588f, nullptr},
    {-237.377f, -88.173f, 65.871f, nullptr},  {-253.605f, -85.059f, 56.342f, nullptr},
    {-272.886f, -94.676f, 42.306f, nullptr},  {-289.627f, -108.123f, 26.978f, nullptr},
    {-304.265f, -105.120f, 20.341f, nullptr}, {-316.857f, -92.162f, 22.999f, nullptr},
    {-332.066f, -75.537f, 27.062f, nullptr},  {-358.184f, -76.459f, 27.212f, nullptr},
    {-388.166f, -81.693f, 23.836f, nullptr},  {-407.733f, -90.216f, 23.385f, nullptr},
    {-420.646f, -122.109f, 23.955f, nullptr}, {-418.916f, -143.640f, 24.135f, nullptr},
    {-419.211f, -171.836f, 24.088f, nullptr}, {-425.798f, -195.843f, 26.290f, nullptr},
    {-445.352f, -195.483f, 35.300f, nullptr}, {-464.614f, -194.387f, 49.409f, nullptr},
    {-477.910f, -193.219f, 54.985f, nullptr}, {-488.230f, -187.985f, 56.729f, nullptr}
};

BattleBotPath vPath_AV_HordeCrossroad3_To_IcebloodTower = {
    {-533.792f, -341.435f, 35.860f, nullptr}, {-551.527f, -332.298f, 38.432f, nullptr},
    {-574.093f, -312.653f, 44.791f, nullptr}
};

BattleBotPath vPath_AV_IcebloodTower_To_HordeCaptain = {
    {-569.690f, -295.928f, 49.096f, nullptr}, {-559.809f, -282.641f, 52.074f, nullptr},
    {-546.890f, -261.488f, 53.194f, nullptr}, {-529.471f, -236.931f, 56.746f, nullptr},
    {-518.182f, -222.736f, 56.922f, nullptr}, {-500.372f, -205.938f, 57.364f, nullptr},
    {-494.455f, -190.473f, 57.190f, nullptr}
};

BattleBotPath vPath_AV_IcebloodTower_To_IcebloodGrave = {
    {-584.305f, -313.025f, 47.651f, nullptr}, {-600.831f, -327.032f, 51.026f, nullptr},
    {-613.276f, -343.187f, 54.958f, nullptr}, {-625.873f, -364.812f, 56.829f, nullptr},
    {-625.494f, -390.816f, 58.781f, nullptr}
};

BattleBotPath vPath_AV_IcebloodGrave_To_TowerBottom = {
    {-635.524f, -393.738f, 59.527f, nullptr}, {-659.484f, -386.214f, 63.131f, nullptr},
    {-679.221f, -374.851f, 65.710f, nullptr}, {-694.579f, -368.145f, 66.017f, nullptr},
    {-726.698f, -346.235f, 66.804f, nullptr}, {-743.446f, -345.899f, 66.566f, nullptr},
    {-754.564f, -344.804f, 67.422f, nullptr}
};

BattleBotPath vPath_AV_TowerBottom_To_HordeCrossroad1 = {
    {-764.722f, -339.262f, 67.669f, nullptr},  {-777.559f, -338.964f, 66.287f, nullptr},
    {-796.674f, -341.982f, 61.848f, nullptr},  {-812.721f, -346.317f, 53.286f, nullptr},
    {-826.586f, -350.765f, 50.140f, nullptr},  {-842.410f, -355.642f, 49.750f, nullptr},
    {-861.475f, -361.517f, 50.514f, nullptr},  {-878.634f, -366.805f, 49.987f, nullptr},
    {-897.097f, -374.362f, 48.931f, nullptr},  {-920.177f, -383.808f, 49.487f, nullptr},
    {-947.087f, -392.660f, 48.533f, nullptr},  {-977.268f, -395.606f, 49.426f, nullptr},
    {-993.685f, -394.251f, 50.180f, nullptr},  {-1016.760f, -390.774f, 50.955f, nullptr},
    {-1042.994f, -383.854f, 50.904f, nullptr}, {-1066.925f, -377.541f, 52.535f, nullptr},
    {-1103.309f, -365.939f, 51.502f, nullptr}, {-1127.469f, -354.968f, 51.502f, nullptr}
};

BattleBotPath vPath_AV_HordeCrossroad1_To_FrostwolfGrave = {
    {-1127.565f, -340.254f, 51.753f, nullptr}, {-1112.843f, -337.645f, 53.368f, nullptr},
    {-1089.873f, -334.993f, 54.580f, nullptr}
};

BattleBotPath vPath_AV_HordeCrossroad1_To_HordeFortress = {
    {-1140.070f, -349.834f, 51.090f, nullptr}, {-1161.807f, -352.447f, 51.782f, nullptr},
    {-1182.047f, -361.856f, 52.458f, nullptr}, {-1203.318f, -365.951f, 54.427f, nullptr},
    {-1228.105f, -367.462f, 58.155f, nullptr}, {-1243.013f, -357.409f, 59.866f, nullptr},
    {-1245.382f, -337.206f, 59.322f, nullptr}, {-1236.790f, -323.051f, 60.500f, nullptr},
    {-1227.642f, -311.981f, 63.269f, nullptr}, {-1217.120f, -299.546f, 69.058f, nullptr},
    {-1207.536f, -288.218f, 71.742f, nullptr}, {-1198.808f, -270.859f, 72.431f, nullptr},
    {-1203.695f, -255.038f, 72.498f, nullptr}, {-1220.292f, -252.715f, 73.243f, nullptr},
    {-1236.539f, -252.215f, 73.326f, nullptr}, {-1246.512f, -257.577f, 73.326f, nullptr},
    {-1257.051f, -272.847f, 73.018f, nullptr}, {-1265.600f, -284.769f, 77.939f, nullptr},
    {-1282.656f, -290.696f, 88.334f, nullptr}, {-1292.589f, -290.809f, 90.446f, nullptr},
    {-1307.385f, -290.950f, 90.681f, nullptr}, {-1318.955f, -291.061f, 90.451f, nullptr},
    {-1332.717f, -291.117f, 90.806f, nullptr}, {-1346.880f, -287.015f, 91.066f, nullptr}
};

BattleBotPath vPath_AV_FrostwolfGrave_To_HordeMine = {
    {-1075.070f, -332.081f, 55.758f, nullptr}, {-1055.926f, -326.469f, 57.026f, nullptr},
    {-1036.115f, -328.239f, 59.368f, nullptr}, {-1018.217f, -332.306f, 59.335f, nullptr},
    {-990.396f, -337.397f, 58.342f, nullptr},  {-963.987f, -335.529f, 60.945f, nullptr},
    {-954.477f, -321.258f, 63.429f, nullptr},  {-951.052f, -301.476f, 64.761f, nullptr},
    {-955.257f, -282.794f, 63.683f, nullptr},  {-960.355f, -261.040f, 64.498f, nullptr},
    {-967.410f, -230.933f, 67.408f, nullptr},  {-967.187f, -207.444f, 68.924f, nullptr},
};
//* HORDE SIDE WAYPOINTS *//

std::vector<BattleBotPath*> const vPaths_AV = {
    &vPath_AV_AllianceSpawn_To_AllianceCrossroad1,
    &vPath_AV_AllianceFortress_To_AllianceCrossroad1,
    &vPath_AV_AllianceCrossroad1_To_AllianceCrossroad2,
    &vPath_AV_StoneheartGrave_To_AllianceCrossroad2,
    &vPath_AV_AllianceCrossroad2_To_StoneheartBunker,
    &vPath_AV_AllianceCrossroad2_To_AllianceCaptain,
    &vPath_AV_AllianceCaptain_To_HordeCrossroad3,
    &vPath_AV_AllianceCrossroads3_To_SnowfallGraveyard,
    //&vPath_AV_AllianceCaptain_To_AllianceCrossroad3,
    &vPath_AV_StoneheartBunker_To_HordeCrossroad3,
    &vPath_AV_AllianceCrossroad1_To_AllianceMine,
    &vPath_AV_HordeSpawn_To_MainRoad,
    &vPath_AV_SnowfallGraveyard_To_HordeCaptain,
    &vPath_AV_HordeCrossroad3_To_IcebloodTower,
    &vPath_AV_IcebloodTower_To_HordeCaptain,
    &vPath_AV_IcebloodTower_To_IcebloodGrave,
    &vPath_AV_IcebloodGrave_To_TowerBottom,
    &vPath_AV_TowerBottom_To_HordeCrossroad1,
    &vPath_AV_HordeCrossroad1_To_FrostwolfGrave,
    &vPath_AV_HordeCrossroad1_To_HordeFortress,
    &vPath_AV_FrostwolfGrave_To_HordeMine
};

static std::vector<std::pair<uint8, uint32>> AV_AttackObjectives_Horde = {
    // Attack - these are in order they should be attacked
    {BG_AV_NODES_STONEHEART_GRAVE, BG_AV_OBJECT_FLAG_A_STONEHEART_GRAVE},
    {BG_AV_NODES_STONEHEART_BUNKER, BG_AV_OBJECT_FLAG_A_STONEHEART_BUNKER},
    {BG_AV_NODES_ICEWING_BUNKER, BG_AV_OBJECT_FLAG_A_ICEWING_BUNKER},
    {BG_AV_NODES_STORMPIKE_GRAVE, BG_AV_OBJECT_FLAG_A_STORMPIKE_GRAVE},
    {BG_AV_NODES_DUNBALDAR_SOUTH, BG_AV_OBJECT_FLAG_A_DUNBALDAR_SOUTH},
    {BG_AV_NODES_DUNBALDAR_NORTH, BG_AV_OBJECT_FLAG_A_DUNBALDAR_NORTH},
    {BG_AV_NODES_FIRSTAID_STATION, BG_AV_OBJECT_FLAG_A_FIRSTAID_STATION},
};

static std::vector<std::pair<uint8, uint32>> AV_AttackObjectives_Alliance = {
    // Attack - these are in order they should be attacked
    {BG_AV_NODES_ICEBLOOD_GRAVE, BG_AV_OBJECT_FLAG_H_ICEBLOOD_GRAVE},
    {BG_AV_NODES_ICEBLOOD_TOWER, BG_AV_OBJECT_FLAG_H_ICEBLOOD_TOWER},
    {BG_AV_NODES_TOWER_POINT, BG_AV_OBJECT_FLAG_H_TOWER_POINT},
    {BG_AV_NODES_FROSTWOLF_GRAVE, BG_AV_OBJECT_FLAG_H_FROSTWOLF_GRAVE},
    {BG_AV_NODES_FROSTWOLF_ETOWER, BG_AV_OBJECT_FLAG_H_FROSTWOLF_ETOWER},
    {BG_AV_NODES_FROSTWOLF_WTOWER, BG_AV_OBJECT_FLAG_H_FROSTWOLF_WTOWER},
    {BG_AV_NODES_FROSTWOLF_HUT, BG_AV_OBJECT_FLAG_H_FROSTWOLF_HUT},
};

static std::vector<std::pair<uint8, uint32>> AV_DefendObjectives_Horde = {
    {BG_AV_NODES_FROSTWOLF_HUT, BG_AV_OBJECT_FLAG_H_FROSTWOLF_HUT},
    {BG_AV_NODES_FROSTWOLF_WTOWER, BG_AV_OBJECT_FLAG_H_FROSTWOLF_WTOWER},
    {BG_AV_NODES_FROSTWOLF_ETOWER, BG_AV_OBJECT_FLAG_H_FROSTWOLF_ETOWER},
    {BG_AV_NODES_FROSTWOLF_GRAVE, BG_AV_OBJECT_FLAG_H_FROSTWOLF_GRAVE},
    {BG_AV_NODES_TOWER_POINT, BG_AV_OBJECT_FLAG_H_TOWER_POINT},
    {BG_AV_NODES_ICEBLOOD_TOWER, BG_AV_OBJECT_FLAG_H_ICEBLOOD_TOWER},
    {BG_AV_NODES_ICEBLOOD_GRAVE, BG_AV_OBJECT_FLAG_H_ICEBLOOD_GRAVE},
};

static std::vector<std::pair<uint8, uint32>> AV_DefendObjectives_Alliance = {
    {BG_AV_NODES_FIRSTAID_STATION, BG_AV_OBJECT_FLAG_A_FIRSTAID_STATION},
    {BG_AV_NODES_DUNBALDAR_SOUTH, BG_AV_OBJECT_FLAG_A_DUNBALDAR_SOUTH},
    {BG_AV_NODES_DUNBALDAR_NORTH, BG_AV_OBJECT_FLAG_A_DUNBALDAR_NORTH},
    {BG_AV_NODES_STORMPIKE_GRAVE, BG_AV_OBJECT_FLAG_A_STORMPIKE_GRAVE},
    {BG_AV_NODES_ICEWING_BUNKER, BG_AV_OBJECT_FLAG_A_ICEWING_BUNKER},
    {BG_AV_NODES_STONEHEART_BUNKER, BG_AV_OBJECT_FLAG_A_STONEHEART_BUNKER},
    {BG_AV_NODES_STONEHEART_GRAVE, BG_AV_OBJECT_FLAG_A_STONEHEART_GRAVE},
};

void BGTactics::moveToStartAV(Battleground* bg)
{
    if (bot->GetTeamId() == TEAM_HORDE)
        MoveTo(bg->GetMapId(), AV_WAITING_POS_HORDE.GetPositionX() + frand(-5.0f, 5.0f),
               AV_WAITING_POS_HORDE.GetPositionY() + frand(-3.0f, 3.0f), AV_WAITING_POS_HORDE.GetPositionZ());
    else
        MoveTo(bg->GetMapId(), AV_WAITING_POS_ALLIANCE.GetPositionX() + frand(-5.0f, 5.0f),
               AV_WAITING_POS_ALLIANCE.GetPositionY() + frand(-3.0f, 3.0f), AV_WAITING_POS_ALLIANCE.GetPositionZ());
}

bool BGTactics::avLeaveCave()
{
    // get bots out of cave when they respawn there (otherwise path selection happens while they're deep within cave
    // and the results arent good)
    Position const caveSpawn = bot->GetTeamId() == TEAM_ALLIANCE ? AV_CAVE_SPAWN_ALLIANCE : AV_CAVE_SPAWN_HORDE;
    if (bot->GetDistance(caveSpawn) < 16.0f)
    {
        return moveToStart(true);
    }
    return false;
}

bool BGTactics::selectObjectiveAV(Battleground* bg, PositionMap& posMap, PositionInfo& pos)
{
    WorldObject* BgObjective = nullptr;

    BattlegroundAV* av = static_cast<BattlegroundAV*>(bg);
    TeamId team = bot->GetTeamId();
    uint8 role = context->GetValue<uint32>("bg role")->Get();
    AVBotStrategy strategyHorde = static_cast<AVBotStrategy>(GetBotStrategyForTeam(bg, TEAM_HORDE));
    AVBotStrategy strategyAlliance = static_cast<AVBotStrategy>(GetBotStrategyForTeam(bg, TEAM_ALLIANCE));
    AVBotStrategy strategy = (team == TEAM_ALLIANCE) ? strategyAlliance : strategyHorde;
    AVBotStrategy enemyStrategy = (team == TEAM_ALLIANCE) ? strategyHorde : strategyAlliance;

    uint8 defendersProhab = 4;
    bool enableMineCapture = true;
    bool enableSnowfall = true;

    switch (strategy)
    {
        case AV_STRATEGY_BALANCED:
            defendersProhab = 4;
            break;
        case AV_STRATEGY_OFFENSIVE:
            defendersProhab = 1;
            enableMineCapture = false;
            break;
        case AV_STRATEGY_DEFENSIVE:
            defendersProhab = 9;
            enableSnowfall = false;
            break;
        default:
            break;
    }

    if (enemyStrategy == AV_STRATEGY_DEFENSIVE)
        defendersProhab = 0;

    bool isDefender = role < defendersProhab;
    bool isAdvanced = !isDefender && role > 8;

    auto const& attackObjectives =
        (team == TEAM_HORDE) ? AV_AttackObjectives_Horde : AV_AttackObjectives_Alliance;
    auto const& defendObjectives =
        (team == TEAM_HORDE) ? AV_DefendObjectives_Horde : AV_DefendObjectives_Alliance;

    uint32 destroyedNodes = 0;
    for (auto const& [nodeId, _] : defendObjectives)
        if (av->GetAVNodeInfo(nodeId).State == POINT_DESTROYED)
            destroyedNodes++;

    float botX = bot->GetPositionX();
    if (isDefender)
    {
        if ((team == TEAM_HORDE && botX >= -62.0f) || (team == TEAM_ALLIANCE && botX <= -462.0f))
            isDefender = false;
    }

    if (isDefender && destroyedNodes > 0)
    {
        uint32 switchChance = 20 + (destroyedNodes * 15);
        if (urand(0, 99) < switchChance)
            isDefender = false;
    }

    // --- Mine Capture (rarely works, needs some improvement) ---
    if (!BgObjective && enableMineCapture && role == 0)
    {
        BG_AV_OTHER_VALUES mineType = (team == TEAM_HORDE) ? AV_SOUTH_MINE : AV_NORTH_MINE;
        if (av->GetMineOwner(mineType) != team)
        {
            uint32 bossEntry = (team == TEAM_HORDE) ? AV_CPLACE_MINE_S_3 : AV_CPLACE_MINE_N_3;
            Creature* mBossNeutral = bg->GetBGCreature(bossEntry);
            Position const* minePositions[] = {(team == TEAM_HORDE) ? &AV_MINE_SOUTH_1 : &AV_MINE_NORTH_1,
                                               (team == TEAM_HORDE) ? &AV_MINE_SOUTH_2 : &AV_MINE_NORTH_2,
                                               (team == TEAM_HORDE) ? &AV_MINE_SOUTH_3 : &AV_MINE_NORTH_3};

            Position const* chosen = minePositions[urand(0, 2)];
            pos.Set(chosen->GetPositionX(), chosen->GetPositionY(), chosen->GetPositionZ(), bot->GetMapId());
            posMap["bg objective"] = pos;
            BgObjective = mBossNeutral;
        }
    }

    // --- Nearby Enemy ---
    if (!BgObjective && slips(8))
    {
        if (Unit* enemy = AI_VALUE(Unit*, "enemy player target"))
        {
            if (bot->GetDistance(enemy) < 500.0f)
            {
                pos.Set(enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetPositionZ(), bot->GetMapId());
                posMap["bg objective"] = pos;
                BgObjective = enemy;
            }
        }
    }

    // --- Snowfall ---
    bool hasSnowfallRole = enableSnowfall && ((team == TEAM_ALLIANCE && role < 6) || (team == TEAM_HORDE && role < 5));

    if (!BgObjective && hasSnowfallRole)
    {
        BG_AV_NodeInfo const& snowfallNode = av->GetAVNodeInfo(BG_AV_NODES_SNOWFALL_GRAVE);
        if (snowfallNode.OwnerId == TEAM_NEUTRAL)
        {
            if (GameObject* go = bg->GetBGObject(BG_AV_OBJECT_FLAG_N_SNOWFALL_GRAVE))
            {
                Position objPos = go->GetPosition();
                float rx, ry, rz;
                bot->GetRandomPoint(objPos, frand(5.0f, 15.0f), rx, ry, rz);

                pos.Set(rx, ry, rz, go->GetMapId());
                posMap["bg objective"] = pos;
                BgObjective = go;
            }
        }
    }

    // --- Captain ---
    if (!BgObjective && !isDefender && urand(0, 99) < 90)
    {
        if (av->IsCaptainAlive(team == TEAM_HORDE ? TEAM_ALLIANCE : TEAM_HORDE))
        {
            uint32 creatureId = (team == TEAM_HORDE) ? AV_CREATURE_A_CAPTAIN : AV_CREATURE_H_CAPTAIN;
            if (Creature* captain = bg->GetBGCreature(creatureId))
            {
                if (captain->IsAlive())
                {
                    BgObjective = captain;
                }
            }
        }
    }

    // --- Defender Logic ---
    if (!BgObjective && isDefender)
    {
        std::vector<GameObject*> contestedObjectives;
        std::vector<GameObject*> availableObjectives;

        for (auto const& [nodeId, goId] : defendObjectives)
        {
            BG_AV_NodeInfo const& node = av->GetAVNodeInfo(nodeId);
            if (node.State == POINT_DESTROYED)
                continue;

            GameObject* go = bg->GetBGObject(goId);
            if (!go)
                continue;

            if (node.State == POINT_ASSAULTED)
                contestedObjectives.push_back(go);
            else
                availableObjectives.push_back(go);
        }

        // pick by role so a bot keeps the same post across objective re-checks
        if (!contestedObjectives.empty())
            BgObjective = contestedObjectives[role % contestedObjectives.size()];
        else if (!availableObjectives.empty())
            BgObjective = availableObjectives[role % availableObjectives.size()];
    }

    // --- Enemy Boss ---
    if (!BgObjective)
    {
        uint32 towersDown = 0;
        for (auto const& [nodeId, _] : attackObjectives)
            if (av->GetAVNodeInfo(nodeId).State == POINT_DESTROYED)
                towersDown++;

        if ((towersDown >= 2) || (strategy == AV_STRATEGY_OFFENSIVE))
        {
            uint8 lastGY = (team == TEAM_HORDE) ? BG_AV_NODES_FIRSTAID_STATION : BG_AV_NODES_FROSTWOLF_HUT;
            // OwnerId flips on assault already; only a finished capture counts
            bool ownsFinalGY = av->GetAVNodeInfo(lastGY).TotalOwnerId == team;

            uint32 bossId = (team == TEAM_HORDE) ? AV_CREATURE_A_BOSS : AV_CREATURE_H_BOSS;
            if (Creature* boss = bg->GetBGCreature(bossId))
            {
                if (boss->IsAlive())
                {
                    uint32 nearbyCount = getPlayersInArea(team, boss->GetPosition(), 200.0f, false);
                    if (ownsFinalGY || nearbyCount >= 20)
                        BgObjective = boss;
                }
            }
        }
    }

    // --- Attacker Logic ---
    if (!BgObjective)
    {
        std::vector<GameObject*> candidates;

        for (auto const& [nodeId, goId] : attackObjectives)
        {
            BG_AV_NodeInfo const& node = av->GetAVNodeInfo(nodeId);
            GameObject* go = bg->GetBGObject(goId);
            // a graveyard we took that the enemy is assaulting back
            bool const retaking =
                node.TotalOwnerId == team && node.State == POINT_ASSAULTED && node.OwnerId != team;
            if (!go || node.State == POINT_DESTROYED || (node.TotalOwnerId == team && !retaking))
                continue;

            if (node.State == POINT_ASSAULTED && !retaking && urand(0, 99) >= 1)
                continue;

            candidates.push_back(go);

            if (((strategy == AV_STRATEGY_BALANCED && candidates.size() >= 2) ||
                 (strategy == AV_STRATEGY_OFFENSIVE && candidates.size() >= 3) ||
                 (strategy == AV_STRATEGY_DEFENSIVE && candidates.size() >= 1)) ||
                isAdvanced)
                break;
        }

        if (!candidates.empty())
            BgObjective = candidates[role % candidates.size()];
        else
        {
            // Fallback: gather at the boss wait position; the boss branch above decides when to pull
            Position const& waitPos = (team == TEAM_HORDE) ? AV_BOSS_WAIT_H : AV_BOSS_WAIT_A;

            float rx, ry, rz;
            bot->GetRandomPoint(waitPos, 5.0f, rx, ry, rz);

            pos.Set(rx, ry, rz, bot->GetMapId());
            posMap["bg objective"] = pos;
            return true;
        }
    }

    // --- Movement logic for any valid objective ---
    if (BgObjective)
    {
        Position objPos = BgObjective->GetPosition();

        Optional<uint8> linkedNodeId;
        for (auto const& [nodeId, goId] : attackObjectives)
            if (bg->GetBGObject(goId) == BgObjective)
                linkedNodeId = nodeId;

        if (!linkedNodeId)
        {
            for (auto const& [nodeId, goId] : defendObjectives)
                if (bg->GetBGObject(goId) == BgObjective)
                    linkedNodeId = nodeId;
        }

        float rx, ry, rz;
        if (linkedNodeId && AVNodeMovementTargets.count(*linkedNodeId))
        {
            AVNodePositionData const& data = AVNodeMovementTargets[*linkedNodeId];
            bot->GetRandomPoint(data.pos, frand(-data.maxRadius, data.maxRadius), rx, ry, rz);
        }
        else
            bot->GetRandomPoint(objPos, frand(-2.0f, 2.0f), rx, ry, rz);

        pos.Set(rx, ry, rz, BgObjective->GetMapId());
        posMap["bg objective"] = pos;
        return true;
    }

    return false;
}
