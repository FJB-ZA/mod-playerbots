/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Reads "oom" and "ready" from party chat, and decides when a bot healer calls oom or says it is drinking.

#ifndef _PLAYERBOTS_LEAD_CHAT_POLICY_H
#define _PLAYERBOTS_LEAD_CHAT_POLICY_H

#include <cstdint>
#include <iterator>
#include <string_view>

namespace DungeonLead
{
enum class Signal : uint8_t
{
    None,
    Oom,
    Ready
};

constexpr char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

constexpr bool IsWordChar(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }

constexpr bool SameWord(std::string_view word, std::string_view lower)
{
    if (word.size() != lower.size())
        return false;
    for (size_t i = 0; i < word.size(); ++i)
        if (Lower(word[i]) != lower[i])
            return false;
    return true;
}

constexpr bool IsAny(std::string_view word, std::string_view const* list, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (SameWord(word, list[i]))
            return true;
    return false;
}

// "oom" and "drinking" always mean wait; "mana" means wait unless a ready word says it is back ("full mana").
// A bare "drink" is the bot command, not a call.
constexpr Signal Parse(std::string_view text)
{
    constexpr std::string_view oomWords[] = {"oom", "omm", "drinking", "drinkin"};
    constexpr std::string_view readyWords[] = {"ready", "rdy", "full", "ok", "go"};
    bool oom = false;
    bool mana = false;
    bool ready = false;
    size_t i = 0;
    while (i < text.size())
    {
        while (i < text.size() && !IsWordChar(text[i]))
            ++i;
        size_t start = i;
        while (i < text.size() && IsWordChar(text[i]))
            ++i;
        std::string_view const word = text.substr(start, i - start);
        if (word.empty())
            continue;
        if (IsAny(word, oomWords, std::size(oomWords)))
            oom = true;
        else if (SameWord(word, "mana"))
            mana = true;
        else if (IsAny(word, readyWords, std::size(readyWords)))
            ready = true;
    }
    if (oom || (mana && !ready))
        return Signal::Oom;
    return ready ? Signal::Ready : Signal::None;
}

enum class Call : uint8_t
{
    None,
    Oom,
    Drinking
};

struct CallState
{
    bool oom = false;
    bool drinking = false;
};

constexpr float OomCallPct = 20.0f;
// A top-up sip is not worth announcing.
constexpr float DrinkCallPct = 70.0f;
constexpr float CallRearmPct = 95.0f;

// One "oom" per low spell and one "drinking" per sit; both re-arm once the healer is nearly full.
constexpr Call NextCall(CallState& state, float manaPct, bool drinking)
{
    if (manaPct >= CallRearmPct)
    {
        state = {};
        return Call::None;
    }
    if (drinking)
    {
        if (state.drinking || manaPct >= DrinkCallPct)
            return Call::None;
        state.drinking = true;
        return Call::Drinking;
    }
    state.drinking = false;
    if (manaPct < OomCallPct && !state.oom)
    {
        state.oom = true;
        return Call::Oom;
    }
    return Call::None;
}
}  // namespace DungeonLead

#endif
