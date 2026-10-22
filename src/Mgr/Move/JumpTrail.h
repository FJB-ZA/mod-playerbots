/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The recent jumps and drops a bot's leader made, so the bot can take the same line.

#ifndef _PLAYERBOTS_JUMP_TRAIL_H
#define _PLAYERBOTS_JUMP_TRAIL_H

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace BotMove
{
struct Point
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline float Distance(Point a, Point b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float Distance2d(Point a, Point b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

struct JumpLink
{
    uint32_t mapId = 0;
    Point takeoff;
    Point landing;
    uint32_t atMs = 0;
    // The jumper died from the fall.
    bool fatal = false;
};

// Small hops on flat ground are not worth remembering.
constexpr float LinkMinHorizontal = 2.0f;
constexpr float LinkMinDrop = 3.0f;
constexpr bool WorthRemembering(float horizontal, float drop)
{
    return horizontal >= LinkMinHorizontal || drop >= LinkMinDrop;
}

// Two jumps this close at both ends are the same jump.
constexpr float SameLinkRadius = 3.0f;
constexpr uint32_t LinkLifetimeMs = 10 * 60 * 1000;

class JumpTrail
{
public:
    static constexpr std::size_t Capacity = 8;

    void Add(JumpLink const& link)
    {
        for (JumpLink& known : _links)
        {
            if (known.atMs && known.mapId == link.mapId && Distance(known.takeoff, link.takeoff) <= SameLinkRadius &&
                Distance(known.landing, link.landing) <= SameLinkRadius)
                known = JumpLink{};
        }
        _links[_next] = link;
        _next = (_next + 1) % Capacity;
    }

    // Newest first, skipping other maps and links older than the lifetime; returns how many were written.
    std::size_t Fresh(uint32_t mapId, uint32_t nowMs, JumpLink* out, std::size_t max) const
    {
        std::size_t count = 0;
        for (std::size_t step = 1; step <= Capacity && count < max; ++step)
        {
            JumpLink const& link = _links[(_next + Capacity - step) % Capacity];
            if (!link.atMs || link.mapId != mapId || nowMs - link.atMs > LinkLifetimeMs)
                continue;
            out[count++] = link;
        }
        return count;
    }

    void Clear() { _links = {}; }

private:
    std::array<JumpLink, Capacity> _links{};
    std::size_t _next = 0;
};
}  // namespace BotMove

#endif
