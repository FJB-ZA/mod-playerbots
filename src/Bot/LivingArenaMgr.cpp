// World-thread side of living arena: season reset, team formation, match scheduling, queue caps and gear shopping.

#include "LivingArenaMgr.h"

#include <algorithm>
#include <array>
#include <limits>

#include "ArenaSeasonMgr.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "Item.h"
#include "LivingBondMgr.h"
#include "LivingWorldMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "PlayerbotsDatabase.h"
#include "Random.h"
#include "RandomPlayerbotFactory.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "StatsWeightCalculator.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"

namespace
{
uint32 constexpr PVP_TRINKET_SPELL = 42292;
// A scheduled bot team that hasn't started its match by then is dropped, and benched for a while if a human waited.
time_t constexpr SHOW_UP_SECONDS = 240;
time_t constexpr NO_SHOW_BENCH_SECONDS = 600;

struct CompSlot
{
    uint8 cls;
    char const* spec;
};

struct Comp
{
    uint8 type;
    std::vector<CompSlot> slots;
};

// Common Wrath arena compositions; the first slot captains the team.
std::vector<Comp> const& Comps()
{
    static std::vector<Comp> const comps = {
        {2, {{CLASS_ROGUE, "subtlety pvp"}, {CLASS_PRIEST, "disc pvp"}}},
        {2, {{CLASS_WARRIOR, "arms pvp"}, {CLASS_PALADIN, "holy pvp"}}},
        {2, {{CLASS_DEATH_KNIGHT, "unholy pvp"}, {CLASS_PALADIN, "holy pvp"}}},
        {2, {{CLASS_ROGUE, "subtlety pvp"}, {CLASS_DRUID, "resto pvp"}}},
        {2, {{CLASS_WARLOCK, "affli pvp"}, {CLASS_DRUID, "resto pvp"}}},
        {2, {{CLASS_MAGE, "frost pvp"}, {CLASS_PRIEST, "disc pvp"}}},
        {2, {{CLASS_WARRIOR, "arms pvp"}, {CLASS_DRUID, "resto pvp"}}},
        {2, {{CLASS_HUNTER, "bm pvp"}, {CLASS_SHAMAN, "resto pvp"}}},
        {2, {{CLASS_DEATH_KNIGHT, "unholy pvp"}, {CLASS_PRIEST, "disc pvp"}}},
        {2, {{CLASS_ROGUE, "subtlety pvp"}, {CLASS_MAGE, "frost pvp"}}},
        {2, {{CLASS_WARLOCK, "affli pvp"}, {CLASS_PRIEST, "disc pvp"}}},
        {2, {{CLASS_SHAMAN, "enh pvp"}, {CLASS_PALADIN, "holy pvp"}}},
        {3, {{CLASS_ROGUE, "subtlety pvp"}, {CLASS_MAGE, "frost pvp"}, {CLASS_PRIEST, "disc pvp"}}},
        {3, {{CLASS_WARRIOR, "arms pvp"}, {CLASS_WARLOCK, "affli pvp"}, {CLASS_DRUID, "resto pvp"}}},
        {3, {{CLASS_WARRIOR, "arms pvp"}, {CLASS_DEATH_KNIGHT, "unholy pvp"}, {CLASS_PALADIN, "holy pvp"}}},
        {3, {{CLASS_ROGUE, "subtlety pvp"}, {CLASS_WARLOCK, "affli pvp"}, {CLASS_SHAMAN, "resto pvp"}}},
        {3, {{CLASS_WARLOCK, "affli pvp"}, {CLASS_PRIEST, "shadow pvp"}, {CLASS_PALADIN, "holy pvp"}}},
        {3, {{CLASS_HUNTER, "bm pvp"}, {CLASS_DRUID, "cat pvp"}, {CLASS_PALADIN, "holy pvp"}}},
        {3, {{CLASS_SHAMAN, "ele pvp"}, {CLASS_MAGE, "frost pvp"}, {CLASS_PALADIN, "holy pvp"}}},
        {3, {{CLASS_ROGUE, "subtlety pvp"}, {CLASS_WARLOCK, "affli pvp"}, {CLASS_DRUID, "resto pvp"}}},
        {3, {{CLASS_WARRIOR, "arms pvp"}, {CLASS_MAGE, "frost pvp"}, {CLASS_SHAMAN, "resto pvp"}}},
        {5, {{CLASS_WARRIOR, "arms pvp"}, {CLASS_DEATH_KNIGHT, "unholy pvp"}, {CLASS_WARLOCK, "affli pvp"},
             {CLASS_PALADIN, "holy pvp"}, {CLASS_PRIEST, "disc pvp"}}},
        {5, {{CLASS_ROGUE, "subtlety pvp"}, {CLASS_MAGE, "frost pvp"}, {CLASS_HUNTER, "mm pvp"},
             {CLASS_PRIEST, "disc pvp"}, {CLASS_DRUID, "resto pvp"}}},
    };
    return comps;
}

char const* DefaultHealerSpec(uint8 cls)
{
    switch (cls)
    {
        case CLASS_PRIEST:
            return "disc pvp";
        case CLASS_PALADIN:
            return "holy pvp";
        case CLASS_SHAMAN:
            return "resto pvp";
        case CLASS_DRUID:
            return "resto pvp";
        default:
            return nullptr;
    }
}

char const* DefaultDpsSpec(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
            return "arms pvp";
        case CLASS_PALADIN:
            return "ret pvp";
        case CLASS_HUNTER:
            return "bm pvp";
        case CLASS_ROGUE:
            return "subtlety pvp";
        case CLASS_PRIEST:
            return "shadow pvp";
        case CLASS_DEATH_KNIGHT:
            return "unholy pvp";
        case CLASS_SHAMAN:
            return "enh pvp";
        case CLASS_MAGE:
            return "frost pvp";
        case CLASS_WARLOCK:
            return "affli pvp";
        case CLASS_DRUID:
            return "cat pvp";
        default:
            return nullptr;
    }
}

int32 constexpr NO_CHEMISTRY = std::numeric_limits<int32>::min();

// How a bot gets on with a roster in both directions: friends add up, any rival or wary pair rules it out.
int32 Chemistry(Player* bot, std::vector<std::pair<Player*, int>> const& roster)
{
    int32 score = 0;
    for (auto const& [member, specNo] : roster)
    {
        for (auto const& [from, to] : {std::pair{bot, member}, std::pair{member, bot}})
        {
            LivingWorld::Attitude const attitude =
                LivingBondMgr::instance().AttitudeOf(from->GetGUID().GetCounter(), to->GetGUID().GetCounter());
            if (attitude == LivingWorld::Attitude::Rival || attitude == LivingWorld::Attitude::Wary)
                return NO_CHEMISTRY;
            score += attitude == LivingWorld::Attitude::Friend ? 3 : attitude == LivingWorld::Attitude::Friendly ? 1 : 0;
        }
    }
    return score;
}

bool IsFree(Player* player)
{
    if (!player || !player->IsInWorld() || !player->IsAlive() || player->IsInCombat() ||
        player->InBattleground() || player->InBattlegroundQueue() || player->IsBeingTeleported())
        return false;
    if (player->GetMap() && player->GetMap()->Instanceable())
        return false;
    PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
    return ai && !ai->HasGameClientMaster();
}

uint32 SlotRating(Player* bot, uint8 slot)
{
    uint32 const teamId = bot->GetArenaTeamId(slot);
    if (!teamId)
        return 0;
    ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(teamId);
    if (!team)
        return 0;
    ArenaTeamMember* member = team->GetMember(bot->GetGUID());
    return member ? member->PersonalRating : 0;
}

uint32 BestPersonalRating(Player* bot)
{
    uint32 best = 0;
    for (uint8 slot = 0; slot < MAX_ARENA_SLOT; ++slot)
        best = std::max(best, SlotRating(bot, slot));
    return best;
}

