// Eye of the Storm bot tactics: waypoints, node objectives, flag runs and the spawn jump.

#include "BattleGroundTactics.h"

#include "BattlegroundEY.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "PositionValue.h"

Position const EY_WAITING_POS_HORDE = {1809.102f, 1540.854f, 1267.142f, 0.0f};
Position const EY_WAITING_POS_ALLIANCE = {2523.827f, 1596.915f, 1270.204f, 0.0f};
Position const EY_FLAG_RETURN_POS_RETREAT_HORDE = {1885.529f, 1532.157f, 1200.635f, 0.0f};
Position const EY_FLAG_RETURN_POS_RETREAT_ALLIANCE = {2452.253f, 1602.356f, 1203.617f, 0.0f};
Position const EY_GY_CAMPING_HORDE = {1874.854f, 1530.405f, 1207.432f, 0.0f};
Position const EY_GY_CAMPING_ALLIANCE = {2456.887f, 1599.025f, 1206.280f, 0.0f};

std::vector<uint32> const vFlagsEY = {BG_OBJECT_FLAG2_EY_ENTRY, BG_OBJECT_FLAG3_EY_ENTRY};

BattleBotPath vPath_EY_Horde_Spawn_to_Crossroad1Horde = {
    {1809.102f, 1540.854f, 1267.142f, nullptr}, {1832.335f, 1539.495f, 1256.417f, nullptr},
    {1846.995f, 1539.792f, 1243.077f, nullptr}, {1846.243f, 1530.716f, 1238.477f, nullptr},
    {1883.154f, 1532.143f, 1202.143f, nullptr}, {1941.452f, 1549.086f, 1176.700f, nullptr}};

BattleBotPath vPath_EY_Horde_Crossroad1Horde_to_Crossroad2Horde = {{1951.647f, 1545.187f, 1174.831f, nullptr},
                                                                   {1992.266f, 1546.962f, 1169.816f, nullptr},
                                                                   {2045.865f, 1543.925f, 1163.759f, nullptr}};

BattleBotPath vPath_EY_Crossroad1Horde_to_Blood_Elf_Tower = {{1952.907f, 1539.857f, 1174.638f, nullptr},
                                                             {2000.130f, 1508.182f, 1169.778f, nullptr},
                                                             {2044.239f, 1483.860f, 1166.165f, nullptr},
                                                             {2048.773f, 1389.578f, 1193.903f, nullptr}};

BattleBotPath vPath_EY_Crossroad1Horde_to_Fel_Reaver_Ruins = {{1944.301f, 1557.170f, 1176.370f, nullptr},
                                                              {1992.953f, 1625.188f, 1173.616f, nullptr},
                                                              {2040.421f, 1676.989f, 1177.079f, nullptr},
                                                              {2045.527f, 1736.398f, 1189.661f, nullptr}};

BattleBotPath vPath_EY_Crossroad2Horde_to_Blood_Elf_Tower = {{2049.363f, 1532.337f, 1163.178f, nullptr},
                                                             {2050.149f, 1484.721f, 1165.099f, nullptr},
                                                             {2046.865f, 1423.937f, 1188.882f, nullptr},
                                                             {2048.478f, 1389.491f, 1193.878f, nullptr}};

BattleBotPath vPath_EY_Crossroad2Horde_to_Fel_Reaver_Ruins = {{2052.267f, 1555.692f, 1163.147f, nullptr},
                                                              {2047.684f, 1614.272f, 1165.397f, nullptr},
                                                              {2045.993f, 1668.937f, 1174.978f, nullptr},
                                                              {2044.286f, 1733.128f, 1189.739f, nullptr}};

BattleBotPath vPath_EY_Crossroad2Horde_to_Flag = {{2059.276f, 1546.143f, 1162.394f, nullptr},
                                                  {2115.978f, 1559.244f, 1156.362f, nullptr},
                                                  {2149.140f, 1556.570f, 1158.412f, nullptr},
                                                  {2170.601f, 1567.113f, 1159.456f, nullptr}};

