/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef _PLAYERBOT_LIVING_BOND_COMMENTS_H
#define _PLAYERBOT_LIVING_BOND_COMMENTS_H

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "LivingBondPolicy.h"

// What a living bot says when a player wrongs it or is good to it, and when it does the same to a player: one line,
// in party chat when they share a group, or aloud when near (LivingBondScripts.cpp). Pure, so tests check it.
namespace LivingWorld
{
enum class Deed : uint8_t
{
    // The player wronged the bot, which says so
    NinjaArmour,     // needed gear that isn't their armour type
    NinjaUnusable,   // needed gear they can't use
    KilledBot,       // killed it in world PvP
    GankedBot,       // killed it again soon after, or far above its level
    KickedBot,       // kicked it from the group
    AbandonedBot,    // left it in a dungeon
    RudeToBot,       // a rude emote at it
    // The bot wronged the player, and comments
    BotNinjaed,      // needed gear the player rolled on that isn't its to need
    BotKilled,
    BotGanked,
    BotKicked,
    BotAbandoned,    // left the player's dungeon group
    // The player was good to the bot, which thanks them
    RezzedBot,
    HealedBot,       // from outside its group
    BuffedBot,       // from outside its group
    GiftToBot,
    FriendlyToBot,   // a friendly emote at it
    LostDuelToYou,   // the player won a duel against it
    // The bot was good to the player, and says so
    BotRezzed,
    BotHealed,
    BotBuffed,
    BotGift,
    WonDuelAgainstYou,
    Count
};

inline constexpr uint32_t CommentCooldown = 60;  // seconds between one bot's comments about one player

// Each deed for the GM test command (.playerbots bonds test <name>): its name, the bond event it is, and whether the
// bot did it (so the player remembers it) or the player did (so the bot does)
struct DeedInfo
{
    std::string_view name;
    BondEvent event;
    bool byBot;
};

inline DeedInfo const& InfoFor(Deed deed)
{
    static std::array<DeedInfo, size_t(Deed::Count)> const all = {{
        {"ninja", BondEvent::Ninja, false},
        {"ninja-unusable", BondEvent::Ninja, false},
        {"kill", BondEvent::KilledBy, false},
        {"gank", BondEvent::Ganked, false},
        {"kick", BondEvent::Kicked, false},
        {"abandon", BondEvent::Abandoned, false},
        {"rude", BondEvent::RudeEmote, false},
        {"bot-ninja", BondEvent::Ninja, true},
        {"bot-kill", BondEvent::KilledBy, true},
        {"bot-gank", BondEvent::Ganked, true},
        {"bot-kick", BondEvent::Kicked, true},
        {"bot-abandon", BondEvent::Abandoned, true},
        {"rez", BondEvent::Resurrected, false},
        {"heal", BondEvent::Healed, false},
        {"buff", BondEvent::Buffed, false},
        {"gift", BondEvent::Gift, false},
        {"wave", BondEvent::FriendlyEmote, false},
        {"duel-won", BondEvent::Duel, false},
        {"bot-rez", BondEvent::Resurrected, true},
        {"bot-heal", BondEvent::Healed, true},
        {"bot-buff", BondEvent::Buffed, true},
        {"bot-gift", BondEvent::Gift, true},
        {"duel-lost", BondEvent::Duel, true},
    }};
    return all[size_t(deed)];
}

// The deed with this name; Deed::Count when there's none
inline Deed DeedNamed(std::string_view name)
{
    for (uint8_t d = 0; d < uint8_t(Deed::Count); ++d)
        if (InfoFor(Deed(d)).name == name)
            return Deed(d);
    return Deed::Count;
}

namespace Detail
{
using Lines = std::array<std::string_view, 3>;
struct DeedLines
{
    Lines lines;
    std::string_view friendLine;  // when the bot counts the player as friendly or a friend
};

// "%" is the player's name
inline DeedLines const& LinesFor(Deed deed)
{
    static std::array<DeedLines, size_t(Deed::Count)> const all = {{
        {{"Really, %? That's not even your armour type.", "Need on that, %? It's not your armour type.",
          "Nice ninja, %. That isn't even your armour."},
         "Come on, %, that's not your armour type. I wanted that."},
        {{"Really, %? You can't even use that.", "%, you can't even equip that. Why Need?",
          "Needing on something you can't use, %? Classy."},
         "%, you can't even use that. Why take it?"},
        {{"Was that really necessary, %?", "I'll remember that, %.", "Enjoy it while it lasts, %."},
         "%! I thought we were friends!"},
        {{"Again, %? Pick on someone your own size.", "Seriously, %? Camping my corpse now?",
          "%, have you nothing better to do?"},
         "%, what did I ever do to you?"},
        {{"Fine, %. Good luck without me.", "Kicked? Really, %?", "Your loss, %."},
         "%... I thought we were a team."},
        {{"% just left us mid-dungeon. Great.", "And there goes %. Thanks for nothing.",
          "Well, % bailed. Anyone else want to leave?"},
         "% had to go? Shame, we were doing well."},
        {{"Charming, %.", "Same to you, %.", "Rude, %."}, "Ouch, %. What was that for?"},
        {{"Sorry %, I needed it more.", "Finders keepers, %.", "It'll look better on me, %."},
         "Sorry %, I really did need that. Next one's yours."},
        {{"Nothing personal, %.", "Better luck next time, %.", "That's how it goes out here, %."},
         "Sorry %, got carried away there."},
        {{"Should have stayed in town, %.", "Back for more, %?", "Stay down, %."},
         "Sorry %, wrong place, wrong time. Again."},
        {{"Sorry %, it wasn't working out.", "Bye, %.", "We'll manage without you, %."},
         "Sorry %, we needed the spot. Nothing personal."},
        {{"Sorry %, I have to go.", "I'm out. Good luck, %.", "Something came up, sorry %."},
         "Sorry %, I really have to go. Next time!"},
        {{"Thanks for the rez, %!", "I owe you one, %.", "Back on my feet. Thanks, %."},
         "%, you're a lifesaver. Literally."},
        {{"Thanks for the heal, %.", "Appreciate the heal, %!", "That was close. Thanks, %."},
         "Thanks, %. Always got my back."},
        {{"Thanks for the buff, %!", "Oh, nice buff. Thanks, %.", "Much appreciated, %."},
         "Thanks, %! You're the best."},
        {{"For me? Thanks, %!", "That's kind of you, %.", "Thanks, %. I'll put it to good use."},
         "%, you shouldn't have. Thank you!"},
        {{"Hello to you too, %.", "Ha, likewise, %.", "Good to see you, %."}, "Always a pleasure, %!"},
        {{"Good fight, %.", "Well fought, %. You got me.", "Nice one, %. Rematch sometime?"},
         "Good fight, %! You're getting better at this."},
        {{"Up you get, %.", "Can't leave you lying there, %.", "On your feet, %."},
         "Up you get, %. Can't lose you now."},
        {{"Careful out there, %.", "Patched you up, %.", "Watch yourself, %."}, "I've got you, %."},
        {{"Here, %, have a buff.", "That should help, %.", "Stay safe, %."}, "A little something for you, %."},
        {{"Here, %, take this.", "You can have this, %.", "Thought you could use this, %."},
         "Here, %, for you. Don't mention it."},
        {{"Good fight, %.", "Better luck next time, %.", "Close one, %."}, "Good fight, %! Rematch soon?"},
    }};
    return all[size_t(deed)];
}
}  // namespace Detail

// The line for `deed` about `name`: the friend line when the bot thinks well of the player, else one of three picked
// by `seed`. Empty for an unknown deed.
inline std::string DeedComment(Deed deed, std::string_view name, bool friends, uint32_t seed)
{
    if (deed >= Deed::Count)
        return {};
    Detail::DeedLines const& lines = Detail::LinesFor(deed);
    std::string_view const line = friends ? lines.friendLine : lines.lines[seed % lines.lines.size()];
    std::string out;
    for (char c : line)
    {
        if (c == '%')
            out += name;
        else
            out += c;
    }
    return out;
}
}  // namespace LivingWorld

#endif