bool OnAnyArenaTeam(Player* bot)
{
    for (uint8 slot = 0; slot < MAX_ARENA_SLOT; ++slot)
    {
        if (bot->GetArenaTeamId(slot))
            return true;
    }
    return false;
}

// Equipment slots an item may replace without breaking the current weapon setup.
std::vector<uint8> CandidateSlots(Player* bot, ItemTemplate const* proto)
{
    Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* offHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    bool const mainIsTwoHand = mainHand && mainHand->GetTemplate()->InventoryType == INVTYPE_2HWEAPON;
    switch (proto->InventoryType)
    {
        case INVTYPE_FINGER:
            return {EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2};
        case INVTYPE_TRINKET:
            return {EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2};
        case INVTYPE_2HWEAPON:
            if (mainHand && !mainIsTwoHand)
                return {};
            return {EQUIPMENT_SLOT_MAINHAND};
        case INVTYPE_WEAPON:
        {
            if (mainIsTwoHand)
                return {};
            std::vector<uint8> slots = {EQUIPMENT_SLOT_MAINHAND};
            if (bot->CanDualWield() && (!offHand || offHand->GetTemplate()->Class == ITEM_CLASS_WEAPON))
                slots.push_back(EQUIPMENT_SLOT_OFFHAND);
            return slots;
        }
        case INVTYPE_WEAPONMAINHAND:
            if (mainIsTwoHand)
                return {};
            return {EQUIPMENT_SLOT_MAINHAND};
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_SHIELD:
        case INVTYPE_HOLDABLE:
            if (mainIsTwoHand || (offHand && offHand->GetTemplate()->InventoryType != proto->InventoryType))
                return {};
            return {EQUIPMENT_SLOT_OFFHAND};
        default:
        {
            uint8 const slot = bot->FindEquipSlot(proto, NULL_SLOT, true);
            if (slot == NULL_SLOT)
                return {};
            return {slot};
        }
    }
}
}  // namespace

LivingArenaMgr& LivingArenaMgr::instance()
{
    static LivingArenaMgr instance;
    return instance;
}

bool LivingArenaMgr::IsEnabled() const
{
    return sPlayerbotAIConfig.LivingArenaEnabled && sPlayerbotAIConfig.LivingWorldEnabled;
}

uint32 LivingArenaMgr::GetArenaLevel() const
{
    uint32 const level = sPlayerbotAIConfig.RandomBotMaxLevel;
    return level >= 80 ? 80 : level >= 70 ? 70 : 0;
}

uint32 LivingArenaMgr::ResolveSeason() const
{
    if (sPlayerbotAIConfig.LivingArenaSeason)
        return sPlayerbotAIConfig.LivingArenaSeason;
    switch (GetArenaLevel())
    {
        case 80:
            return sConfigMgr->GetOption<uint32>("IndividualProgression.WotLK.ArenaSeason", 5, false);
        case 70:
            return sConfigMgr->GetOption<uint32>("IndividualProgression.TBC.ArenaSeason", 1, false);
        default:
            return 0;
    }
}

void LivingArenaMgr::Load()
{
    _loaded = true;
    _season = ResolveSeason();
    if (!_season)
    {
        LOG_INFO("playerbots", "Living arena: no arena season in this era");
        return;
    }

    LoadCatalog();

    if (QueryResult result = PlayerbotsDatabase.Query("SELECT guid, gear_score FROM playerbots_living_arena_bots"))
    {
        do
        {
            Field* fields = result->Fetch();
            _gearScores[fields[0].Get<uint32>()] = fields[1].Get<uint32>();
        } while (result->NextRow());
    }

    uint32 stored = 0;
    if (QueryResult result = PlayerbotsDatabase.Query("SELECT season FROM playerbots_living_arena_season WHERE id = 1"))
        stored = result->Fetch()[0].Get<uint32>();
    if (stored != _season)
        StartSeason();

    LOG_INFO("playerbots", "Living arena: season {} at level {}, {} vendor items, {} known gear scores", _season,
             GetArenaLevel(), _catalog.size(), _gearScores.size());
}

void LivingArenaMgr::StartSeason()
{
    bool const legacy = sArenaSeasonMgr->GetCurrentSeason() < 6;
    uint32 const rating = legacy ? sWorld->getIntConfig(CONFIG_LEGACY_ARENA_START_RATING)
                                 : sWorld->getIntConfig(CONFIG_ARENA_START_RATING);
    uint32 personal = sWorld->getIntConfig(CONFIG_ARENA_START_PERSONAL_RATING);
    if (!personal && legacy)
        personal = 1500;
    uint32 const mmr = sWorld->getIntConfig(CONFIG_ARENA_START_MATCHMAKER_RATING);

    uint32 reset = 0;
    for (auto const& [id, team] : sArenaTeamMgr->GetArenaTeams())
    {
        if (!IsBotTeam(team))
            continue;
        ArenaTeamStats stats = team->GetStats();
        stats.Rating = rating;
        stats.WeekGames = stats.WeekWins = stats.SeasonGames = stats.SeasonWins = 0;
        team->SetArenaTeamStats(stats);
        for (ArenaTeamMember& member : team->GetMembers())
        {
            member.PersonalRating = personal;
            member.MatchMakerRating = mmr;
            member.MaxMMR = mmr;
            member.WeekGames = member.WeekWins = member.SeasonGames = member.SeasonWins = 0;
        }
        team->SaveToDB(true);
        ++reset;
    }

    PlayerbotsDatabase.Execute("REPLACE INTO playerbots_living_arena_season (id, season, started) VALUES (1, {}, {})",
                               _season, uint32(time(nullptr)));
    LOG_INFO("playerbots", "Living arena: season {} started, {} bot teams back to {} rating / {} MMR", _season, reset,
             rating, mmr);
}

void LivingArenaMgr::LoadCatalog()
{
    _catalog.clear();
    bool const wrath = GetArenaLevel() >= 80;
    std::string const season = wrath ? "npc_ipp_wotlk_S" + std::to_string(_season)
                                     : "npc_ipp_tbc_S" + std::to_string(_season);
    QueryResult result = WorldDatabase.Query(
        "SELECT DISTINCT v.item, v.ExtendedCost, c.ScriptName FROM creature c JOIN npc_vendor v ON v.entry = c.id "
        "WHERE c.ScriptName IN ('{}', 'npc_ipp_tbc_pvp', 'npc_ipp_tbc') AND v.ExtendedCost > 0",
        season);
    if (!result)
        return;

    std::unordered_map<uint32, CatalogItem> cheapest;
    do
    {
        Field* fields = result->Fetch();
        uint32 const itemId = fields[0].Get<uint32>();
        uint32 const costId = fields[1].Get<uint32>();
        std::string const script = fields[2].Get<std::string>();
        // The TBC-wide vendors only supply PvP trinkets, and only while TBC is the era.
        if (script != season && wrath)
            continue;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        ItemExtendedCostEntry const* cost = sItemExtendedCostStore.LookupEntry(costId);
        if (!proto || !cost || proto->InventoryType == INVTYPE_NON_EQUIP || proto->InventoryType == INVTYPE_BAG ||
            proto->InventoryType == INVTYPE_AMMO || proto->InventoryType == INVTYPE_QUIVER ||
            proto->InventoryType == INVTYPE_TABARD || proto->InventoryType == INVTYPE_BODY)
            continue;
        if (script == "npc_ipp_tbc" && proto->Spells[0].SpellId != PVP_TRINKET_SPELL)
            continue;
        if (std::any_of(std::begin(cost->reqitem), std::end(cost->reqitem), [](uint32 item) { return item != 0; }))
            continue;

        CatalogItem entry{itemId, cost->reqhonorpoints, cost->reqarenapoints, cost->reqarenaslot,
                          cost->reqpersonalarenarating};
        auto found = cheapest.find(itemId);
        if (found == cheapest.end() || entry.honor + entry.arena * 10 < found->second.honor + found->second.arena * 10)
            cheapest[itemId] = entry;
    } while (result->NextRow());

    for (auto const& [itemId, entry] : cheapest)
        _catalog.push_back(entry);
}

