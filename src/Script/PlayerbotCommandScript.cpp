/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <optional>
#include <sstream>

#include "BattleGroundTactics.h"
#include "Chat.h"
#include "CharacterCache.h"
#include "ChatCommandTags.h"
#include "GuildTaskMgr.h"
#include "LivingBondMgr.h"
#include "LivingWorldMgr.h"
#include "ObjectMgr.h"
#include "PerfMonitor.h"
#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"

using namespace Acore::ChatCommands;

class playerbots_commandscript : public CommandScript
{
public:
    playerbots_commandscript() : CommandScript("playerbots_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable playerbotsDebugCommandTable = {
            {"bg", HandleDebugBGCommand, SEC_GAMEMASTER, Console::Yes},
        };

        static ChatCommandTable playerbotsAccountCommandTable = {
            {"setKey", HandleSetSecurityKeyCommand, SEC_PLAYER, Console::No},
            {"link", HandleLinkAccountCommand, SEC_PLAYER, Console::No},
            {"linkedAccounts", HandleViewLinkedAccountsCommand, SEC_PLAYER, Console::No},
            {"unlink", HandleUnlinkAccountCommand, SEC_PLAYER, Console::No},
        };

        static ChatCommandTable playerbotsCommandTable = {
            {"bot", HandlePlayerbotCommand, SEC_PLAYER, Console::No},
            {"exec", HandleExecCommand, SEC_GAMEMASTER, Console::Yes},
            {"inspect", HandleInspectCommand, SEC_GAMEMASTER, Console::Yes},
            {"gtask", HandleGuildTaskCommand, SEC_GAMEMASTER, Console::Yes},
            {"pmon", HandlePerfMonCommand, SEC_GAMEMASTER, Console::Yes},
            {"rndbot", HandleRandomPlayerbotCommand, SEC_GAMEMASTER, Console::Yes},
            {"bonds", HandleBondsCommand, SEC_GAMEMASTER, Console::Yes},
            {"debug", playerbotsDebugCommandTable},
            {"account", playerbotsAccountCommandTable},
        };

        static ChatCommandTable commandTable = {
            {"playerbots", playerbotsCommandTable},
        };

