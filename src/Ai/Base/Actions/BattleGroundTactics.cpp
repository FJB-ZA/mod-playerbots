/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BattleGroundTactics.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "BattleGroundJoinAction.h"
#include "Battleground.h"
#include "BattlegroundAB.h"
#include "BattlegroundAV.h"
#include "BattlegroundBE.h"
#include "BattlegroundDS.h"
#include "BattlegroundEY.h"
#include "BattlegroundIC.h"
#include "BattlegroundMgr.h"
#include "BattlegroundNA.h"
#include "BattlegroundRL.h"
#include "BattlegroundRV.h"
#include "BattlegroundSA.h"
#include "BattlegroundWS.h"
#include "Event.h"
#include "GameObject.h"
#include "IVMapMgr.h"
#include "LivingWorldMgr.h"
#include "PathGenerator.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "PvpTriggers.h"
#include "ServerFacade.h"
#include "Vehicle.h"
#include <algorithm>

// Per battleground positions, paths and objectives live in src/Ai/Battleground/<Name>Tactics.cpp

std::unordered_map<uint32, BGStrategyData> bgStrategies;
std::mutex bgStrategiesLock;

namespace
{
bool IsOneWay(BattleBotPath* path)
{
    return std::find(vPaths_OneWay_WS.begin(), vPaths_OneWay_WS.end(), path) != vPaths_OneWay_WS.end() ||
           std::find(vPaths_OneWay_IC.begin(), vPaths_OneWay_IC.end(), path) != vPaths_OneWay_IC.end();
}
}  // namespace

// useful commands for fixing BG bugs and checking waypoints/paths
bool BGTactics::HandleConsoleCommand(ChatHandler* handler, char const* args)
{
    if (!sPlayerbotAIConfig.Enabled)
    {
        handler->PSendSysMessage("|cffff0000Playerbot system is currently disabled!");
        return true;
    }
    WorldSession* session = handler->GetSession();
    if (!session)
    {
        handler->PSendSysMessage("Command can only be used from an active session");
        return true;
    }
    std::string const commandOutput = HandleConsoleCommandPrivate(session, args);
    if (!commandOutput.empty())
        handler->PSendSysMessage(commandOutput.c_str());
    return true;
}

// has different name to above as some compilers get confused
std::string const BGTactics::HandleConsoleCommandPrivate(WorldSession* session, char const* args)
{
    Player* player = session->GetPlayer();
    if (!player)
        return "Error - session player not found";
    if (!player->CanBeGameMaster())
        return "Command can only be used by a GM";
    Battleground* bg = player->GetBattleground();
    if (!bg)
        return "Command can only be used within a battleground";
    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);
    char const* usage = "usage: showpath(=[num]) / showcreature=[num] / showobject=[num]";
    char* cmd = strtok((char*)args, " ");
    // char* charname = strtok(nullptr, " ");
    if (!cmd)
        return usage;

    if (!strncmp(cmd, "showpath", 8))
    {
        int num = -1;
        if (!strncmp(cmd, "showpath=all", 12))
        {
            num = -2;
        }
        else if (!strncmp(cmd, "showpath=", 9))
        {
            if (sscanf(cmd, "showpath=%d", &num) == -1 || num < 0)
                return "Bad showpath parameter";
        }
        std::vector<BattleBotPath*> const* vPaths;
        switch (bgType)
        {
            case BATTLEGROUND_AB:
                vPaths = &vPaths_AB;
                break;
            case BATTLEGROUND_AV:
                vPaths = &vPaths_AV;
                break;
            case BATTLEGROUND_WS:
                vPaths = &vPaths_WS;
                break;
            case BATTLEGROUND_EY:
                vPaths = &vPaths_EY;
                break;
            case BATTLEGROUND_IC:
                vPaths = &vPaths_IC;
                break;
            default:
                vPaths = nullptr;
                break;
        }
        if (!vPaths)
            return "This battleground has no paths and is unsupported";
        if (num == -1)
        {
            float closestPoint = FLT_MAX;
            for (uint32 j = 0; j < vPaths->size(); j++)
            {
                auto const& path = (*vPaths)[j];
                for (uint32 i = 0; i < path->size(); i++)
                {
                    BattleBotWaypoint& waypoint = ((*path)[i]);
                    float dist = player->GetDistance(waypoint.x, waypoint.y, waypoint.z);
                    if (closestPoint > dist)
                    {
                        closestPoint = dist;
                        num = j;
                    }
                }
            }
        }
        uint32 min = 0u;
        uint32 max = vPaths->size() - 1;
        if (num >= 0)  // num specified or found
        {
            if (uint32(num) > max)
                return fmt::format("Path {} of range of 0 - {}", num, max);
            min = num;
            max = num;
        }
        for (uint32 j = min; j <= max; j++)
        {
            auto const& path = (*vPaths)[j];
            for (uint32 i = 0; i < path->size(); i++)
            {
                BattleBotWaypoint& waypoint = ((*path)[i]);
                Creature* wpCreature = player->SummonCreature(15631, waypoint.x, waypoint.y, waypoint.z, 0,
                                                              TEMPSUMMON_TIMED_DESPAWN, 15000u);
                wpCreature->SetOwnerGUID(player->GetGUID());
            }
        }
        if (num >= 0)
            return fmt::format("Showing path {}", num);
        return fmt::format("Showing paths 0 - {}", max);
    }

    if (!strncmp(cmd, "showcreature=", 13))
    {
        uint32 num;
        if (sscanf(cmd, "showcreature=%u", &num) == -1)
            return "Bad showcreature parameter";
        if (num >= bg->BgCreatures.size())
            return fmt::format("Creature out of range of 0 - {}", bg->BgCreatures.size() - 1);
        Creature* c = bg->GetBGCreature(num);
        if (!c)
            return "Creature not found";
        Creature* wpCreature = player->SummonCreature(15631, c->GetPositionX(), c->GetPositionY(), c->GetPositionZ(), 0,
                                                      TEMPSUMMON_TIMED_DESPAWN, 15000u);
        wpCreature->SetOwnerGUID(player->GetGUID());
        float distance = player->GetDistance(c);
        float exactDistance = player->GetExactDist(c);
        return fmt::format("Showing Creature {} location={:.3f},{:.3f},{:.3f} distance={} exactDistance={}",
            num, c->GetPositionX(), c->GetPositionY(), c->GetPositionZ(), distance, exactDistance);
    }

    if (!strncmp(cmd, "showobject=", 11))
    {
        uint32 num;
        if (sscanf(cmd, "showobject=%u", &num) == -1)
            return "Bad showobject parameter";
        if (num >= bg->BgObjects.size())
            return fmt::format("Object out of range of 0 - {}", bg->BgObjects.size() - 1);
        GameObject* o = bg->GetBGObject(num);
        if (!o)
            return "GameObject not found";
        Creature* wpCreature = player->SummonCreature(15631, o->GetPositionX(), o->GetPositionY(), o->GetPositionZ(), 0,
                                                      TEMPSUMMON_TIMED_DESPAWN, 15000u);
        wpCreature->SetOwnerGUID(player->GetGUID());
        float distance = player->GetDistance(o);
        float exactDistance = player->GetExactDist(o);
        return fmt::format("Showing GameObject {} location={:.3f},{:.3f},{:.3f} distance={} exactDistance={}",
            num, o->GetPositionX(), o->GetPositionY(), o->GetPositionZ(), distance, exactDistance);
    }

    return usage;
}

