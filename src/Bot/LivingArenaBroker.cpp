// Dalaran arena partner broker: register a team, recruit bots by role and PvP spec, gather, respec and release them.

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "Creature.h"
#include "Group.h"
#include "LivingArenaMgr.h"
#include "LivingBondMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace
{
uint32 constexpr TEXT_GREETING = 9101001;
uint32 constexpr TEXT_BRACKET = 9101002;
uint32 constexpr TEXT_ROLE = 9101003;
uint32 constexpr TEXT_CANDIDATES = 9101004;
uint32 constexpr TEXT_INFO = 9101005;
uint32 constexpr TEXT_CLOSED = 9101006;

enum BrokerSender : uint32
{
    SENDER_MAIN = 1,
    SENDER_BRACKET,
    SENDER_ROLE,
    SENDER_SPEC,
    SENDER_PICK,
    SENDER_RESPEC_WHO,
    SENDER_RESPEC_SPEC,
    SENDER_RELEASE,
    SENDER_REGISTER
};

enum BrokerMain : uint32
{
    MAIN_RECRUIT = 1,
    MAIN_GATHER,
    MAIN_RESPEC,
    MAIN_RELEASE,
    MAIN_INFO,
    MAIN_BACK
};

struct SpecChoice
{
    uint8 role;
    uint8 cls;
    char const* spec;
    char const* label;
};

// Roles: 0 healer, 1 melee, 2 ranged physical, 3 caster.
std::vector<SpecChoice> const& SpecChoices()
{
    static std::vector<SpecChoice> const choices = {
        {0, CLASS_PRIEST, "disc pvp", "Discipline Priest"},
        {0, CLASS_PRIEST, "holy pvp", "Holy Priest"},
        {0, CLASS_PALADIN, "holy pvp", "Holy Paladin"},
        {0, CLASS_SHAMAN, "resto pvp", "Restoration Shaman"},
        {0, CLASS_DRUID, "resto pvp", "Restoration Druid"},
        {1, CLASS_WARRIOR, "arms pvp", "Arms Warrior"},
        {1, CLASS_WARRIOR, "fury pvp", "Fury Warrior"},
        {1, CLASS_PALADIN, "ret pvp", "Retribution Paladin"},
        {1, CLASS_ROGUE, "subtlety pvp", "Subtlety Rogue"},
        {1, CLASS_ROGUE, "combat pvp", "Combat Rogue"},
        {1, CLASS_ROGUE, "as pvp", "Assassination Rogue"},
        {1, CLASS_DEATH_KNIGHT, "unholy pvp", "Unholy Death Knight"},
        {1, CLASS_DEATH_KNIGHT, "frost pvp", "Frost Death Knight"},
        {1, CLASS_DEATH_KNIGHT, "blood pvp", "Blood Death Knight"},
        {1, CLASS_SHAMAN, "enh pvp", "Enhancement Shaman"},
        {1, CLASS_DRUID, "cat pvp", "Feral Druid"},
        {2, CLASS_HUNTER, "bm pvp", "Beast Mastery Hunter"},
        {2, CLASS_HUNTER, "mm pvp", "Marksmanship Hunter"},
        {2, CLASS_HUNTER, "surv pvp", "Survival Hunter"},
        {3, CLASS_MAGE, "frost pvp", "Frost Mage"},
        {3, CLASS_MAGE, "fire pvp", "Fire Mage"},
        {3, CLASS_MAGE, "arcane pvp", "Arcane Mage"},
        {3, CLASS_WARLOCK, "affli pvp", "Affliction Warlock"},
        {3, CLASS_WARLOCK, "demo pvp", "Demonology Warlock"},
        {3, CLASS_WARLOCK, "destro pvp", "Destruction Warlock"},
        {3, CLASS_PRIEST, "shadow pvp", "Shadow Priest"},
        {3, CLASS_SHAMAN, "ele pvp", "Elemental Shaman"},
        {3, CLASS_DRUID, "balance pvp", "Balance Druid"},
    };
    return choices;
}

char const* LabelFor(uint8 cls, int specNo)
{
    for (SpecChoice const& choice : SpecChoices())
    {
        if (choice.cls == cls && LivingArenaMgr::FindSpecNo(cls, choice.spec) == specNo)
            return choice.label;
    }
    return nullptr;
}

char const* Reputation(uint32 skill)
{
    switch (LivingArena::TierFor(skill))
    {
        case LivingArena::Tier::Expert:
            return "renowned";
        case LivingArena::Tier::Skilled:
            return "seasoned";
        case LivingArena::Tier::Regular:
            return "capable";
        default:
            return "green";
    }
}

struct BrokerState
{
    uint8 type = 0;
    uint8 cls = 0;
    int specNo = -1;
    std::vector<ObjectGuid> candidates;
    std::vector<ObjectGuid> partners;
    ObjectGuid respec;
};

std::mutex brokerMutex;
std::unordered_map<ObjectGuid::LowType, BrokerState> brokerStates;

BrokerState GetState(Player* player)
{
    std::lock_guard<std::mutex> lock(brokerMutex);
    return brokerStates[player->GetGUID().GetCounter()];
}

void SetState(Player* player, BrokerState const& state)
{
    std::lock_guard<std::mutex> lock(brokerMutex);
    brokerStates[player->GetGUID().GetCounter()] = state;
}

void Tell(Player* player, std::string const& text) { ChatHandler(player->GetSession()).SendSysMessage(text); }

uint32 CharterCost(uint8 type)
{
    switch (type)
    {
        case 2:
            return sWorld->getIntConfig(CONFIG_CHARTER_COST_ARENA_2v2);
        case 3:
            return sWorld->getIntConfig(CONFIG_CHARTER_COST_ARENA_3v3);
        default:
            return sWorld->getIntConfig(CONFIG_CHARTER_COST_ARENA_5v5);
    }
}

void Invite(Player* player, Player* bot)
{
    if (player->GetGroup() && player->GetGroup()->IsMember(bot->GetGUID()))
        return;
    WorldPacket packet(CMSG_GROUP_INVITE, 10);
    packet << bot->GetName();
    packet << uint32(0);
    player->GetSession()->HandleGroupInviteOpcode(packet);
}

std::string NameOf(ObjectGuid guid)
{
    std::string name;
    if (!sCharacterCache->GetCharacterNameByGuid(guid, name))
        name = "someone";
    return name;
}
}  // namespace

