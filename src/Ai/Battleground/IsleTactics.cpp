// Isle of Conquest bot tactics: waypoints, start spots, objectives and the keep portals.

#include "BattleGroundTactics.h"

#include <array>

#include "Battleground.h"
#include "BattlegroundIC.h"
#include "GameObject.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "ServerFacade.h"

Position const IC_WAITING_POS_HORDE = {1166.322f, -762.402f, 48.628f};
Position const IC_WEST_WAITING_POS_HORDE = {1217.666f, -685.449f, 48.915f};
Position const IC_EAST_WAITING_POS_HORDE = {1219.068f, -838.791f, 48.916f};

Position const IC_WAITING_POS_ALLIANCE = {387.893f, -833.384f, 48.714f};
Position const IC_WEST_WAITING_POS_ALLIANCE = {352.129f, -788.029f, 48.916f};
Position const IC_EAST_WAITING_POS_ALLIANCE = {351.517f, -882.477f, 48.916f};

Position const IC_CANNON_POS_HORDE1 = {1140.938f, -838.685f, 88.124f, 2.30f};
Position const IC_CANNON_POS_HORDE2 = {1139.695f, -686.574f, 88.173f, 3.95f};
Position const IC_CANNON_POS_ALLIANCE1 = {424.860f, -855.795f, 87.96f, 0.44f};
Position const IC_CANNON_POS_ALLIANCE2 = {425.525f, -779.538f, 87.717f, 5.88f};

Position const IC_GATE_ATTACK_POS_HORDE = {478.3f, -830.2f, 40.0f, 0.0f};  // z from the map at runtime
Position const IC_GATE_ATTACK_POS_ALLIANCE = {1091.273f, -763.619f, 42.352f, 0.0f};

std::vector<uint32> const vFlagsIC = {GO_HORDE_BANNER,
                                      GO_ALLIANCE_BANNER,
                                      GO_WORKSHOP_BANNER,
                                      GO_DOCKS_BANNER,
                                      GO_HANGAR_BANNER,
                                      GO_QUARRY_BANNER,
                                      GO_REFINERY_BANNER,
                                      GO_ALLIANCE_BANNER_DOCK,
                                      GO_ALLIANCE_BANNER_DOCK_CONT,
                                      GO_HORDE_BANNER_DOCK,
                                      GO_HORDE_BANNER_DOCK_CONT,
                                      GO_HORDE_BANNER_HANGAR,
                                      GO_HORDE_BANNER_HANGAR_CONT,
                                      GO_ALLIANCE_BANNER_HANGAR,
                                      GO_ALLIANCE_BANNER_HANGAR_CONT,
                                      GO_ALLIANCE_BANNER_QUARRY,
                                      GO_ALLIANCE_BANNER_QUARRY_CONT,
                                      GO_HORDE_BANNER_QUARRY,
                                      GO_HORDE_BANNER_QUARRY_CONT,
                                      GO_ALLIANCE_BANNER_REFINERY,
                                      GO_ALLIANCE_BANNER_REFINERY_CONT,
                                      GO_HORDE_BANNER_REFINERY,
                                      GO_HORDE_BANNER_REFINERY_CONT,
                                      GO_ALLIANCE_BANNER_WORKSHOP,
                                      GO_ALLIANCE_BANNER_WORKSHOP_CONT,
                                      GO_HORDE_BANNER_WORKSHOP,
                                      GO_HORDE_BANNER_WORKSHOP_CONT,
                                      GO_ALLIANCE_BANNER_GRAVEYARD_A,
                                      GO_ALLIANCE_BANNER_GRAVEYARD_A_CONT,
                                      GO_HORDE_BANNER_GRAVEYARD_A,
                                      GO_HORDE_BANNER_GRAVEYARD_A_CONT,
                                      GO_ALLIANCE_BANNER_GRAVEYARD_H,
                                      GO_ALLIANCE_BANNER_GRAVEYARD_H_CONT,
                                      GO_HORDE_BANNER_GRAVEYARD_H,
                                      GO_HORDE_BANNER_GRAVEYARD_H_CONT};

BattleBotPath vPath_IC_Ally_Keep_to_Ally_Front_Crossroad = {
    //{ 351.652f, -834.837f, 48.916f, nullptr },
    {434.768f, -833.976f, 46.090f, nullptr},
    {506.782f, -828.594f, 24.313f, nullptr},
    {524.955f, -799.002f, 19.498f, nullptr}};

BattleBotPath vPath_IC_Ally_Front_Crossroad_to_Workshop = {
    {524.955f, -799.002f, 19.498f, nullptr}, {573.557f, -804.838f, 9.6291f, nullptr},
    {627.977f, -810.197f, 3.5154f, nullptr}, {681.501f, -805.208f, 3.1464f, nullptr},
    {721.905f, -797.917f, 4.5112f, nullptr}, {774.466f, -801.058f, 6.3428f, nullptr}};

BattleBotPath vPath_IC_Ally_Keep_to_Ally_Dock_Crossroad = {{434.768f, -833.976f, 46.090f, nullptr},
                                                           {446.710f, -776.008f, 48.783f, nullptr},
                                                           {463.745f, -742.368f, 48.584f, nullptr},
                                                           {488.201f, -714.563f, 36.564f, nullptr},
                                                           {525.923f, -666.880f, 25.425f, nullptr}};

BattleBotPath vPath_IC_Ally_Front_Crossroad_to_Ally_Dock_Crossroad = {{524.955f, -799.002f, 19.498f, nullptr},
                                                                      {542.225f, -745.142f, 18.348f, nullptr},
                                                                      {545.309f, -712.497f, 22.005f, nullptr},
                                                                      {538.678f, -748.361f, 18.261f, nullptr},
                                                                      {525.923f, -666.880f, 25.425f, nullptr}};

BattleBotPath vPath_IC_Lower_Graveyard_to_Lower_Graveyard_Crossroad = {{443.095f, -310.797f, 51.749f, nullptr},
                                                                       {462.733f, -323.587f, 48.706f, nullptr},
                                                                       {471.540f, -343.914f, 40.706f, nullptr},
                                                                       {475.622f, -360.728f, 34.384f, nullptr},
                                                                       {484.458f, -379.796f, 33.122f, nullptr}};