// Depends on OnBattlegroundStart in playerbots.cpp
uint8 BGTactics::GetBotStrategyForTeam(Battleground* bg, TeamId teamId)
{
    std::lock_guard<std::mutex> guard(bgStrategiesLock);
    auto itr = bgStrategies.find(bg->GetInstanceID());
    if (itr == bgStrategies.end())
        return 0;

    return teamId == TEAM_ALLIANCE ? itr->second.allianceStrategy : itr->second.hordeStrategy;
}

//
// actual bg tactics below
//
bool BGTactics::Execute(Event /*event*/)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
    {
        botAI->ResetStrategies();
        return false;
    }

    if (bg->GetStatus() == STATUS_WAIT_LEAVE)
        return BGStatusAction::LeaveBG(botAI);

    if (bg->isArena())
    {
        // can't use this in arena - no vPaths/vFlagIds (will crash server)
        botAI->ResetStrategies();
        return false;
    }

    if (bg->GetStatus() == STATUS_IN_PROGRESS)
        botAI->ChangeStrategy("-buff", BOT_STATE_NON_COMBAT);

    std::vector<BattleBotPath*> const* vPaths;
    std::vector<uint32> const* vFlagIds;
    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bot->GetBattleground()->GetBgTypeID(true);

    switch (bgType)
    {
        case BATTLEGROUND_AB:
        {
            vPaths = &vPaths_AB;
            vFlagIds = &vFlagsAB;
            break;
        }
        case BATTLEGROUND_AV:
        {
            vPaths = &vPaths_AV;
            vFlagIds = &vFlagsAV;
            break;
        }
        case BATTLEGROUND_WS:
        {
            vPaths = &vPaths_WS;
            vFlagIds = &vFlagsWS;
            break;
        }
        case BATTLEGROUND_EY:
        {
            vPaths = &vPaths_EY;
            vFlagIds = &vFlagsEY;
            break;
        }
        case BATTLEGROUND_IC:
        {
            vPaths = &vPaths_IC;
            vFlagIds = &vFlagsIC;
            break;
        }
        case BATTLEGROUND_SA:
        {
            vPaths = &vPaths_SA;
            vFlagIds = &vFlagsSA;
            break;
        }
        default:
            // can't use this in this BG - no vPaths/vFlagIds (will crash server)
            botAI->ResetStrategies();
            return false;
    }

    if (getName() == "move to start")
        return moveToStart();

    if (getName() == "reset objective force")
    {
        bool isCarryingFlag =
            bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) ||
            bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
            bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL);

        if (!isCarryingFlag)
        {
            bot->StopMoving();
            bot->GetMotionMaster()->Clear();
            return resetObjective();  // Reset objective to not use "old" data
        }
    }

    if (getName() == "select objective")
        return selectObjective();

    if (getName() == "protect fc")
    {
        if (protectFC())
            return true;
    }

    if (getName() == "move to objective")
    {
        if (bg->GetStatus() == STATUS_WAIT_JOIN)
            return false;

        if (bot->isMoving())
            return false;

        if (!bot->IsStopped())
            return false;

        switch (bot->GetMotionMaster()->GetCurrentMovementGeneratorType())
        {
            // TODO: should ESCORT_MOTION_TYPE be here seeing as bots use it by default?
            case IDLE_MOTION_TYPE:
            case CHASE_MOTION_TYPE:
            case POINT_MOTION_TYPE:
                break;
            default:
                return true;
        }

        if (vFlagIds && atFlag(*vPaths, *vFlagIds))
            return true;

        if (useBuff())
            return true;

        // NOTE: can't use IsInCombat() when in vehicle as player is stuck in combat forever while in vehicle (ac bug?)
        bool inCombat = bot->GetVehicle() ? (bool)AI_VALUE(Unit*, "enemy player target") : bot->IsInCombat();
        // a vehicle driver with a siege position keeps driving to it; its weapons still fire on the way
        bool const siegeDrive = botAI->IsInVehicle(true) &&
                                context->GetValue<PositionMap&>("position")->Get()["bg siege"].isSet();
        if (inCombat && !siegeDrive && !PlayerHasFlag::IsCapturingFlag(bot))
        {
            // bot->GetMotionMaster()->MovementExpired();
            return false;
        }

        if (!moveToObjective(false))
            if (!selectObjectiveWp(*vPaths))
                return moveToObjective(true);

        // bot with flag should only move to objective
        if (bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
            bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL))
            return false;

        // EotS routes meet at the bases, bridges and center: head straight to the objective instead of a random route
        if (bgType == BATTLEGROUND_EY)
            return moveToObjective(true);

        if (!startNewPathBegin(*vPaths))
            return moveToObjective(true);

        if (!startNewPathFree(*vPaths))
            return moveToObjective(true);
    }

    if (getName() == "use buff")
        return useBuff();

    if (getName() == "check flag")
    {
        if (vFlagIds && atFlag(*vPaths, *vFlagIds))
            return true;
    }

    if (getName() == "check objective")
        return resetObjective();

    return false;
}

bool BGTactics::moveToStart(bool force)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    if (!force && bg->GetStatus() != STATUS_WAIT_JOIN)
        return false;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    switch (bgType)
    {
        case BATTLEGROUND_WS:
            moveToStartWS(bg);
            break;
        case BATTLEGROUND_AB:
            moveToStartAB(bg);
            break;
        case BATTLEGROUND_AV:
            moveToStartAV(bg);
            break;
        case BATTLEGROUND_IC:
            moveToStartIC(bg);
            break;
        case BATTLEGROUND_SA:
            return false;  // attackers wait on the boats, defenders inside the walls
        default:
            break;
    }

    return true;
}