class npc_living_arena_broker : public CreatureScript
{
public:
    npc_living_arena_broker() : CreatureScript("npc_living_arena_broker") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        LivingArenaMgr& arena = LivingArenaMgr::instance();
        if (!arena.IsEnabled() || !arena.GetArenaLevel() || player->GetLevel() < arena.GetArenaLevel())
        {
            SendGossipMenuFor(player, TEXT_CLOSED, creature->GetGUID());
            return true;
        }
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "I'm looking for an arena partner.", SENDER_MAIN, MAIN_RECRUIT);
        if (!arena.GetPartners(player).empty())
        {
            AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Bring my arena partners to me.", SENDER_MAIN, MAIN_GATHER);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Change a partner's specialization.", SENDER_MAIN,
                             MAIN_RESPEC);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Release a partner from my team.", SENDER_MAIN, MAIN_RELEASE);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "How does this work?", SENDER_MAIN, MAIN_INFO);
        SendGossipMenuFor(player, TEXT_GREETING, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        ClearGossipMenuFor(player);
        BrokerState state = GetState(player);
        switch (sender)
        {
            case SENDER_MAIN:
                return Main(player, creature, state, action);
            case SENDER_BRACKET:
                state.type = uint8(action);
                SetState(player, state);
            {
                ArenaTeam* team =
                    sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ArenaTeam::GetSlotByType(state.type)));
                if (!team)
                    return ShowRegister(player, creature, state.type);
                if (team->GetCaptain() != player->GetGUID())
                {
                    Tell(player, "Only the captain of <" + team->GetName() + "> can sign up new members.");
                    CloseGossipMenuFor(player);
                    return true;
                }
                return ShowRoles(player, creature);
            }
            case SENDER_ROLE:
                return ShowSpecs(player, creature, uint8(action));
            case SENDER_SPEC:
                return ShowCandidates(player, creature, state, uint8(action / 32), int(action % 32));
            case SENDER_PICK:
                return Recruit(player, creature, state, action);
            case SENDER_RESPEC_WHO:
                return ShowRespecSpecs(player, creature, state, action);
            case SENDER_RESPEC_SPEC:
                return Respec(player, creature, state, int(action));
            case SENDER_RELEASE:
                return Release(player, creature, state, action);
            default:
                CloseGossipMenuFor(player);
                return true;
        }
    }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, char const* code) override
    {
        ClearGossipMenuFor(player);
        if (sender != SENDER_REGISTER || !code)
        {
            CloseGossipMenuFor(player);
            return true;
        }
        uint8 const type = uint8(action);
        std::string const name = code;
        uint32 const cost = CharterCost(type);
        if (player->GetArenaTeamId(ArenaTeam::GetSlotByType(type)))
            return ShowRoles(player, creature);
        if (!ObjectMgr::IsValidCharterName(name) || sArenaTeamMgr->GetArenaTeamByName(name))
        {
            Tell(player, "That team name is taken or not allowed.");
            return ShowRegister(player, creature, type);
        }
        if (!player->HasEnoughMoney(cost))
        {
            Tell(player, "You can't afford the registration fee.");
            CloseGossipMenuFor(player);
            return true;
        }
        ArenaTeam* team = new ArenaTeam();
        if (!team->Create(player->GetGUID(), type, name, 0xFF000000 | urand(0, 0xFFFFFF), urand(0, 101),
                          0xFF000000 | urand(0, 0xFFFFFF), urand(0, 5), 0xFF000000 | urand(0, 0xFFFFFF)))
        {
            delete team;
            Tell(player, "The arena registrar refused that team.");
            CloseGossipMenuFor(player);
            return true;
        }
        sArenaTeamMgr->AddArenaTeam(team);
        player->ModifyMoney(-int32(cost));
        Tell(player, "Registered <" + name + ">.");
        BrokerState state = GetState(player);
        state.type = type;
        SetState(player, state);
        return ShowRoles(player, creature);
    }