BattleBotPath vPath_IC_Lower_Graveyard_Crossroad_to_Ally_Docks_Crossroad = {
    {484.458f, -379.796f, 33.122f, nullptr}, {509.786f, -380.592f, 33.122f, nullptr},
    {532.549f, -381.576f, 33.122f, nullptr}, {553.506f, -386.102f, 33.507f, nullptr},
    {580.533f, -398.536f, 33.416f, nullptr}, {605.112f, -409.843f, 33.121f, nullptr},
    {619.212f, -419.169f, 33.121f, nullptr}, {631.702f, -428.763f, 33.070f, nullptr},
    {648.483f, -444.714f, 28.629f, nullptr}};

BattleBotPath vPath_IC_Lower_Graveyard_Crossroad_to_Ally_Docks_Second_Crossroad = {
    {484.458f, -379.796f, 33.122f, nullptr}, {470.771f, -394.789f, 33.112f, nullptr},
    {461.191f, -409.475f, 33.120f, nullptr}, {452.794f, -431.842f, 33.120f, nullptr},
    {452.794f, -456.896f, 33.658f, nullptr}, {453.279f, -481.742f, 33.052f, nullptr},
    {453.621f, -504.979f, 32.956f, nullptr}, {452.006f, -526.792f, 32.221f, nullptr},
    {453.150f, -548.212f, 29.133f, nullptr}, {455.224f, -571.323f, 26.119f, nullptr},
    {465.486f, -585.424f, 25.756f, nullptr}, {475.366f, -598.414f, 25.784f, nullptr},
    {477.702f, -605.757f, 25.714f, nullptr}};

BattleBotPath vPath_IC_Ally_Dock_Crossroad_to_Ally_Docks_Second_Crossroad = {{525.923f, -666.880f, 25.425f, nullptr},
                                                                             {497.190f, -630.709f, 25.626f, nullptr},
                                                                             {477.702f, -605.757f, 25.714f, nullptr}};

BattleBotPath vPath_IC_Ally_Docks_Second_Crossroad_to_Ally_Docks_Crossroad = {{477.702f, -605.757f, 25.714f, nullptr},
                                                                              {493.697f, -555.838f, 26.014f, nullptr},
                                                                              {522.939f, -525.199f, 26.014f, nullptr},
                                                                              {580.398f, -486.274f, 26.013f, nullptr},
                                                                              {650.132f, -445.811f, 28.503f, nullptr}};

BattleBotPath vPath_IC_Ally_Docks_Crossroad_to_Docks_Flag = {{650.132f, -445.811f, 28.503f, nullptr},
                                                             {690.527f, -452.961f, 18.039f, nullptr},
                                                             {706.813f, -430.003f, 13.797f, nullptr},
                                                             {726.427f, -364.849f, 17.815f, nullptr}};

BattleBotPath vPath_IC_Docks_Graveyard_to_Docks_Flag = {
    {638.142f, -283.782f, 11.512f, nullptr}, {655.760f, -284.433f, 13.220f, nullptr},
    {661.656f, -299.912f, 12.756f, nullptr}, {675.068f, -317.192f, 12.627f, nullptr},
    {692.712f, -323.866f, 12.686f, nullptr}, {712.968f, -341.285f, 13.350f, nullptr},
    {726.427f, -364.849f, 17.815f, nullptr}};

BattleBotPath vPath_IC_Ally_Keep_to_Quarry_Crossroad = {
    {320.547f, -919.896f, 48.481f, nullptr}, {335.384f, -922.371f, 49.518f, nullptr},
    {353.471f, -920.316f, 48.660f, nullptr}, {353.305f, -958.823f, 47.665f, nullptr},
    {369.196f, -989.960f, 37.719f, nullptr}, {380.671f, -1023.51f, 29.369f, nullptr}};

BattleBotPath vPath_IC_Quarry_Crossroad_to_Quarry_Flag = {
    {380.671f, -1023.51f, 29.369f, nullptr}, {361.584f, -1052.89f, 27.445f, nullptr},
    {341.853f, -1070.17f, 24.024f, nullptr}, {295.845f, -1075.22f, 16.164f, nullptr},
    {253.030f, -1094.06f, 4.1517f, nullptr}, {211.391f, -1121.80f, 1.9591f, nullptr},
    {187.836f, -1155.66f, 1.9749f, nullptr}, {221.193f, -1186.87f, 8.0247f, nullptr},
    {249.181f, -1162.09f, 16.687f, nullptr}};

BattleBotPath vPath_IC_Ally_Front_Crossroad_to_Hangar_First_Crossroad = {{524.955f, -799.002f, 19.498f, nullptr},
                                                                         {512.563f, -840.166f, 23.913f, nullptr},
                                                                         {513.418f, -877.726f, 26.333f, nullptr},
                                                                         {512.962f, -945.951f, 39.382f, nullptr}};

BattleBotPath vPath_IC_Ally_Keep_to_Hangar_First_Crossroad = {{434.768f, -833.976f, 46.090f, nullptr},
                                                              {486.355f, -909.736f, 26.112f, nullptr},
                                                              {512.962f, -945.951f, 39.382f, nullptr}};

BattleBotPath vPath_IC_Hangar_First_Crossroad_to_Hangar_Second_Crossroad = {{512.962f, -945.951f, 39.382f, nullptr},
                                                                            {499.525f, -985.850f, 47.659f, nullptr},
                                                                            {492.794f, -1016.36f, 49.834f, nullptr},
                                                                            {481.738f, -1052.67f, 60.190f, nullptr}};

BattleBotPath vPath_IC_Quarry_Crossroad_to_Hangar_Second_Crossroad = {{380.671f, -1023.51f, 29.369f, nullptr},
                                                                      {430.997f, -1021.72f, 31.021f, nullptr},
                                                                      {439.528f, -1044.88f, 41.827f, nullptr},
                                                                      {455.062f, -1060.67f, 67.209f, nullptr},
                                                                      {481.738f, -1052.67f, 60.190f, nullptr}};

BattleBotPath vPath_IC_Hangar_Second_Crossroad_to_Hangar_Flag = {
    {508.945f, -1103.30f, 79.054f, nullptr}, {536.397f, -1145.79f, 95.478f, nullptr},
    {573.242f, -1138.19f, 109.26f, nullptr}, {609.051f, -1112.93f, 128.31f, nullptr},
    {645.569f, -1094.58f, 132.13f, nullptr}, {689.621f, -1068.33f, 132.87f, nullptr},
    {730.045f, -1042.67f, 133.03f, nullptr}, {755.322f, -1030.28f, 133.30f, nullptr},
    {801.685f, -1005.46f, 132.39f, nullptr}, {806.404f, -1001.709f, 132.382f, nullptr}};

