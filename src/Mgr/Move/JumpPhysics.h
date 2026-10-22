// WotLK jump physics: how long a player's jump stays in the air, how far it reaches, and what its packet carries.

#ifndef _PLAYERBOTS_JUMP_PHYSICS_H
#define _PLAYERBOTS_JUMP_PHYSICS_H

#include <cmath>

namespace BotMove
{
// Same value as Movement::gravity in the core.
constexpr float Gravity = 19.29110527f;
// Vertical launch speed of a player's jump in the 3.3.5 client.
constexpr float JumpSpeedZ = 7.95555f;
constexpr float JumpApex = JumpSpeedZ * JumpSpeedZ / (2.0f * Gravity);

// Seconds until a jump is back down at `rise` yards above take-off; negative if it never gets that high.
inline float AirTime(float rise)
{
    float disc = JumpSpeedZ * JumpSpeedZ - 2.0f * Gravity * rise;
    if (disc < 0.0f)
        return -1.0f;
    return (JumpSpeedZ + std::sqrt(disc)) / Gravity;
}

// Horizontal yards a running jump covers when it lands `rise` yards above the take-off.
inline float JumpReach(float runSpeed, float rise)
{
    float time = AirTime(rise);
    return time > 0.0f ? runSpeed * time : 0.0f;
}

// Height above the take-off `seconds` into a jump.
constexpr float JumpHeightAt(float seconds) { return JumpSpeedZ * seconds - 0.5f * Gravity * seconds * seconds; }

// What a player's jump packet carries; other clients play the jump animation and arc from it.
struct ClientJump
{
    float zspeed = 0.0f;
    float xyspeed = 0.0f;
    float seconds = 0.0f;
};

// Forward speed is set so the real jump arc comes down on the landing spot.
inline ClientJump ClientJumpFor(float horizontal, float rise)
{
    ClientJump jump;
    jump.seconds = AirTime(rise);
    if (jump.seconds <= 0.0f)
        return ClientJump{};
    jump.zspeed = JumpSpeedZ;
    jump.xyspeed = horizontal / jump.seconds;
    return jump;
}
}  // namespace BotMove

#endif