BattleBotPath vPath_EY_Alliance_Spawn_to_Crossroad1Alliance = {
    {2502.110f, 1604.330f, 1260.750f, nullptr}, {2497.077f, 1596.198f, 1257.302f, nullptr},
    {2483.930f, 1597.062f, 1244.660f, nullptr}, {2486.549f, 1617.651f, 1225.837f, nullptr},
    {2449.150f, 1601.792f, 1201.552f, nullptr}, {2395.737f, 1588.287f, 1176.570f, nullptr}};

BattleBotPath vPath_EY_Alliance_Crossroad1Alliance_to_Crossroad2Alliance = {
    {2380.262f, 1586.757f, 1173.567f, nullptr},
    {2333.956f, 1586.052f, 1169.873f, nullptr},
    {2291.210f, 1591.435f, 1166.048f, nullptr},
};

BattleBotPath vPath_EY_Crossroad1Alliance_to_Mage_Tower = {{2380.973f, 1593.445f, 1173.189f, nullptr},
                                                           {2335.762f, 1621.922f, 1169.007f, nullptr},
                                                           {2293.526f, 1643.972f, 1166.501f, nullptr},
                                                           {2288.198f, 1688.568f, 1172.790f, nullptr},
                                                           {2284.286f, 1737.889f, 1189.708f, nullptr}};

BattleBotPath vPath_EY_Crossroad1Alliance_to_Draenei_Ruins = {{2388.687f, 1576.089f, 1175.975f, nullptr},
                                                              {2354.921f, 1522.763f, 1176.060f, nullptr},
                                                              {2300.056f, 1459.208f, 1184.181f, nullptr},
                                                              {2289.880f, 1415.640f, 1196.755f, nullptr},
                                                              {2279.870f, 1387.461f, 1195.003f, nullptr}};

BattleBotPath vPath_EY_Crossroad2Alliance_to_Mage_Tower = {{2282.525f, 1597.721f, 1164.553f, nullptr},
                                                           {2281.028f, 1651.310f, 1165.426f, nullptr},
                                                           {2284.633f, 1736.082f, 1189.708f, nullptr}};

BattleBotPath vPath_EY_Crossroad2Alliance_to_Draenei_Ruins = {{2282.487f, 1581.630f, 1165.318f, nullptr},
                                                              {2284.728f, 1525.618f, 1170.812f, nullptr},
                                                              {2287.697f, 1461.228f, 1183.450f, nullptr},
                                                              {2290.861f, 1413.606f, 1197.115f, nullptr}};

BattleBotPath vPath_EY_Crossroad2Alliance_to_Flag = {{2275.622f, 1586.123f, 1164.469f, nullptr},
                                                     {2221.334f, 1575.123f, 1158.277f, nullptr},
                                                     {2178.372f, 1572.144f, 1159.462f, nullptr}};

BattleBotPath vPath_EY_Draenei_Ruins_to_Blood_Elf_Tower = {
    {2287.925f, 1406.976f, 1197.004f, nullptr}, {2283.283f, 1454.769f, 1184.243f, nullptr},
    {2237.519f, 1398.161f, 1178.191f, nullptr}, {2173.150f, 1388.084f, 1170.185f, nullptr},
    {2105.039f, 1381.507f, 1162.911f, nullptr}, {2074.315f, 1404.387f, 1178.141f, nullptr},
    {2047.649f, 1411.681f, 1192.032f, nullptr}, {2049.197f, 1387.392f, 1193.799f, nullptr}};

BattleBotPath vPath_EY_Fel_Reaver_to_Mage_Tower = {
    {2044.519f, 1726.113f, 1189.395f, nullptr}, {2045.408f, 1682.986f, 1177.574f, nullptr},
    {2097.595f, 1736.117f, 1170.419f, nullptr}, {2158.866f, 1746.998f, 1161.184f, nullptr},
    {2220.635f, 1757.837f, 1151.886f, nullptr}, {2249.922f, 1721.807f, 1161.550f, nullptr},
    {2281.021f, 1694.735f, 1174.020f, nullptr}, {2284.522f, 1728.234f, 1189.015f, nullptr}};

