/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOTS_SUBCLASS_BOT_MGR_H
#define _PLAYERBOTS_SUBCLASS_BOT_MGR_H

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "Define.h"
#include "SharedDefines.h"

class Player;

// Sub-class paths of the solo-wow core fork (its subclass_* tables; the core puts bots on paths). A path without its
// own talent tree mirrors class spells, and its bots play the class AI: the class spell names and ids the AI asks for
// map to the mirrors the bot knows. A path with its own tree has its own strategy (src/Ai/Subclass) instead. Loaded
// once on the world thread at startup and read-only afterwards.
class SubclassBotMgr
{
public:
    struct Alias
    {
        std::string name;
        std::wstring lowerName;
        std::vector<uint32> spells;  // the mirror's ranks; a bot that knows any of them is on the path
    };

    static SubclassBotMgr& instance();
    void Load();
    // The mirror that stands in for the class spell `lowerName` (lowercase) for this bot, or nullptr.
    Alias const* FindAlias(Player* bot, std::wstring const& lowerName) const;
    // True when the bot knows `spellId` or a mirror of it.
    bool HasSpellOrMirror(Player* bot, uint32 spellId) const;
    // The class spell or demon a mirror stands in for, or the id itself.
    uint32 OriginalSpell(uint32 spellId) const;
    uint32 OriginalCreature(uint32 entry) const;

private:
    void LoadSwaps();

    std::array<std::unordered_map<std::wstring, std::vector<Alias>>, MAX_CLASSES> _aliases;
    std::unordered_map<uint32, std::vector<uint32>> _mirrors;
    std::unordered_map<uint32, uint32> _originalSpells;
    std::unordered_map<uint32, uint32> _originalCreatures;
};

#endif