void LivingArenaMgr::Update(uint32 diff)
{
    if (!IsEnabled())
        return;
    _timer += diff;
    if (_timer < 5000)
        return;
    _timer = 0;

    if (!_loaded)
        Load();
    if (!_season)
        return;

    time_t const now = time(nullptr);
    if (now >= _nextFormation)
    {
        _nextFormation = now + 300;
        ReviewPartners();
        FormTeams();
    }
    ScheduleMatches(now);
    if (now >= _nextShopping)
    {
        _nextShopping = now + 60;
        Shop(now);
    }
}

bool LivingArenaMgr::IsBot(Player* player) { return player && GET_PLAYERBOT_AI(player) && !IsSelfBot(player); }

uint32 LivingArenaMgr::GetGearScore(Player* player) { return PlayerbotAI::GetMixedGearScore(player, false, false); }

bool LivingArenaMgr::IsBotTeam(ArenaTeam const* team) const
{
    if (!team)
        return false;
    uint32 const accountId = sCharacterCache->GetCharacterAccountIdByGuid(team->GetCaptain());
    return accountId && sPlayerbotAIConfig.IsInRandomAccountList(accountId) &&
           !sRandomPlayerbotMgr.IsAddClassAccount(accountId);
}

int LivingArenaMgr::FindSpecNo(uint8 cls, std::string const& name)
{
    for (int specNo = 0; specNo < MAX_SPECNO; ++specNo)
    {
        if (sPlayerbotAIConfig.PremadeSpecName[cls][specNo] == name)
            return specNo;
    }
    return -1;
}

bool LivingArenaMgr::IsPvpSpec(uint8 cls, int specNo)
{
    if (specNo < 0 || specNo >= MAX_SPECNO)
        return false;
    std::string const& name = sPlayerbotAIConfig.PremadeSpecName[cls][specNo];
    return name.find("pvp") != std::string::npos && name.find("prot") == std::string::npos;
}

bool LivingArenaMgr::IsHealerSpec(uint8 cls, int specNo)
{
    char const* healer = DefaultHealerSpec(cls);
    if (!IsPvpSpec(cls, specNo) || !healer)
        return false;
    std::string const& name = sPlayerbotAIConfig.PremadeSpecName[cls][specNo];
    return name == healer || name == "holy pvp";
}

void LivingArenaMgr::ApplySpec(Player* bot, int specNo)
{
    if (!bot || specNo < 0)
        return;
    PlayerbotFactory::InitTalentsBySpecNo(bot, specNo, true);
    PlayerbotFactory factory(bot, bot->GetLevel());
    factory.InitGlyphs(false);
    if (PlayerbotAI* ai = GET_PLAYERBOT_AI(bot))
        ai->ResetStrategies();
}

bool LivingArenaMgr::CanJoinBotTeam(Player* bot) const
{
    if (!bot || !IsFree(bot) || bot->GetLevel() != GetArenaLevel() || OnAnyArenaTeam(bot) ||
        !LivingWorldMgr::instance().IsEnabled(bot))
        return false;
    uint32 const accountId = bot->GetSession()->GetAccountId();
    if (sRandomPlayerbotMgr.IsAddClassAccount(accountId) || bot->GetGroup())
        return false;
    return LivingArena::Eligible(LivingWorldMgr::instance().GetProfile(bot).motivation, bot->GetGUID().GetCounter());
}

void LivingArenaMgr::FormTeams()
{
    std::vector<Player*> pool;
    for (auto const& [guid, bot] : sRandomPlayerbotMgr.GetAllBots())
    {
        if (CanJoinBotTeam(bot))
            pool.push_back(bot);
    }

    std::array<uint32, 6> count{};
    for (auto const& [id, team] : sArenaTeamMgr->GetArenaTeams())
    {
        if (IsBotTeam(team) && team->GetType() < count.size())
            ++count[team->GetType()];
    }

    std::array<std::pair<uint8, uint32>, 3> const targets = {{{2, sPlayerbotAIConfig.RandomBotArenaTeam2v2Count},
                                                             {3, sPlayerbotAIConfig.RandomBotArenaTeam3v3Count},
                                                             {5, sPlayerbotAIConfig.RandomBotArenaTeam5v5Count}}};
    for (auto const& [type, target] : targets)
    {
        while (count[type] < target && pool.size() >= type && FormTeam(type, pool))
            ++count[type];
    }
}

bool LivingArenaMgr::FormTeam(uint8 type, std::vector<Player*>& pool)
{
    std::vector<Comp const*> comps;
    for (Comp const& comp : Comps())
    {
        if (comp.type == type)
            comps.push_back(&comp);
    }
    std::shuffle(comps.begin(), comps.end(), RandomEngine::Instance());

    std::array<TeamId, 2> factions = {TEAM_ALLIANCE, TEAM_HORDE};
    if (urand(0, 1))
        std::swap(factions[0], factions[1]);

    // Friends of the roster first, then bots already playing the role; anyone with a grudge stays out.
    auto pick = [&](TeamId faction, uint8 cls, bool healer, std::vector<std::pair<Player*, int>>& roster) -> Player*
    {
        Player* best = nullptr;
        int32 bestScore = std::numeric_limits<int32>::min();
        std::vector<Player*> shuffled = pool;
        std::shuffle(shuffled.begin(), shuffled.end(), RandomEngine::Instance());
        for (Player* bot : shuffled)
        {
            if (bot->GetTeamId() != faction || bot->getClass() != cls)
                continue;
            if (std::any_of(roster.begin(), roster.end(), [bot](auto const& slot) { return slot.first == bot; }))
                continue;
            int32 const chemistry = Chemistry(bot, roster);
            if (chemistry == NO_CHEMISTRY)
                continue;
            int32 const score = chemistry * 2 + (PlayerbotAI::IsHeal(bot, true) == healer ? 1 : 0);
            if (score > bestScore)
            {
                bestScore = score;
                best = bot;
            }
        }
        return best;
    };

    for (TeamId faction : factions)
    {
        for (Comp const* comp : comps)
        {
            std::vector<std::pair<Player*, int>> roster;
            for (CompSlot const& slot : comp->slots)
            {
                int const specNo = FindSpecNo(slot.cls, slot.spec);
                Player* bot = specNo < 0 ? nullptr : pick(faction, slot.cls, IsHealerSpec(slot.cls, specNo), roster);
                if (!bot)
                    break;
                roster.emplace_back(bot, specNo);
            }
            if (roster.size() == comp->slots.size() && CreateTeam(roster, type))
            {
                pool.erase(std::remove_if(pool.begin(), pool.end(),
                                          [&roster](Player* bot)
                                          {
                                              return std::any_of(roster.begin(), roster.end(),
                                                                 [bot](auto const& slot) { return slot.first == bot; });
                                          }),
                           pool.end());
                return true;
            }
        }

        // No classic composition available: one healer plus damage dealers, as pick-up teams form.
        std::vector<std::pair<Player*, int>> roster;
        std::vector<Player*> candidates;
        for (Player* bot : pool)
        {
            if (bot->GetTeamId() == faction)
                candidates.push_back(bot);
        }
        std::shuffle(candidates.begin(), candidates.end(), RandomEngine::Instance());
        for (Player* bot : candidates)
        {
            char const* healer = DefaultHealerSpec(bot->getClass());
            if (healer && PlayerbotAI::IsHeal(bot, true))
            {
                roster.emplace_back(bot, FindSpecNo(bot->getClass(), healer));
                break;
            }
        }
        if (roster.empty())
            continue;
        for (Player* bot : candidates)
        {
            if (roster.size() >= type)
                break;
            char const* dps = DefaultDpsSpec(bot->getClass());
            if (bot != roster.front().first && dps && !PlayerbotAI::IsHeal(bot, true) &&
                Chemistry(bot, roster) != NO_CHEMISTRY)
                roster.emplace_back(bot, FindSpecNo(bot->getClass(), dps));
        }
        if (roster.size() < type)
            continue;
        std::rotate(roster.begin(), roster.begin() + 1, roster.end());
        if (CreateTeam(roster, type))
        {
            for (auto const& [bot, specNo] : roster)
                pool.erase(std::remove(pool.begin(), pool.end(), bot), pool.end());
            return true;
        }
    }
    return false;
}

