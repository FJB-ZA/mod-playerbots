/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_LIVING_BOND_PROTOCOL_H
#define _PLAYERBOT_LIVING_BOND_PROTOCOL_H

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

#include "LivingBondPolicy.h"

// The SoloWowBonds addon's hidden whispers (addons/SoloWowBonds in solo-wow). The player whispers themself
// "SoloWowBonds\t<body>"; the server answers the same way.
//   client: "Q"                    list what the bots think of me, and what I think of them
//           "A\t<guid>\t<1|0>"      avoid (1) or stop avoiding (0) a bot
//   server: "B\t<rows>"            a list starts
//           "R\t<row fields>"      one bot (FormatRow)
//           "E"                    the list ends
//           "S\t<guid>\t<1|0>"      an avoid change was saved
namespace LivingWorld::BondsProtocol
{
inline constexpr std::string_view Prefix = "SoloWowBonds";
// Rows sent per list: the strongest feelings first, then avoided bots without a bond. One addon message per row.
inline constexpr size_t MaxRows = 200;

enum class Request : uint8_t
{
    None,
    Query,
    Avoid
};

struct ParsedRequest
{
    Request kind = Request::None;
    uint32_t guid = 0;
    bool avoid = false;
};

inline ParsedRequest Parse(std::string_view body)
{
    ParsedRequest parsed;
    if (body == "Q")
    {
        parsed.kind = Request::Query;
        return parsed;
    }
    if (body.size() < 5 || body.substr(0, 2) != "A\t")
        return parsed;
    body.remove_prefix(2);
    size_t const tab = body.find('\t');
    if (tab == std::string_view::npos || tab + 2 != body.size())
        return parsed;
    std::string_view const guid = body.substr(0, tab);
    char const flag = body[tab + 1];
    uint32_t value = 0;
    auto const [end, error] = std::from_chars(guid.data(), guid.data() + guid.size(), value);
    if (error != std::errc() || end != guid.data() + guid.size() || !value || (flag != '0' && flag != '1'))
        return parsed;
    parsed.kind = Request::Avoid;
    parsed.guid = value;
    parsed.avoid = flag == '1';
    return parsed;
}

struct Row
{
    uint32_t guid = 0;
    std::string name;
    uint8_t classId = 0;
    uint8_t race = 0;
    uint8_t gender = 0;
    uint8_t level = 0;
    Bond bond;
    uint32_t secondsAgo = 0;  // since the bot last counted anything with the player; 0 when never
    bool avoided = false;
    bool online = false;
    std::string zone;         // where the bot is, when online
    Bond mine;                // the player's side: what the bot has done to them
};

// Names and zones never hold tabs; a stray one is cut so the row's fields stay in place.
inline std::string Clean(std::string_view text)
{
    std::string out(text.substr(0, 48));
    for (char& c : out)
        if (c == '\t' || c == '\n' || c == '\r')
            c = ' ';
    return out;
}

// R guid name class race gender level attitude score quests bosses battlegrounds minutes helps duels kills ninjas
//   slights ago avoided online zone, then the player's side: myAttitude myScore myHelps myDuels myKills myNinjas
//   mySlights (after the zone, so an older addon reads the rest as before)
inline std::string FormatRow(Row const& row)
{
    std::string out = "R";
    auto add = [&out](auto value)
    {
        out += '\t';
        out += std::to_string(value);
    };
    add(row.guid);
    out += '\t';
    out += Clean(row.name);
    add(unsigned(row.classId));
    add(unsigned(row.race));
    add(unsigned(row.gender));
    add(unsigned(row.level));
    add(unsigned(AttitudeFor(row.bond.score)));
    add(int(row.bond.score));
    add(row.bond.quests);
    add(row.bond.bosses);
    add(row.bond.battlegrounds);
    add(row.bond.minutes);
    add(row.bond.helps);
    add(row.bond.duels);
    add(row.bond.kills);
    add(row.bond.ninjas);
    add(row.bond.slights);
    add(row.secondsAgo);
    add(row.avoided ? 1 : 0);
    add(row.online ? 1 : 0);
    out += '\t';
    out += Clean(row.zone);
    add(unsigned(AttitudeFor(row.mine.score)));
    add(int(row.mine.score));
    add(row.mine.helps);
    add(row.mine.duels);
    add(row.mine.kills);
    add(row.mine.ninjas);
    add(row.mine.slights);
    return out;
}

inline std::string FormatBegin(size_t rows) { return "B\t" + std::to_string(rows); }
inline std::string FormatSaved(uint32_t guid, bool avoided) { return "S\t" + std::to_string(guid) + (avoided ? "\t1" : "\t0"); }
}  // namespace LivingWorld::BondsProtocol

#endif
