// Stuck detection for a moving bot, and when a stuck follower may catch up to its leader.

#ifndef _PLAYERBOTS_PROGRESS_POLICY_H
#define _PLAYERBOTS_PROGRESS_POLICY_H

#include <cmath>
#include <cstdint>

namespace BotMove
{
// Moving this far from the last anchor counts as progress.
constexpr float MovedEnough = 2.0f;
// A bot that stopped asking to move for this long was idle, not stuck, so the clock restarts.
constexpr uint32_t IdleResetMs = 3000;

struct Progress
{
    bool set = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint32_t sinceMs = 0;
    uint32_t seenMs = 0;
};

inline void TrackProgress(Progress& progress, float x, float y, float z, uint32_t nowMs)
{
    float dx = x - progress.x;
    float dy = y - progress.y;
    float dz = z - progress.z;
    bool idle = nowMs - progress.seenMs > IdleResetMs;
    progress.seenMs = nowMs;
    if (progress.set && !idle && std::sqrt(dx * dx + dy * dy + dz * dz) < MovedEnough)
        return;
    progress.set = true;
    progress.x = x;
    progress.y = y;
    progress.z = z;
    progress.sinceMs = nowMs;
}

// Unsigned subtraction survives the clock wrapping; a bot idle since its last move is not stuck.
inline uint32_t StuckFor(Progress const& progress, uint32_t nowMs)
{
    if (!progress.set || nowMs - progress.seenMs > IdleResetMs)
        return 0;
    return nowMs - progress.sinceMs;
}

struct CatchUpMoment
{
    bool enabled = false;
    bool inInstance = false;
    bool sameMap = false;
    bool leaderAlive = false;
    bool onVehicle = false;
    uint32_t stuckMs = 0;
    float distance = 0.0f;
};

struct CatchUpLimits
{
    uint32_t stuckMs = 8000;
    float minDistance = 30.0f;
};

// The last resort when no walk or jump reaches the leader: only in instances, never from a vehicle.
constexpr bool ShouldCatchUp(CatchUpMoment moment, CatchUpLimits limits = {})
{
    return moment.enabled && moment.inInstance && moment.sameMap && moment.leaderAlive && !moment.onVehicle &&
           moment.stuckMs >= limits.stuckMs && moment.distance >= limits.minDistance;
}
}  // namespace BotMove

#endif