ArenaTeam* LivingArenaMgr::CreateTeam(std::vector<std::pair<Player*, int>> const& roster, uint8 type)
{
    std::string name = RandomPlayerbotFactory::CreateRandomArenaTeamName();
    if (name.empty())
        return nullptr;

    Player* captain = roster.front().first;
    ArenaTeam* team = new ArenaTeam();
    if (!team->Create(captain->GetGUID(), type, name, urand(0xFF000000, 0xFFFFFFFF), urand(0, 101),
                      urand(0xFF000000, 0xFFFFFFFF), urand(0, 5), urand(0xFF000000, 0xFFFFFFFF)))
    {
        delete team;
        RandomPlayerbotFactory::_availableArenaTeamNames.push_back(std::move(name));
        return nullptr;
    }
    sArenaTeamMgr->AddArenaTeam(team);
    for (size_t i = 1; i < roster.size(); ++i)
        team->AddMember(roster[i].first->GetGUID());
    team->SaveToDB(true);

    std::string members;
    for (auto const& [bot, specNo] : roster)
    {
        ApplySpec(bot, specNo);
        RememberGearScore(bot);
        members += " " + bot->GetName() + " (" + sPlayerbotAIConfig.PremadeSpecName[bot->getClass()][specNo] + ")";
    }
    LOG_INFO("playerbots", "Living arena: formed {}v{} team <{}>:{}", type, type, team->GetName(), members);
    return team;
}

uint32 LivingArenaMgr::GetTeamMmr(ArenaTeam* team) const
{
    uint32 sum = 0;
    uint32 count = 0;
    for (ArenaTeamMember const& member : team->GetMembers())
    {
        sum += member.MatchMakerRating;
        ++count;
    }
    return count ? sum / count : 0;
}

uint32 LivingArenaMgr::GetTeamGearScore(ArenaTeam* team)
{
    uint32 sum = 0;
    uint32 count = 0;
    for (ArenaTeamMember const& member : team->GetMembers())
    {
        uint32 score = 0;
        if (Player* player = ObjectAccessor::FindConnectedPlayer(member.Guid))
            score = GetGearScore(player);
        else if (auto found = _gearScores.find(member.Guid.GetCounter()); found != _gearScores.end())
            score = found->second;
        if (!score)
            continue;
        sum += score;
        ++count;
    }
    return count ? sum / count : 0;
}

uint32 LivingArenaMgr::GetTeamDedication(ArenaTeam* team) const
{
    uint32 sum = 0;
    for (ArenaTeamMember const& member : team->GetMembers())
        sum += LivingArena::Dedication(member.Guid.GetCounter());
    return team->GetMembersSize() ? sum / team->GetMembersSize() : 0;
}

bool LivingArenaMgr::IsTeamReady(ArenaTeam* team) const
{
    if (team->GetMembersSize() < team->GetType())
        return false;
    if (!IsFree(ObjectAccessor::FindConnectedPlayer(team->GetCaptain())))
        return false;
    uint32 available = 0;
    for (ArenaTeamMember const& member : team->GetMembers())
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(member.Guid);
        if (!player || IsFree(player))
            ++available;
    }
    return available >= team->GetType();
}

bool LivingArenaMgr::IsTeamBusy(ArenaTeam* team, uint8 type) const
{
    BattlegroundQueueTypeId const queueType = BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_AA, type);
    Player* captain = ObjectAccessor::FindConnectedPlayer(team->GetCaptain());
    return captain && (captain->InBattlegroundQueueForBattlegroundQueueType(queueType) || captain->InArena());
}

bool LivingArenaMgr::IsTeamInArena(ArenaTeam* team) const
{
    for (ArenaTeamMember const& member : team->GetMembers())
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(member.Guid);
        if (player && player->InArena() && player->GetBattleground() && player->GetBattleground()->isRated())
            return true;
    }
    return false;
}

bool LivingArenaMgr::IsTeamInvited(ArenaTeam* team, uint8 type) const
{
    BattlegroundQueue& queue = sBattlegroundMgr->GetBattlegroundQueue(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_AA, type));
    GroupQueueInfo info;
    return queue.GetPlayerGroupInfoData(team->GetCaptain(), &info) && info.IsInvitedToBGInstanceGUID;
}

bool LivingArenaMgr::IsReserved(uint32 teamId, time_t now) const
{
    if (auto found = _noShow.find(teamId); found != _noShow.end() && now < found->second)
        return true;
    for (auto const& [humanTeam, responder] : _responders)
    {
        if (responder.team == teamId)
            return true;
    }
    return std::any_of(_pending.begin(), _pending.end(),
                       [teamId](Pending const& pending) { return pending.teamA == teamId || pending.teamB == teamId; });
}

bool LivingArenaMgr::IsHumanGroup(GroupQueueInfo const* group)
{
    for (ObjectGuid guid : group->Players)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(guid);
        if (!player || !IsBot(player))
            return true;
    }
    return false;
}

uint32 LivingArenaMgr::GetGroupGearScore(GroupQueueInfo const* group)
{
    uint32 sum = 0;
    uint32 count = 0;
    for (ObjectGuid guid : group->Players)
    {
        if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        {
            sum += GetGearScore(player);
            ++count;
        }
    }
    return count ? sum / count : 0;
}

void LivingArenaMgr::LoginMembers(ArenaTeam* team) const
{
    for (ArenaTeamMember const& member : team->GetMembers())
    {
        if (!ObjectAccessor::FindConnectedPlayer(member.Guid))
            sRandomPlayerbotMgr.AddPlayerBot(member.Guid, 0);
    }
}

void LivingArenaMgr::LeaveQueue(ArenaTeam* team, uint8 type) const
{
    BattlegroundQueueTypeId const queueType = BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_AA, type);
    BattlegroundQueue& queue = sBattlegroundMgr->GetBattlegroundQueue(queueType);
    for (ArenaTeamMember const& member : team->GetMembers())
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(member.Guid);
        if (!player || !player->InBattlegroundQueueForBattlegroundQueueType(queueType))
            continue;
        GroupQueueInfo info;
        if (queue.GetPlayerGroupInfoData(player->GetGUID(), &info) && info.IsInvitedToBGInstanceGUID)
            continue;
        queue.RemovePlayer(player->GetGUID(), true);
        player->RemoveBattlegroundQueueId(queueType);
    }
}

ArenaTeam* LivingArenaMgr::ChooseOpponent(uint8 type, uint32 mmr, uint32 maxAbove, uint32 gearCap, uint32 exclude,
                                          uint32 previousOpponent, time_t now, bool rested)
{
    uint32 const window = sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE)
                              ? sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE)
                              : 5000;
    std::vector<std::pair<uint32, ArenaTeam*>> fits;
    for (auto const& [id, team] : sArenaTeamMgr->GetArenaTeams())
    {
        if (id == exclude || id == previousOpponent || team->GetType() != type || !IsBotTeam(team) ||
            IsReserved(id, now))
            continue;
        uint32 const teamMmr = GetTeamMmr(team);
        if (!LivingArena::WithinRatingCap(mmr, teamMmr, std::min(maxAbove, window), window))
            continue;
        if (rested && now < _lastQueued[id] + time_t(LivingArena::RestMinutes(GetTeamDedication(team))) * 60)
            continue;
        if (IsTeamBusy(team, type) || !IsTeamReady(team))
            continue;
        if (gearCap)
        {
            uint32 const score = GetTeamGearScore(team);
            if (score && !LivingArena::WithinGearCap(gearCap, score, sPlayerbotAIConfig.LivingArenaMaxGearScoreAbove))
                continue;
        }
        fits.emplace_back(teamMmr > mmr ? teamMmr - mmr : mmr - teamMmr, team);
    }
    if (fits.empty())
        return nullptr;
    std::sort(fits.begin(), fits.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    // Pick among near-equal opponents so the same pairings don't repeat every evening.
    size_t closest = 1;
    while (closest < fits.size() && closest < 3 && fits[closest].first <= fits[0].first + 25)
        ++closest;
    return fits[urand(0, closest - 1)].second;
}