BattleBotPath vPath_IC_Horde_Keep_to_Horde_Front_Crossroad = {{1128.646f, -763.221f, 48.385f, nullptr},
                                                              {1091.273f, -763.619f, 42.352f, nullptr},
                                                              {1032.825f, -763.024f, 30.420f, nullptr},
                                                              {991.4235f, -807.672f, 21.788f, nullptr}};

BattleBotPath vPath_IC_Horde_Front_Crossroad_to_Horde_Hangar_Crossroad = {{991.4235f, -807.672f, 21.788f, nullptr},
                                                                          {999.1844f, -855.182f, 21.484f, nullptr},
                                                                          {1012.089f, -923.098f, 19.296f, nullptr}};

BattleBotPath vPath_IC_Horde_Keep_to_Horde_Hangar_Crossroad = {{1128.646f, -763.221f, 48.385f, nullptr},
                                                               {1121.090f, -816.666f, 49.008f, nullptr},
                                                               {1107.106f, -851.459f, 48.804f, nullptr},
                                                               {1072.313f, -888.355f, 30.853f, nullptr},
                                                               {1012.089f, -923.098f, 19.296f, nullptr}};

BattleBotPath vPath_IC_Horde_Hangar_Crossroad_to_Hangar_Flag = {
    {1001.745f, -973.174f, 15.784f, nullptr},  {1015.437f, -1019.47f, 15.578f, nullptr},
    {1009.622f, -1067.78f, 15.777f, nullptr},  {988.0692f, -1113.32f, 18.254f, nullptr},
    {943.7221f, -1134.50f, 32.296f, nullptr},  {892.2205f, -1115.16f, 63.319f, nullptr},
    {849.6576f, -1090.88f, 91.943f, nullptr},  {814.9168f, -1056.42f, 117.275f, nullptr},
    {799.0856f, -1034.62f, 129.000f, nullptr}, {801.685f, -1005.46f, 132.39f, nullptr}};

BattleBotPath vPath_IC_Horde_Keep_to_Horde_Dock_Crossroad = {{1128.646f, -763.221f, 48.385f, nullptr},
                                                             {1116.203f, -723.328f, 48.655f, nullptr},
                                                             {1093.246f, -696.880f, 37.041f, nullptr},
                                                             {1034.226f, -653.581f, 24.432f, nullptr}};

BattleBotPath vPath_IC_Horde_Front_Crossroad_to_Horde_Dock_Crossroad = {{991.4235f, -807.672f, 21.788f, nullptr},
                                                                        {1025.305f, -757.165f, 29.241f, nullptr},
                                                                        {1029.308f, -710.366f, 26.366f, nullptr},
                                                                        {1034.226f, -653.581f, 24.432f, nullptr}};

BattleBotPath vPath_IC_Horde_Dock_Crossroad_to_Refinery_Crossroad = {{1034.226f, -653.581f, 24.432f, nullptr},
                                                                     {1102.358f, -617.505f, 5.4963f, nullptr},
                                                                     {1116.255f, -580.956f, 18.184f, nullptr},
                                                                     {1114.414f, -546.731f, 23.422f, nullptr},
                                                                     {1148.358f, -503.947f, 23.423f, nullptr}};

BattleBotPath vPath_IC_Refinery_Crossroad_to_Refinery_Base = {{1148.358f, -503.947f, 23.423f, nullptr},
                                                              {1201.885f, -500.425f, 4.7262f, nullptr},
                                                              {1240.595f, -471.971f, 0.8933f, nullptr},
                                                              {1265.993f, -435.419f, 10.669f, nullptr}};

BattleBotPath vPath_IC_Horde_Side_Gate_to_Refinery_Base = {
    {1218.676f, -660.487f, 47.870f, nullptr}, {1211.677f, -626.181f, 46.085f, nullptr},
    {1212.720f, -562.300f, 19.514f, nullptr}, {1238.803f, -538.997f, 3.9892f, nullptr},
    {1248.875f, -482.852f, 0.8933f, nullptr}, {1265.993f, -435.419f, 10.669f, nullptr}};

BattleBotPath vPath_IC_Refinery_Crossroad_to_Docks_Crossroad = {
    {1148.358f, -503.947f, 23.423f, nullptr}, {1127.010f, -469.451f, 23.422f, nullptr},
    {1100.976f, -431.146f, 21.312f, nullptr}, {1053.812f, -405.457f, 12.749f, nullptr},
    {1005.570f, -375.439f, 12.695f, nullptr}, {963.4349f, -353.282f, 12.356f, nullptr},
    {907.1394f, -380.470f, 11.912f, nullptr}};

BattleBotPath vPath_IC_Horde_Dock_Crossroad_to_Docks_Crossroad = {
    {1034.226f, -653.581f, 24.432f, nullptr}, {1013.435f, -622.066f, 24.486f, nullptr},
    {988.1990f, -547.937f, 24.424f, nullptr}, {982.4955f, -508.332f, 24.524f, nullptr},
    {982.5065f, -462.920f, 16.833f, nullptr}, {948.8842f, -421.200f, 16.877f, nullptr},
    {907.1394f, -380.470f, 11.912f, nullptr}};

BattleBotPath vPath_IC_Docks_Crossroad_to_Docks_Flag = {{907.1394f, -380.470f, 11.912f, nullptr},
                                                        {851.5726f, -382.503f, 11.906f, nullptr},
                                                        {808.1441f, -381.199f, 11.906f, nullptr},
                                                        {761.1740f, -381.854f, 14.504f, nullptr},
                                                        {726.427f, -364.849f, 17.815f, nullptr}};

BattleBotPath vPath_IC_Horde_Front_Crossroad_to_Workshop = {
    {991.4235f, -807.672f, 21.788f, nullptr}, {944.5518f, -800.344f, 13.155f, nullptr},
    {907.1300f, -798.892f, 8.3237f, nullptr}, {842.9721f, -795.224f, 5.2007f, nullptr},
    {804.5959f, -794.269f, 5.9836f, nullptr}, {774.466f, -801.058f, 6.3428f, nullptr}};

BattleBotPath vPath_IC_Central_Graveyard_to_Workshop = {
    {775.377f, -664.151f, 8.388f, nullptr}, {776.299f, -684.079f, 5.036f, nullptr},
    {777.451f, -707.525f, 0.051f, nullptr}, {779.059f, -734.611f, 1.695f, nullptr},
    {779.643f, -767.010f, 4.843f, nullptr}, {774.466f, -801.058f, 6.3428f, nullptr}};

