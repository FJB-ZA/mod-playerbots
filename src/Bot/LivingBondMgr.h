/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOTS_LIVING_BOND_MGR_H
#define _PLAYERBOTS_LIVING_BOND_MGR_H

#include <array>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Define.h"
#include "LivingBondComments.h"
#include "LivingBondPolicy.h"

class Group;
class Player;

// Remembers, per living random bot, how it feels about each character it has played with or
// against, and per real player, what each living bot has done to them (the player's side, shown in the
// SoloWowBonds addon's Bonds tab). Hooks run on map threads, so every access is locked; saving is asynchronous.
class LivingBondMgr
{
public:
    static LivingBondMgr& instance();
    void Load();
    // Records an event in `bot`'s memory of `other`, unless that kind of event was counted for the
    // pair within its cooldown. Ignored unless `bot` is a living random bot, or a real player and `other` a
    // living random bot (the player's side). True when counted. `ignoreCooldown` is for the GM test command.
    bool Record(Player* bot, Player* other, LivingWorld::BondEvent event, bool ignoreCooldown = false);
    // A living bot says what just happened between it and a real player (LivingBondComments.h): in party (or raid)
    // chat when the player is in `via` (a group the bot was just in) or the bot's group, else aloud when the player
    // is near enough to hear. Quiet towards a player who avoids it, and at most once a minute per player unless
    // `force` (the GM test command, which may also pick the friend line or not). True when said.
    bool Comment(Player* bot, Player* player, LivingWorld::Deed deed, Group* via = nullptr, bool force = false,
                 std::optional<bool> friendLine = {});
    // Both sides of the bond between two characters, gone (the GM test command's clean-up).
    void Forget(uint32 a, uint32 b);
    // Seconds since `bot` last counted `event` for `other` (0 and false when never).
    bool SecondsSince(uint32 bot, uint32 other, LivingWorld::BondEvent event, uint32& seconds);
    LivingWorld::Bond Get(uint32 bot, uint32 other);
    // The bot's strongest bonds, best first then worst, at most `limit` of each.
    std::vector<std::pair<uint32, LivingWorld::Bond>> Strongest(uint32 bot, size_t limit);
    LivingWorld::Attitude AttitudeOf(uint32 bot, uint32 other);
    // Bots that have a bond with `other`, best first.
    std::vector<std::pair<uint32, LivingWorld::Bond>> KnownBy(uint32 other);
    // Everyone `owner` has a bond with (for a player, the bots), in no order.
    std::vector<std::pair<uint32, LivingWorld::Bond>> BondsOf(uint32 owner);
    // True (and starts the cooldown) when `bot` may greet `other` again in this channel.
    bool TryGreet(uint32 bot, uint32 other, bool whisper);
    // True (and starts the weekly cooldown) when `bot` may offer `other` its guild again.
    bool TryGuildInvite(uint32 bot, uint32 other);
    // True (and starts the cooldown) when `bot` may comment on something between it and `other` again.
    bool TryComment(uint32 bot, uint32 other);
    // World PvP kills by living bots, so a bot does not camp the corpse: it leaves alone a character it
    // killed within the gank window unless that character attacks it again.
    void RecordKill(Player* killer, Player* victim);
    bool KilledRecently(uint32 bot, uint32 other);

    // Group membership times, for time spent adventuring together.
    void OnGroupJoin(uint32 group, uint32 member);
    // Credits time grouped between `member` and everyone still in the group, and forgets `member`.
    void OnGroupLeave(uint32 group, Player* member);
    void OnGroupDisband(uint32 group);

    // Bots a player chose to avoid (the SoloWowBonds addon). Kept apart from the bonds: avoiding a bot is the player's
    // private choice and does not change how the bot feels. An avoided bot stops whispering, greeting and inviting the
    // player, never picks them for role-play, and the Dungeon Finder keeps it out of their groups.
    bool Avoids(uint32 player, uint32 bot);
    void SetAvoided(uint32 player, uint32 bot, bool avoided);
    std::vector<uint32> AvoidedBy(uint32 player);

private:
    static uint64 Key(uint32 bot, uint32 other) { return (uint64(bot) << 32) | other; }
    void CreditGroupTime(Player* a, Player* b, uint32 seconds);

    std::mutex _mutex;
    std::unordered_map<uint64, LivingWorld::Bond> _bonds;
    std::unordered_map<uint64, std::array<uint32, size_t(LivingWorld::BondEvent::Count)>> _lastEvent;
    std::unordered_map<uint32, std::unordered_map<uint32, uint32>> _groupJoins;  // group -> member -> joined
    std::unordered_map<uint64, std::array<uint32, 4>> _lastGreet;  // pair -> [in person, whisper, guild invite, comment]
    std::unordered_map<uint64, uint32> _lastKill;                               // bot, victim -> time of the kill
    std::unordered_set<uint64> _avoids;                                         // player, bot
};

#endif