bool BGTactics::selectObjective(bool reset)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    if (bg->GetStatus() != STATUS_IN_PROGRESS)
        return false;

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = context->GetValue<PositionMap&>("position")->Get()["bg objective"];
    if (pos.isSet() && !reset)
        return false;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);
    switch (bgType)
    {
        case BATTLEGROUND_AV:
            return selectObjectiveAV(bg, posMap, pos);
        case BATTLEGROUND_WS:
            return selectObjectiveWS(bg, posMap, pos);
        case BATTLEGROUND_AB:
            return selectObjectiveAB(bg, posMap, pos);
        case BATTLEGROUND_EY:
            return selectObjectiveEY(bg, posMap, pos);
        case BATTLEGROUND_IC:
            return selectObjectiveIC(bg, posMap, pos);
        case BATTLEGROUND_SA:
            return selectObjectiveSA(bg, posMap, pos);
        default:
            break;
    }

    return false;
}

bool BGTactics::moveToObjective(bool ignoreDist)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    PositionInfo pos = context->GetValue<PositionMap&>("position")->Get()["bg objective"];
    if (!pos.isSet())
        return selectObjective();
    else
    {
        // Use portals in Isle of Conquest Base
        if (bgType == BATTLEGROUND_IC)
        {
            if (IsLockedInsideKeep())
                return true;
        }

        if (!ignoreDist && ServerFacade::instance().IsDistanceGreaterThan(ServerFacade::instance().GetDistance2d(bot, pos.x, pos.y), 100.0f))
        {
            // std::ostringstream out;
            // out << "It is too far away! " << pos.x << ", " << pos.y << ", Distance: " <<
            // ServerFacade::instance().GetDistance2d(bot, pos.x, pos.y); bot->Say(out.str(), LANG_UNIVERSAL);
            return false;
        }

        // don't try to move if already close
        if (bot->GetDistance(pos.x, pos.y, pos.z) < 4.0f)
        {
            resetObjective();
            return true;
        }

        // std::ostringstream out; out << "Moving to objective " << pos.x << ", " << pos.y << ", Distance: " <<
        // ServerFacade::instance().GetDistance2d(bot, pos.x, pos.y); bot->Say(out.str(), LANG_UNIVERSAL);

        // dont increase from 1.5 will cause bugs with horde capping AV towers
        return MoveNear(bot->GetMapId(), pos.x, pos.y, pos.z, 1.5f);
    }
    return false;
}

bool BGTactics::selectObjectiveWp(std::vector<BattleBotPath*> const& vPaths)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    PositionInfo pos = context->GetValue<PositionMap&>("position")->Get()["bg objective"];
    if (!pos.isSet())
        return false;

    if (bgType == BATTLEGROUND_WS)
    {
        // the cliff jump leads to mid: never take it toward our own flag room
        bool const downhill = bot->GetTeamId() == TEAM_HORDE ? pos.x > 1060.0f : pos.x < 1390.0f;
        if (downhill && wsJumpDown())
            return true;
    }
    else if (bgType == BATTLEGROUND_AV)
    {
        if (avLeaveCave())
            return true;
    }
    else if (bgType == BATTLEGROUND_EY)
    {
        if (eyJumpDown())
            return true;
    }
    else if (bgType == BATTLEGROUND_SA)
    {
        if (saLeaveBoat())
            return true;
    }

    float chosenPathScore = FLT_MAX;  // lower score is better
    BattleBotPath* chosenPath = nullptr;
    uint32 chosenPathPoint = 0;
    bool chosenPathReverse = false;

    float botDistanceLimit = 50.0f;         // limit for how far path can be from bot
    float botDistanceScoreSubtract = 8.0f;  // path score modifier - lower = less likely to chose a further path (it's
                                            // basically the distance from bot that's ignored)
    float botDistanceScoreMultiply =
        3.0f;  // path score modifier - higher = less likely to chose a further path (it's basically a multiplier on
               // distance from bot - makes distance from bot more signifcant than distance from destination)

    if (bgType == BATTLEGROUND_IC)
    {
        // botDistanceLimit = 80.0f;
        botDistanceScoreMultiply = 8.0f;
        botDistanceScoreSubtract = 2.0f;
    }
    else if (bgType == BATTLEGROUND_AV)
    {
        botDistanceLimit = 80.0f;
        botDistanceScoreMultiply = 8.0f;
    }
    else if (bgType == BATTLEGROUND_AB || bgType == BATTLEGROUND_EY || bgType == BATTLEGROUND_WS)
    {
        botDistanceScoreSubtract = 2.0f;
        botDistanceScoreMultiply = 4.0f;
    }

    // uint32 index = -1;
    // uint32 chosenPathIndex = -1;
    for (auto const& path : vPaths)
    {
        // index++;
        // TODO need to remove sqrt from these two and distToBot but it totally throws path scoring out of
        // whack if you do that without changing how its implemented (I'm amazed it works as well as it does
        // using sqrt'ed distances)
        // In a reworked version maybe compare the differences of path distances to point (ie: against best path)
        // or maybe ratio's (where if a path end is twice the difference in distance from destination we basically
        // use that to multiply the total score?
        BattleBotWaypoint& startPoint = ((*path)[0]);
        float const startPointDistToDestination =
            sqrt(Position(pos.x, pos.y, pos.z, 0.f).GetExactDist(startPoint.x, startPoint.y, startPoint.z));
        BattleBotWaypoint& endPoint = ((*path)[path->size() - 1]);
        float const endPointDistToDestination =
            sqrt(Position(pos.x, pos.y, pos.z, 0.f).GetExactDist(endPoint.x, endPoint.y, endPoint.z));

        bool reverse = startPointDistToDestination < endPointDistToDestination;

        // dont travel reverse if it's a reverse paths
        if (reverse && IsOneWay(path))
            continue;

        int closestPointIndex = -1;
        float closestPointDistToBot = FLT_MAX;
        for (uint32 i = 0; i < path->size(); i++)
        {
            BattleBotWaypoint& waypoint = ((*path)[i]);
            float const distToBot = sqrt(bot->GetDistance(waypoint.x, waypoint.y, waypoint.z));
            if (closestPointDistToBot > distToBot)
            {
                closestPointDistToBot = distToBot;
                closestPointIndex = i;
            }
        }

        // don't pick path where bot is already closest to the paths closest point to target (it means path cant lead it
        // anywhere) don't pick path where closest point is too far away
        if (closestPointIndex == int(reverse ? 0 : path->size() - 1) || closestPointDistToBot > botDistanceLimit)
            continue;

        // creates a score based on dist-to-bot and dist-to-destination, where lower is better, and dist-to-bot is more
        // important (when its beyond a certain distance) dist-to-bot is more important because otherwise they cant
        // reach it at all (or will fly through air with MM::MovePoint()), also bot may need to use multiple paths (one
        // after another) anyway
        float distToDestination = reverse ? startPointDistToDestination : endPointDistToDestination;
        float pathScore = (closestPointDistToBot < botDistanceScoreSubtract
                               ? 0.0f
                               : ((closestPointDistToBot - botDistanceScoreSubtract) * botDistanceScoreMultiply)) +
                          distToDestination;

        // LOG_INFO("playerbots", "bot={}\t{:6.1f}\t{:4.1f}\t{:4.1f}\t{}", bot->GetName(), pathScore,
        // closestPointDistToBot, distToDestination, vPaths_AB_name[pathNum]);

        if (chosenPathScore > pathScore)
        {
            chosenPathScore = pathScore;
            chosenPath = path;
            chosenPathPoint = closestPointIndex;
            chosenPathReverse = reverse;
            // chosenPathIndex = index;
        }
    }

    if (!chosenPath)
        return false;

    // LOG_INFO("playerbots", "{} bot={} path={}", (bot->GetTeamId() == TEAM_HORDE ? "HORDE" : "ALLIANCE"),
    // bot->GetName(), chosenPathIndex);

    return moveToObjectiveWp(chosenPath, chosenPathPoint, chosenPathReverse);
}

