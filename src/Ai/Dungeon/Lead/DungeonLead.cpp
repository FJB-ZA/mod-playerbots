/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Who leads a bot-led dungeon run, the leader's tier, resting checks, and party chat "oom" calls.

#include "DungeonLead.h"

#include "Group.h"
#include "LivingArenaMgr.h"
#include "Map.h"
#include "MythicPlusBotMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SpellAuraDefines.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Timer.h"

namespace
{
// The cheat drink bots use when they have no food or water.
constexpr uint32 SPELL_BOT_REFRESHMENT = 25990;

bool IsPlayerControlled(Player* player) { return IsRealPlayer(player) || IsSelfBot(player); }
}  // namespace

namespace DungeonLead
{
bool Enabled() { return sPlayerbotAIConfig.DungeonLeadEnabled; }

bool EnabledFor(Player* bot) { return Enabled() || MythicPlusBotMgr::instance().InRun(bot); }

State& StateOf(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<State&>("dungeon lead state")->Get();
}

Player* Leader(PlayerbotAI* botAI)
{
    if (!botAI || !EnabledFor(botAI->GetBot()))
        return nullptr;

    ObjectGuid const guid = botAI->GetAiObjectContext()->GetValue<ObjectGuid>("dungeon leader")->Get();
    if (guid.IsEmpty())
        return nullptr;

    Player* bot = botAI->GetBot();
    Player* leader = ObjectAccessor::GetPlayer(*bot, guid);
    if (!leader || !leader->IsInWorld() || !leader->IsAlive() || leader->GetMap() != bot->GetMap())
        return nullptr;

    PlayerbotAI* leaderAI = GET_PLAYERBOT_AI(leader);
    if (!leaderAI)
        return nullptr;
    State const& state = StateOf(leaderAI);
    if (state.done || state.idle)
        return nullptr;

    return leader;
}

Tier TierOf(Player* bot) { return TierFor(LivingArenaMgr::instance().GetSkill(bot)); }

bool IsResting(Player* player)
{
    if (!player || !player->IsSitState())
        return false;

    if (player->HasAura(SPELL_BOT_REFRESHMENT))
        return true;

    // Food and drink end when the player stands up; buffs like Blessing of Wisdom do not.
    for (auto const& [spellId, application] : player->GetAppliedAuras())
        if (application->GetBase()->GetSpellInfo()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED)
            return true;
    return false;
}

void OnPartyChat(Player* sender, uint32 type, std::string const& msg, Group* group)
{
    if (!Enabled() || !sender || !group || !IsPlayerControlled(sender))
        return;

    if (type != CHAT_MSG_PARTY && type != CHAT_MSG_PARTY_LEADER)
        return;

    Map* map = sender->GetMap();
    if (!map || !map->IsNonRaidDungeon())
        return;

    Signal const signal = Parse(msg);
    if (signal == Signal::None)
        return;

    uint64 const raw = sender->GetGUID().GetRawValue();
    uint32 const now = getMSTime();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        PlayerbotAI* memberAI = member && member != sender ? GET_PLAYERBOT_AI(member) : nullptr;
        if (!memberAI || member->GetMap() != map)
            continue;

        State& state = StateOf(memberAI);
        state.oomAcked.erase(raw);
        if (signal == Signal::Oom)
            state.oomCalls[raw] = now;
        else
            state.oomCalls.erase(raw);
        state.readyCheckedAt = 0;
    }
}
}  // namespace DungeonLead

ObjectGuid DungeonLeaderValue::Calculate()
{
    Group* group = bot->GetGroup();
    bool const botRun = MythicPlusBotMgr::instance().IsRunGroup(group);
    if ((!DungeonLead::Enabled() && !botRun) || !botAI->IsInNonRaidDungeon())
        return ObjectGuid::Empty;

    if (!group || group->isRaidGroup())
        return ObjectGuid::Empty;

    // Groups with a player in them, or bots running a key on their own; a player who tanks runs the dungeon themselves.
    bool hasPlayer = botRun;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !IsPlayerControlled(member))
            continue;
        if (PlayerbotAI::IsTank(member))
            return ObjectGuid::Empty;
        hasPlayer = true;
    }
    if (!hasPlayer)
        return ObjectGuid::Empty;

    ObjectGuid const tankGuid = PlayerbotAI::GetMainTankGuid(group);
    Player* tank = tankGuid.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*bot, tankGuid);
    if (!tank || IsPlayerControlled(tank) || tank->GetMap() != bot->GetMap() || !PlayerbotAI::IsTank(tank))
        return ObjectGuid::Empty;

    // Without the strategy (instance strategies off, or removed by hand) the tank would lead nowhere.
    PlayerbotAI* tankAI = GET_PLAYERBOT_AI(tank);
    if (!tankAI || !tankAI->HasStrategy("dungeon lead", BOT_STATE_NON_COMBAT))
        return ObjectGuid::Empty;

    return tankGuid;
}