BattleBotPath vPath_IC_Horde_East_Gate_to_Horde_Keep = {
    {1216.1918f, -864.922f, 48.852f, nullptr}, {1197.3117f, -866.054f, 48.916f, nullptr},
    {1174.195f, -867.931f, 48.621f, nullptr},  {1149.671f, -869.240f, 48.096f, nullptr},
    {1128.257f, -860.087f, 49.562f, nullptr},  {1118.730f, -829.959f, 49.074f, nullptr},
    {1123.201f, -806.498f, 48.896f, nullptr},  {1129.685f, -787.156f, 48.680f, nullptr},
    {1128.646f, -763.221f, 48.385f, nullptr}};

BattleBotPath vPath_IC_Workshop_to_Workshop_Keep = {{773.792f, -825.637f, 8.127f, nullptr},
                                                    {772.706f, -841.881f, 11.622f, nullptr},
                                                    {773.057f, -859.936f, 12.418f, nullptr}};

BattleBotPath vPath_IC_Alliance_Base = {
    {402.020f, -832.289f, 48.627f, nullptr}, {384.784f, -832.551f, 48.830f, nullptr},
    {369.413f, -832.480f, 48.916f, nullptr}, {346.677f, -832.355f, 48.916f, nullptr},
    {326.296f, -832.189f, 48.916f, nullptr}, {311.174f, -832.204f, 48.916f, nullptr},
};

BattleBotPath vPath_IC_Horde_Base = {
    {1158.652f, -762.680f, 48.628f, nullptr}, {1171.598f, -762.628f, 48.649f, nullptr},
    {1189.102f, -763.484f, 48.915f, nullptr}, {1208.599f, -764.332f, 48.915f, nullptr},
    {1227.592f, -764.782f, 48.915f, nullptr}, {1253.676f, -765.441f, 48.915f, nullptr},
};

BattleBotPath vPath_IC_Workshop_to_North_West = {
    {786.051f, -801.798f, 5.968f, nullptr},   {801.767f, -800.845f, 6.201f, nullptr},
    {830.190f, -800.059f, 5.163f, nullptr},   {858.827f, -797.691f, 5.602f, nullptr},
    {881.579f, -795.190f, 6.441f, nullptr},   {905.796f, -792.252f, 8.008f, nullptr},
    {929.874f, -788.912f, 10.674f, nullptr},  {960.001f, -784.233f, 15.521f, nullptr},
    {991.890f, -780.605f, 22.402f, nullptr},  {1014.062f, -761.863f, 28.672f, nullptr},
    {1019.925f, -730.822f, 27.421f, nullptr}, {1022.320f, -698.581f, 25.993f, nullptr},
    {1022.539f, -665.405f, 24.574f, nullptr}, {1016.279f, -632.780f, 24.487f, nullptr},
    {1004.839f, -602.680f, 24.501f, nullptr}, {992.826f, -567.833f, 24.558f, nullptr},
    {984.998f, -535.303f, 24.485f, nullptr},
};

BattleBotPath vPath_IC_South_West_Crossroads = {
    {528.932f, -667.953f, 25.413f, nullptr}, {514.800f, -650.381f, 26.171f, nullptr},
    {488.367f, -621.869f, 25.820f, nullptr}, {479.491f, -594.284f, 26.095f, nullptr},
    {498.094f, -557.031f, 26.015f, nullptr}, {528.272f, -528.761f, 26.015f, nullptr},
    {595.746f, -480.009f, 26.007f, nullptr}, {632.156f, -458.182f, 27.416f, nullptr},
    {656.013f, -446.685f, 28.003f, nullptr},
};

BattleBotPath vPath_IC_Hanger_to_Workshop = {
    {808.923f, -1003.441f, 132.380f, nullptr}, {804.031f, -1002.785f, 132.382f, nullptr},
    {798.466f, -1021.472f, 132.292f, nullptr}, {795.472f, -1031.693f, 130.232f, nullptr},
    {804.631f, -1042.672f, 125.310f, nullptr}, {827.549f, -1061.778f, 111.276f, nullptr},
    {847.424f, -1077.930f, 98.061f, nullptr},  {868.225f, -1091.563f, 83.794f, nullptr},
    {894.100f, -1104.828f, 66.570f, nullptr},  {923.615f, -1117.689f, 47.391f, nullptr},
    {954.614f, -1119.716f, 27.880f, nullptr},  {970.397f, -1116.281f, 22.124f, nullptr},
    {985.906f, -1105.714f, 18.572f, nullptr},  {996.308f, -1090.605f, 17.669f, nullptr},
    {1003.693f, -1072.916f, 16.583f, nullptr}, {1008.660f, -1051.310f, 15.970f, nullptr},
    {1008.437f, -1026.678f, 15.623f, nullptr}, {1000.103f, -999.047f, 16.487f, nullptr},
    {988.412f, -971.096f, 17.796f, nullptr},   {971.503f, -945.742f, 14.438f, nullptr},
    {947.420f, -931.306f, 13.209f, nullptr},   {922.920f, -916.960f, 10.859f, nullptr},
    {901.607f, -902.203f, 9.854f, nullptr},    {881.932f, -882.226f, 7.848f, nullptr},
    {861.795f, -862.477f, 6.731f, nullptr},    {844.186f, -845.952f, 6.192f, nullptr},
    {826.565f, -826.551f, 5.171f, nullptr},    {809.497f, -815.351f, 6.179f, nullptr},
    {790.787f, -809.678f, 6.450f, nullptr},
};

