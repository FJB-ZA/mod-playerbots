/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_LIVING_TRANSMOG_ACTIONS_H
#define PLAYERBOTS_LIVING_TRANSMOG_ACTIONS_H

#include "ObjectGuid.h"
#include "Value.h"

class PlayerbotAI;

// A bot in a capital that wants a new look heads for the city's Warpweaver while visiting NPCs.
struct LivingTransmogTrip
{
    bool go = false;
    ObjectGuid warpweaver;
};

class LivingTransmogTripValue : public CalculatedValue<LivingTransmogTrip>
{
public:
    LivingTransmogTripValue(PlayerbotAI* botAI) : CalculatedValue(botAI, "living transmog trip", 60000) {}
    LivingTransmogTrip Calculate() override;
};

namespace LivingTransmog
{
// Talks to the Warpweaver on the world thread, like a player clicking through its menu, and ends the trip.
bool QueueVisit(PlayerbotAI* botAI, ObjectGuid warpweaver);
// Drops the wish until the next cooldown, also when the Warpweaver could not be reached.
void EndTrip(PlayerbotAI* botAI);
}  // namespace LivingTransmog

#endif