bool BGTactics::resetObjective()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    // Adjust role-change chance based on battleground type
    uint32 oddsToChangeRole = 1;  // default low
    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    if (bgType == BATTLEGROUND_WS)
        oddsToChangeRole = 2;
    else if (bgType == BATTLEGROUND_EY || bgType == BATTLEGROUND_IC || bgType == BATTLEGROUND_AB)
        oddsToChangeRole = 1;
    else if (bgType == BATTLEGROUND_AV)
        oddsToChangeRole = 0;

    bool isCarryingFlag =
        bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) ||
        bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
        bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL);

    // Change role if allowed by odds and not carrying flag
    if (urand(0, 99) < oddsToChangeRole && !isCarryingFlag)
        context->GetValue<uint32>("bg role")->Set(LivingWorldMgr::instance().GetBattlegroundRole(bot, urand(0, 9)));

    // Reset objective position
    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = context->GetValue<PositionMap&>("position")->Get()["bg objective"];
    pos.Reset();
    posMap["bg objective"] = pos;

    return selectObjective(true);
}

bool BGTactics::moveToObjectiveWp(BattleBotPath* const& currentPath, uint32 currentPoint, bool reverse)
{
    if (!currentPath)
        return false;

    uint32 const lastPointInPath = reverse ? 0 : ((*currentPath).size() - 1);

    // NOTE: can't use IsInCombat() when in vehicle as player is stuck in combat forever while in vehicle (ac bug?)
    bool inCombat = bot->GetVehicle() ? (bool)AI_VALUE(Unit*, "enemy player target") : bot->IsInCombat();
    if (currentPoint == lastPointInPath || (inCombat && !PlayerHasFlag::IsCapturingFlag(bot)) || !bot->IsAlive())
    {
        // Path is over.
        // std::ostringstream out;
        // out << "Reached path end!";
        // bot->Say(out.str(), LANG_UNIVERSAL);
        resetObjective();
        return false;
    }

    uint32 currPoint = currentPoint;

    if (reverse)
        currPoint--;
    else
        currPoint++;

    uint32 nPoint = reverse ? std::max((int)(currPoint - urand(1, 5)), 0)
                            : std::min((uint32)(currPoint + urand(1, 5)), lastPointInPath);
    BattleBotWaypoint& nextPoint = currentPath->at(nPoint);

    // std::ostringstream out;
    // out << "WP: ";
    // reverse ? out << currPoint << " <<< -> " << nPoint : out << currPoint << ">>> ->" << nPoint;
    // out << ", " << nextPoint.x << ", " << nextPoint.y << " Path Size: " << currentPath->size() << ", Dist: " <<
    // ServerFacade::instance().GetDistance2d(bot, nextPoint.x, nextPoint.y); bot->Say(out.str(), LANG_UNIVERSAL);

    return MoveTo(bot->GetMapId(), nextPoint.x + frand(-2, 2), nextPoint.y + frand(-2, 2), nextPoint.z);
}

bool BGTactics::startNewPathBegin(std::vector<BattleBotPath*> const& vPaths)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    if (bgType == BATTLEGROUND_IC)
        return false;

    PositionInfo pos = context->GetValue<PositionMap&>("position")->Get()["bg objective"];
    if (!pos.isSet())
        return false;

    // Important for eye of the storm bg, so that bots stay near bg objective (see selectObjective for more Info)
    if (bot->GetDistance(pos.x, pos.y, pos.z) < 25.0f)
        return false;

    struct AvailablePath
    {
        AvailablePath(BattleBotPath* pPath_, bool reverse_) : pPath(pPath_), reverse(reverse_) {}

        BattleBotPath* pPath = nullptr;
        bool reverse = false;
    };

    std::vector<AvailablePath> availablePaths;
    for (auto const& pPath : vPaths)
    {
        // TODO remove sqrt
        BattleBotWaypoint* pStart = &((*pPath)[0]);
        if (sqrt(bot->GetDistance(pStart->x, pStart->y, pStart->z)) < INTERACTION_DISTANCE)
            availablePaths.emplace_back(AvailablePath(pPath, false));

        // Some paths are not allowed backwards.
        if (IsOneWay(pPath))
            continue;

        // TODO remove sqrt
        BattleBotWaypoint* pEnd = &((*pPath)[(*pPath).size() - 1]);
        if (sqrt(bot->GetDistance(pEnd->x, pEnd->y, pEnd->z)) < INTERACTION_DISTANCE)
            availablePaths.emplace_back(AvailablePath(pPath, true));
    }

    if (availablePaths.empty())
        return false;

    uint32 randomPath = urand(0, availablePaths.size() - 1);
    AvailablePath const* chosenPath = &availablePaths[randomPath];

    BattleBotPath* currentPath = chosenPath->pPath;
    bool reverse = chosenPath->reverse;
    uint32 currentPoint = reverse ? currentPath->size() - 1 : 0;

    return moveToObjectiveWp(currentPath, currentPoint, reverse);
}