void LivingArenaMgr::ScheduleMatches(time_t now)
{
    // Rated queues that real players are waiting in, one entry per human team.
    std::unordered_map<uint32, HumanQueue> humans;
    for (Player* player : sRandomPlayerbotMgr.GetPlayers())
    {
        if (!player || !player->InBattlegroundQueue())
            continue;
        for (uint8 slot = 0; slot < PLAYER_MAX_BATTLEGROUND_QUEUES; ++slot)
        {
            BattlegroundQueueTypeId const queueType = player->GetBattlegroundQueueTypeId(slot);
            uint8 const type = BattlegroundMgr::BGArenaType(queueType);
            if (!type)
                continue;
            GroupQueueInfo info;
            if (!sBattlegroundMgr->GetBattlegroundQueue(queueType).GetPlayerGroupInfoData(player->GetGUID(), &info) ||
                !info.IsRated || info.IsInvitedToBGInstanceGUID)
                continue;
            HumanQueue& human = humans[info.ArenaTeamId];
            human.type = type;
            human.mmr = info.ArenaMatchmakerRating;
            human.previousOpponent = info.PreviousOpponentsTeamId;
            uint32 sum = 0;
            uint32 count = 0;
            for (ObjectGuid guid : info.Players)
            {
                if (Player* member = ObjectAccessor::FindConnectedPlayer(guid))
                {
                    sum += GetGearScore(member);
                    ++count;
                }
            }
            human.gearScore = count ? sum / count : 0;
        }
    }

    std::set<uint8> humanTypes;
    for (auto const& [humanTeam, human] : humans)
        humanTypes.insert(human.type);

    // Drop pairings that started, never formed, or haven't been invited yet when a human waits in the bracket.
    _pending.erase(std::remove_if(_pending.begin(), _pending.end(),
                                  [&](Pending const& pending)
                                  {
                                      ArenaTeam* a = sArenaTeamMgr->GetArenaTeamById(pending.teamA);
                                      ArenaTeam* b = sArenaTeamMgr->GetArenaTeamById(pending.teamB);
                                      if (!a || !b)
                                          return true;
                                      if (IsTeamInArena(a) && IsTeamInArena(b))
                                          return true;
                                      bool const yield = humanTypes.count(pending.type) &&
                                                         !IsTeamInvited(a, pending.type) &&
                                                         !IsTeamInvited(b, pending.type);
                                      if (!yield && now < pending.created + SHOW_UP_SECONDS)
                                          return false;
                                      LeaveQueue(a, pending.type);
                                      LeaveQueue(b, pending.type);
                                      return true;
                                  }),
                   _pending.end());

    for (auto it = _responders.begin(); it != _responders.end();)
    {
        ArenaTeam* responder = sArenaTeamMgr->GetArenaTeamById(it->second.team);
        bool const waiting = humans.count(it->first) > 0;
        // Still not in the match after the grace period, or playing someone else: pick another team for this human
        bool const noShow =
            waiting && responder && (IsTeamInArena(responder) || now >= it->second.assigned + SHOW_UP_SECONDS);
        if (waiting && responder && !noShow)
        {
            ++it;
            continue;
        }
        if (noShow)
        {
            _noShow[it->second.team] = now + NO_SHOW_BENCH_SECONDS;
            LOG_INFO("playerbots", "Living arena: <{}> didn't start against a human team in time; choosing another",
                     responder->GetName());
        }
        if (responder && !IsTeamInArena(responder))
            LeaveQueue(responder, responder->GetType());
        it = _responders.erase(it);
    }

    for (auto const& [humanTeam, human] : humans)
    {
        if (_responders.count(humanTeam))
            continue;
        ArenaTeam* responder = ChooseOpponent(human.type, human.mmr, sPlayerbotAIConfig.LivingArenaMaxRatingAbove,
                                              human.gearScore, humanTeam, human.previousOpponent, now, false);
        if (!responder)
            continue;
        _responders[humanTeam] = {responder->GetId(), now};
        _lastQueued[responder->GetId()] = now;
        LoginMembers(responder);
        LOG_INFO("playerbots", "Living arena: <{}> ({} MMR) answers a human {}v{} queue at {} MMR", responder->GetName(),
                 GetTeamMmr(responder), human.type, human.type, human.mmr);
    }

    // Bot pairings pause while a human waits in that bracket so they can't take the human's place.
    std::array<std::pair<uint8, uint32>, 3> const targets = {
        {{2, sPlayerbotAIConfig.RandomBotAutoJoinBGRatedArena2v2Count},
         {3, sPlayerbotAIConfig.RandomBotAutoJoinBGRatedArena3v3Count},
         {5, sPlayerbotAIConfig.RandomBotAutoJoinBGRatedArena5v5Count}}};
    for (auto const& [type, target] : targets)
    {
        if (target && !humanTypes.count(type))
            ScheduleBotMatch(type, target, now);
    }

    std::set<std::pair<ObjectGuid::LowType, uint8>> allowed;
    for (Pending const& pending : _pending)
    {
        for (uint32 id : {pending.teamA, pending.teamB})
        {
            if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(id))
                allowed.emplace(team->GetCaptain().GetCounter(), pending.type);
        }
    }
    for (auto const& [humanTeam, responder] : _responders)
    {
        if (ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(responder.team))
            allowed.emplace(team->GetCaptain().GetCounter(), uint8(team->GetType()));
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _allowed = std::move(allowed);
}

void LivingArenaMgr::ScheduleBotMatch(uint8 type, uint32 target, time_t now)
{
    uint32 inArena = 0;
    std::vector<ArenaTeam*> ready;
    for (auto const& [id, team] : sArenaTeamMgr->GetArenaTeams())
    {
        if (team->GetType() != type || !IsBotTeam(team))
            continue;
        if (IsTeamInArena(team))
            ++inArena;
        else if (now >= _lastQueued[id] + time_t(LivingArena::RestMinutes(GetTeamDedication(team))) * 60 &&
                 !IsReserved(id, now) && !IsTeamBusy(team, type) && IsTeamReady(team))
            ready.push_back(team);
    }
    uint32 pending = std::count_if(_pending.begin(), _pending.end(), [type](Pending const& p) { return p.type == type; });
    if (inArena / 2 + pending >= target || ready.size() < 2)
        return;

    std::shuffle(ready.begin(), ready.end(), RandomEngine::Instance());
    for (ArenaTeam* team : ready)
    {
        ArenaTeam* opponent = ChooseOpponent(type, GetTeamMmr(team), sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE),
                                             0, team->GetId(), team->GetPreviousOpponents(), now, true);
        if (!opponent)
            continue;
        _pending.push_back({team->GetId(), opponent->GetId(), type, now});
        _lastQueued[team->GetId()] = now;
        _lastQueued[opponent->GetId()] = now;
        LoginMembers(team);
        LoginMembers(opponent);
        LOG_INFO("playerbots", "Living arena: {}v{} <{}> ({} MMR) vs <{}> ({} MMR)", type, type, team->GetName(),
                 GetTeamMmr(team), opponent->GetName(), GetTeamMmr(opponent));
        return;
    }
}

bool LivingArenaMgr::MayQueueRated(Player* captain, uint8 arenaType)
{
    if (!IsEnabled() || !captain)
        return true;
    std::lock_guard<std::mutex> lock(_mutex);
    return _allowed.count({captain->GetGUID().GetCounter(), arenaType}) > 0;
}

