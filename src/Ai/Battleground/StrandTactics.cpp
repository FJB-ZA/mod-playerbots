// Strand of the Ancients bot tactics: lane plans, demolisher sieges, seaforium charges, graveyards and the relic.

#include "BattleGroundTactics.h"

#include "BattlegroundSA.h"
#include "LivingArenaMgr.h"
#include "LivingBattlegroundPolicy.h"
#include "Playerbots.h"
#include "Vehicle.h"

// no waypoint paths: the beach, ramps and courtyards are open ground for the pathfinder
std::vector<BattleBotPath*> const vPaths_SA = {};
// both owners of the central, right and left graveyard banners
std::vector<uint32> const vFlagsSA = {191305, 191306, 191307, 191308, 191309, 191310};

namespace
{
uint32 constexpr SA_SEAFORIUM_CHARGE = 39213;
float constexpr SA_PLANT_RANGE = 8.0f;
float constexpr SA_PILE_RANGE = 80.0f;

Position const SA_EAST_LANDING = {1600.4f, -106.3f, 8.87f, 0.0f};
Position const SA_WEST_LANDING = {1618.0f, 61.4f, 7.25f, 0.0f};

// lane 0 is the west wall (green, then purple), lane 1 the east wall (blue, then red)
uint32 const SA_OUTER_GATE[2] = {BG_SA_GREEN_GATE, BG_SA_BLUE_GATE};
uint32 const SA_INNER_GATE[2] = {BG_SA_PURPLE_GATE, BG_SA_RED_GATE};
uint32 const SA_SIDE_FLAG[2] = {BG_SA_LEFT_FLAG, BG_SA_RIGHT_FLAG};

// the relic takes the attackers' faction each round
TeamId Attackers(Battleground* bg)
{
    GameObject* relic = bg->GetBGObject(BG_SA_TITAN_RELIC);
    if (!relic)
        return TEAM_NEUTRAL;
    return relic->GetUInt32Value(GAMEOBJECT_FACTION) == BG_SA_Factions[TEAM_ALLIANCE] ? TEAM_ALLIANCE : TEAM_HORDE;
}

bool GateDown(Battleground* bg, uint32 gate)
{
    GameObject* go = bg->GetBGObject(gate);
    return go && go->GetDestructibleState() == GO_DESTRUCTIBLE_DESTROYED;
}

uint32 GateHealth(Battleground* bg, uint32 gate)
{
    GameObject* go = bg->GetBGObject(gate);
    return go ? go->GetGOValue()->Building.Health : 0;
}

// The gate a lane attacks next; BG_SA_TITAN_RELIC once the way to it is open
uint32 NextGate(Battleground* bg, uint8 lane)
{
    bool const outerDown[2] = {GateDown(bg, SA_OUTER_GATE[0]), GateDown(bg, SA_OUTER_GATE[1])};
    if (!outerDown[0] && !outerDown[1])
        return SA_OUTER_GATE[lane];
    if (!outerDown[lane])
        lane ^= 1;
    if (!GateDown(bg, SA_INNER_GATE[0]) && !GateDown(bg, SA_INNER_GATE[1]))
        return SA_INNER_GATE[lane];
    if (!GateDown(bg, BG_SA_YELLOW_GATE))
        return BG_SA_YELLOW_GATE;
    if (!GateDown(bg, BG_SA_ANCIENT_GATE))
        return BG_SA_ANCIENT_GATE;
    return BG_SA_TITAN_RELIC;
}

// A point on a gate's facing: positive distances lie outside toward the attackers, negative inside the wall
Position GatePoint(Battleground* bg, uint32 gate, float dist)
{
    float const* loc = BG_SA_ObjSpawnlocs[gate];
    float const x = loc[0] + std::cos(loc[3]) * dist;
    float const y = loc[1] + std::sin(loc[3]) * dist;
    float const ground = bg->GetBgMap()->GetHeight(x, y, loc[2] + 5.0f);
    return Position(x, y, ground > INVALID_HEIGHT ? ground : loc[2], 0.0f);
}

// Fixed per bot and match, so a bot keeps its plan across the frequent objective checks
uint32 StableRoll(Player* bot, Battleground* bg, uint32 salt)
{
    return LivingWorld::Mix(bot->GetGUID().GetCounter() ^ (bg->GetInstanceID() << 16) ^ (salt * 0x9e3779b9U)) % 100;
}

void UseObject(Player* bot, GameObject* go)
{
    WorldPacket data(CMSG_GAMEOBJ_USE);
    data << go->GetGUID();
    bot->GetSession()->HandleGameObjectUseOpcode(data);
}
}  // namespace