bool BGTactics::startNewPathFree(std::vector<BattleBotPath*> const& vPaths)
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    if (bgType == BATTLEGROUND_IC)
        return false;

    PositionInfo pos = context->GetValue<PositionMap&>("position")->Get()["bg objective"];
    if (!pos.isSet())
        return false;

    // Important for eye of the storm bg, so that bots stay near bg objective (see selectObjective for more Info)
    if (bot->GetDistance(pos.x, pos.y, pos.z) < 25.0f)
        return false;

    BattleBotPath* pClosestPath = nullptr;
    uint32 closestPoint = 0;
    float closestDistance = FLT_MAX;

    for (auto const& pPath : vPaths)
    {
        for (uint32 i = 0; i < pPath->size(); i++)
        {
            BattleBotWaypoint& waypoint = ((*pPath)[i]);
            // TODO remove sqrt
            float const distanceToPoint = sqrt(bot->GetDistance(waypoint.x, waypoint.y, waypoint.z));
            if (distanceToPoint < closestDistance)
            {
                pClosestPath = pPath;
                closestPoint = i;
                closestDistance = distanceToPoint;
            }
        }
    }

    if (!pClosestPath)
        return false;

    BattleBotPath* currentPath = pClosestPath;
    bool reverse = false;
    uint32 currentPoint = closestPoint - 1;

    return moveToObjectiveWp(currentPath, currentPoint, reverse);
}

/**
 * @brief Handles flag/base capturing gameplay in battlegrounds
 *
 * This function manages the logic for capturing flags and bases in various battlegrounds.
 * It handles:
 * - Enemy detection and combat near objectives
 * - Coordination with friendly players who are capturing
 * - Different capture mechanics for each battleground type
 * - Proper positioning and movement
 *
 * @param vPaths Vector of possible paths the bot can take
 * @param vFlagIds Vector of flag/base GameObjects that can be captured
 * @return true if handling a flag/base action, false otherwise
 */
