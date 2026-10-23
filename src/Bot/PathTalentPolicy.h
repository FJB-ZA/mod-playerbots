/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Which premade spec a bot takes when its class has a sub-class path's tree as a fourth talent page.

#ifndef _PLAYERBOTS_PATH_TALENT_POLICY_H
#define _PLAYERBOTS_PATH_TALENT_POLICY_H

#include <cstdint>

namespace PathTalents
{
// The class's own trees are pages 0 to 2; a path's tree is the fourth page.
constexpr uint32_t PathPage = 3;

// The first premade spec whose links spend points on the path's page, or -1.
constexpr int PathSpec(bool const* usesPathTree, int count)
{
    for (int spec = 0; spec < count; ++spec)
        if (usesPathTree[spec])
            return spec;
    return -1;
}

// A spec on the path's tree can't be rolled while that tree is closed.
constexpr uint32_t RollWeight(uint32_t weight, bool usesPathTree, bool treeOpen)
{
    return usesPathTree && !treeOpen ? 0 : weight;
}

constexpr uint32_t TotalWeight(uint32_t const* weights, bool const* usesPathTree, int count, bool treeOpen)
{
    uint32_t total = 0;
    for (int spec = 0; spec < count; ++spec)
        total += RollWeight(weights[spec], usesPathTree[spec], treeOpen);
    return total;
}

// The spec a roll in [1, TotalWeight] lands on, or -1 when nothing can be rolled.
constexpr int SpecForRoll(uint32_t const* weights, bool const* usesPathTree, int count, bool treeOpen, uint32_t roll)
{
    uint32_t reached = 0;
    for (int spec = 0; spec < count; ++spec)
    {
        uint32_t const weight = RollWeight(weights[spec], usesPathTree[spec], treeOpen);
        reached += weight;
        if (weight && roll <= reached)
            return spec;
    }
    return -1;
}

// An open path tree takes its own spec, else the biggest page unless it's the closed path page; -1 rolls a spec.
constexpr int KeptSpec(bool treeOpen, int pathSpec, bool keepCurrent, uint32_t currentPage)
{
    if (treeOpen && pathSpec >= 0)
        return pathSpec;
    if (keepCurrent && currentPage != PathPage)
        return int(currentPage);
    return -1;
}

// Leftover points go to the other pages of the class's own trees, never to a path's page.
constexpr uint32_t LeftoverPage(uint32_t specPage, uint32_t step) { return (specPage + step) % PathPage; }
}  // namespace PathTalents

#endif