bool LivingArenaMgr::AllowMatchingGroup(BattlegroundQueue* queue, GroupQueueInfo* group, uint8 bracketId)
{
    if (!IsEnabled() || !group || !group->ArenaType || group->IsRated || bracketId >= MAX_BATTLEGROUND_BRACKETS)
        return true;

    uint32 botScore = 0;
    for (ObjectGuid guid : group->Players)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(guid);
        if (!player || !IsBot(player))
            return true;
        botScore = std::max(botScore, GetGearScore(player));
    }

    // Any human waiting for a skirmish in this bracket caps the gear of the bots let in.
    uint32 humanScore = 0;
    for (uint8 index = 0; index < BG_QUEUE_MAX; ++index)
    {
        for (GroupQueueInfo* queued : queue->m_QueuedGroups[bracketId][index])
        {
            if (!queued || queued->IsRated || queued->ArenaType != group->ArenaType)
                continue;
            for (ObjectGuid guid : queued->Players)
            {
                Player* player = ObjectAccessor::FindConnectedPlayer(guid);
                if (player && !IsBot(player))
                    humanScore = humanScore ? std::min(humanScore, GetGearScore(player)) : GetGearScore(player);
            }
        }
    }
    return !humanScore || LivingArena::WithinGearCap(humanScore, botScore, sPlayerbotAIConfig.LivingArenaMaxGearScoreAbove);
}

bool LivingArenaMgr::AllowRatedUpdate(BattlegroundQueue* queue, uint8 bracketId, uint8 arenaType, uint32 arenaRating)
{
    if (!IsEnabled() || !queue || bracketId >= MAX_BATTLEGROUND_BRACKETS)
        return true;

    // Mirrors the rated pick in BattlegroundQueue::BattlegroundQueueUpdate; keep in step with the pinned core.
    auto& groups = queue->m_QueuedGroups[bracketId];
    if (!arenaRating)
    {
        GroupQueueInfo* alliance = groups[BG_QUEUE_PREMADE_ALLIANCE].empty() ? nullptr : groups[BG_QUEUE_PREMADE_ALLIANCE].front();
        GroupQueueInfo* horde = groups[BG_QUEUE_PREMADE_HORDE].empty() ? nullptr : groups[BG_QUEUE_PREMADE_HORDE].front();
        if (!alliance && !horde)
            return true;
        arenaRating = horde && (!alliance || horde->JoinTime <= alliance->JoinTime) ? horde->ArenaMatchmakerRating
                                                                                     : alliance->ArenaMatchmakerRating;
    }
    uint32 const difference = sBattlegroundMgr->GetMaxRatingDifference();
    uint32 const minRating = arenaRating <= difference ? 0 : arenaRating - difference;
    uint32 const maxRating = arenaRating + difference;
    int32 const discardTime = GameTime::GetGameTimeMS().count() - sBattlegroundMgr->GetRatingDiscardTimer();
    int32 const discardOpponentsTime =
        GameTime::GetGameTimeMS().count() - sWorld->getIntConfig(CONFIG_ARENA_PREV_OPPONENTS_DISCARD_TIMER);
    auto inRange = [&](GroupQueueInfo const* group)
    {
        return !group->IsInvitedToBGInstanceGUID &&
               ((group->ArenaMatchmakerRating >= minRating && group->ArenaMatchmakerRating <= maxRating) ||
                int32(group->JoinTime) < discardTime);
    };

    std::array<GroupQueueInfo*, 2> picked{};
    uint8 found = 0;
    uint8 queueIndex = 0;
    for (uint8 index = BG_QUEUE_PREMADE_ALLIANCE; index < BG_QUEUE_NORMAL_ALLIANCE; ++index)
    {
        auto match = std::find_if(groups[index].begin(), groups[index].end(), inRange);
        if (match != groups[index].end())
        {
            picked[found++] = *match;
            queueIndex = index;
        }
    }
    if (found == 1)
    {
        auto& same = groups[queueIndex];
        auto match = std::find_if(std::find(same.begin(), same.end(), picked[0]), same.end(),
                                  [&](GroupQueueInfo const* group)
                                  {
                                      return inRange(group) &&
                                             (picked[0]->ArenaTeamId != group->PreviousOpponentsTeamId ||
                                              int32(group->JoinTime) < discardOpponentsTime) &&
                                             picked[0]->ArenaTeamId != group->ArenaTeamId;
                                  });
        if (match != same.end())
            picked[found++] = *match;
    }
    if (found < 2)
        return true;

    bool const firstHuman = IsHumanGroup(picked[0]);
    if (firstHuman == IsHumanGroup(picked[1]))
        return true;
    GroupQueueInfo const* human = firstHuman ? picked[0] : picked[1];
    GroupQueueInfo const* bots = firstHuman ? picked[1] : picked[0];

    // Only the team scheduled for this human may play it, and only while both caps still hold.
    uint32 const window = sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE)
                              ? sWorld->getIntConfig(CONFIG_ARENA_MAX_RATING_DIFFERENCE)
                              : 5000;
    uint32 const humanScore = GetGroupGearScore(human);
    uint32 const botScore = GetGroupGearScore(bots);
    auto assigned = _responders.find(human->ArenaTeamId);
    bool const isResponder = assigned != _responders.end() && assigned->second.team == bots->ArenaTeamId;
    bool const fits =
        LivingArena::WithinRatingCap(human->ArenaMatchmakerRating, bots->ArenaMatchmakerRating,
                                     std::min(sPlayerbotAIConfig.LivingArenaMaxRatingAbove, window), window) &&
        (!humanScore || !botScore ||
         LivingArena::WithinGearCap(humanScore, botScore, sPlayerbotAIConfig.LivingArenaMaxGearScoreAbove));
    if (isResponder && fits)
        return true;

    ArenaTeam* botTeam = sArenaTeamMgr->GetArenaTeamById(bots->ArenaTeamId);
    if (!botTeam)
        return true;
    LOG_INFO("playerbots", "Living arena: kept <{}> ({} MMR, gear {}) from a human {}v{} team ({} MMR, gear {}): {}",
             botTeam->GetName(), bots->ArenaMatchmakerRating, botScore, arenaType, arenaType,
             human->ArenaMatchmakerRating, humanScore, isResponder ? "over the caps" : "not its scheduled opponent");
    if (isResponder)
    {
        _noShow[botTeam->GetId()] = time(nullptr) + NO_SHOW_BENCH_SECONDS;
        _responders.erase(assigned);
    }
    // Its bot pairing can't go ahead with one side gone.
    for (auto it = _pending.begin(); it != _pending.end();)
    {
        if (it->teamA != botTeam->GetId() && it->teamB != botTeam->GetId())
        {
            ++it;
            continue;
        }
        if (ArenaTeam* partner = sArenaTeamMgr->GetArenaTeamById(it->teamA == botTeam->GetId() ? it->teamB : it->teamA))
            LeaveQueue(partner, arenaType);
        it = _pending.erase(it);
    }
    // Out of the queue, so the next queue update can pair the human properly.
    LeaveQueue(botTeam, arenaType);
    return false;
}

void LivingArenaMgr::SetSkill(Player* bot, uint32 skill)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _skillOverrides[bot->GetGUID().GetCounter()] = skill;
}

uint32 LivingArenaMgr::GetSkill(Player* bot) const
{
    if (!bot)
        return 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (auto found = _skillOverrides.find(bot->GetGUID().GetCounter()); found != _skillOverrides.end())
            return found->second;
    }
    if (sRandomPlayerbotMgr.IsRandomBot(bot))
        return LivingArena::Skill(bot->GetGUID().GetCounter());
    return sPlayerbotAIConfig.LivingArenaPartnerSkill;
}

uint32 LivingArenaMgr::GetTeamSkill(Player* bot) const
{
    Battleground* bg = bot ? bot->GetBattleground() : nullptr;
    if (!bg)
        return GetSkill(bot);
    uint32 sum = 0;
    uint32 count = 0;
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (!player || player->GetBgTeamId() != bot->GetBgTeamId() || !IsBot(player))
            continue;
        sum += GetSkill(player);
        ++count;
    }
    return count ? sum / count : 0;
}