bool BGTactics::atFlag(std::vector<BattleBotPath*> const& vPaths, std::vector<uint32> const& vFlagIds)
{
    // Basic sanity checks
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    // Get the actual BG type (in case of random BG)
    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    // Initialize vectors for nearby objects and players
    GuidVector closeObjects;
    GuidVector closePlayers;
    float flagRange = 0.0f;

    // Eye of the Storm helpers used later when handling capture positioning
    BattlegroundEY* eyeBg = nullptr;
    GameObject* eyCenterFlag = nullptr;
    if (bgType == BATTLEGROUND_EY)
    {
        eyeBg = static_cast<BattlegroundEY*>(bg);
        if (eyeBg)
            eyCenterFlag = eyeBg->GetBGObject(BG_EY_OBJECT_FLAG_NETHERSTORM);
    }

    // Set up appropriate search ranges and object lists based on BG type
    switch (bgType)
    {
        case BATTLEGROUND_AV:
        case BATTLEGROUND_AB:
        case BATTLEGROUND_IC:
        case BATTLEGROUND_SA:
        {
            // For territory control BGs, use standard interaction range
            closeObjects = *context->GetValue<GuidVector>("closest game objects");
            closePlayers = *context->GetValue<GuidVector>("closest friendly players");
            flagRange = INTERACTION_DISTANCE;
            break;
        }
        case BATTLEGROUND_WS:
        case BATTLEGROUND_EY:
        {
            // For flag carrying BGs, use wider range and ignore LOS
            closeObjects = *context->GetValue<GuidVector>("nearest game objects no los");
            closePlayers = *context->GetValue<GuidVector>("closest friendly players");
            flagRange = 25.0f;
            break;
        }
        default:
            break;
    }

    if (closeObjects.empty())
        return false;

    auto keepStationaryWhileCapturing = [&](CurrentSpellTypes spellType)
    {
        Spell* currentSpell = bot->GetCurrentSpell(spellType);
        if (!currentSpell || !currentSpell->m_spellInfo || currentSpell->m_spellInfo->Id != SPELL_CAPTURE_BANNER)
            return false;

        // If the capture target is no longer available (another bot already captured it), stop channeling
        GameObject* targetFlag = currentSpell->m_targets.GetGOTarget();
        if (!targetFlag || !targetFlag->isSpawned() || targetFlag->GetGoState() != GO_STATE_READY)
        {
            bot->CastStop();
            resetObjective();
            return false;
        }

        if (bot->IsMounted())
        {
            bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
        }

        if (bot->IsInDisallowedMountForm())
        {
            bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
        }

        if (bot->isMoving())
        {
            bot->StopMoving();
        }

        return true;
    };

    // If we are already channeling the capture spell, keep the bot stationary and dismounted
    if (keepStationaryWhileCapturing(CURRENT_CHANNELED_SPELL) || keepStationaryWhileCapturing(CURRENT_GENERIC_SPELL))
        return true;

    // First identify which flag/base we're trying to interact with
    GameObject* targetFlag = nullptr;
    for (ObjectGuid const guid : closeObjects)
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (!go)
        {
            continue;
        }

        bool const isEyCenterFlag = eyeBg && eyCenterFlag && eyCenterFlag->GetGUID() == go->GetGUID();

        // Check if this object is a valid capture target
        std::vector<uint32>::const_iterator f = std::find(vFlagIds.begin(), vFlagIds.end(), go->GetEntry());
        if (f == vFlagIds.end() && !isEyCenterFlag)
        {
            continue;
        }

        // Verify the object is active and ready
        if (!go->isSpawned() || go->GetGoState() != GO_STATE_READY)
        {
            continue;
        }

        if (bgType == BATTLEGROUND_SA && !saCanCapture(bg, go))
            continue;

        // Check if we're in range (using double range for enemy detection)
        float const dist = bot->GetDistance(go);
        if (flagRange && dist > flagRange * 2.0f)
        {
            continue;
        }

        targetFlag = go;
        break;
    }

    // If we found a valid flag/base to interact with
    if (bgType != BATTLEGROUND_WS && bgType != BATTLEGROUND_AV)
    {
        if (targetFlag)
        {
            // Check for enemy players near the flag using bot's targeting system
            Unit* enemyPlayer = AI_VALUE(Unit*, "enemy player target");
            if (enemyPlayer && enemyPlayer->IsAlive())
            {
                // If enemy is near the flag, engage them before attempting capture
                float enemyDist = enemyPlayer->GetDistance(targetFlag);
                if (enemyDist < flagRange * 2.0f)
                {
                    // Set enemy as current target and let combat AI handle it
                    context->GetValue<Unit*>("current target")->Set(enemyPlayer);
                    return false;
                }
            }
        }
    }

    // Check if friendly players are already capturing
    if (!closePlayers.empty() && bgType != BATTLEGROUND_EY)
    {
        // Track number of friendly players capturing and the closest one
        uint32 numCapturing = 0;
        Unit* capturingPlayer = nullptr;
        for (auto& guid : closePlayers)
        {
            if (Unit* pFriend = botAI->GetUnit(guid))
            {
                // Check if they're casting or channeling the capture spell
                Spell* spell = pFriend->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                if (!spell)
                {
                    spell = pFriend->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
                }

                if (spell && spell->m_spellInfo && spell->m_spellInfo->Id == SPELL_CAPTURE_BANNER)
                {
                    numCapturing++;
                    capturingPlayer = pFriend;
                }
            }
        }

        // If friendlies are capturing, stay to defend but don't capture
        if (numCapturing > 0 && capturingPlayer && bot->GetGUID() != capturingPlayer->GetGUID())
        {
            // Move away if too close to avoid crowding
            if (bot->GetDistance2d(capturingPlayer) < 3.0f)
            {
                float angle = bot->GetAngle(capturingPlayer);
                float x = bot->GetPositionX() + 5.0f * cos(angle);
                float y = bot->GetPositionY() + 5.0f * sin(angle);
                MoveTo(bot->GetMapId(), x, y, bot->GetPositionZ());
            }

            // Reset objective and take new path for defending
            resetObjective();
            if (!startNewPathBegin(vPaths))
                moveToObjective(true);
            return true;
        }
    }

    // Area is clear of enemies and no friendlies are capturing
    // Proceed with capture mechanics
    for (ObjectGuid const guid : closeObjects)
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (!go)
            continue;

        bool const isEyCenterFlag = eyeBg && eyCenterFlag && eyCenterFlag->GetGUID() == go->GetGUID();

        // Validate this is a capture target
        std::vector<uint32>::const_iterator f = std::find(vFlagIds.begin(), vFlagIds.end(), go->GetEntry());
        if (f == vFlagIds.end() && !isEyCenterFlag)
            continue;

        // Check object is active
        if (!go->isSpawned() || go->GetGoState() != GO_STATE_READY)
            continue;

        if (bgType == BATTLEGROUND_SA && !saCanCapture(bg, go))
            continue;

        // WSG flags fail the faction part of this check; immunity and Recently Dropped Flag still apply
        if (!bot->CanUseBattlegroundObject(bgType == BATTLEGROUND_WS ? nullptr : go))
            continue;

        float const dist = bot->GetDistance(go);
        if (flagRange && dist > flagRange)
            continue;

        // Special handling for WSG and EY base flags
        bool isWsBaseFlag = bgType == BATTLEGROUND_WS && go->GetEntry() == vFlagsWS[bot->GetTeamId()];
        bool isEyBaseFlag = bgType == BATTLEGROUND_EY && go->GetEntry() == vFlagsEY[0];

        // Ensure bots are inside the Eye of the Storm capture circle before casting
        if (bgType == BATTLEGROUND_EY)
        {
            GameObject* captureFlag = (isEyBaseFlag && eyCenterFlag) ? eyCenterFlag : go;
            float const requiredRange = 2.5f;
            if (!bot->IsWithinDistInMap(captureFlag, requiredRange))
            {
                // Stay mounted while relocating to avoid mount/dismount loops
                return MoveTo(bot->GetMapId(), captureFlag->GetPositionX(), captureFlag->GetPositionY(),
                              captureFlag->GetPositionZ());
            }

            // Once inside the circle, dismount and stop before starting the channel
            if (bot->IsMounted())
            {
                bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
            }

            if (bot->IsInDisallowedMountForm())
            {
                bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
            }

            if (bot->isMoving())
            {
                bot->StopMoving();
            }
        }

        // Don't capture own flag in WSG unless carrying enemy flag
        if (isWsBaseFlag && bgType == BATTLEGROUND_WS &&
            !(bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG)))
            continue;

        // Handle capture mechanics based on BG type
        switch (bgType)
        {
            case BATTLEGROUND_AV:
            case BATTLEGROUND_AB:
            case BATTLEGROUND_IC:
            case BATTLEGROUND_SA:
            {
                // Prevent capturing from inside flag pole
                if (dist == 0.0f)
                {
                    float const moveDist = bot->GetObjectSize() + go->GetObjectSize() + 0.1f;
                    return MoveTo(bot->GetMapId(), go->GetPositionX() + (urand(0, 1) ? -moveDist : moveDist),
                                  go->GetPositionY() + (urand(0, 1) ? -moveDist : moveDist), go->GetPositionZ());
                }

                // Dismount before capturing
                if (bot->IsMounted())
                    bot->RemoveAurasByType(SPELL_AURA_MOUNTED);

                if (bot->IsInDisallowedMountForm())
                    bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);

                // Cast the capture spell
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_CAPTURE_BANNER);
                if (!spellInfo)
                    return false;

                Spell* spell = new Spell(bot, spellInfo, TRIGGERED_NONE);
                spell->m_targets.SetGOTarget(go);
                spell->prepare(&spell->m_targets);

                botAI->WaitForSpellCast(spell);

                resetObjective();
                return true;
            }
            case BATTLEGROUND_WS:
            {
                if (dist < INTERACTION_DISTANCE)
                {
                    // Handle flag capture at base
                    if (isWsBaseFlag)
                    {
                        if (bot->GetTeamId() == TEAM_HORDE)
                        {
                            WorldPacket data(CMSG_AREATRIGGER);
                            data << uint32(BG_WS_TRIGGER_HORDE_FLAG_SPAWN);
                            bot->GetSession()->HandleAreaTriggerOpcode(data);
                        }
                        else
                        {
                            WorldPacket data(CMSG_AREATRIGGER);
                            data << uint32(BG_WS_TRIGGER_ALLIANCE_FLAG_SPAWN);
                            bot->GetSession()->HandleAreaTriggerOpcode(data);
                        }
                        return true;
                    }

                    // Dismount before picking up flag
                    if (bot->IsMounted())
                        bot->RemoveAurasByType(SPELL_AURA_MOUNTED);

                    if (bot->IsInDisallowedMountForm())
                        bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);

                    // Pick up the flag
                    WorldPacket data(CMSG_GAMEOBJ_USE);
                    data << go->GetGUID();
                    bot->GetSession()->HandleGameObjectUseOpcode(data);

                    resetObjective();
                    return true;
                }
                else
                {
                    // Move to flag if not in range
                    return MoveTo(bot->GetMapId(), go->GetPositionX(), go->GetPositionY(), go->GetPositionZ());
                }
            }
            case BATTLEGROUND_EY:
            {  // Handle Netherstorm flag capture requiring a channel
                if (dist < INTERACTION_DISTANCE)
                {
                    // Dismount before interacting
                    if (bot->IsMounted())
                    {
                        bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
                    }

                    if (bot->IsInDisallowedMountForm())
                    {
                        bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
                    }

                    // Handle center flag differently (requires spell cast)
                    if (isEyCenterFlag)
                    {
                        for (uint8 type = CURRENT_MELEE_SPELL; type <= CURRENT_CHANNELED_SPELL; ++type)
                        {
                            if (Spell* currentSpell = bot->GetCurrentSpell(static_cast<CurrentSpellTypes>(type)))
                            {
                                // m_spellInfo may be null in some states: protect access
                                if (currentSpell->m_spellInfo && currentSpell->m_spellInfo->Id == SPELL_CAPTURE_BANNER)
                                {
                                    bot->StopMoving();
                                    botAI->SetNextCheckDelay(500);
                                    return true;
                                }
                            }
                        }

                        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_CAPTURE_BANNER);
                        if (!spellInfo)
                            return false;

                        Spell* spell = new Spell(bot, spellInfo, TRIGGERED_NONE);
                        spell->m_targets.SetGOTarget(go);

                        bot->StopMoving();
                        spell->prepare(&spell->m_targets);

                        botAI->WaitForSpellCast(spell);
                        resetObjective();
                        return true;
                    }

                    // Pick up dropped flag
                    WorldPacket data(CMSG_GAMEOBJ_USE);
                    data << go->GetGUID();
                    bot->GetSession()->HandleGameObjectUseOpcode(data);

                    resetObjective();
                    return true;
                }
                else
                {
                    // Move to flag if not in range
                    return MoveTo(bot->GetMapId(), go->GetPositionX(), go->GetPositionY(), go->GetPositionZ());
                }
            }
            default:
                break;
        }
    }

    return false;
}

