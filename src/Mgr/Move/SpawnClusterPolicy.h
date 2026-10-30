/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Where a cluster of creature spawns is anchored, so bots walk to and land on a spot that is really there.

#ifndef _PLAYERBOTS_SPAWN_CLUSTER_POLICY_H
#define _PLAYERBOTS_SPAWN_CLUSTER_POLICY_H

#include <cstddef>
#include <vector>

#include "JumpTrail.h"

namespace BotMove
{
// Side of the grid cells spawns are grouped by; a cell corner can sit up to half of this off the ground.
constexpr float SpawnCellSize = 50.0f;

// The spawn nearest the cluster's centre: unlike the cell corner, it stands on ground the creature really uses.
inline Point ClusterAnchor(std::vector<Point> const& spawns)
{
    if (spawns.empty())
        return {};

    Point centre;
    for (Point const& spawn : spawns)
    {
        centre.x += spawn.x;
        centre.y += spawn.y;
        centre.z += spawn.z;
    }
    float const count = static_cast<float>(spawns.size());
    centre = {centre.x / count, centre.y / count, centre.z / count};

    std::size_t best = 0;
    for (std::size_t i = 1; i < spawns.size(); ++i)
        if (Distance(spawns[i], centre) < Distance(spawns[best], centre))
            best = i;
    return spawns[best];
}
}  // namespace BotMove

#endif