private:
    bool Main(Player* player, Creature* creature, BrokerState& state, uint32 action)
    {
        LivingArenaMgr& arena = LivingArenaMgr::instance();
        switch (action)
        {
            case MAIN_RECRUIT:
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "2v2", SENDER_BRACKET, 2);
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "3v3", SENDER_BRACKET, 3);
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "5v5", SENDER_BRACKET, 5);
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", SENDER_MAIN, MAIN_BACK);
                SendGossipMenuFor(player, TEXT_BRACKET, creature->GetGUID());
                return true;
            case MAIN_GATHER:
                Gather(player);
                CloseGossipMenuFor(player);
                return true;
            case MAIN_RESPEC:
            case MAIN_RELEASE:
            {
                state.partners = arena.GetPartners(player);
                SetState(player, state);
                uint32 const sender = action == MAIN_RESPEC ? SENDER_RESPEC_WHO : SENDER_RELEASE;
                for (uint32 i = 0; i < state.partners.size(); ++i)
                {
                    std::string label = NameOf(state.partners[i]);
                    if (action == MAIN_RESPEC && !ObjectAccessor::FindConnectedPlayer(state.partners[i]))
                        label += " (not here)";
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, sender, i);
                }
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", SENDER_MAIN, MAIN_BACK);
                SendGossipMenuFor(player, TEXT_GREETING, creature->GetGUID());
                return true;
            }
            case MAIN_INFO:
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", SENDER_MAIN, MAIN_BACK);
                SendGossipMenuFor(player, TEXT_INFO, creature->GetGUID());
                return true;
            default:
                return OnGossipHello(player, creature);
        }
    }

    bool ShowRegister(Player* player, Creature* creature, uint8 type)
    {
        std::string const label = "Register a " + std::to_string(type) + "v" + std::to_string(type) + " team.";
        AddGossipItemFor(player, GOSSIP_ICON_TABARD, label, SENDER_REGISTER, type, "Name your team:", CharterCost(type),
                         true);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", SENDER_MAIN, MAIN_RECRUIT);
        SendGossipMenuFor(player, TEXT_BRACKET, creature->GetGUID());
        return true;
    }

    bool ShowRoles(Player* player, Creature* creature)
    {
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "A healer.", SENDER_ROLE, 0);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "A melee fighter.", SENDER_ROLE, 1);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "A hunter.", SENDER_ROLE, 2);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "A caster.", SENDER_ROLE, 3);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", SENDER_MAIN, MAIN_RECRUIT);
        SendGossipMenuFor(player, TEXT_ROLE, creature->GetGUID());
        return true;
    }

    bool ShowSpecs(Player* player, Creature* creature, uint8 role)
    {
        for (SpecChoice const& choice : SpecChoices())
        {
            int const specNo = LivingArenaMgr::FindSpecNo(choice.cls, choice.spec);
            if (choice.role == role && specNo >= 0)
                AddGossipItemFor(player, GOSSIP_ICON_DOT, choice.label, SENDER_SPEC, choice.cls * 32 + specNo);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", SENDER_BRACKET, GetState(player).type);
        SendGossipMenuFor(player, TEXT_ROLE, creature->GetGUID());
        return true;
    }

    bool ShowCandidates(Player* player, Creature* creature, BrokerState& state, uint8 cls, int specNo)
    {
        LivingArenaMgr& arena = LivingArenaMgr::instance();
        std::vector<Player*> const candidates = arena.FindPartnerCandidates(player, state.type, cls, 3);
        state.cls = cls;
        state.specNo = specNo;
        state.candidates.clear();
        for (Player* bot : candidates)
        {
            state.candidates.push_back(bot->GetGUID());
            LivingWorld::Attitude const attitude =
                LivingBondMgr::instance().AttitudeOf(bot->GetGUID().GetCounter(), player->GetGUID().GetCounter());
            std::string label = bot->GetName() + " - gear " + std::to_string(LivingArenaMgr::GetGearScore(bot)) +
                                ", " + Reputation(arena.GetSkill(bot));
            if (attitude == LivingWorld::Attitude::Friend)
                label += ", your friend";
            else if (attitude == LivingWorld::Attitude::Friendly)
                label += ", knows you";
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, SENDER_PICK, uint32(state.candidates.size() - 1));
        }
        SetState(player, state);
        if (candidates.empty())
            Tell(player, "Nobody of that class is looking for a team right now. Try again later.");
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", SENDER_BRACKET, state.type);
        SendGossipMenuFor(player, TEXT_CANDIDATES, creature->GetGUID());
        return true;
    }

    bool Recruit(Player* player, Creature* /*creature*/, BrokerState& state, uint32 index)
    {
        CloseGossipMenuFor(player);
        if (index >= state.candidates.size())
            return true;
        Player* bot = ObjectAccessor::FindConnectedPlayer(state.candidates[index]);
        ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ArenaTeam::GetSlotByType(state.type)));
        if (team && team->GetCaptain() != player->GetGUID())
        {
            Tell(player, "Only the captain of <" + team->GetName() + "> can sign up new members.");
            return true;
        }
        if (!bot || !team || bot->GetArenaTeamId(team->GetSlot()) || bot->GetGroup())
        {
            Tell(player, "They've moved on. Ask me again.");
            return true;
        }
        if (team->GetMembersSize() >= team->GetType() * 2)
        {
            Tell(player, "<" + team->GetName() + "> has no room for another member.");
            return true;
        }
        if (!team->AddMember(bot->GetGUID()))
        {
            Tell(player, bot->GetName() + " couldn't join <" + team->GetName() + ">.");
            return true;
        }
        LivingArenaMgr::instance().ApplySpec(bot, state.specNo);
        Invite(player, bot);
        char const* label = LabelFor(state.cls, state.specNo);
        Tell(player, bot->GetName() + " joins <" + team->GetName() + ">" + (label ? std::string(" as ") + label : "") + ".");
        return true;
    }

    void Gather(Player* player)
    {
        for (ObjectGuid const& guid : LivingArenaMgr::instance().GetPartners(player))
        {
            if (Player* bot = ObjectAccessor::FindConnectedPlayer(guid))
            {
                Invite(player, bot);
                Tell(player, bot->GetName() + " is on the way.");
                continue;
            }
            uint32 const accountId = sCharacterCache->GetCharacterAccountIdByGuid(guid);
            if (accountId == player->GetSession()->GetAccountId())
            {
                if (PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(player))
                    mgr->AddPlayerBot(guid, accountId);
            }
            else if (sPlayerbotAIConfig.IsInRandomAccountList(accountId))
                sRandomPlayerbotMgr.AddPlayerBot(guid, 0);
            Tell(player, NameOf(guid) + " is being sent for. Ask me again in a moment.");
        }
    }

    bool ShowRespecSpecs(Player* player, Creature* creature, BrokerState& state, uint32 index)
    {
        Player* bot = index < state.partners.size() ? ObjectAccessor::FindConnectedPlayer(state.partners[index]) : nullptr;
        if (!bot)
        {
            Tell(player, "Bring them here first.");
            CloseGossipMenuFor(player);
            return true;
        }
        state.respec = bot->GetGUID();
        SetState(player, state);
        for (SpecChoice const& choice : SpecChoices())
        {
            int const specNo = LivingArenaMgr::FindSpecNo(choice.cls, choice.spec);
            if (choice.cls == bot->getClass() && specNo >= 0)
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER, choice.label, SENDER_RESPEC_SPEC, specNo);
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Back.", SENDER_MAIN, MAIN_RESPEC);
        SendGossipMenuFor(player, TEXT_ROLE, creature->GetGUID());
        return true;
    }

    bool Respec(Player* player, Creature* /*creature*/, BrokerState& state, int specNo)
    {
        CloseGossipMenuFor(player);
        // The bot may have left the team since the menu opened, so check it is still a partner.
        std::vector<ObjectGuid> const partners = LivingArenaMgr::instance().GetPartners(player);
        if (std::find(partners.begin(), partners.end(), state.respec) == partners.end())
        {
            Tell(player, "They aren't on a team you captain any more.");
            return true;
        }
        Player* bot = ObjectAccessor::FindConnectedPlayer(state.respec);
        if (!bot || !LivingArenaMgr::IsPvpSpec(bot->getClass(), specNo) || bot->InBattleground())
            return true;
        LivingArenaMgr::instance().ApplySpec(bot, specNo);
        char const* label = LabelFor(bot->getClass(), specNo);
        Tell(player, bot->GetName() + " is now " + (label ? label : "respecced") + ".");
        return true;
    }

    bool Release(Player* player, Creature* /*creature*/, BrokerState& state, uint32 index)
    {
        CloseGossipMenuFor(player);
        if (index >= state.partners.size())
            return true;
        ObjectGuid const guid = state.partners[index];
        std::vector<ArenaTeam*> teams;
        for (uint8 slot = 0; slot < MAX_ARENA_SLOT; ++slot)
        {
            ArenaTeam* team = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(slot));
            if (team && team->GetCaptain() == player->GetGUID() && team->IsMember(guid) && team->GetCaptain() != guid)
                teams.push_back(team);
        }
        for (ArenaTeam* team : teams)
        {
            if (LivingArenaMgr::instance().IsRosterLocked(team, guid))
            {
                Tell(player, "<" + team->GetName() + "> is in a match or has one waiting. Ask me again after it.");
                return true;
            }
        }
        bool const released = !teams.empty();
        for (ArenaTeam* team : teams)
            team->DelMember(guid, true);
        if (!released)
        {
            Tell(player, NameOf(guid) + " isn't on a team you captain.");
            return true;
        }
        // Through the normal kick so party leadership and dungeon finder rules still apply
        if (Group* group = player->GetGroup(); group && group->IsMember(guid))
        {
            WorldPacket packet(CMSG_GROUP_UNINVITE_GUID, 8 + 1);
            packet << guid;
            packet << std::string("Released from the arena team");
            player->GetSession()->HandleGroupUninviteGuidOpcode(packet);
        }
        Tell(player, NameOf(guid) + " has left your team.");
        return true;
    }
};

void AddLivingArenaBrokerScript() { new npc_living_arena_broker(); }