bool BGTactics::protectFC()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    Unit* teamFC = AI_VALUE(Unit*, "team flag carrier");

    if (!teamFC || teamFC == bot)
    {
        return false;
    }

    if (!bot->IsInCombat() && !bot->IsWithinDistInMap(teamFC, 20.0f))
    {
        // Get the flag carrier's position
        float fcX = teamFC->GetPositionX();
        float fcY = teamFC->GetPositionY();
        float fcZ = teamFC->GetPositionZ();
        uint32 mapId = bot->GetMapId();

        return MoveNear(mapId, fcX, fcY, fcZ, 5.0f, MovementPriority::MOVEMENT_NORMAL);
    }

    return false;
}

bool BGTactics::useBuff()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    BattlegroundTypeId bgType = bg->GetBgTypeID();
    if (bgType == BATTLEGROUND_RB)
        bgType = bg->GetBgTypeID(true);

    GuidVector closeObjects = AI_VALUE(GuidVector, "nearest game objects no los");
    if (closeObjects.empty())
        return false;

    bool needRegen = bot->GetHealthPct() < sPlayerbotAIConfig.MediumHealth ||
                     (AI_VALUE2(bool, "has mana", "self target") &&
                      AI_VALUE2(uint8, "mana", "self target") < sPlayerbotAIConfig.MediumMana);
    bool needSpeed = (bgType != BATTLEGROUND_WS || bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) ||
                      bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) || bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL)) ||
                     !(teamFlagTaken() || flagTaken());
    bool foundBuff = false;

    for (ObjectGuid const guid : closeObjects)
    {
        GameObject* go = botAI->GetGameObject(guid);
        if (!go)
            continue;

        if (!go->isSpawned())
            continue;

        // use speed buff only if close
        if (ServerFacade::instance().IsDistanceGreaterThan(ServerFacade::instance().GetDistance2d(bot, go),
                                                 go->GetEntry() == Buff_Entries[0] ? 20.0f : 50.0f))
            continue;

        if (needSpeed && go->GetEntry() == Buff_Entries[0])
            foundBuff = true;

        if (needRegen && go->GetEntry() == Buff_Entries[1])
            foundBuff = true;

        // do not move to Berserk buff if bot is healer or has flag
        if (!(bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
              bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL)) &&
            !PlayerbotAI::IsHeal(bot) && go->GetEntry() == Buff_Entries[2])
            foundBuff = true;

        if (foundBuff)
        {
            // std::ostringstream out;
            // out << "Moving to buff...";
            // bot->Say(out.str(), LANG_UNIVERSAL);
            return MoveTo(go->GetMapId(), go->GetPositionX(), go->GetPositionY(), go->GetPositionZ());
        }
    }

    return false;
}

uint32 BGTactics::getPlayersInArea(TeamId teamId, Position point, float range, bool combat)
{
    uint32 defCount = 0;

    if (!bot->InBattleground())
        return false;

    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return 0;

    for (auto& guid : bg->GetBgMap()->GetPlayers())
    {
        Player* player = guid.GetSource();
        if (!player)
            continue;

        if (player->IsAlive() && (teamId == TEAM_NEUTRAL || teamId == player->GetTeamId()))
        {
            if (!combat && player->IsInCombat())
                continue;

            if (ServerFacade::instance().GetDistance2d(player, point.GetPositionX(), point.GetPositionY()) < range)
                ++defCount;
        }
    }

    return defCount;
}