std::vector<BattleBotPath*> const vPaths_EY = {
    &vPath_EY_Horde_Spawn_to_Crossroad1Horde,
    &vPath_EY_Horde_Crossroad1Horde_to_Crossroad2Horde,
    &vPath_EY_Crossroad1Horde_to_Blood_Elf_Tower,
    &vPath_EY_Crossroad1Horde_to_Fel_Reaver_Ruins,
    &vPath_EY_Crossroad2Horde_to_Blood_Elf_Tower,
    &vPath_EY_Crossroad2Horde_to_Fel_Reaver_Ruins,
    &vPath_EY_Crossroad2Horde_to_Flag,
    &vPath_EY_Alliance_Spawn_to_Crossroad1Alliance,
    &vPath_EY_Alliance_Crossroad1Alliance_to_Crossroad2Alliance,
    &vPath_EY_Crossroad1Alliance_to_Mage_Tower,
    &vPath_EY_Crossroad1Alliance_to_Draenei_Ruins,
    &vPath_EY_Crossroad2Alliance_to_Mage_Tower,
    &vPath_EY_Crossroad2Alliance_to_Draenei_Ruins,
    &vPath_EY_Crossroad2Alliance_to_Flag,
    &vPath_EY_Draenei_Ruins_to_Blood_Elf_Tower,
    &vPath_EY_Fel_Reaver_to_Mage_Tower,
};

static std::tuple<uint32, uint32, uint32> EY_AttackObjectives[] = {
    {POINT_FEL_REAVER, BG_EY_OBJECT_FLAG_FEL_REAVER, AT_FEL_REAVER_POINT},
    {POINT_BLOOD_ELF, BG_EY_OBJECT_FLAG_BLOOD_ELF, AT_BLOOD_ELF_POINT},
    {POINT_DRAENEI_RUINS, BG_EY_OBJECT_FLAG_DRAENEI_RUINS, AT_DRAENEI_RUINS_POINT},
    {POINT_MAGE_TOWER, BG_EY_OBJECT_FLAG_MAGE_TOWER, AT_MAGE_TOWER_POINT}
};

static std::unordered_map<uint32, Position> EY_NodePositions = {
    {POINT_FEL_REAVER, Position(2044.173f, 1727.503f, 1189.505f)},
    {POINT_BLOOD_ELF, Position(2048.277f, 1395.093f, 1194.255f)},
    {POINT_DRAENEI_RUINS, Position(2286.245f, 1404.683f, 1196.991f)},
    {POINT_MAGE_TOWER, Position(2284.720f, 1728.457f, 1189.153f)}
};

bool BGTactics::eyJumpDown()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;
    Position const hordeJumpPositions[] = {
        EY_WAITING_POS_HORDE,
        {1838.007f, 1539.856f, 1253.383f},
        {1846.264f, 1535.062f, 1240.796f},
        {1849.813f, 1527.303f, 1237.262f},
        {1849.041f, 1518.884f, 1223.624f},
    };
    Position const allianceJumpPositions[] = {
        EY_WAITING_POS_ALLIANCE,
        {2492.955f, 1597.769f, 1254.828f},
        {2484.601f, 1598.209f, 1244.344f},
        {2478.424f, 1609.539f, 1238.651f},
        {2475.926f, 1619.658f, 1218.706f},
    };
    Position const* positons = bot->GetTeamId() == TEAM_HORDE ? hordeJumpPositions : allianceJumpPositions;
    {
        if (bot->GetDistance(positons[0]) < 16.0f)
        {
            MoveTo(bg->GetMapId(), positons[1].GetPositionX(), positons[1].GetPositionY(), positons[1].GetPositionZ());
            return true;
        }
        if (bot->GetDistance(positons[1]) < 4.0f)
        {
            JumpTo(bg->GetMapId(), positons[2].GetPositionX(), positons[2].GetPositionY(), positons[2].GetPositionZ());
            return true;
        }
        if (bot->GetDistance(positons[2]) < 4.0f)
        {
            MoveTo(bg->GetMapId(), positons[3].GetPositionX(), positons[3].GetPositionY(), positons[3].GetPositionZ());
            return true;
        }
        if (bot->GetDistance(positons[3]) < 4.0f)
        {
            JumpTo(bg->GetMapId(), positons[4].GetPositionX(), positons[4].GetPositionY(), positons[4].GetPositionZ());
            return true;
        }
    }
    return false;
}