        return commandTable;
    }

    static bool HandlePlayerbotCommand(ChatHandler* handler, char const* args)
    {
        return PlayerbotMgr::HandlePlayerbotMgrCommand(handler, args);
    }

    // Sends its own error so the core does not append its generic help text.
    static PlayerbotAI* GetOnlineBotAI(ChatHandler* handler, PlayerIdentifier const& target)
    {
        Player* player = target.GetConnectedPlayer();
        PlayerbotAI* botAI = player ? PlayerbotsMgr::instance().GetPlayerbotAI(player) : nullptr;
        if (!botAI)
            handler->SendErrorMessage("{} is not an online bot", target.GetName());
        return botAI;
    }

    // From the console or SOAP there is no player object to send the command, so the bot sends it to itself.
    // The bot's security check always lets a bot command itself.
    static bool HandleExecCommand(ChatHandler* handler, PlayerIdentifier target, Tail command)
    {
        PlayerbotAI* botAI = GetOnlineBotAI(handler, target);
        if (!botAI)
            return false;

        if (command.empty())
        {
            handler->SendErrorMessage("Usage: .playerbots exec <bot> <command> (state-changing commands; "
                                      "query replies go to the bot's master - use inspect)");
            return false;
        }

        Player* from = handler->GetSession() ? handler->GetSession()->GetPlayer() : botAI->GetBot();
        botAI->HandleCommand(CHAT_MSG_WHISPER, std::string(command), from);
        handler->PSendSysMessage("{}: '{}' sent", target.GetName(), std::string(command));
        return true;
    }

    static bool HandleInspectCommand(ChatHandler* handler, PlayerIdentifier target, Tail command)
    {
        PlayerbotAI* botAI = GetOnlineBotAI(handler, target);
        if (!botAI)
            return false;

        if (command.empty())
        {
            handler->SendErrorMessage("Usage: .playerbots inspect <bot> <query> (state, position, tpos, "
                                      "movement, target, hp, strategy, action, values, travel, budget)");
            return false;
        }

        std::string const result = botAI->HandleRemoteCommand(std::string(command));
        handler->PSendSysMessage("{}: {}", target.GetName(), result.empty() ? "<empty>" : result);
        return true;
    }

    static bool HandleRandomPlayerbotCommand(ChatHandler* handler, char const* args)
    {
        return RandomPlayerbotMgr::HandlePlayerbotConsoleCommand(handler, args);
    }

    static std::string Feeling(LivingWorld::Bond const& bond)
    {
        return Acore::StringFormat("{} ({})", LivingWorld::AttitudeName(LivingWorld::AttitudeFor(bond.score)),
                                   bond.score);
    }

    // .playerbots bonds test ..., with a living bot targeted, to try the Bonds tab and the bots' comments quickly:
    //   <deed> [friend]  acts the deed out between you and the bot: the bond changes as if it happened (cooldowns
    //                    skipped) and the bot says its line (the friend line with "friend")
    //   all [friend]     the bot says its line for every deed, nothing recorded
    //   forget           both sides of your bond with the bot, gone
    //   list             the deeds
    static bool HandleBondsTestCommand(ChatHandler* handler, std::string const& args)
    {
        std::istringstream in(args);
        std::string what, option;
        in >> what >> option;
        if (what.empty() || what == "list")
        {
            handler->SendSysMessage("Usage: .playerbots bonds test <deed>|all|forget [friend], with a living bot "
                                    "targeted. The bot speaks in party chat if grouped with you, else /say within 60 yd.");
            std::string toBot, byBot;
            for (uint8 d = 0; d < uint8(LivingWorld::Deed::Count); ++d)
            {
                LivingWorld::DeedInfo const& info = LivingWorld::InfoFor(LivingWorld::Deed(d));
                (info.byBot ? byBot : toBot) += " " + std::string(info.name);
            }
            handler->SendSysMessage("You to the bot:" + toBot);
            handler->SendSysMessage("The bot to you:" + byBot);
            return true;
        }
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        Player* bot = handler->getSelectedPlayer();
        if (!player || !bot || bot == player || !LivingWorldMgr::instance().IsEnabled(bot))
        {
            handler->SendSysMessage("Target a living-world bot first (in game).");
            return true;
        }
        LivingBondMgr& bonds = LivingBondMgr::instance();
        uint32 const botGuid = bot->GetGUID().GetCounter();
        uint32 const playerGuid = player->GetGUID().GetCounter();
        if (what == "forget")
        {
            bonds.Forget(playerGuid, botGuid);
            handler->SendSysMessage(Acore::StringFormat("Forgot both sides of your bond with {}.", bot->GetName()));
            return true;
        }
        if (bonds.Avoids(playerGuid, botGuid))
        {
            handler->SendSysMessage(Acore::StringFormat("You avoid {}, so it stays quiet: /bonds unavoid {} first.",
                                                        bot->GetName(), bot->GetName()));
            return true;
        }
        std::optional<bool> const friendLine = option == "friend" ? std::optional<bool>(true) : std::nullopt;
        char const* const unheard = "It couldn't be heard: group with it, or stand within 60 yards.";
        if (what == "all")
        {
            uint32 said = 0;
            for (uint8 d = 0; d < uint8(LivingWorld::Deed::Count); ++d)
                said += bonds.Comment(bot, player, LivingWorld::Deed(d), nullptr, true, friendLine.value_or(false));
            if (!said)
                handler->SendSysMessage(unheard);
            return true;
        }
        LivingWorld::Deed const deed = LivingWorld::DeedNamed(what);
        if (deed == LivingWorld::Deed::Count)
        {
            handler->SendSysMessage(Acore::StringFormat("No deed called '{}': .playerbots bonds test list", what));
            return true;
        }
        LivingWorld::DeedInfo const& info = LivingWorld::InfoFor(deed);
        Player* remembers = info.byBot ? player : bot;
        Player* about = info.byBot ? bot : player;
        if (info.event == LivingWorld::BondEvent::Ganked)  // a gank is a kill too
            bonds.Record(remembers, about, LivingWorld::BondEvent::KilledBy, true);
        bonds.Record(remembers, about, info.event, true);  // nothing for a goodbye (no bond event)
        if (deed == LivingWorld::Deed::SaidGoodbye)
        {
            bonds.SayFarewell(playerGuid);         // leaving your group in the next five minutes isn't abandoning it
            bonds.TakeFarewellReply(playerGuid);   // the targeted bot answers below, not the first one in the group
        }
        bool const said = bonds.Comment(bot, player, deed, nullptr, true, friendLine);
        handler->SendSysMessage(Acore::StringFormat("{} now thinks of you: {}. You think of it: {}.{}", bot->GetName(),
                                                    Feeling(bonds.Get(botGuid, playerGuid)),
                                                    Feeling(bonds.Get(playerGuid, botGuid)),
                                                    said ? "" : std::string(" ") + unheard));
        return true;
    }

    // .playerbots bonds <bot name>: who a living bot likes and dislikes, and why. .playerbots bonds test: see above.
    static bool HandleBondsCommand(ChatHandler* handler, char const* args)
    {
        std::string name = args ? args : "";
        if (name == "test" || name.rfind("test ", 0) == 0)
            return HandleBondsTestCommand(handler, name.substr(4));
        if (name.empty() || !normalizePlayerName(name))
        {
            handler->SendSysMessage("Usage: .playerbots bonds <bot name>, or .playerbots bonds test (with a bot targeted)");
            return true;
        }
        ObjectGuid const guid = sCharacterCache->GetCharacterGuidByName(name);
        if (!guid)
        {
            handler->SendSysMessage(Acore::StringFormat("No character named {}.", name));
            return true;
        }
        auto const bonds = LivingBondMgr::instance().Strongest(guid.GetCounter(), 5);
        if (bonds.empty())
        {
            handler->SendSysMessage(Acore::StringFormat("{} has no memories of anyone yet.", name));
            return true;
        }
        handler->SendSysMessage(Acore::StringFormat("{} remembers:", name));
        for (auto const& [other, bond] : bonds)
        {
            std::string otherName;
            if (!sCharacterCache->GetCharacterNameByGuid(ObjectGuid::Create<HighGuid::Player>(other), otherName))
                otherName = "#" + std::to_string(other);
            handler->SendSysMessage(Acore::StringFormat(
                "  {} - {} ({}): grouped {} min, {} quests, {} bosses, {} battlegrounds, {} helps, {} duels | "
                "killed by them {}, ninja-looted {}, slights {}",
                otherName, LivingWorld::AttitudeName(LivingWorld::AttitudeFor(bond.score)), bond.score, bond.minutes,
                bond.quests, bond.bosses, bond.battlegrounds, bond.helps, bond.duels, bond.kills, bond.ninjas,
                bond.slights));
        }
        return true;
    }

    static bool HandleGuildTaskCommand(ChatHandler* handler, char const* args)
    {
        return GuildTaskMgr::HandleConsoleCommand(handler, args);
    }

    static bool HandlePerfMonCommand(ChatHandler* /*handler*/, char const* args)
    {
        if (!strcmp(args, "reset"))
        {
            sPerfMonitor.Reset();
            return true;
        }

        if (!strcmp(args, "tick"))
        {
            sPerfMonitor.PrintStats(true, false);
            sPerfMonitor.DumpJson(true);
            return true;
        }

        if (!strcmp(args, "stack"))
        {
            sPerfMonitor.PrintStats(false, true);
            sPerfMonitor.DumpJson(false);
            return true;
        }

        if (!strcmp(args, "toggle"))
        {
            sPlayerbotAIConfig.PerfMonEnabled = !sPlayerbotAIConfig.PerfMonEnabled;
            if (sPlayerbotAIConfig.PerfMonEnabled)
                LOG_INFO("playerbots", "Performance monitor enabled");
            else
                LOG_INFO("playerbots", "Performance monitor disabled");
            return true;
        }

        sPerfMonitor.PrintStats();
        sPerfMonitor.DumpJson(false);
        return true;
    }

    static bool HandleDebugBGCommand(ChatHandler* handler, char const* args)
    {
        return BGTactics::HandleConsoleCommand(handler, args);
    }

    static bool HandleSetSecurityKeyCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
        {
            handler->PSendSysMessage("Usage: .playerbots account setKey <securityKey>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        std::string key = args;

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleSetSecurityKeyCommand(player, key);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleLinkAccountCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        char* accountName = strtok((char*)args, " ");
        char* key = strtok(nullptr, " ");

        if (!accountName || !key)
        {
            handler->PSendSysMessage("Usage: .playerbots account link <accountName> <securityKey>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleLinkAccountCommand(player, accountName, key);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleViewLinkedAccountsCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleViewLinkedAccountsCommand(player);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }

    static bool HandleUnlinkAccountCommand(ChatHandler* handler, char const* args)
    {
        if (!args || !*args)
            return false;

        char* accountName = strtok((char*)args, " ");
        if (!accountName)
        {
            handler->PSendSysMessage("Usage: .playerbots account unlink <accountName>");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        PlayerbotMgr* mgr = PlayerbotsMgr::instance().GetPlayerbotMgr(player);
        if (mgr)
        {
            mgr->HandleUnlinkAccountCommand(player, accountName);
            return true;
        }
        else
        {
            handler->PSendSysMessage("PlayerbotMgr instance not found.");
            return false;
        }
    }
};

void AddPlayerbotsCommandscripts() { new playerbots_commandscript(); }
