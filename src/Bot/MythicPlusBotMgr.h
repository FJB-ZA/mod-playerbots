/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Bot groups run their own Mythic+ keys: forms them around a key they can handle, takes them in and brings them home.

#ifndef _PLAYERBOTS_MYTHIC_PLUS_BOT_MGR_H
#define _PLAYERBOTS_MYTHIC_PLUS_BOT_MGR_H

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ObjectGuid.h"
#include "Position.h"

class Group;
class Map;
class Player;

class MythicPlusBotMgr
{
public:
    static MythicPlusBotMgr& instance();

    // World thread
    void Update(uint32 diff);

    // Any thread
    bool InRun(Player* bot) const;
    bool IsRunGroup(Group const* group) const;

private:
    struct Key
    {
        uint16 map = 0;
        uint8 level = 0;
    };

    struct Run
    {
        ObjectGuid group;
        ObjectGuid holder;
        Key key;
        std::vector<ObjectGuid> members;
        std::unordered_map<ObjectGuid, WorldLocation> homes;
        WorldLocation entrance;
        uint32 startedAt = 0;
        uint32 bossAt = 0;
        uint32 bossMask = 0;
        uint32 calmSince = 0;
        uint32 checkedAt = 0;
        uint64 runsBefore = 0;
        bool started = false;
        uint32 endedAt = 0;
    };

    void TryForm();
    bool Launch(std::vector<Player*> const& members, Player* holder, Key key);
    // Returns true once the run is over and its group gone
    bool Supervise(Run& run);
    void CheckRecord(Run& run, uint32 now);
    void Revive(Run& run, std::vector<Player*> const& online, Map* map);
    void Finish(Run& run, char const* why);

    mutable std::mutex _lock;
    std::unordered_set<ObjectGuid> _members;
    std::unordered_set<ObjectGuid> _groups;
    std::vector<Run> _runs;
    uint32 _formTimer = 0;
    uint32 _tickTimer = 0;
};

#endif