bool BGTactics::selectObjectiveEY(Battleground* bg, PositionMap& posMap, PositionInfo& pos)
{
    BattlegroundEY* eyeOfTheStormBG = (BattlegroundEY*)bg;
    TeamId team = bot->GetTeamId();
    uint8 role = context->GetValue<uint32>("bg role")->Get();

    EYBotStrategy strategyHorde = static_cast<EYBotStrategy>(GetBotStrategyForTeam(bg, TEAM_HORDE));
    EYBotStrategy strategyAlliance = static_cast<EYBotStrategy>(GetBotStrategyForTeam(bg, TEAM_ALLIANCE));
    EYBotStrategy strategy = (team == TEAM_ALLIANCE) ? strategyAlliance : strategyHorde;

    auto IsOwned = [&](uint32 nodeId) -> bool
    { return eyeOfTheStormBG->GetCapturePointInfo(nodeId)._ownerTeamId == team; };

    uint8 defendersProhab = 4;
    switch (strategy)
    {
        case EY_STRATEGY_FLAG_FOCUS:
            defendersProhab = 2;
            break;
        case EY_STRATEGY_FRONT_FOCUS:
        case EY_STRATEGY_BACK_FOCUS:
            defendersProhab = 3;
            break;
        default:
            defendersProhab = 4;
            break;
    }

    bool isDefender = role < defendersProhab;

    std::tuple<uint32, uint32, uint32> front[2];
    std::tuple<uint32, uint32, uint32> back[2];

    if (team == TEAM_HORDE)
    {
        front[0] = EY_AttackObjectives[0];
        front[1] = EY_AttackObjectives[1];
        back[0] = EY_AttackObjectives[2];
        back[1] = EY_AttackObjectives[3];
    }
    else
    {
        front[0] = EY_AttackObjectives[2];
        front[1] = EY_AttackObjectives[3];
        back[0] = EY_AttackObjectives[0];
        back[1] = EY_AttackObjectives[1];
    }

    bool foundObjective = false;

    // --- PRIORITY 1: FLAG CARRIER ---
    if (bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL))
    {
        uint32 bestNodeId = 0;
        float bestDist = FLT_MAX;
        uint32 bestTrigger = 0;

        for (auto const& [nodeId, _, areaTrigger] : EY_AttackObjectives)
        {
            if (!IsOwned(nodeId))
                continue;

            auto it = EY_NodePositions.find(nodeId);
            if (it == EY_NodePositions.end())
                continue;

            float dist = bot->GetDistance(it->second);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestNodeId = nodeId;
                bestTrigger = areaTrigger;
            }
        }

        if (bestTrigger != 0)  // Fel Reaver is point 0, so check the trigger
        {
            // run onto the capture trigger itself: it is a 3 yd circle that random spots around the node miss
            AreaTrigger const* captureTrigger = sObjectMgr->GetAreaTrigger(bestTrigger);
            Position const& targetPos = EY_NodePositions[bestNodeId];
            if (captureTrigger)
                pos.Set(captureTrigger->x, captureTrigger->y, captureTrigger->z, bot->GetMapId());
            else
                pos.Set(targetPos.GetPositionX(), targetPos.GetPositionY(), targetPos.GetPositionZ(),
                        bot->GetMapId());

            if (captureTrigger && bot->IsInAreaTriggerRadius(captureTrigger))
            {
                WorldPacket data(CMSG_AREATRIGGER);
                data << uint32(bestTrigger);
                bot->GetSession()->HandleAreaTriggerOpcode(data);
                pos.Reset();
            }

            foundObjective = true;
        }
        else
        {
            Position const& fallback = (team == TEAM_ALLIANCE) ? EY_FLAG_RETURN_POS_RETREAT_ALLIANCE
                                                               : EY_FLAG_RETURN_POS_RETREAT_HORDE;

            float rx, ry, rz;
            bot->GetRandomPoint(fallback, 5.0f, rx, ry, rz);

            pos.Set(rx, ry, rz, bot->GetMapId());
            foundObjective = true;
        }
    }

    // --- PRIORITY 2: Nearby unowned contested node ---
    if (!foundObjective)
    {
        for (auto const& [nodeId, _, __] : EY_AttackObjectives)
        {
            if (IsOwned(nodeId))
                continue;

            auto it = EY_NodePositions.find(nodeId);
            if (it == EY_NodePositions.end())
                continue;

            Position const& p = it->second;
            if (bot->IsWithinDist2d(p.GetPositionX(), p.GetPositionY(), 125.0f))
            {
                float rx, ry, rz;
                bot->GetRandomPoint(p, 5.0f, rx, ry, rz);
                pos.Set(rx, ry, rz, bot->GetMapId());
                foundObjective = true;
            }
        }
    }

    // --- PRIORITY 3: Random nearby enemy (20%, less on better teams) ---
    if (!foundObjective && slips(20))
    {
        if (Unit* enemy = AI_VALUE(Unit*, "enemy player target"))
        {
            if (bot->GetDistance(enemy) < 250.0f)
            {
                pos.Set(enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetPositionZ(), bot->GetMapId());
                foundObjective = true;
            }
        }
    }

    // --- PRIORITY 4: Defender Logic ---
    if (!foundObjective && isDefender && urand(0, 99) <= 80)
    {
        // 1. Chase enemy flag carrier
        if (Unit* enemyFC = AI_VALUE(Unit*, "enemy flag carrier"))
        {
            if (bot->CanSeeOrDetect(enemyFC, false, false, true) && enemyFC->IsAlive())
            {
                pos.Set(enemyFC->GetPositionX(), enemyFC->GetPositionY(), enemyFC->GetPositionZ(), bot->GetMapId());
                foundObjective = true;
            }
        }

        // 2. Support friendly flag carrier
        if (!foundObjective && coordinated())
        {
            if (Unit* friendlyFC = AI_VALUE(Unit*, "team flag carrier"))
            {
                if (friendlyFC->IsAlive())
                {
                    pos.Set(friendlyFC->GetPositionX(), friendlyFC->GetPositionY(), friendlyFC->GetPositionZ(), bot->GetMapId());
                    foundObjective = true;
                }
            }
        }

        // 3. Pick up the flag
        if (!foundObjective)
        {
            if (GameObject* flag = bg->GetBGObject(BG_EY_OBJECT_FLAG_NETHERSTORM); flag && flag->isSpawned())
            {
                pos.Set(flag->GetPositionX(), flag->GetPositionY(), flag->GetPositionZ(), flag->GetMapId());
                foundObjective = true;
            }
        }

        // 4. Default: defend owned node
        if (!foundObjective && urand(0, 99) < 50)
        {
            std::vector<uint32> owned;
            for (auto const& obj : front)
            {
                uint32 nodeId = std::get<0>(obj);
                if (IsOwned(nodeId))
                    owned.push_back(nodeId);
            }

            if (!owned.empty())
            {
                uint32 chosenId = owned[urand(0, owned.size() - 1)];
                if (EY_NodePositions.contains(chosenId))
                {
                    Position const& p = EY_NodePositions[chosenId];
                    float rx, ry, rz;
                    bot->GetRandomPoint(p, 5.0f, rx, ry, rz);
                    pos.Set(rx, ry, rz, bot->GetMapId());
                    foundObjective = true;
                }
            }
        }
    }

    // --- PRIORITY 5: Flag Strategy ---
    if (!foundObjective && strategy == EY_STRATEGY_FLAG_FOCUS)
    {
        bool ownsAny = false;
        for (auto const& [nodeId, _, __] : EY_AttackObjectives)
        {
            if (IsOwned(nodeId))
            {
                ownsAny = true;
                break;
            }
        }

        if (ownsAny)
        {
            if (Unit* fc = AI_VALUE(Unit*, "enemy flag carrier"))
            {
                pos.Set(fc->GetPositionX(), fc->GetPositionY(), fc->GetPositionZ(), bot->GetMapId());
                foundObjective = true;
            }
            else if (GameObject* flag = bg->GetBGObject(BG_EY_OBJECT_FLAG_NETHERSTORM);
                     flag && flag->isSpawned())
            {
                pos.Set(flag->GetPositionX(), flag->GetPositionY(), flag->GetPositionZ(), flag->GetMapId());
                foundObjective = true;
            }
        }
        else
        {
            float bestDist = FLT_MAX;
            Optional<uint32> bestNode;

            for (auto const& [nodeId, _, __] : EY_AttackObjectives)
            {
                if (IsOwned(nodeId))
                    continue;

                auto it = EY_NodePositions.find(nodeId);
                if (it == EY_NodePositions.end())
                    continue;

                float dist = bot->GetDistance(it->second);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestNode = nodeId;
                }
            }

            if (bestNode && EY_NodePositions.contains(*bestNode))
            {
                Position const& p = EY_NodePositions[*bestNode];
                float rx, ry, rz;
                bot->GetRandomPoint(p, 5.0f, rx, ry, rz);
                pos.Set(rx, ry, rz, bot->GetMapId());
                foundObjective = true;
            }
        }
    }

    // --- PRIORITY 6: Strategy Objectives ---
    if (!foundObjective)
    {
        std::vector<std::tuple<uint32, uint32, uint32>> priority, secondary;

        switch (strategy)
        {
            case EY_STRATEGY_FRONT_FOCUS:
                priority = {front[0], front[1]};
                secondary = {back[0], back[1]};
                break;
            case EY_STRATEGY_BACK_FOCUS:
                priority = {back[0], back[1]};
                secondary = {front[0], front[1]};
                break;
            case EY_STRATEGY_BALANCED:
            default:
                priority = {front[0], front[1], back[0], back[1]};
                break;
        }

        std::vector<uint32> candidates;

        for (auto const& obj : priority)
        {
            uint32 nodeId = std::get<0>(obj);
            if (!IsOwned(nodeId))
                candidates.push_back(nodeId);
        }

        if (candidates.empty())
        {
            for (auto const& obj : secondary)
            {
                uint32 nodeId = std::get<0>(obj);
                if (!IsOwned(nodeId))
                    candidates.push_back(nodeId);
            }
        }

        if (!candidates.empty())
        {
            uint32 chosen = candidates[urand(0, candidates.size() - 1)];
            if (EY_NodePositions.contains(chosen))
            {
                Position const& p = EY_NodePositions[chosen];
                pos.Set(p.GetPositionX(), p.GetPositionY(), p.GetPositionZ(), bot->GetMapId());
                foundObjective = true;
            }
        }
    }

    // --- PRIORITY 7: Camp GY if everything is owned ---
    bool ownsAll = IsOwned(std::get<0>(front[0])) && IsOwned(std::get<0>(front[1])) &&
                   IsOwned(std::get<0>(back[0])) && IsOwned(std::get<0>(back[1]));

    if (!foundObjective && ownsAll)
    {
        Position camp = (team == TEAM_HORDE) ? EY_GY_CAMPING_ALLIANCE : EY_GY_CAMPING_HORDE;
        float rx, ry, rz;
        bot->GetRandomPoint(camp, 10.0f, rx, ry, rz);
        pos.Set(rx, ry, rz, bot->GetMapId());
        foundObjective = true;
    }

    if (foundObjective)
        posMap["bg objective"] = pos;

    return true;
}