bool LivingArenaMgr::HasHumanTeammate(Player* bot) const
{
    Battleground* bg = bot ? bot->GetBattleground() : nullptr;
    if (!bg)
        return false;
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (player && player->GetBgTeamId() == bot->GetBgTeamId() && !IsBot(player))
            return true;
    }
    return false;
}

bool LivingArenaMgr::ClaimTeamCall(ObjectGuid group, uint32 intervalMs)
{
    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> lock(_mutex);
    uint32& last = _calls[group.GetRawValue()];
    if (last && getMSTimeDiff(last, now) < intervalMs)
        return false;
    last = now ? now : 1;
    return true;
}

void LivingArenaMgr::CheckJuke(Player* bot, PlayerbotAI* ai)
{
    if (!bot->InArena() || !IsEnabled() || !IsBot(bot))
        return;
    Spell* spell = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!spell || spell->getState() != SPELL_STATE_PREPARING || spell->GetCastTime() < 1500)
        return;
    // Past 60% of the cast it's committed; before that an expert fakes it once if a kick is ready.
    if (spell->GetCastTimeRemaining() * 10 < spell->GetCastTime() * 4 ||
        LivingArena::TierFor(GetSkill(bot)) != LivingArena::Tier::Expert)
        return;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        uintptr_t& seen = _jukes[bot->GetGUID().GetCounter()];
        if (seen == reinterpret_cast<uintptr_t>(spell))
            return;
        seen = reinterpret_cast<uintptr_t>(spell);
    }

    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return;
    for (auto const& [guid, enemy] : bg->GetPlayers())
    {
        if (!enemy || !enemy->IsAlive() || enemy->GetBgTeamId() == bot->GetBgTeamId() ||
            enemy->GetTarget() != bot->GetGUID())
            continue;
        uint32 interrupt = 0;
        float range = 6.0f;
        switch (enemy->getClass())
        {
            case CLASS_ROGUE:
                interrupt = 1766;
                break;
            case CLASS_WARRIOR:
                interrupt = 6552;
                break;
            case CLASS_DEATH_KNIGHT:
                interrupt = 47528;
                break;
            case CLASS_MAGE:
                interrupt = 2139;
                range = 30.0f;
                break;
            case CLASS_SHAMAN:
                interrupt = 57994;
                range = 25.0f;
                break;
            case CLASS_HUNTER:
                interrupt = 34490;
                range = 35.0f;
                break;
            default:
                break;
        }
        if (!interrupt || !enemy->HasSpell(interrupt) || enemy->HasSpellCooldown(interrupt) ||
            !enemy->IsWithinDistInMap(bot, range))
            continue;
        if (urand(1, 100) <= 60)
            ai->RequestSpellInterrupt();
        return;
    }
}

void LivingArenaMgr::OnBattlegroundEnd(Battleground* bg, TeamId winner)
{
    if (!bg || !bg->isArena())
        return;
    // An arena side is at most five, so unlike battlegrounds every teammate pair is remembered, bots included.
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (!player || !player->IsInWorld())
            continue;
        for (auto const& [otherGuid, other] : bg->GetPlayers())
        {
            if (other && other != player && other->IsInWorld() && other->GetBgTeamId() == player->GetBgTeamId())
                LivingBondMgr::instance().Record(player, other,
                                                 player->GetBgTeamId() == winner
                                                     ? LivingWorld::BondEvent::BattlegroundWin
                                                     : LivingWorld::BondEvent::BattlegroundLoss);
        }
    }
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (player && player->GetGroup())
            _calls.erase(player->GetGroup()->GetGUID().GetRawValue());
    }
}

std::vector<ObjectGuid> LivingArenaMgr::GetPartners(Player* player) const
{
    std::vector<ObjectGuid> partners;
    for (uint8 slot = 0; slot < MAX_ARENA_SLOT; ++slot)
    {
        ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(slot));
        if (!team || team->GetCaptain() != player->GetGUID())
            continue;
        for (ArenaTeamMember const& member : team->GetMembers())
        {
            if (member.Guid == player->GetGUID() ||
                std::find(partners.begin(), partners.end(), member.Guid) != partners.end())
                continue;
            uint32 const accountId = sCharacterCache->GetCharacterAccountIdByGuid(member.Guid);
            Player* online = ObjectAccessor::FindConnectedPlayer(member.Guid);
            if ((online && IsBot(online)) || sPlayerbotAIConfig.IsInRandomAccountList(accountId) ||
                accountId == player->GetSession()->GetAccountId())
                partners.push_back(member.Guid);
        }
    }
    return partners;
}

void LivingArenaMgr::ReviewPartners()
{
    std::vector<std::pair<ArenaTeam*, ObjectGuid>> leaving;
    for (auto const& [id, team] : sArenaTeamMgr->GetArenaTeams())
    {
        if (IsBotTeam(team))
            continue;
        std::vector<ObjectGuid> people;
        std::vector<ObjectGuid> randomBots;
        for (ArenaTeamMember const& member : team->GetMembers())
        {
            uint32 const accountId = sCharacterCache->GetCharacterAccountIdByGuid(member.Guid);
            (sPlayerbotAIConfig.IsInRandomAccountList(accountId) ? randomBots : people).push_back(member.Guid);
        }
        for (ObjectGuid bot : randomBots)
        {
            if (bot == team->GetCaptain())
                continue;
            bool const fallenOut = std::any_of(people.begin(), people.end(),
                                               [bot](ObjectGuid person)
                                               {
                                                   return LivingBondMgr::instance().AttitudeOf(bot.GetCounter(),
                                                                                                person.GetCounter()) ==
                                                          LivingWorld::Attitude::Rival;
                                               });
            if (fallenOut)
                leaving.emplace_back(team, bot);
        }
    }
    for (auto const& [team, bot] : leaving)
    {
        std::string name;
        sCharacterCache->GetCharacterNameByGuid(bot, name);
        for (ArenaTeamMember const& member : team->GetMembers())
        {
            Player* person = ObjectAccessor::FindConnectedPlayer(member.Guid);
            if (person && !IsBot(person))
                ChatHandler(person->GetSession()).SendSysMessage(name + " has had enough of you and left <" +
                                                                 team->GetName() + ">.");
        }
        LOG_INFO("playerbots", "Living arena: {} left <{}> after falling out with a teammate", name, team->GetName());
        team->DelMember(bot, true);
    }
}

