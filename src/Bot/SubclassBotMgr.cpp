/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SubclassBotMgr.h"

#include <algorithm>

#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Util.h"

namespace
{
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
    if (!WorldDatabase.Query("SHOW TABLES LIKE 'subclass_path'"))
        return;

    LoadSwaps();
}

void SubclassBotMgr::LoadSwaps()
{
    uint32 aliases = 0;
    uint32 missing = 0;
    QueryResult result = WorldDatabase.Query(
        "SELECT p.class, s.spell, s.replacement FROM subclass_spell_swap s "
        "JOIN subclass_path p ON p.id = s.path_id WHERE p.talent_tab = 0");
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

    if (QueryResult creatures = WorldDatabase.Query("SELECT entry, replacement FROM subclass_creature_swap"))
    {
        do
        {
            Field* fields = creatures->Fetch();
            _originalCreatures[fields[1].Get<uint32>()] = fields[0].Get<uint32>();
        } while (creatures->NextRow());
    }

    LOG_INFO("playerbots", "Loaded {} sub-class spell names for bots", aliases);
    if (missing)
        LOG_WARN("playerbots", "{} sub-class mirror spells are missing from the spell data, so bots skip them",
                 missing);
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