std::vector<BattleBotPath*> const vPaths_IC = {
    &vPath_IC_Ally_Dock_Crossroad_to_Ally_Docks_Second_Crossroad,
    &vPath_IC_Ally_Docks_Crossroad_to_Docks_Flag,
    &vPath_IC_Ally_Docks_Second_Crossroad_to_Ally_Docks_Crossroad,
    &vPath_IC_Lower_Graveyard_Crossroad_to_Ally_Docks_Crossroad,
    &vPath_IC_Lower_Graveyard_Crossroad_to_Ally_Docks_Second_Crossroad,
    &vPath_IC_Lower_Graveyard_to_Lower_Graveyard_Crossroad,
    &vPath_IC_Ally_Front_Crossroad_to_Ally_Dock_Crossroad,
    &vPath_IC_Ally_Front_Crossroad_to_Hangar_First_Crossroad,
    &vPath_IC_Ally_Front_Crossroad_to_Workshop,
    &vPath_IC_Ally_Keep_to_Ally_Dock_Crossroad,
    &vPath_IC_Ally_Keep_to_Ally_Front_Crossroad,
    &vPath_IC_Ally_Keep_to_Hangar_First_Crossroad,
    &vPath_IC_Ally_Keep_to_Quarry_Crossroad,
    &vPath_IC_Docks_Crossroad_to_Docks_Flag,
    &vPath_IC_Docks_Graveyard_to_Docks_Flag,
    &vPath_IC_Hangar_First_Crossroad_to_Hangar_Second_Crossroad,
    &vPath_IC_Hangar_Second_Crossroad_to_Hangar_Flag,
    &vPath_IC_Horde_Dock_Crossroad_to_Docks_Crossroad,
    &vPath_IC_Horde_Dock_Crossroad_to_Refinery_Crossroad,
    &vPath_IC_Horde_Front_Crossroad_to_Horde_Dock_Crossroad,
    &vPath_IC_Horde_Front_Crossroad_to_Horde_Hangar_Crossroad,
    &vPath_IC_Horde_Front_Crossroad_to_Workshop,
    &vPath_IC_Horde_Hangar_Crossroad_to_Hangar_Flag,
    &vPath_IC_Horde_Keep_to_Horde_Dock_Crossroad,
    &vPath_IC_Horde_Keep_to_Horde_Front_Crossroad,
    &vPath_IC_Horde_Keep_to_Horde_Hangar_Crossroad,
    &vPath_IC_Horde_Side_Gate_to_Refinery_Base,
    &vPath_IC_Quarry_Crossroad_to_Hangar_Second_Crossroad,
    &vPath_IC_Quarry_Crossroad_to_Quarry_Flag,
    &vPath_IC_Refinery_Crossroad_to_Docks_Crossroad,
    &vPath_IC_Refinery_Crossroad_to_Refinery_Base,
    &vPath_IC_Central_Graveyard_to_Workshop,
    &vPath_IC_Horde_East_Gate_to_Horde_Keep,
    &vPath_IC_Workshop_to_Workshop_Keep,
    &vPath_IC_Alliance_Base,
    &vPath_IC_Horde_Base,
    &vPath_IC_Workshop_to_North_West,
    &vPath_IC_South_West_Crossroads,
    &vPath_IC_Hanger_to_Workshop,
};

std::vector<BattleBotPath*> const vPaths_OneWay_IC = {&vPath_IC_Central_Graveyard_to_Workshop,
                                                      &vPath_IC_Docks_Graveyard_to_Docks_Flag};

static std::pair<uint32, uint32> IC_AttackObjectives[] = {
    {NODE_TYPE_WORKSHOP, BG_IC_GO_WORKSHOP_BANNER},
    {NODE_TYPE_DOCKS, BG_IC_GO_DOCKS_BANNER},
    {NODE_TYPE_HANGAR, BG_IC_GO_HANGAR_BANNER},
};

void BGTactics::moveToStartIC(Battleground* bg)
{
    uint32 role = context->GetValue<uint32>("bg role")->Get();

    if (bot->GetTeamId() == TEAM_HORDE)
    {
        if (role == 9)  // refinery
            MoveTo(bg->GetMapId(), IC_WEST_WAITING_POS_HORDE.GetPositionX() + frand(-5.0f, 5.0f),
                   IC_WEST_WAITING_POS_HORDE.GetPositionY() + frand(-5.0f, 5.0f),
                   IC_WEST_WAITING_POS_HORDE.GetPositionZ());
        else if (role >= 3 && role < 6)  // hanger
            MoveTo(bg->GetMapId(), IC_EAST_WAITING_POS_HORDE.GetPositionX() + frand(-5.0f, 5.0f),
                   IC_EAST_WAITING_POS_HORDE.GetPositionY() + frand(-5.0f, 5.0f),
                   IC_EAST_WAITING_POS_HORDE.GetPositionZ());
        else  // everything else
            MoveTo(bg->GetMapId(), IC_WAITING_POS_HORDE.GetPositionX() + frand(-5.0f, 5.0f),
                   IC_WAITING_POS_HORDE.GetPositionY() + frand(-5.0f, 5.0f), IC_WAITING_POS_HORDE.GetPositionZ());
    }
    else
    {
        if (role < 3)  // docks
            MoveTo(
                bg->GetMapId(), IC_WAITING_POS_ALLIANCE.GetPositionX() + frand(-5.0f, 5.0f),
                IC_WAITING_POS_ALLIANCE.GetPositionY() + frand(-5.0f, 5.0f),
                IC_WAITING_POS_ALLIANCE.GetPositionZ());  // dont bother using west, there's no paths to use anyway
        else if (role == 9 || (role >= 3 && role < 6))    // quarry and hanger
            MoveTo(bg->GetMapId(), IC_EAST_WAITING_POS_ALLIANCE.GetPositionX() + frand(-5.0f, 5.0f),
                   IC_EAST_WAITING_POS_ALLIANCE.GetPositionY() + frand(-5.0f, 5.0f),
                   IC_EAST_WAITING_POS_ALLIANCE.GetPositionZ());
        else  // everything else
            MoveTo(bg->GetMapId(), IC_WAITING_POS_ALLIANCE.GetPositionX() + frand(-5.0f, 5.0f),
                   IC_WAITING_POS_ALLIANCE.GetPositionY() + frand(-5.0f, 5.0f),
                   IC_WAITING_POS_ALLIANCE.GetPositionZ());
    }
}

