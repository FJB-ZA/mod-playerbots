// Records the jumps and drops real players make so the bots in their group can take the same line, or refuse it.

#include "MovementPlanner.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"

namespace
{
struct GroundedSpot : public DataMap::Base
{
    bool set = false;
    uint32 mapId = 0;
    BotMove::Point point;
};

constexpr char const* GroundedSpotKey = "playerbots.grounded";
}  // namespace

class PlayerbotsMovementScript : public MovementHandlerScript
{
public:
    PlayerbotsMovementScript() : MovementHandlerScript("PlayerbotsMovementScript", {MOVEMENTHOOK_ON_PLAYER_MOVE}) {}

    void OnPlayerMove(Player* player, MovementInfo movementInfo, uint32 opcode) override
    {
        if (!player->GetGroup() || !MovementPlanner::IsActiveFor(player))
            return;

        GroundedSpot* spot = player->CustomData.GetDefault<GroundedSpot>(GroundedSpotKey);
        BotMove::Point here{movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY(),
                            movementInfo.pos.GetPositionZ()};

        // A knockback flight is no line to follow, and a spot from another map means a teleport
        if (opcode == CMSG_MOVE_KNOCK_BACK_ACK || spot->mapId != player->GetMapId())
            spot->set = false;

        if (opcode == MSG_MOVE_FALL_LAND)
        {
            // The core has already dealt the fall damage, so a fatal jump shows as a dead jumper
            if (spot->set)
                MovementPlanner::ShareJump(player, spot->point, here, player->isDead());
        }
        // A jump packet carries the exact take-off; walking off a ledge leaves the last grounded packet as take-off
        else if (opcode != MSG_MOVE_JUMP && movementInfo.HasMovementFlag(MOVEMENTFLAG_FALLING))
            return;

        spot->set = true;
        spot->mapId = player->GetMapId();
        spot->point = here;
    }
};

void AddPlayerbotsMovementScripts() { new PlayerbotsMovementScript(); }