bool ArenaTactics::Execute(Event /*event*/)
{
    if (!bot->InBattleground())
    {
        bool IsRandomBot = sRandomPlayerbotMgr.IsRandomBot(bot->GetGUID().GetCounter());
        botAI->ChangeStrategy("-arena", BOT_STATE_COMBAT);
        botAI->ChangeStrategy("-arena", BOT_STATE_NON_COMBAT);
        botAI->ResetStrategies(!IsRandomBot);
        return false;
    }

    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return false;

    if (bg->GetStatus() == STATUS_WAIT_LEAVE)
        return BGStatusAction::LeaveBG(botAI);

    if (bg->GetStatus() != STATUS_IN_PROGRESS)
        return false;

    if (bot->isDead())
        return false;

    if (bot->isMoving())
        return false;

    // startup phase
    if (bg->GetStartDelayTime() > 0)
        return false;

    if (botAI->HasStrategy("collision", BOT_STATE_NON_COMBAT))
        botAI->ChangeStrategy("-collision", BOT_STATE_NON_COMBAT);

    if (botAI->HasStrategy("buff", BOT_STATE_NON_COMBAT))
        botAI->ChangeStrategy("-buff", BOT_STATE_NON_COMBAT);

    Unit* target = bot->GetVictim();
    if (target)
    {
        bool losBlocked = !bot->IsWithinLOSInMap(target) || fabs(bot->GetPositionZ() - target->GetPositionZ()) > 5.0f;

        if (losBlocked)
        {
            PathGenerator path(bot);
            path.CalculatePath(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), false);

            if (path.GetPathType() != PATHFIND_NOPATH)
            {
                bot->CastStop();

                float x, y, z;
                target->GetPosition(x, y, z);
                botAI->TellMasterNoFacing("Repositioning to exit the LoS target!");
                return MoveTo(target->GetMapId(), x + frand(-1, +1), y + frand(-1, +1), z, false, true);
            }
        }
    }

    if (!bot->IsInCombat())
        return moveToCenter(bg);

    return true;
}

bool ArenaTactics::moveToCenter(Battleground* bg)
{
    // Sanity check
    if (!bg)
    {
        return true;
    }
    uint32 Preference = 6;

    switch (bot->getClass())
    {
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            Preference = 3;
            break;
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_ROGUE:
        case CLASS_DEATH_KNIGHT:
            Preference = 6;
            break;
        case CLASS_HUNTER:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            Preference = 9;
            break;
    }

    switch (bg->GetBgTypeID())
    {
        case BATTLEGROUND_BE:
            if (bg->GetTeamStartPosition(bot->GetBgTeamId())->GetPositionY() < 240)
            {
                if (Preference == 3)
                    MoveTo(bg->GetMapId(), 6226.65f + frand(-1, +1), 264.36f + frand(-1, +1), 1.31f, false, true);
                else if (Preference == 6)
                    MoveTo(bg->GetMapId(), 6239.89f + frand(-1, +1), 261.11f + frand(-1, +1), 0.89f, false, true);
                else
                    MoveTo(bg->GetMapId(), 6235.60f + frand(-1, +1), 258.27f + frand(-1, +1), 0.89f, false, true);
            }
            else
            {
                if (Preference == 3)
                    MoveTo(bg->GetMapId(), 6265.72f + frand(-1, +1), 271.92f + frand(-1, +1), 3.65f, false, true);
                else if (Preference == 6)
                    MoveTo(bg->GetMapId(), 6239.89f + frand(-1, +1), 261.11f + frand(-1, +1), 0.89f, false, true);
                else
                    MoveTo(bg->GetMapId(), 6250.50f + frand(-1, +1), 266.66f + frand(-1, +1), 2.63f, false, true);
            }
            break;
        case BATTLEGROUND_RL:
            if (bg->GetTeamStartPosition(bot->GetBgTeamId())->GetPositionY() < 1600)
            {
                if (Preference == 3)
                    MoveTo(bg->GetMapId(), 1262.14f + frand(-1, +1), 1657.63f + frand(-1, +1), 33.76f, false, true);
                else if (Preference == 6)
                    MoveTo(bg->GetMapId(), 1266.85f + frand(-1, +1), 1663.52f + frand(-1, +1), 34.04f, false, true);
                else
                    MoveTo(bg->GetMapId(), 1274.07f + frand(-1, +1), 1656.36f + frand(-1, +1), 34.58f, false, true);
            }
            else
            {
                if (Preference == 3)
                    MoveTo(bg->GetMapId(), 1261.93f + frand(-1, +1), 1669.27f + frand(-1, +1), 34.25f, false, true);
                else if (Preference == 6)
                    MoveTo(bg->GetMapId(), 1266.85f + frand(-1, +1), 1663.52f + frand(-1, +1), 34.04f, false, true);
                else
                    MoveTo(bg->GetMapId(), 1266.37f + frand(-1, +1), 1672.40f + frand(-1, +1), 34.21f, false, true);
            }
            break;
        case BATTLEGROUND_NA:
            if (bg->GetTeamStartPosition(bot->GetBgTeamId())->GetPositionY() < 2870)
            {
                if (Preference == 3)
                    MoveTo(bg->GetMapId(), 4068.85f + frand(-1, +1), 2911.98f + frand(-1, +1), 12.99f, false, true);
                else if (Preference == 6)
                    MoveTo(bg->GetMapId(), 4056.99f + frand(-1, +1), 2919.75f + frand(-1, +1), 13.51f, false, true);
                else
                    MoveTo(bg->GetMapId(), 4056.27f + frand(-1, +1), 2905.33f + frand(-1, +1), 12.90f, false, true);
            }
            else
            {
                if (Preference == 3)
                    MoveTo(bg->GetMapId(), 4043.66f + frand(-1, +1), 2927.93f + frand(-1, +1), 13.17f, false, true);
                else if (Preference == 6)
                    MoveTo(bg->GetMapId(), 4056.99f + frand(-1, +1), 2919.75f + frand(-1, +1), 13.51f, false, true);
                else
                    MoveTo(bg->GetMapId(), 4054.80f + frand(-1, +1), 2934.28f + frand(-1, +1), 13.72f, false, true);
            }
            break;
        case BATTLEGROUND_DS:
            if (!MoveTo(bg->GetMapId(), 1291.58f + frand(-5, +5), 790.87f + frand(-5, +5), 7.8f, false, true))
            {
                // they like to hang around at the tip of the pipes doing nothing, so we just teleport them down
                if (bot->GetDistance(1333.07f, 817.18f, 13.35f) < 4)
                {
                    bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
                    bot->TeleportTo(bg->GetMapId(), 1330.96f, 816.75f, 3.2f, bot->GetOrientation());
                }
                if (bot->GetDistance(1250.13f, 764.79f, 13.34f) < 4)
                {
                    bot->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_TELEPORTED | AURA_INTERRUPT_FLAG_CHANGE_MAP);
                    bot->TeleportTo(bg->GetMapId(), 1252.19f, 765.41f, 3.2f, bot->GetOrientation());
                }
            }
            break;
        case BATTLEGROUND_RV:
            MoveTo(bg->GetMapId(), 764.65f + frand(-2, +2), -283.85f + frand(-2, +2), 28.28f, false, true);
            break;
        default:
            break;
    }

    return true;
}
