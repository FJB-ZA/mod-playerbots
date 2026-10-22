/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Shared state and lookups for bot-led dungeon runs: who leads, at which skill tier, and who is sitting to rest.

#ifndef _PLAYERBOTS_DUNGEON_LEAD_H
#define _PLAYERBOTS_DUNGEON_LEAD_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "LeadChatPolicy.h"
#include "LeadTierPolicy.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "PullReadyPolicy.h"
#include "Value.h"

class Group;
class Player;
class PlayerbotAI;

namespace DungeonLead
{
struct State
{
    uint32 instanceId = 0;
    bool announced = false;
    bool done = false;  // no boss left: the group goes back to following its master
    bool idle = false;  // nothing to pull and no way on: same, until a pack turns up
    bool hasGoal = false;
    Position goal;
    uint32 goalCheckedAt = 0;
    uint32 goalBlockedAt = 0;
    ObjectGuid pack;  // the pack's mob nearest the tank when it was picked
    uint32 packCheckedAt = 0;
    bool hasSpot = false;
    Position spot;  // where the group gathers
    bool hasPullSpot = false;
    Position pullSpot;      // where the tank pulls from, in sight of the pack
    bool stepping = false;  // the tank has left the group to pull
    bool pulled = false;
    uint32 pulledAt = 0;
    bool fought = false;  // the pull turned into a fight
    uint32 failedPulls = 0;
    std::unordered_set<uint64> skipped;  // packs the tank cannot pull
    uint32 pulls = 0;
    uint32 restFrom = 0;
    uint32 restMs = 0;
    Wait wait = Wait::None;
    uint32 readyCheckedAt = 0;
    ObjectGuid waitingOn;
    uint32 waitSince = 0;
    bool waitAnnounced = false;
    std::unordered_map<uint64, uint32> waitStart;  // member's raw guid -> when they started holding the pull
    ObjectGuid overpull;
    uint32 overpullAt = 0;
    uint32 marksCheckedAt = 0;
    std::unordered_map<uint64, uint32> oomCalls;  // player's raw guid -> when they called oom
    std::unordered_map<uint64, bool> oomAcked;
    CallState calls;
};

bool Enabled();
State& StateOf(PlayerbotAI* botAI);
// The bot tank leading this run, or nullptr when the feature is off, a player tanks, or the tank has nowhere to go.
Player* Leader(PlayerbotAI* botAI);
Tier TierOf(Player* bot);
bool IsResting(Player* player);
void OnPartyChat(Player* sender, uint32 type, std::string const& msg, Group* group);
}  // namespace DungeonLead

class DungeonLeaderValue : public CalculatedValue<ObjectGuid>
{
public:
    DungeonLeaderValue(PlayerbotAI* botAI) : CalculatedValue<ObjectGuid>(botAI, "dungeon leader", 2) {}

    ObjectGuid Calculate() override;
};

class DungeonLeadStateValue : public ManualSetValue<DungeonLead::State&>
{
public:
    DungeonLeadStateValue(PlayerbotAI* botAI) : ManualSetValue<DungeonLead::State&>(botAI, data, "dungeon lead state")
    {
    }

private:
    DungeonLead::State data;
};

#endif
