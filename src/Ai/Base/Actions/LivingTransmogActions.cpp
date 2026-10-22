/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "LivingTransmogActions.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "Containers.h"
#include "Creature.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "LivingProfessionPolicy.h"
#include "LivingTransmogPolicy.h"
#include "LivingWorldMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "Playerbots.h"
#include "Random.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace
{
struct LookWish
{
    time_t next = 0;
    time_t started = 0;
    bool wants = false;
};

struct LookLedger
{
    std::mutex lock;
    std::unordered_map<uint32, LookWish> bots;
};

LookLedger& Looks()
{
    static LookLedger ledger;
    return ledger;
}

// Rolled at most once per retry window, so a bot that declined does not re-roll every minute in town.
bool WantsNewLook(Player* bot)
{
    uint32 const percent = LivingWorld::TransmogVisitPercent(LivingWorldMgr::instance().GetProfile(bot).motivation);
    time_t const now = GameTime::GetGameTime().count();
    LookLedger& ledger = Looks();
    std::lock_guard<std::mutex> guard(ledger.lock);
    LookWish& wish = ledger.bots[bot->GetGUID().GetCounter()];
    if (wish.wants && !LivingWorld::TransmogTripOver(wish.started, now))
        return true;
    if (!wish.wants && now < wish.next)
        return false;
    wish.wants = !wish.wants && urand(1, 100) <= percent;
    if (wish.wants)
        wish.started = now;
    else
        wish.next = now + LivingWorld::TransmogRetrySeconds;
    return wish.wants;
}

void FinishLook(uint32 guid)
{
    LookLedger& ledger = Looks();
    std::lock_guard<std::mutex> guard(ledger.lock);
    LookWish& wish = ledger.bots[guid];
    wish.wants = false;
    wish.next = GameTime::GetGameTime().count() + LivingWorld::TransmogCooldownSeconds;
}

// Drives mod-transmog's gossip menu with the same packets a player's client sends, so its own rules,
// collection and prices apply. Gossip selection is thread-unsafe, hence the world thread.
class LivingTransmogOperation : public PlayerbotOperation
{
public:
    LivingTransmogOperation(ObjectGuid bot, ObjectGuid warpweaver) : _bot(bot), _warpweaver(warpweaver) {}
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "Living transmog"; }

    bool Execute() override
    {
        Player* player = ObjectAccessor::FindPlayer(_bot);
        if (!player || !LivingWorldMgr::instance().IsEnabled(player) ||
            !LivingWorld::IsTransmogCity(player->GetZoneId()))
            return false;
        Creature* npc = player->GetNPCIfCanInteractWith(_warpweaver, UNIT_NPC_FLAG_GOSSIP);
        if (!npc || npc->GetEntry() != LivingWorld::WarpweaverEntry || !OpenMenu(player))
            return false;

        std::vector<uint32> slots;
        for (auto const& [index, option] : player->PlayerTalkClass->GetGossipMenu().GetMenuItems())
            if (option.Sender == EQUIPMENT_SLOT_END && option.OptionType < EQUIPMENT_SLOT_END)
                slots.push_back(option.OptionType);
        Acore::Containers::RandomShuffle(slots);

        uint32 const wanted =
            LivingWorld::TransmogSlotsPerVisit(LivingWorldMgr::instance().GetProfile(player).motivation);
        uint32 const attempts = LivingWorld::TransmogSlotAttempts(wanted);
        uint32 restyled = 0;
        uint32 tried = 0;
        for (uint32 slot : slots)
        {
            if (restyled >= wanted || tried++ >= attempts)
                break;
            restyled += Restyle(player, slot);
        }
        player->PlayerTalkClass->SendCloseGossip();
        LOG_INFO("playerbots", "Living bot {} visited the Warpweaver in zone {}: {} of {} slots restyled",
                 player->GetName(), player->GetZoneId(), restyled, slots.size());
        return restyled > 0;
    }

private:
    struct Look
    {
        uint32 option = 0;
        uint32 entry = 0;
        uint32 quality = 0;
    };

    bool OpenMenu(Player* player)
    {
        WorldPacket packet(CMSG_GOSSIP_HELLO);
        packet << _warpweaver;
        player->GetSession()->HandleGossipHelloOpcode(packet);
        GossipMenu const& menu = player->PlayerTalkClass->GetGossipMenu();
        return menu.GetSenderGUID() == _warpweaver && !menu.Empty();
    }

    void Choose(Player* player, uint32 option)
    {
        WorldPacket packet(CMSG_GOSSIP_SELECT_OPTION);
        packet << _warpweaver << player->PlayerTalkClass->GetGossipMenu().GetMenuId() << option;
        player->GetSession()->HandleGossipSelectOptionOpcode(packet);
    }

    bool Restyle(Player* player, uint32 slot)
    {
        if (!player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot) || !OpenMenu(player))
            return false;
        GossipMenu const& menu = player->PlayerTalkClass->GetGossipMenu();
        auto const slotOption =
            std::find_if(menu.GetMenuItems().begin(), menu.GetMenuItems().end(), [slot](auto const& item)
                         { return item.second.Sender == EQUIPMENT_SLOT_END && item.second.OptionType == slot; });
        if (slotOption == menu.GetMenuItems().end())
            return false;
        Choose(player, slotOption->first);

        // Look options carry the slot as sender and the item entry as action; 0 is search, max is "hide".
        std::vector<Look> looks;
        for (auto const& [option, item] : menu.GetMenuItems())
        {
            if (item.Sender != slot || !item.OptionType || item.OptionType == std::numeric_limits<uint32>::max())
                continue;
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item.OptionType);
            if (proto && LivingWorld::WantsLook(proto->Quality) &&
                LivingWorld::CanAffordTraining(player->GetMoney(), item.BoxMoney, player->GetLevel()))
                looks.push_back({option, proto->ItemId, proto->Quality});
        }
        if (looks.empty())
            return false;
        std::stable_sort(looks.begin(), looks.end(),
                         [](Look const& a, Look const& b) { return a.quality > b.quality; });
        uint32 const bestCount = static_cast<uint32>(std::count_if(
            looks.begin(), looks.end(), [&looks](Look const& look) { return look.quality == looks.front().quality; }));
        Look const look = looks[LivingWorld::PickLook(bestCount, static_cast<uint32>(looks.size()), rand32())];

        uint32 const before = player->GetMoney();
        Choose(player, look.option);
        if (player->GetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + slot * 2) != look.entry)
            return false;
        LOG_INFO("playerbots", "Living bot {} restyled slot {} as item {} for {} copper", player->GetName(), slot,
                 look.entry, before - player->GetMoney());
        return true;
    }

    ObjectGuid _bot;
    ObjectGuid _warpweaver;
};
}  // namespace