bool BGTactics::saCanCapture(Battleground* bg, GameObject* banner)
{
    TeamId const attackers = Attackers(bg);
    if (bot->GetTeamId() != attackers)
        return false;
    // Alliance banners have the even entries
    TeamId const owner = banner->GetEntry() % 2 == 0 ? TEAM_ALLIANCE : TEAM_HORDE;
    return owner != attackers && !banner->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
}

// Attackers still on a boat once the round runs go ashore at the dock their boat heads for
bool BGTactics::saLeaveBoat()
{
    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS || bot->GetPositionX() < 1650.0f)
        return false;
    Position const& landing = bot->GetPositionY() < 0.0f ? SA_EAST_LANDING : SA_WEST_LANDING;
    return bot->TeleportTo(bot->GetMapId(), landing.GetPositionX(), landing.GetPositionY(), landing.GetPositionZ(),
                           bot->GetOrientation());
}

bool BGTactics::selectObjectiveSA(Battleground* bg, PositionMap& posMap, PositionInfo& pos)
{
    TeamId const attackers = Attackers(bg);
    if (attackers == TEAM_NEUTRAL)
        return false;

    TeamId const team = bot->GetTeamId();
    bool const attacking = team == attackers;
    uint8 const role = context->GetValue<uint32>("bg role")->Get();
    auto const strategy = static_cast<SABotStrategy>(GetBotStrategyForTeam(bg, team));
    auto const tier = static_cast<LivingBattleground::TeamTier>(teamTier());
    bool const tiered = sPlayerbotAIConfig.LivingWorldEnabled;
    bool const steady = StableRoll(bot, bg, 1) <
                        LivingBattleground::TeamPlayChance(tiered, tier, LivingArenaMgr::instance().GetSkill(bot));
    bool const strays = StableRoll(bot, bg, 2) < LivingBattleground::WanderChance(tiered, tier, attacking ? 20 : 15);

    auto setPos = [&](Position const& p, float spread)
    {
        pos.Set(p.GetPositionX() + frand(-spread, spread), p.GetPositionY() + frand(-spread, spread), p.GetPositionZ(),
                bot->GetMapId());
        posMap["bg objective"] = pos;
        return true;
    };

    uint8 lane = 0;
    if (attacking)
    {
        lane = (bg->GetInstanceID() + attackers) % 2;
        if (tier == LivingBattleground::TeamTier::Perfect && !GateDown(bg, SA_OUTER_GATE[0]) &&
            !GateDown(bg, SA_OUTER_GATE[1]))
        {
            uint32 const west = GateHealth(bg, SA_OUTER_GATE[0]);
            uint32 const east = GateHealth(bg, SA_OUTER_GATE[1]);
            if (west != east)
                lane = west < east ? 0 : 1;
        }
        if (strategy == SA_STRATEGY_MAIN_FEINT && role == 0)
            lane ^= 1;
        else if (strategy == SA_STRATEGY_SPLIT)
            lane ^= role % 2;
    }
    else
    {
        lane = role % 2;
        if (steady)
        {
            uint32 const west = GateHealth(bg, NextGate(bg, 0));
            uint32 const east = GateHealth(bg, NextGate(bg, 1));
            if (west != east)
                lane = west < east ? 0 : 1;
        }
    }
    if (strays)
        lane ^= 1;

    uint32 const gate = NextGate(bg, lane);

    if (botAI->IsInVehicle())
    {
        if (!botAI->IsInVehicle(true))
            return false;

        Unit* vehicle = bot->GetVehicleBase();
        if (vehicle->GetEntry() == NPC_ANTI_PERSONNAL_CANNON)
        {
            // guns 2g and 2g+1 stand on gate g
            for (uint32 gun = BG_SA_GUN_1; gun <= BG_SA_GUN_10; ++gun)
                if (Creature* cannon = bg->GetBGCreature(gun);
                    cannon && cannon->GetGUID() == vehicle->GetGUID() && GateDown(bg, gun / 2))
                    bot->ExitVehicle();
            return false;
        }
        if (vehicle->GetEntry() != NPC_DEMOLISHER_SA)
            return false;

        if (gate == BG_SA_TITAN_RELIC)
        {
            bot->ExitVehicle();
            return false;
        }

        PositionInfo siegePos = posMap["bg siege"];
        siegePos.Set(BG_SA_ObjSpawnlocs[gate][0], BG_SA_ObjSpawnlocs[gate][1], BG_SA_ObjSpawnlocs[gate][2],
                     bot->GetMapId());
        posMap["bg siege"] = siegePos;

        // close enough for Ram (15 yd from the gate) while Hurl Boulder lands on it
        Position const stage = GatePoint(bg, gate, 10.0f);
        if (vehicle->GetExactDist2d(stage.GetPositionX(), stage.GetPositionY()) < 4.0f)
            return setPos(vehicle->GetPosition(), 0.0f);
        return setPos(stage, 2.0f);
    }

    if (!attacking)
    {
        if (gate == BG_SA_TITAN_RELIC || GateDown(bg, BG_SA_ANCIENT_GATE))
        {
            float const* relic = BG_SA_ObjSpawnlocs[BG_SA_TITAN_RELIC];
            return setPos(Position(relic[0], relic[1], relic[2], 0.0f), 6.0f);
        }

        if (role < 3 && gate != BG_SA_ANCIENT_GATE)
        {
            for (uint32 gun = gate * 2; gun <= gate * 2 + 1; ++gun)
            {
                Creature* cannon = bg->GetBGCreature(gun);
                if (cannon && cannon->IsAlive() && cannon->GetVehicleKit() &&
                    !cannon->GetVehicleKit()->IsVehicleInUse())
                    return setPos(cannon->GetPosition(), 0.0f);
            }
        }

        // a coordinated defense meets the demolishers before they reach the gate
        if (steady)
        {
            Position const face = GatePoint(bg, gate, 0.0f);
            std::list<Creature*> demolishers;
            bot->GetCreatureListWithEntryInGrid(demolishers, NPC_DEMOLISHER_SA, 150.0f);
            for (Creature* demolisher : demolishers)
                if (demolisher->IsAlive() && !demolisher->IsFriendlyTo(bot) &&
                    demolisher->GetVehicleKit() && demolisher->GetVehicleKit()->IsVehicleInUse() &&
                    demolisher->GetExactDist2d(face.GetPositionX(), face.GetPositionY()) < 50.0f)
                    return setPos(demolisher->GetPosition(), 4.0f);
        }

        float const hold = strategy == SA_STRATEGY_SINGLE_PUSH ? -10.0f
                           : strategy == SA_STRATEGY_MAIN_FEINT ? 4.0f
                                                                : 15.0f;
        return setPos(GatePoint(bg, gate, hold), 5.0f);
    }

    if (gate == BG_SA_TITAN_RELIC)
    {
        GameObject* relic = bg->GetBGObject(BG_SA_TITAN_RELIC);
        if (!relic)
            return false;
        if (relic->IsWithinDistInMap(bot, relic->GetInteractionDistance()))
            UseObject(bot, relic);
        return setPos(relic->GetPosition(), 0.0f);
    }

    if (bot->HasItemCount(SA_SEAFORIUM_CHARGE))
    {
        Position const face = GatePoint(bg, gate, 3.0f);
        if (bot->GetExactDist2d(face.GetPositionX(), face.GetPositionY()) < SA_PLANT_RANGE)
        {
            if (bot->IsMounted())
                bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
            botAI->ImbueItem(bot->GetItemByEntry(SA_SEAFORIUM_CHARGE));
            return setPos(GatePoint(bg, gate, 25.0f), 3.0f);
        }
        return setPos(face, 0.0f);
    }

    if (role < (steady ? 2 : 1))
        if (GameObject* flag = bg->GetBGObject(SA_SIDE_FLAG[lane]); flag && saCanCapture(bg, flag))
            return setPos(flag->GetPosition(), 0.0f);

    if (steady && role == 2)
        if (GameObject* flag = bg->GetBGObject(BG_SA_CENTRAL_FLAG); flag && saCanCapture(bg, flag))
            return setPos(flag->GetPosition(), 0.0f);

    if (role >= 3 && role < (steady ? 6 : 4))
    {
        GameObject* pile = nullptr;
        float nearest = SA_PILE_RANGE;
        for (uint32 i = BG_SA_BOMB; i < BG_SA_MAXOBJ; ++i)
        {
            GameObject* go = bg->GetBGObject(i);
            if (!go || !go->isSpawned())
                continue;
            float const dist = bot->GetDistance(go);
            if (dist < nearest)
            {
                nearest = dist;
                pile = go;
            }
        }
        if (pile)
        {
            if (pile->IsWithinDistInMap(bot, pile->GetInteractionDistance()))
                UseObject(bot, pile);
            return setPos(pile->GetPosition(), 0.0f);
        }
    }

    if (steady)
    {
        Creature* escort = nullptr;
        float nearest = 100.0f;
        std::list<Creature*> demolishers;
        bot->GetCreatureListWithEntryInGrid(demolishers, NPC_DEMOLISHER_SA, nearest);
        for (Creature* demolisher : demolishers)
        {
            if (!demolisher->IsAlive() || !demolisher->IsFriendlyTo(bot) || !demolisher->GetVehicleKit() ||
                !demolisher->GetVehicleKit()->IsVehicleInUse())
                continue;
            float const dist = bot->GetDistance(demolisher);
            if (dist < nearest)
            {
                nearest = dist;
                escort = demolisher;
            }
        }
        if (escort)
            return setPos(escort->GetPosition(), 6.0f);
    }

    return setPos(GatePoint(bg, gate, 12.0f), 6.0f);
}
