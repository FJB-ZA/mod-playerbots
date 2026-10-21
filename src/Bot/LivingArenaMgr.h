// Arena seasons for living-world bots: rosters and specs, match scheduling, human caps, season gear, partners.

#ifndef _PLAYERBOTS_LIVING_ARENA_MGR_H
#define _PLAYERBOTS_LIVING_ARENA_MGR_H

#include <ctime>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "LivingArenaPolicy.h"
#include "ObjectGuid.h"

enum TeamId : uint8;

class ArenaTeam;
class Battleground;
class BattlegroundQueue;
class Player;
class PlayerbotAI;
struct GroupQueueInfo;

class LivingArenaMgr
{
public:
    static LivingArenaMgr& instance();

    bool IsEnabled() const;
    uint32 GetArenaLevel() const;
    uint32 GetSeason() const { return _season; }

    void Update(uint32 diff);

    bool MayQueueRated(Player* captain, uint8 arenaType);
    bool AllowMatchingGroup(BattlegroundQueue* queue, GroupQueueInfo* group, uint8 bracketId);
    // Vetoes the rated pairing the core is about to start when it puts a human against a bot team not assigned to it.
    bool AllowRatedUpdate(BattlegroundQueue* queue, uint8 bracketId, uint8 arenaType, uint32 arenaRating);

    uint32 GetSkill(Player* bot) const;
    void SetSkill(Player* bot, uint32 skill);
    // Average skill of the bots on this bot's arena side; 0 when no bot plays there.
    uint32 GetTeamSkill(Player* bot) const;
    bool HasHumanTeammate(Player* bot) const;
    bool ClaimTeamCall(ObjectGuid group, uint32 intervalMs);
    // Runs before each bot AI tick because the engine sleeps through the bot's own casts.
    void CheckJuke(Player* bot, PlayerbotAI* ai);
    void OnBattlegroundEnd(Battleground* bg, TeamId winner);

    static bool IsBot(Player* player);
    static uint32 GetGearScore(Player* player);
    static int FindSpecNo(uint8 cls, std::string const& name);
    static bool IsPvpSpec(uint8 cls, int specNo);
    static bool IsHealerSpec(uint8 cls, int specNo);
    void ApplySpec(Player* bot, int specNo);

    bool IsBotTeam(ArenaTeam const* team) const;
    std::vector<Player*> FindPartnerCandidates(Player* player, uint8 arenaType, uint8 cls, uint32 limit);
    // Bot members of the arena teams this player captains
    std::vector<ObjectGuid> GetPartners(Player* player) const;
    // Same rule as the core's leave and remove handlers: no roster change mid-match or with an invite out.
    bool IsRosterLocked(ArenaTeam* team, ObjectGuid member) const;

private:
    struct CatalogItem
    {
        uint32 item = 0;
        uint32 honor = 0;
        uint32 arena = 0;
        uint32 arenaSlot = 0;
        uint32 rating = 0;
    };

    struct Pending
    {
        uint32 teamA = 0;
        uint32 teamB = 0;
        uint8 type = 0;
        time_t created = 0;
    };

    struct HumanQueue
    {
        uint8 type = 0;
        uint32 mmr = 0;
        uint32 gearScore = 0;
        uint32 previousOpponent = 0;
    };

    struct Responder
    {
        uint32 team = 0;
        time_t assigned = 0;
    };

    struct Juke
    {
        uint32 spellId = 0;
        uint32 seen = 0;
        uint32 window = 0;
    };

    void Load();
    uint32 ResolveSeason() const;
    void StartSeason();
    void LoadCatalog();
    void FormTeams();
    // A random-bot partner who now counts a teammate as a rival quits that team.
    void ReviewPartners();
    bool FormTeam(uint8 type, std::vector<Player*>& pool);
    bool CanJoinBotTeam(Player* bot) const;
    ArenaTeam* CreateTeam(std::vector<std::pair<Player*, int>> const& roster, uint8 type);
    void ScheduleMatches(time_t now);
    void ScheduleBotMatch(uint8 type, uint32 target, time_t now);
    ArenaTeam* ChooseOpponent(uint8 type, uint32 mmr, uint32 maxAbove, uint32 gearCap, uint32 exclude,
                              uint32 previousOpponent, time_t now, bool rested);
    bool IsTeamReady(ArenaTeam* team) const;
    bool IsTeamBusy(ArenaTeam* team, uint8 type) const;
    bool IsTeamInArena(ArenaTeam* team) const;
    bool IsTeamInvited(ArenaTeam* team, uint8 type) const;
    // Already promised to a human or a bot pairing, or recently failed to show up
    bool IsReserved(uint32 teamId, time_t now) const;
    static bool IsHumanGroup(GroupQueueInfo const* group);
    static uint32 GetGroupGearScore(GroupQueueInfo const* group);
    void LoginMembers(ArenaTeam* team) const;
    void LeaveQueue(ArenaTeam* team, uint8 type) const;
    uint32 GetTeamMmr(ArenaTeam* team) const;
    uint32 GetTeamGearScore(ArenaTeam* team);
    uint32 GetTeamDedication(ArenaTeam* team) const;
    void Shop(time_t now);
    std::vector<Player*> GetShoppers() const;
    bool TryPurchase(Player* bot);
    void RememberGearScore(Player* bot);

    bool _loaded = false;
    uint32 _season = 0;
    uint32 _timer = 0;
    time_t _nextFormation = 0;
    time_t _nextShopping = 0;
    std::vector<CatalogItem> _catalog;
    std::unordered_map<uint32, uint32> _gearScores;
    std::unordered_map<uint32, time_t> _nextShop;
    std::vector<Pending> _pending;
    std::unordered_map<uint32, Responder> _responders;
    std::unordered_map<uint32, time_t> _noShow;
    std::unordered_map<uint32, time_t> _lastQueued;

    mutable std::mutex _mutex;
    std::set<std::pair<ObjectGuid::LowType, uint8>> _allowed;
    std::unordered_map<uint64, uint32> _calls;
    std::unordered_map<uint32, Juke> _jukes;
    std::unordered_map<uint32, uint32> _skillOverrides;
};

void AddLivingArenaScripts();

#endif