LivingTransmogTrip LivingTransmogTripValue::Calculate()
{
    LivingTransmogTrip trip;
    if (!sPlayerbotAIConfig.LivingTransmogEnabled || !LivingWorldMgr::instance().IsEnabled(bot) ||
        bot->GetLevel() < LivingWorld::TransmogMinLevel || !LivingWorld::IsTransmogCity(bot->GetZoneId()) ||
        bot->GetMoney() < LivingWorld::TransmogMinPurse(bot->GetLevel()) || !WantsNewLook(bot))
        return trip;
    if (Creature* warpweaver = bot->FindNearestCreature(LivingWorld::WarpweaverEntry, LivingWorld::TransmogSearchRange))
    {
        trip.go = true;
        trip.warpweaver = warpweaver->GetGUID();
    }
    return trip;
}

void LivingTransmog::EndTrip(PlayerbotAI* botAI)
{
    FinishLook(botAI->GetBot()->GetGUID().GetCounter());
    botAI->GetAiObjectContext()->GetValue<LivingTransmogTrip>("living transmog trip")->Set({});
}

bool LivingTransmog::QueueVisit(PlayerbotAI* botAI, ObjectGuid warpweaver)
{
    EndTrip(botAI);
    return PlayerbotWorldThreadProcessor::instance().QueueOperation(
        std::make_unique<LivingTransmogOperation>(botAI->GetBot()->GetGUID(), warpweaver));
}