std::vector<Player*> LivingArenaMgr::FindPartnerCandidates(Player* player, uint8 arenaType, uint8 cls, uint32 limit)
{
    uint8 const slot = ArenaTeam::GetSlotByType(arenaType);
    uint32 const playerScore = GetGearScore(player);
    std::vector<std::pair<uint32, Player*>> found;
    for (auto const& [guid, bot] : sRandomPlayerbotMgr.GetAllBots())
    {
        if (!bot || bot->getClass() != cls || bot->GetTeamId() != player->GetTeamId() ||
            bot->GetLevel() != player->GetLevel() || !IsFree(bot) || bot->GetGroup() ||
            bot->GetArenaTeamId(slot) || sRandomPlayerbotMgr.IsAddClassAccount(bot->GetSession()->GetAccountId()))
            continue;
        bool onBotTeam = false;
        for (uint8 other = 0; other < MAX_ARENA_SLOT; ++other)
            onBotTeam = onBotTeam || IsBotTeam(sArenaTeamMgr->GetArenaTeamById(bot->GetArenaTeamId(other)));
        if (onBotTeam)
            continue;
        // Bots wary of you or holding a grudge won't sign up; friends are offered before strangers.
        LivingWorld::Attitude const attitude =
            LivingBondMgr::instance().AttitudeOf(bot->GetGUID().GetCounter(), player->GetGUID().GetCounter());
        if (attitude == LivingWorld::Attitude::Rival || attitude == LivingWorld::Attitude::Wary)
            continue;
        uint32 const familiarity = attitude == LivingWorld::Attitude::Friend     ? 0
                                   : attitude == LivingWorld::Attitude::Friendly ? 1
                                                                                 : 2;
        uint32 const score = GetGearScore(bot);
        // Someone at or a little below your gear first; better-geared strangers last.
        uint32 const distance = score > playerScore ? (score - playerScore) * 3 : playerScore - score;
        found.emplace_back(familiarity * 1000000 + std::min<uint32>(distance, 999999), bot);
    }
    std::sort(found.begin(), found.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    std::vector<Player*> result;
    for (auto const& [distance, bot] : found)
    {
        if (result.size() >= limit)
            break;
        result.push_back(bot);
    }
    return result;
}

std::vector<Player*> LivingArenaMgr::GetShoppers() const
{
    std::vector<Player*> shoppers;
    for (auto const& [guid, bot] : sRandomPlayerbotMgr.GetAllBots())
    {
        if (bot && bot->GetLevel() == GetArenaLevel() && OnAnyArenaTeam(bot))
            shoppers.push_back(bot);
    }
    for (Player* player : sRandomPlayerbotMgr.GetPlayers())
    {
        PlayerbotMgr* mgr = player ? GET_PLAYERBOT_MGR(player) : nullptr;
        if (!mgr)
            continue;
        for (auto it = mgr->GetPlayerBotsBegin(); it != mgr->GetPlayerBotsEnd(); ++it)
        {
            Player* bot = it->second;
            if (bot && bot->GetLevel() == GetArenaLevel() && OnAnyArenaTeam(bot) &&
                std::find(shoppers.begin(), shoppers.end(), bot) == shoppers.end())
                shoppers.push_back(bot);
        }
    }
    return shoppers;
}

void LivingArenaMgr::Shop(time_t now)
{
    if (_catalog.empty())
        return;
    uint32 visited = 0;
    for (Player* bot : GetShoppers())
    {
        if (visited >= 20)
            break;
        uint32 const guid = bot->GetGUID().GetCounter();
        time_t& next = _nextShop[guid];
        if (now < next || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() || bot->InBattleground())
            continue;
        ++visited;
        next = now + urand(20, 40) * MINUTE;
        if (urand(1, 100) > LivingArena::ShoppingChance(LivingArena::Dedication(guid)))
            continue;
        bool bought = false;
        for (uint32 i = 0; i < 3 && TryPurchase(bot); ++i)
            bought = true;
        if (bought && LivingArena::Tunes(BestPersonalRating(bot), LivingArena::Dedication(guid)))
        {
            PlayerbotFactory factory(bot, bot->GetLevel());
            factory.ApplyEnchantAndGemsNew();
        }
        RememberGearScore(bot);
    }
}

bool LivingArenaMgr::TryPurchase(Player* bot)
{
    StatsWeightCalculator calculator(bot);
    calculator.SetPvpSpec(sRandomPlayerbotMgr.IsSpecPvp(bot->GetGUID().GetCounter(), bot->getClass()));
    uint32 const dedication = LivingArena::Dedication(bot->GetGUID().GetCounter());

    struct Choice
    {
        CatalogItem const* entry;
        uint8 slot;
        float gain;
    };
    std::vector<Choice> choices;
    for (CatalogItem const& entry : _catalog)
    {
        if (entry.honor > bot->GetHonorPoints() || entry.arena > bot->GetArenaPoints())
            continue;
        if (entry.rating && bot->GetMaxPersonalArenaRatingRequirement(entry.arenaSlot) < entry.rating)
            continue;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry.item);
        if (!proto || bot->CanUseItem(proto) != EQUIP_ERR_OK)
            continue;
        float const value = calculator.CalculateItem(entry.item);
        if (value <= 0)
            continue;

        uint8 bestSlot = NULL_SLOT;
        float weakest = 0;
        for (uint8 slot : CandidateSlots(bot, proto))
        {
            Item* current = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (current && current->GetEntry() == entry.item)
            {
                bestSlot = NULL_SLOT;
                break;
            }
            float const score =
                current ? calculator.CalculateItem(current->GetEntry(), current->GetItemRandomPropertyId()) : 0;
            if (bestSlot == NULL_SLOT || score < weakest)
            {
                bestSlot = slot;
                weakest = score;
            }
        }
        if (bestSlot == NULL_SLOT || value <= weakest * 1.03f)
            continue;
        choices.push_back({&entry, bestSlot, value - weakest});
    }
    if (choices.empty())
        return false;

    // Dedicated players buy the biggest upgrade; casual ones buy whatever catches their eye.
    Choice choice = choices.front();
    if (urand(1, 100) <= dedication)
        choice = *std::max_element(choices.begin(), choices.end(),
                                   [](Choice const& a, Choice const& b) { return a.gain < b.gain; });
    else
        choice = choices[urand(0, choices.size() - 1)];

    uint16 dest = 0;
    if (bot->CanEquipNewItem(choice.slot, dest, choice.entry->item, true) != EQUIP_ERR_OK)
        return false;

    std::string replaced = "nothing";
    if (Item* old = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, choice.slot))
    {
        replaced = old->GetTemplate()->Name1;
        bot->ModifyMoney(old->GetTemplate()->SellPrice);
        bot->DestroyItem(INVENTORY_SLOT_BAG_0, choice.slot, true);
    }
    bot->ModifyHonorPoints(-int32(choice.entry->honor));
    bot->ModifyArenaPoints(-int32(choice.entry->arena));
    Item* item = bot->EquipNewItem(dest, choice.entry->item, true);
    if (!item)
        return false;
    bot->AutoUnequipOffhandIfNeed();
    LOG_INFO("playerbots", "Living arena: {} bought {} for {} honor / {} arena points (rating {}), replacing {}",
             bot->GetName(), item->GetTemplate()->Name1, choice.entry->honor, choice.entry->arena,
             BestPersonalRating(bot), replaced);
    return true;
}

void LivingArenaMgr::RememberGearScore(Player* bot)
{
    uint32 const guid = bot->GetGUID().GetCounter();
    uint32 const score = GetGearScore(bot);
    uint32& known = _gearScores[guid];
    if (known == score)
        return;
    known = score;
    PlayerbotsDatabase.Execute("REPLACE INTO playerbots_living_arena_bots (guid, gear_score) VALUES ({}, {})", guid,
                               score);
}

class LivingArenaBattlegroundScript : public AllBattlegroundScript
{
public:
    LivingArenaBattlegroundScript()
        : AllBattlegroundScript("LivingArenaBattlegroundScript",
                                {ALLBATTLEGROUNDHOOK_CAN_ADD_GROUP_TO_MATCHING_POOL, ALLBATTLEGROUNDHOOK_ON_QUEUE_UPDATE_VALIDITY,
                                 ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_END})
    {
    }

    bool OnQueueUpdateValidity(BattlegroundQueue* queue, uint32 /*diff*/, BattlegroundTypeId /*bgTypeId*/,
                               BattlegroundBracketId bracketId, uint8 arenaType, bool isRated, uint32 arenaRating) override
    {
        return !isRated || !arenaType ||
               LivingArenaMgr::instance().AllowRatedUpdate(queue, bracketId, arenaType, arenaRating);
    }

    bool CanAddGroupToMatchingPool(BattlegroundQueue* queue, GroupQueueInfo* group, uint32 /*poolPlayerCount*/,
                                   Battleground* /*bg*/, BattlegroundBracketId bracketId) override
    {
        return LivingArenaMgr::instance().AllowMatchingGroup(queue, group, bracketId);
    }

    void OnBattlegroundEnd(Battleground* bg, TeamId winnerTeamId) override
    {
        LivingArenaMgr::instance().OnBattlegroundEnd(bg, winnerTeamId);
    }
};

void AddLivingArenaBrokerScript();

void AddLivingArenaScripts()
{
    new LivingArenaBattlegroundScript();
    AddLivingArenaBrokerScript();
}