bool BGTactics::selectObjectiveIC(Battleground* bg, PositionMap& posMap, PositionInfo& pos)
{
    WorldObject* BgObjective = nullptr;

    BattlegroundIC* isleOfConquestBG = (BattlegroundIC*)bg;
    // the Horde siege spot's ground height, looked up once
    static float const icParkZ = bg->GetBgMap()->GetHeight(IC_GATE_ATTACK_POS_HORDE.GetPositionX(),
                                                            IC_GATE_ATTACK_POS_HORDE.GetPositionY(), 60.0f);
    Position icHordePark = IC_GATE_ATTACK_POS_HORDE;
    if (icParkZ > INVALID_HEIGHT)
        icHordePark.m_positionZ = icParkZ;

    uint32 role = context->GetValue<uint32>("bg role")->Get();
    bool inVehicle = botAI->IsInVehicle();
    bool controlsVehicle = botAI->IsInVehicle(true);
    uint32 vehicleId = inVehicle ? bot->GetVehicleBase()->GetEntry() : 0;

    // skip if not the driver
    if (inVehicle && !controlsVehicle)
        return false;

    /* TACTICS */
    if (bot->GetTeamId() == TEAM_HORDE)  // HORDE
    {
        bool gateOpen = false;
        if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_DOODAD_PORTCULLISACTIVE02))
        {
            if (pGO->isSpawned() && pGO->getLootState() == GO_ACTIVATED)
            {
                gateOpen = true;
                PositionInfo siegePos = context->GetValue<PositionMap&>("position")->Get()["bg siege"];
                siegePos.Reset();
                posMap["bg siege"] = siegePos;
            }
        }
        // the keep is open: drivers get out and push in on foot
        if (gateOpen && controlsVehicle)
        {
            bot->ExitVehicle();
            return false;
        }
        if (gateOpen && !controlsVehicle &&
            isleOfConquestBG->GetICNodePoint(NODE_TYPE_GRAVEYARD_A).nodeState ==
                NODE_STATE_CONTROLLED_H)  // target enemy boss
        {
            if (Creature* enemyBoss = bg->GetBGCreature(BG_IC_NPC_HIGH_COMMANDER_HALFORD_WYRMBANE))
            {
                if (enemyBoss->IsVisible())
                {
                    BgObjective = enemyBoss;
                    // LOG_INFO("playerbots", "bot={} attack boss", bot->GetName());
                }
            }
        }

        if (!BgObjective && gateOpen && !controlsVehicle)
        {
            if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_ALLIANCE_BANNER))  // capture flag within keep
            {
                BgObjective = pGO;
                // LOG_INFO("playerbots", "bot={} attack keep", bot->GetName());
            }
        }

        if (!BgObjective && !gateOpen && controlsVehicle)  // attack gates
        {
            // TODO: check for free vehicles
            if (GameObject* gate = bg->GetBGObject(BG_IC_GO_ALLIANCE_GATE_3))
            {
                if (vehicleId == NPC_SIEGE_ENGINE_H)  // target gate directly if siege engine
                {
                    BgObjective = gate;
                    PositionInfo siegePos = context->GetValue<PositionMap&>("position")->Get()["bg siege"];
                    siegePos.Set(gate->GetPositionX(), gate->GetPositionY(), gate->GetPositionZ(), bot->GetMapId());
                    posMap["bg siege"] = siegePos;
                    // LOG_INFO("playerbots", "bot={} (in siege-engine) attack gate", bot->GetName());
                }
                else  // target gate directly at range if other vehicle
                {
                    // just make bot stay where it is if already close
                    // (stops them shifting around between the random spots)
                    if (bot->GetDistance(icHordePark) < 8.0f)
                        pos.Set(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
                    else
                        pos.Set(icHordePark.GetPositionX() + frand(-5.0f, +5.0f),
                                icHordePark.GetPositionY() + frand(-5.0f, +5.0f),
                                icHordePark.GetPositionZ(), bot->GetMapId());
                    posMap["bg objective"] = pos;
                    // set siege position
                    PositionInfo siegePos = context->GetValue<PositionMap&>("position")->Get()["bg siege"];
                    siegePos.Set(gate->GetPositionX(), gate->GetPositionY(), gate->GetPositionZ(),
                                 bot->GetMapId());
                    posMap["bg siege"] = siegePos;
                    // LOG_INFO("playerbots", "bot={} (in vehicle={}) attack gate", bot->GetName(), vehicleId);
                    return true;
                }
            }
        }

        // If gates arent down and not in vehicle, split tasks
        if (!BgObjective && !controlsVehicle && role == 9)  // Capture Side base
        {
            // Capture Refinery
            ICNodePoint const& nodePoint = isleOfConquestBG->GetICNodePoint(NODE_TYPE_REFINERY);
            if (nodePoint.nodeState != NODE_STATE_CONFLICT_H && nodePoint.nodeState != NODE_STATE_CONTROLLED_H)
            {
                BgObjective = bg->GetBGObject(BG_IC_GO_REFINERY_BANNER);
                // LOG_INFO("playerbots", "bot={} attack refinery", bot->GetName());
            }
        }

        if (!BgObjective && !controlsVehicle && role < 3)  // Capture Docks
        {
            ICNodePoint const& nodePoint = isleOfConquestBG->GetICNodePoint(NODE_TYPE_DOCKS);
            if (nodePoint.nodeState != NODE_STATE_CONFLICT_H && nodePoint.nodeState != NODE_STATE_CONTROLLED_H)
            {
                if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_DOCKS_BANNER))
                {
                    BgObjective = pGO;
                    // LOG_INFO("playerbots", "bot={} attack docks", bot->GetName());
                }
            }
        }
        if (!BgObjective && !controlsVehicle && role < 6)  // Capture Hangar
        {
            ICNodePoint const& nodePoint = isleOfConquestBG->GetICNodePoint(NODE_TYPE_HANGAR);
            if (nodePoint.nodeState != NODE_STATE_CONFLICT_H && nodePoint.nodeState != NODE_STATE_CONTROLLED_H)
            {
                if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_HANGAR_BANNER))
                {
                    BgObjective = pGO;
                    // LOG_INFO("playerbots", "bot={} attack hangar", bot->GetName());
                }
            }
        }
        if (!BgObjective && !controlsVehicle)  // Capture Workshop
        {
            ICNodePoint const& nodePoint = isleOfConquestBG->GetICNodePoint(NODE_TYPE_WORKSHOP);
            if (nodePoint.nodeState != NODE_STATE_CONFLICT_H && nodePoint.nodeState != NODE_STATE_CONTROLLED_H)
            {
                if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_WORKSHOP_BANNER))
                {
                    BgObjective = pGO;
                    // LOG_INFO("playerbots", "bot={} attack workshop", bot->GetName());
                }
            }
        }
        if (!BgObjective)  // Guard point that's not fully capped (also gets them in place to board vehicle)
        {
            uint32 len = end(IC_AttackObjectives) - begin(IC_AttackObjectives);
            for (uint32 i = 0; i < len; i++)
            {
                auto const& objective =
                    IC_AttackObjectives[(i + role) %
                                        len];  // use role to determine which objective checked first
                if (isleOfConquestBG->GetICNodePoint(objective.first).nodeState != NODE_STATE_CONTROLLED_H)
                {
                    if (GameObject* pGO = bg->GetBGObject(objective.second))
                    {
                        BgObjective = pGO;
                        // LOG_INFO("playerbots", "bot={} guard point while it captures", bot->GetName());
                        break;
                    }
                }
            }
        }
        if (!BgObjective)  // guard vehicles as they seige
        {
            // just make bot stay where it is if already close
            // (stops them shifting around between the random spots)
            if (bot->GetDistance(icHordePark) < 8.0f)
                pos.Set(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
            else
                pos.Set(icHordePark.GetPositionX() + frand(-5.0f, +5.0f),
                        icHordePark.GetPositionY() + frand(-5.0f, +5.0f),
                        icHordePark.GetPositionZ(), bot->GetMapId());
            posMap["bg objective"] = pos;
            // LOG_INFO("playerbots", "bot={} guard vehicles as they attack gate", bot->GetName());
            return true;
        }
    }

    if (bot->GetTeamId() == TEAM_ALLIANCE)  // ALLIANCE
    {
        bool gateOpen = false;
        if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_HORDE_KEEP_PORTCULLIS))
        {
            if (pGO->isSpawned() && pGO->getLootState() == GO_ACTIVATED)
            {
                gateOpen = true;
                PositionInfo siegePos = context->GetValue<PositionMap&>("position")->Get()["bg siege"];
                siegePos.Reset();
                posMap["bg siege"] = siegePos;
            }
        }
        // the keep is open: drivers get out and push in on foot
        if (gateOpen && controlsVehicle)
        {
            bot->ExitVehicle();
            return false;
        }

        if (gateOpen && !controlsVehicle &&
            isleOfConquestBG->GetICNodePoint(NODE_TYPE_GRAVEYARD_H).nodeState ==
                NODE_STATE_CONTROLLED_A)  // target enemy boss
        {
            if (Creature* enemyBoss = bg->GetBGCreature(BG_IC_NPC_OVERLORD_AGMAR))
            {
                if (enemyBoss->IsVisible())
                {
                    BgObjective = enemyBoss;
                    // LOG_INFO("playerbots", "bot={} attack boss", bot->GetName());
                }
            }
        }

        if (!BgObjective && gateOpen && !controlsVehicle)
        {
            if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_HORDE_BANNER))  // capture flag within keep
            {
                BgObjective = pGO;
                // LOG_INFO("playerbots", "bot={} attack keep", bot->GetName());
            }
        }

        if (!BgObjective && !gateOpen && controlsVehicle)  // attack gates
        {
            // TODO: check for free vehicles
            if (GameObject* gate = bg->GetBGObject(BG_IC_GO_HORDE_GATE_1))
            {
                if (vehicleId == NPC_SIEGE_ENGINE_A)  // target gate directly if siege engine
                {
                    BgObjective = gate;
                    PositionInfo siegePos = context->GetValue<PositionMap&>("position")->Get()["bg siege"];
                    siegePos.Set(gate->GetPositionX(), gate->GetPositionY(), gate->GetPositionZ(), bot->GetMapId());
                    posMap["bg siege"] = siegePos;
                    // LOG_INFO("playerbots", "bot={} (in siege-engine) attack gate", bot->GetName());
                }
                else  // target gate directly at range if other vehicle
                {
                    // just make bot stay where it is if already close
                    // (stops them shifting around between the random spots)
                    if (bot->GetDistance(IC_GATE_ATTACK_POS_ALLIANCE) < 8.0f)
                        pos.Set(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
                    else
                        pos.Set(IC_GATE_ATTACK_POS_ALLIANCE.GetPositionX() + frand(-5.0f, +5.0f),
                                IC_GATE_ATTACK_POS_ALLIANCE.GetPositionY() + frand(-5.0f, +5.0f),
                                IC_GATE_ATTACK_POS_ALLIANCE.GetPositionZ(), bot->GetMapId());
                    posMap["bg objective"] = pos;
                    // set siege position
                    PositionInfo siegePos = context->GetValue<PositionMap&>("position")->Get()["bg siege"];
                    siegePos.Set(gate->GetPositionX(), gate->GetPositionY(), gate->GetPositionZ(),
                                 bot->GetMapId());
                    posMap["bg siege"] = siegePos;
                    // LOG_INFO("playerbots", "bot={} (in vehicle={}) attack gate", bot->GetName(), vehicleId);
                    return true;
                }
            }
        }

        // If gates arent down and not in vehicle, split tasks
        if (!BgObjective && !controlsVehicle && role == 9)  // Capture Side base
        {
            // Capture Refinery
            ICNodePoint const& nodePoint = isleOfConquestBG->GetICNodePoint(NODE_TYPE_QUARRY);
            if (nodePoint.nodeState != NODE_STATE_CONFLICT_A && nodePoint.nodeState != NODE_STATE_CONTROLLED_A)
            {
                BgObjective = bg->GetBGObject(BG_IC_GO_QUARRY_BANNER);
                // LOG_INFO("playerbots", "bot={} attack quarry", bot->GetName());
            }
        }

        if (!BgObjective && !controlsVehicle && role < 3)  // Capture Docks
        {
            ICNodePoint const& nodePoint = isleOfConquestBG->GetICNodePoint(NODE_TYPE_DOCKS);
            if (nodePoint.nodeState != NODE_STATE_CONFLICT_A && nodePoint.nodeState != NODE_STATE_CONTROLLED_A)
            {
                if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_DOCKS_BANNER))
                {
                    BgObjective = pGO;
                    // LOG_INFO("playerbots", "bot={} attack docks", bot->GetName());
                }
            }
        }
        if (!BgObjective && !controlsVehicle && role < 6)  // Capture Hangar
        {
            ICNodePoint const& nodePoint = isleOfConquestBG->GetICNodePoint(NODE_TYPE_HANGAR);
            if (nodePoint.nodeState != NODE_STATE_CONFLICT_A && nodePoint.nodeState != NODE_STATE_CONTROLLED_A)
            {
                if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_HANGAR_BANNER))
                {
                    BgObjective = pGO;
                    // LOG_INFO("playerbots", "bot={} attack hangar", bot->GetName());
                }
            }
        }
        if (!BgObjective && !controlsVehicle)  // Capture Workshop
        {
            ICNodePoint const& nodePoint = isleOfConquestBG->GetICNodePoint(NODE_TYPE_WORKSHOP);
            if (nodePoint.nodeState != NODE_STATE_CONFLICT_A && nodePoint.nodeState != NODE_STATE_CONTROLLED_A)
            {
                if (GameObject* pGO = bg->GetBGObject(BG_IC_GO_WORKSHOP_BANNER))
                {
                    BgObjective = pGO;
                    // LOG_INFO("playerbots", "bot={} attack workshop", bot->GetName());
                }
            }
        }
        if (!BgObjective)  // Guard point that's not fully capped (also gets them in place to board vehicle)
        {
            uint32 len = end(IC_AttackObjectives) - begin(IC_AttackObjectives);
            for (uint32 i = 0; i < len; i++)
            {
                auto const& objective =
                    IC_AttackObjectives[(i + role) %
                                        len];  // use role to determine which objective checked first
                if (isleOfConquestBG->GetICNodePoint(objective.first).nodeState != NODE_STATE_CONTROLLED_A)
                {
                    if (GameObject* pGO = bg->GetBGObject(objective.second))
                    {
                        BgObjective = pGO;
                        // LOG_INFO("playerbots", "bot={} guard point while it captures", bot->GetName());
                        break;
                    }
                }
            }
        }
        if (!BgObjective)  // guard vehicles as they seige
        {
            // just make bot stay where it is if already close
            // (stops them shifting around between the random spots)
            if (bot->GetDistance(IC_GATE_ATTACK_POS_ALLIANCE) < 8.0f)
                pos.Set(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
            else
                pos.Set(IC_GATE_ATTACK_POS_ALLIANCE.GetPositionX() + frand(-5.0f, +5.0f),
                        IC_GATE_ATTACK_POS_ALLIANCE.GetPositionY() + frand(-5.0f, +5.0f),
                        IC_GATE_ATTACK_POS_ALLIANCE.GetPositionZ(), bot->GetMapId());
            posMap["bg objective"] = pos;
            // LOG_INFO("playerbots", "bot={} guard vehicles as they attack gate", bot->GetName());
            return true;
        }
    }

    if (BgObjective)
    {
        pos.Set(BgObjective->GetPositionX(), BgObjective->GetPositionY(), BgObjective->GetPositionZ(),
                bot->GetMapId());
        posMap["bg objective"] = pos;
        return true;
    }
    return false;
}

