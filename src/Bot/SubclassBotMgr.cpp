/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SubclassBotMgr.h"

#include <algorithm>
#include <unordered_set>

#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "SubclassBotPolicy.h"
#include "Util.h"

namespace
{
constexpr size_t InsertBatch = 500;

bool LowerName(char const* name, std::wstring& lower)
{
    if (!name || !*name || !Utf8toWStr(name, lower))
        return false;

    wstrToLower(lower);
    return true;
}
}  // namespace

SubclassBotMgr& SubclassBotMgr::instance()
{
    static SubclassBotMgr instance;
    return instance;
}

void SubclassBotMgr::Load()
{
    if (!sPlayerbotAIConfig.SubclassBots)
        return;

    if (!WorldDatabase.Query("SHOW TABLES LIKE 'subclass_path'"))
    {
        LOG_INFO("playerbots", "No sub-class paths in the world database, Playerbots.SubclassBots does nothing");
        return;
    }

    LoadSwaps();
    AssignPaths();
}

void SubclassBotMgr::LoadSwaps()
{
    uint32 aliases = 0;
    uint32 missing = 0;
    QueryResult result = WorldDatabase.Query(
        "SELECT p.base_class, s.original_spell, s.replacement_spell FROM subclass_spell_swap s "
        "JOIN subclass_path p ON p.id = s.path_id");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const classId = fields[0].Get<uint8>();
            uint32 const original = fields[1].Get<uint32>();
            uint32 const replacement = fields[2].Get<uint32>();
            SpellInfo const* originalInfo = sSpellMgr->GetSpellInfo(original);
            SpellInfo const* replacementInfo = sSpellMgr->GetSpellInfo(replacement);
            if (classId >= MAX_CLASSES || !originalInfo || !replacementInfo)
            {
                ++missing;
                continue;
            }

            _mirrors[original].push_back(replacement);
            _originalSpells[replacement] = original;

            std::wstring originalName;
            std::wstring replacementName;
            if (!LowerName(originalInfo->SpellName[0], originalName) ||
                !LowerName(replacementInfo->SpellName[0], replacementName) || originalName == replacementName)
                continue;

            std::vector<Alias>& candidates = _aliases[classId][originalName];
            auto alias = std::find_if(candidates.begin(), candidates.end(),
                                      [&](Alias const& known) { return known.lowerName == replacementName; });
            if (alias == candidates.end())
            {
                candidates.push_back({replacementInfo->SpellName[0], replacementName, {}});
                alias = std::prev(candidates.end());
                ++aliases;
            }
            alias->spells.push_back(replacement);
        } while (result->NextRow());
    }

    if (QueryResult creatures =
            WorldDatabase.Query("SELECT original_entry, replacement_entry, guardian_entry FROM subclass_creature_swap"))
    {
        do
        {
            Field* fields = creatures->Fetch();
            _originalCreatures[fields[1].Get<uint32>()] = fields[0].Get<uint32>();
            if (uint32 const guardian = fields[2].Get<uint32>())
                _originalCreatures[guardian] = fields[0].Get<uint32>();
        } while (creatures->NextRow());
    }

    LOG_INFO("playerbots", "Loaded {} sub-class spell names for bots", aliases);
    if (missing)
        LOG_WARN("playerbots", "{} sub-class mirror spells are missing from the spell data, so bots skip them",
                 missing);
}

void SubclassBotMgr::AssignPaths()
{
    if (!CharacterDatabase.Query("SHOW TABLES LIKE 'character_subclass_path'"))
        return;

    std::array<std::vector<SubclassBots::PathShare>, MAX_CLASSES> shares;
    std::string classes;
    if (QueryResult paths = WorldDatabase.Query(
            "SELECT id, base_class, bot_share_pct FROM subclass_path WHERE bot_share_pct > 0 ORDER BY id"))
    {
        do
        {
            Field* fields = paths->Fetch();
            uint8 const classId = fields[1].Get<uint8>();
            if (classId >= MAX_CLASSES)
                continue;

            if (shares[classId].empty())
                classes += (classes.empty() ? "" : ",") + std::to_string(classId);
            shares[classId].push_back({fields[0].Get<uint8>(), fields[2].Get<uint8>()});
        } while (paths->NextRow());
    }

    if (classes.empty())
        return;

    std::unordered_set<uint32> botAccounts;
    if (QueryResult accounts = LoginDatabase.Query("SELECT id FROM account WHERE username LIKE '{}%'",
                                                   sPlayerbotAIConfig.RandomBotAccountPrefix))
    {
        do
        {
            botAccounts.insert(accounts->Fetch()[0].Get<uint32>());
        } while (accounts->NextRow());
    }

    std::vector<std::string> rows;
    QueryResult characters =
        CharacterDatabase.Query("SELECT guid, account, class FROM characters WHERE class IN ({})", classes);
    if (characters)
    {
        do
        {
            Field* fields = characters->Fetch();
            uint32 const guid = fields[0].Get<uint32>();
            if (!botAccounts.count(fields[1].Get<uint32>()))
                continue;

            if (uint8 const path = SubclassBots::PickPath(guid, shares[fields[2].Get<uint8>()]))
                rows.push_back(Acore::StringFormat("({}, 0, {})", guid, path));
        } while (characters->NextRow());
    }

    // Rows that exist are kept, so a bot stays on the path it has and nothing changes for real players.
    for (size_t first = 0; first < rows.size(); first += InsertBatch)
    {
        std::string values;
        for (size_t i = first; i < std::min(rows.size(), first + InsertBatch); ++i)
            values += (values.empty() ? "" : ",") + rows[i];
        CharacterDatabase.DirectExecute(
            "INSERT IGNORE INTO character_subclass_path (guid, talent_group, path_id) VALUES " + values);
    }

    LOG_INFO("playerbots", "{} random bots walk a sub-class path", rows.size());
}

SubclassBotMgr::Alias const* SubclassBotMgr::FindAlias(Player* bot, std::wstring const& lowerName) const
{
    if (!bot || bot->getClass() >= MAX_CLASSES)
        return nullptr;

    auto const& names = _aliases[bot->getClass()];
    if (names.empty())
        return nullptr;

    auto const candidates = names.find(lowerName);
    if (candidates == names.end())
        return nullptr;

    for (Alias const& alias : candidates->second)
        for (uint32 spell : alias.spells)
            if (bot->HasSpell(spell))
                return &alias;

    return nullptr;
}

bool SubclassBotMgr::HasSpellOrMirror(Player* bot, uint32 spellId) const
{
    if (bot->HasSpell(spellId))
        return true;

    auto const mirrors = _mirrors.find(spellId);
    return mirrors != _mirrors.end() && std::any_of(mirrors->second.begin(), mirrors->second.end(),
                                                    [bot](uint32 mirror) { return bot->HasSpell(mirror); });
}

uint32 SubclassBotMgr::OriginalSpell(uint32 spellId) const
{
    auto const original = _originalSpells.find(spellId);
    return original != _originalSpells.end() ? original->second : spellId;
}

uint32 SubclassBotMgr::OriginalCreature(uint32 entry) const
{
    auto const original = _originalCreatures.find(entry);
    return original != _originalCreatures.end() ? original->second : entry;
}