// check Isle of Conquest Keep position
bool BGTactics::IsLockedInsideKeep()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    if (bgType != BATTLEGROUND_IC)
        return false;

    bool isInside = false;
    if (bot->GetTeamId() == TEAM_ALLIANCE && bot->GetPositionX() < 410.0f && bot->GetPositionY() > -900.0f &&
        bot->GetPositionY() < -765.0f)
        isInside = true;
    if (bot->GetTeamId() == TEAM_HORDE && bot->GetPositionX() > 1153.0f && bot->GetPositionY() > -849.0f &&
        bot->GetPositionY() < -679.0f)
        isInside = true;

    if (!isInside)
        return false;

    GuidVector closeObjects;
    closeObjects = *context->GetValue<GuidVector>("nearest game objects no los");
    if (closeObjects.empty())
        return moveToStart(true);

    GameObject* closestPortal = nullptr;
    float closestDistance = 100.0f;
    bool gateLock = false;

    // check inner gates status
    // ALLIANCE
    if (bot->GetTeamId() == TEAM_ALLIANCE)
    {
        if (GameObject* go = bg->GetBGObject(BG_IC_GO_DOODAD_PORTCULLISACTIVE02))
        {
            if (go->isSpawned())
            {
                gateLock = go->getLootState() != GO_ACTIVATED;
            }
            else
            {
                gateLock = false;
            }
        }
    }
    // HORDE
    if (bot->GetTeamId() == TEAM_HORDE)
    {
        if (GameObject* go = bg->GetBGObject(BG_IC_GO_HORDE_KEEP_PORTCULLIS))
        {
            if (go->isSpawned())
            {
                gateLock = go->getLootState() != GO_ACTIVATED;
            }
            else
            {
                gateLock = false;
            }
        }
    }

    for (ObjectGuid const& guid : closeObjects)
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (!go)
            continue;

        // ALLIANCE
        // get closest portal
        if (bot->GetTeamId() == TEAM_ALLIANCE && go->GetEntry() == GO_TELEPORTER_4)
        {
            float tempDist = ServerFacade::instance().GetDistance2d(bot, go->GetPositionX(), go->GetPositionY());

            if (ServerFacade::instance().IsDistanceLessThan(tempDist, closestDistance))
            {
                closestDistance = tempDist;
                closestPortal = go;
            }
        }

        // HORDE
        // get closest portal
        if (bot->GetTeamId() == TEAM_HORDE && go->GetEntry() == GO_TELEPORTER_2)
        {
            float tempDist = ServerFacade::instance().GetDistance2d(bot, go->GetPositionX(), go->GetPositionY());

            if (ServerFacade::instance().IsDistanceLessThan(tempDist, closestDistance))
            {
                closestDistance = tempDist;
                closestPortal = go;
            }
        }
    }

    // portal out of sight: walk to the nearest keep exit, the start spot is too far from them
    if (gateLock && !closestPortal)
    {
        std::array<uint32, 3> const exits = bot->GetTeamId() == TEAM_ALLIANCE
            ? std::array<uint32, 3>{BG_IC_GO_TELEPORTER_4_1, BG_IC_GO_TELEPORTER_4_2, BG_IC_GO_TELEPORTER_4_3}
            : std::array<uint32, 3>{BG_IC_GO_TELEPORTER_2_1, BG_IC_GO_TELEPORTER_2_2, BG_IC_GO_TELEPORTER_2_3};
        GameObject* nearest = nullptr;
        for (uint32 slot : exits)
            if (GameObject* exit = bg->GetBGObject(slot); exit && (!nearest || bot->GetDistance(exit) < bot->GetDistance(nearest)))
                nearest = exit;
        if (!nearest)
            return moveToStart(true);
        return MoveTo(bot->GetMapId(), nearest->GetPositionX(), nearest->GetPositionY(), nearest->GetPositionZ());
    }

    // portal not found, move closer
    if (!gateLock && !closestPortal)
        return moveToStart(true);

    // nothing found, allow move through
    if (!gateLock || !closestPortal)
        return false;

    // portal found
    if (closestPortal)
    {
        // if close
        if (bot->IsWithinDistInMap(closestPortal, INTERACTION_DISTANCE))
        {
            WorldPacket data(CMSG_GAMEOBJ_USE);
            data << closestPortal->GetGUID();
            bot->GetSession()->HandleGameObjectUseOpcode(data);
            return true;
        }
        else
        {
            return MoveTo(bot->GetMapId(), closestPortal->GetPositionX(), closestPortal->GetPositionY(),
                          closestPortal->GetPositionZ());
        }
    }

    return moveToStart(true);

    return false;
}
