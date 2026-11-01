/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The sub-class balance harness (solo-wow): two bots of a path's class, one plain and one on the path, at the same
// level with the same talents and the same gear. In the single-target scenario each fights a training dummy of their
// level for a set time; in the pack scenario each fights five mobs of their level that fight back, until the five or
// the bot are dead; everything they and their pets deal is metered, after an uncounted warm-up fight. A run fights at
// least `fights` times and on, up to SubclassBalance::MaxFights, while the difference is too close to call. In the
// duel scenario the two duel each other until enough duels are decided. The comparison and its verdict follow
// SubclassBalancePolicy.h; the rows go to the GM's chat, the server log and <LogsDir>/balance-<path>.md, to be copied
// into the path's folder.
//
//   .balance start <path id> <level> <plain spec no> <path spec no> [fights 3] [seconds 180] [swap 0] [low high]
//   .balance pack <path id> <level> <plain spec no> <path spec no> [fights 3] [seconds 180] [swap 0] [low high] [size 5]
//   .balance duel <path id> <level> <plain spec no> <path spec no> [duels 10] [seconds 120] [swap 0]
//   .balance status
//   .balance stop
//
// Spec numbers are the class's premade specs in playerbots.conf (Playerbots.PremadeSpecName.<class>.<no>). With
// swap 1 the bot that would have been plain takes the path, to check a result against the two bots' races. In the
// pack `seconds` caps a fight; in a duel it is how long before the duel is called a draw. `low high` holds a path its
// design puts at a share of the plain spec's damage to that band, in percent (the Gladiator against Fury: 75 90). `size`
// is the pack's mobs (`0 0` for no band before it); a pack the plain class dies to in most fights isn't measured.

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <unordered_set>

#include "AiFactory.h"
#include "Bag.h"
#include "Chat.h"
#include "Config.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "LivingWorldMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Pet.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "SubclassBalancePolicy.h"
#include "SubclassPaths.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"

using namespace Acore::ChatCommands;
using SubclassBalance::Scenario;

namespace
{
constexpr uint32 DummyEntry = 31144;      // Grandmaster's Training Dummy: takes no damage, never dies
constexpr uint32 DuelSpell = 7266;
constexpr uint32 LoginTimeoutMs = 60000;
constexpr uint32 PrepareMs = 20000;       // buffs, pets and the path's minions
constexpr uint32 RestMs = 15000;
constexpr uint32 WarmUpMs = 60000;        // a first fight that isn't counted: pets, buffs and shards settle in it
constexpr uint32 ReattackMs = 3000;       // a dummy drops a player 5 seconds after their last direct hit
constexpr uint32 DuelRenewMs = 1000;      // in a duel, from the end of the countdown
constexpr uint32 DuelStartMs = 10000;     // a duel not under way by then didn't start
constexpr float Apart = 40.0f;            // between the two bots' dummies or packs
constexpr float Ahead = 15.0f;            // from a bot to its dummy or pack
constexpr float DuelApart = 8.0f;         // inside the Duel spell's reach

enum class Phase : uint8 { Idle, Login, Prepare, Fight, Rest };

struct Bot
{
    ObjectGuid guid;
    ObjectGuid dummy;
    std::vector<ObjectGuid> pack;
    Position target;  // where its dummy or pack goes
    bool over = false;  // the pack: its fight ended, the five or the bot dead
    SubclassBalance::Fight fight;
    std::vector<SubclassBalance::Fight> fights;
    std::string trace;  // this fight, every renewal: mana and health in tenths, "-" while out of combat
    uint64 white = 0;   // this fight: the bot's own melee and ranged swings that landed, and what they dealt
    uint32 hits = 0;
    uint32 outside = 0;  // this fight: heals blocked and auras taken off that came from outside the run
    std::set<ObjectGuid> intruders;  // this fight: who from outside the run dealt damage into it, each said once
    std::map<uint32, uint64> spells;  // this fight: what each of the bot's own spells dealt, before the target's armor
};

struct Run
{
    Phase phase = Phase::Idle;
    Scenario scenario = Scenario::Dummy;
    ObjectGuid gm;
    uint16 pathId = 0;
    std::string pathName;
    uint8 classId = 0;
    uint8 level = 0;
    int plainSpec = 0, pathSpec = 0;
    uint32 fightsWanted = 3;  // a duel run: the duels to decide
    uint32 fightMs = 180000;
    uint32 packEntry = 0;
    uint32 packSize = SubclassBalance::PackSize;
    bool swap = false;  // the first bot found takes the path, so the two can trade sides between runs
    uint32 elapsed = 0, sinceAttack = 0;
    bool warm = false;  // the warm-up fight is over
    uint32 wins[2] = {};  // duels: the plain bot's, the path bot's
    uint32 draws = 0;
    bool placed = false;    // this rest: the bots are lined up for the next fight
    uint32 placeWaits = 0;  // a duel: ticks spent waiting for the two to land in reach of each other
    bool duelOver = false;
    Bot sides[2];  // 0 the plain class, 1 the path
    SubclassBalance::Band band;  // none: parity within the budget
    std::vector<ObjectGuid> before;  // the GM's bots before the run, so the two it adds are known
};

Run run;

Player* Gm() { return ObjectAccessor::FindConnectedPlayer(run.gm); }

Player* BotOf(int side)
{
    Player* bot = ObjectAccessor::FindConnectedPlayer(run.sides[side].guid);
    return bot && bot->IsInWorld() && GET_PLAYERBOT_AI(bot) ? bot : nullptr;
}

int SideOf(Player* bot) { return bot && bot->GetGUID() == run.sides[1].guid ? 1 : 0; }

void Say(std::string const& text)
{
    LOG_INFO("playerbots", "Balance: {}", text);
    if (Player* gm = Gm())
        ChatHandler(gm->GetSession()).SendSysMessage("Balance: " + text);
}

// The dummy or the pack a side fought, gone
void Clear(Player* gm, Bot& side)
{
    if (Creature* dummy = ObjectAccessor::GetCreature(*gm, side.dummy))
        dummy->DespawnOrUnsummon();
    for (ObjectGuid const& guid : side.pack)
        if (Creature* mob = ObjectAccessor::GetCreature(*gm, guid))
            mob->DespawnOrUnsummon();
    side.dummy.Clear();
    side.pack.clear();
}

void Revive(Player* bot)
{
    if (bot->IsAlive())
        return;
    bot->ResurrectPlayer(1.0f);
    bot->SpawnCorpseBones();
}

void Stop(std::string const& why)
{
    if (Player* plain = BotOf(0); plain && plain->duel)
        plain->DuelComplete(DUEL_INTERRUPTED);
    // The bots are the shared addclass pool's: the path bot goes back to its plain class before it leaves, GM or not
    if (Player* path = BotOf(1); path && SubclassPaths::ActivePathId(path) != SubclassPaths::PlainClass)
        SubclassPaths::ChangeActivePath(path, SubclassPaths::PlainClass, true);
    for (int side = 0; side < 2; ++side)
        if (Player* bot = BotOf(side))
        {
            Revive(bot);
            LivingWorldMgr::instance().Exempt(bot, false);
        }
    if (Player* gm = Gm())
    {
        for (Bot& side : run.sides)
            Clear(gm, side);
        if (PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(gm))
            for (Bot const& side : run.sides)
                if (!side.guid.IsEmpty())
                    mgr->LogoutPlayerBot(side.guid);
    }
    Say("run over: " + why);  // .\wow.ps1 balance waits for this line; "done" is the only measured ending
    run = Run{};
}

std::string SpecName(uint8 classId, int specNo)
{
    return specNo >= 0 && specNo < MAX_SPECNO ? sPlayerbotAIConfig.PremadeSpecName[classId][specNo] : "";
}

// The pack: five of one plain melee humanoid of exactly the run's level, hostile to players, without spells, script or
// a modified health or damage, the lowest such entry so every fight and run meets the same one
uint32 PackEntry(uint8 level)
{
    uint32 best = 0;
    for (auto const& [entry, info] : *sObjectMgr->GetCreatureTemplates())
    {
        if (info.minlevel != level || info.maxlevel != level || info.rank != CREATURE_ELITE_NORMAL ||
            info.unit_class != CLASS_WARRIOR || info.type != CREATURE_TYPE_HUMANOID || !info.AIName.empty() ||
            info.ScriptID || info.npcflag || info.flags_extra || info.spells[0] || info.ModHealth != 1.0f ||
            info.DamageModifier != 1.0f ||
            (info.unit_flags & (UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC)))
            continue;
        FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(info.faction);
        if (faction && faction->IsHostileToPlayers() && (!best || entry < best))
            best = entry;
    }
    return best;
}

void Strategies(Player* bot)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    ai->ResetStrategies();
    ai->ChangeStrategy("-follow,+stay", BOT_STATE_NON_COMBAT);
    // (a bot assisting its group takes the target most of the group attacks: the other bot's dummy, pet and all)
    ai->ChangeStrategy("-dps assist", BOT_STATE_COMBAT);
    // The harness starts and accepts the duels: a bot's own acceptance resets its strategies
    ai->ChangeStrategy("-duel", BOT_STATE_NON_COMBAT);
    ai->ChangeStrategy("-duel", BOT_STATE_COMBAT);
}

void Fresh(Player* bot)
{
    Revive(bot);
    bot->RemoveAllSpellCooldown();
    bot->SetFullHealth();
    bot->SetPower(bot->getPowerType(), bot->GetMaxPower(bot->getPowerType()));
}

// What a bot carries that would buy it damage, health or mana in a fight: potions, elixirs, flasks, scrolls, bandages
// and weapon oils. Randomize hands each bot its own, so one side drank and the other had nothing it could use (at
// level 60 a healing potion is mana to a warlock, through Life Tap); a run is the two classes, not their bags. The
// stones a class makes for itself stay: both sides have the same.
std::vector<Item*> Consumables(Player* bot)
{
    std::vector<Item*> found;
    auto look = [&found](Item* item) {
        if (!item)
            return;
        ItemTemplate const* proto = item->GetTemplate();
        if (proto->Class != ITEM_CLASS_CONSUMABLE)
            return;
        switch (proto->SubClass)
        {
            case ITEM_SUBCLASS_POTION:
            case ITEM_SUBCLASS_ELIXIR:
            case ITEM_SUBCLASS_FLASK:
            case ITEM_SUBCLASS_SCROLL:
            case ITEM_SUBCLASS_ITEM_ENHANCEMENT:
            case ITEM_SUBCLASS_BANDAGE:
                found.push_back(item);
                break;
            default:
                break;
        }
    };
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        look(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag* each = bot->GetBagByPos(bag))
            for (uint32 slot = 0; slot < each->GetBagSize(); ++slot)
                look(each->GetItemByPos(uint8(slot)));
    return found;
}

// What keeps a bot from being the setup the run asked for, or empty: anything here would measure another character
// than the one the row names
std::string Unready(Player* bot, uint16 wantedPath)
{
    std::vector<std::string> wrong;
    for (Item* item : Consumables(bot))
        wrong.push_back(Acore::StringFormat("{} {} in its bags", item->GetCount(), item->GetTemplate()->Name1));
    if (bot->GetLevel() != run.level)
        wrong.push_back(Acore::StringFormat("level {}", bot->GetLevel()));
    if (uint16 const pathId = SubclassPaths::ActivePathId(bot); pathId != wantedPath)
        wrong.push_back(Acore::StringFormat("path {}", pathId));
    if (uint32 const free = bot->GetFreeTalentPoints())
        wrong.push_back(Acore::StringFormat("{} talent points unspent", free));
    uint8 const spec = bot->GetActiveSpec();
    for (auto const& [spellId, talent] : bot->GetTalentMap())
        // A passive talent is an aura on the bot, not a spell it knows: one without an aura (Titan's Grip) leaves
        // nothing to see but its effect, and one for a stance is only there in that stance. A path may take a
        // talent's spell for its own.
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
            info && talent->State != PLAYERSPELL_REMOVED && talent->IsInSpec(spec) && !bot->HasSpell(spellId) &&
            !(info->IsPassive() && (bot->HasAura(spellId) || !info->HasAnyAura() ||
                                    info->CheckShapeshift(bot->GetShapeshiftForm()) != SPELL_CAST_OK)) &&
            !SubclassPaths::ActiveReplacement(bot, spellId))
        {
            auto const known = bot->GetSpellMap().find(spellId);
            wrong.push_back(known == bot->GetSpellMap().end()
                                ? Acore::StringFormat("talent {} without its spell", spellId)
                                : Acore::StringFormat("talent {} with its spell in state {}, slots {}", spellId,
                                                      int(known->second->State), int(known->second->specMask)));
        }
    uint16 const cap = bot->GetMaxSkillValueForLevel();
    if (bot->GetSkillValue(SKILL_DEFENSE) < cap)
        wrong.push_back(Acore::StringFormat("defense {} of {}", bot->GetSkillValue(SKILL_DEFENSE), cap));
    for (WeaponAttackType attack : {BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK})
        if (Item* weapon = bot->GetWeaponForAttack(attack, true))
            if (uint32 const skill = weapon->GetSkill(); bot->GetSkillValue(skill) < cap)
                wrong.push_back(Acore::StringFormat("{} skill {} of {}", weapon->GetTemplate()->Name1,
                                                    bot->GetSkillValue(skill), cap));
    if (bot->GetWeaponForAttack(OFF_ATTACK, true) && !bot->CanDualWield())
        wrong.push_back("an off-hand weapon it can't swing");
    if (Item* offhand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
        offhand && offhand->GetTemplate()->InventoryType == INVTYPE_2HWEAPON && !bot->CanTitanGrip())
        wrong.push_back("a two-hander in its off hand without Titan's Grip");
    // A plain bot that still knows a path's spells fights with them
    if (wantedPath == SubclassPaths::PlainClass)
        for (SubclassPaths::Path const* other : SubclassPaths::ClassPaths(bot->getClass()))
            for (SubclassPaths::SpellSwap const& swap : other->swaps)
                if (bot->HasSpell(swap.replacement))
                    wrong.push_back(Acore::StringFormat("path spell {}", swap.replacement));
    std::string why;
    for (std::string const& each : wrong)
        why += (why.empty() ? "" : ", ") + each;
    return why.empty() ? why : bot->GetName() + ": " + why;
}

// Both bots at the run's level and spec, the path bot on its path, and the plain bot's gear cloned onto the path bot
void Prepare(Player* plain, Player* path)
{
    for (Player* bot : {plain, path})
    {
        // A living-world bot would keep its own life here: Randomize leaves it as it is, or caps its level
        LivingWorldMgr::instance().Exempt(bot, true);
        PlayerbotFactory factory(bot, run.level);
        factory.Randomize(false);
    }
    // (either may still follow a path from a run that ended without its GM). Each keeps its path for the run: a random
    // bot left on plain class would otherwise pick a path of its own and reset its talents.
    for (auto [bot, wanted] : {std::pair{plain, SubclassPaths::PlainClass}, std::pair{path, run.pathId}})
    {
        SubclassPaths::KeepPath(bot);
        if (SubclassPaths::ActivePathId(bot) == wanted)
            continue;
        if (std::string const why = SubclassPaths::ChangeActivePath(bot, wanted, true); !why.empty())
        {
            Stop(Acore::StringFormat("{} couldn't take path {}: {}", bot->GetName(), wanted, why));
            return;
        }
    }
    for (auto [bot, spec] : {std::pair{plain, run.plainSpec}, std::pair{path, run.pathSpec}})
    {
        PlayerbotFactory::InitTalentsBySpecNo(bot, spec, true);
        PlayerbotFactory(bot, bot->GetLevel()).InitGlyphs(false);
    }
    PlayerbotFactory::AutoGear(plain, ITEM_QUALITY_RARE, 0, false);
    PlayerbotFactory::DestroyEquippedGear(path);
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* worn = plain->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!worn)
            continue;
        Item* copy = worn->CloneItem(1, path);
        uint16 dest = 0;
        if (!copy || path->CanEquipItem(slot, dest, copy, false) != EQUIP_ERR_OK)
        {
            delete copy;
            continue;
        }
        // CloneItem keeps the item and its random stats only: its enchants and gems come across here, before the
        // item is worn and they take effect (at level 60 they were a tenth of the plain bot's damage)
        for (uint8 enchant = PERM_ENCHANTMENT_SLOT; enchant < MAX_ENCHANTMENT_SLOT; ++enchant)
        {
            EnchantmentSlot const each = EnchantmentSlot(enchant);
            if (uint32 const id = worn->GetEnchantmentId(each))
                copy->SetEnchantment(each, id, worn->GetEnchantmentDuration(each), worn->GetEnchantmentCharges(each));
        }
        path->EquipItem(dest, copy, true);
    }
    auto gear = [](Player* bot) {
        uint32 items = 0, enchants = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                ++items;
                for (uint8 enchant = PERM_ENCHANTMENT_SLOT; enchant < MAX_ENCHANTMENT_SLOT; ++enchant)
                    enchants += item->GetEnchantmentId(EnchantmentSlot(enchant)) ? 1 : 0;
            }
        return Acore::StringFormat("{} items, {} enchants and gems, {} shadow spell power, {} attack power", items,
                                   enchants, bot->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW),
                                   uint32(bot->GetTotalAttackPowerValue(BASE_ATTACK)));
    };
    for (Player* bot : {plain, path})
        for (Item* item : Consumables(bot))
            bot->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
    Say("plain " + gear(plain) + "; path " + gear(path));
    for (Player* bot : {plain, path})
    {
        Strategies(bot);
        Fresh(bot);
        std::string combat;
        for (std::string const& name : GET_PLAYERBOT_AI(bot)->GetStrategies(BOT_STATE_COMBAT))
            combat += (combat.empty() ? "" : ",") + name;
        Say(Acore::StringFormat("{}: talent tab {}, fights with {}", bot->GetName(), AiFactory::GetPlayerSpecTab(bot),
                                combat));
    }
}

// The ground under (x, y), near the GM's height
float GroundZ(Player* gm, float x, float y)
{
    float const z = gm->GetMap()->GetHeight(gm->GetPhaseMask(), x, y, gm->GetPositionZ() + 10.0f);
    return z > INVALID_HEIGHT ? z : gm->GetPositionZ();
}

bool Sees(Player* gm, float x, float y, float z, float tx, float ty, float tz)
{
    return gm->GetMap()->isInLineOfSight(x, y, z + 2.0f, tx, ty, tz + 2.0f, gm->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS,
                                         VMAP::ModelIgnoreFlags::Nothing);
}

// Each bot to its own spot beside the GM, on the ground and facing a fresh dummy of its level, or the spot its pack
// will come from; for a duel, the two a few yards apart facing each other. A bot drops a target it can't see, so a
// spot with a slope or a tree in the way stops the run.
bool Lineup(Player* gm)
{
    float const facing = gm->GetOrientation();
    if (run.scenario == Scenario::Duel)
    {
        // (towns and starting valleys such as Northshire don't allow duels)
        AreaTableEntry const* area = sAreaTableStore.LookupEntry(gm->GetAreaId());
        if (area && !(area->flags & AREA_FLAG_ALLOW_DUELS))
        {
            Stop(Acore::StringFormat("{} doesn't allow duels; stand where it does (open country) and start again",
                                     area->area_name[LOCALE_enUS]));
            return false;
        }
        float const cx = gm->GetPositionX() + std::cos(facing) * 10.0f, cy = gm->GetPositionY() + std::sin(facing) * 10.0f;
        float spots[2][3];
        for (int side = 0; side < 2; ++side)
        {
            float const angle = facing + (side ? -1.0f : 1.0f) * float(M_PI) / 2;
            spots[side][0] = cx + std::cos(angle) * DuelApart / 2;
            spots[side][1] = cy + std::sin(angle) * DuelApart / 2;
            spots[side][2] = GroundZ(gm, spots[side][0], spots[side][1]);
        }
        if (!Sees(gm, spots[0][0], spots[0][1], spots[0][2], spots[1][0], spots[1][1], spots[1][2]))
        {
            Stop("the two duel spots can't see each other; stand on flat, open ground and start again");
            return false;
        }
        for (int side = 0; side < 2; ++side)
            if (Player* bot = BotOf(side))
                bot->TeleportTo(gm->GetMapId(), spots[side][0], spots[side][1], spots[side][2],
                                facing + (side ? 1.0f : -1.0f) * float(M_PI) / 2);
        return true;
    }
    for (int side = 0; side < 2; ++side)
    {
        Player* bot = BotOf(side);
        if (!bot)
            continue;
        float const angle = facing + (side ? -1.0f : 1.0f) * float(M_PI) / 2;
        float const x = gm->GetPositionX() + std::cos(angle) * Apart / 2;
        float const y = gm->GetPositionY() + std::sin(angle) * Apart / 2;
        float const z = GroundZ(gm, x, y);
        float const dx = x + std::cos(facing) * Ahead, dy = y + std::sin(facing) * Ahead;
        float const dz = GroundZ(gm, dx, dy);
        if (!Sees(gm, x, y, z, dx, dy, dz))
        {
            Stop(Acore::StringFormat("{} can't see its {} from its spot; stand on flat, open ground and start again",
                                     bot->GetName(), run.scenario == Scenario::Pack ? "pack" : "dummy"));
            return false;
        }
        bot->TeleportTo(gm->GetMapId(), x, y, z, facing);
        run.sides[side].target.Relocate(dx, dy, dz, facing + float(M_PI));
        if (run.scenario != Scenario::Dummy)
            continue;
        if (TempSummon* dummy = gm->SummonCreature(DummyEntry, dx, dy, dz, facing + float(M_PI),
                                                   TEMPSUMMON_TIMED_DESPAWN, run.fightMs + RestMs + 60000))
        {
            dummy->SetLevel(run.level);
            dummy->SetFullHealth();
            run.sides[side].dummy = dummy->GetGUID();
        }
    }
    return true;
}

// Each bot's five, in a line across its spot, all on it
void Pull(Player* gm)
{
    for (int side = 0; side < 2; ++side)
    {
        Player* bot = BotOf(side);
        Bot& b = run.sides[side];
        if (!bot)
            continue;
        float const across = b.target.GetOrientation() + float(M_PI) / 2;
        for (uint32 i = 0; i < run.packSize; ++i)
        {
            float const offset = (float(i) - float(run.packSize - 1) / 2) * 2.5f;
            float const x = b.target.GetPositionX() + std::cos(across) * offset;
            float const y = b.target.GetPositionY() + std::sin(across) * offset;
            if (TempSummon* mob = gm->SummonCreature(run.packEntry, x, y, GroundZ(gm, x, y), b.target.GetOrientation(),
                                                     TEMPSUMMON_TIMED_DESPAWN, run.fightMs + RestMs + 60000))
            {
                b.pack.push_back(mob->GetGUID());
                mob->AI()->AttackStart(bot);
            }
        }
    }
}

// The duel: the plain bot challenges, the path bot accepts, and after the countdown the two fight
// (empty when it started; else why not, from the Duel spell's own checks)
std::string Challenge()
{
    Player* plain = BotOf(0);
    Player* path = BotOf(1);
    SpellCastResult const cast = plain->CastSpell(path, DuelSpell, true);
    if (!path->duel || path->duel->State != DUEL_STATE_CHALLENGED)
    {
        auto area = [](Player* bot) {
            AreaTableEntry const* entry = sAreaTableStore.LookupEntry(bot->GetAreaId());
            return Acore::StringFormat("area {} {}", bot->GetAreaId(),
                                       entry && !(entry->flags & AREA_FLAG_ALLOW_DUELS) ? "(no duels)" : "(duels)");
        };
        return Acore::StringFormat("the duel didn't start: cast result {}; {} in {}; {} in {}, {} social data; "
                                   "already dueling {}/{}",
                                   uint32(cast), plain->GetName(), area(plain), path->GetName(), area(path),
                                   path->GetSocial() ? "with" : "without", plain->duel != nullptr, path->duel != nullptr);
    }
    WorldPacket accept(CMSG_DUEL_ACCEPTED, 8);
    accept << plain->GetGuidValue(PLAYER_DUEL_ARBITER);
    path->GetSession()->HandleDuelAcceptedOpcode(accept);
    return "";
}

// Whether a caster is one of the run's two bots, or their pet, guardian or minion
bool OfRun(Unit* caster)
{
    Player* owner = caster ? caster->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    return owner && (owner->GetGUID() == run.sides[0].guid || owner->GetGUID() == run.sides[1].guid);
}

// Whether a guid is one of the run's dummies or pack mobs: what they put on the bots (a daze, say) is the fight's
bool RunFoe(ObjectGuid guid)
{
    for (Bot const& side : run.sides)
        if (guid == side.dummy || std::find(side.pack.begin(), side.pack.end(), guid) != side.pack.end())
            return true;
    return false;
}

// Wild creatures around the run join its fights: a Prowler caught in a pack fight's area spells came for the bots. Before
// each fight the ground around the GM (both bots' spots, and their dummies or packs) is cleared of them; they come back
// on their own respawn. Guards, anyone with a service, and anything a player owns are left be.
uint32 ClearGround(Player* gm)
{
    std::list<Creature*> near;
    Acore::AnyUnitInObjectRangeCheck check(gm, 70.0f);
    Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(gm, near, check);
    Cell::VisitObjects(gm, searcher, 70.0f);
    uint32 cleared = 0;
    for (Creature* creature : near)
    {
        if (RunFoe(creature->GetGUID()) || creature->GetCharmerOrOwnerGUID().IsPlayer() || creature->IsGuard() ||
            creature->GetNpcFlags() != UNIT_NPC_FLAG_NONE || creature->IsTotem())
            continue;
        creature->DespawnOrUnsummon(0ms, 300s);
        ++cleared;
    }
    return cleared;
}

// Takes off a unit's auras from outside the run, each said: the bots fight where random bots pass, and a passing
// priest's heal or a paladin's blessing would measure the passer-by
uint32 Isolate(Unit* unit)
{
    uint32 removed = 0;
    if (!unit)
        return removed;
    Unit::AuraApplicationMap& auras = unit->GetAppliedAuras();
    for (auto itr = auras.begin(); itr != auras.end();)
    {
        Aura const* aura = itr->second->GetBase();
        ObjectGuid const caster = aura->GetCasterGUID();
        if (aura->IsPassive() || caster == unit->GetGUID() || OfRun(ObjectAccessor::GetUnit(*unit, caster)) ||
            RunFoe(caster))
            ++itr;
        else
        {
            Say(Acore::StringFormat("{}: took off {} ({}) from {}", unit->GetName(), aura->GetId(),
                                    aura->GetSpellInfo()->SpellName[LOCALE_enUS], caster.ToString()));
            unit->RemoveAura(itr);
            ++removed;
        }
    }
    return removed;
}

void Trace(Player* bot)
{
    std::string& trace = run.sides[SideOf(bot)].trace;
    Bot& b = run.sides[SideOf(bot)];
    b.outside += Isolate(bot);
    if (Creature* dummy = ObjectAccessor::GetCreature(*bot, b.dummy))
        b.outside += Isolate(dummy);
    for (ObjectGuid const& guid : b.pack)
        b.outside += Isolate(ObjectAccessor::GetCreature(*bot, guid));
    trace += Acore::StringFormat("{}{}/{} ", bot->IsInCombat() ? "" : "-",
                                 bot->GetMaxPower(bot->getPowerType()) ?
                                     bot->GetPower(bot->getPowerType()) * 10 / bot->GetMaxPower(bot->getPowerType()) : 0,
                                 bot->GetHealthPct() >= 100.0f ? 10 : uint32(bot->GetHealthPct() / 10));
}

// As the bots' own attack action does it: the target their current target, the combat engine on
void Attack(Player* bot, Unit* target)
{
    PlayerbotAI* ai = GET_PLAYERBOT_AI(bot);
    // A bot that dropped out of combat after the first renewal: once a fight, what might keep it from fighting
    if (!bot->IsInCombat() && run.elapsed > 2 * ReattackMs && !run.sides[SideOf(bot)].fight.damage)
    {
        static std::unordered_set<uint64> told;
        uint64 const key = bot->GetGUID().GetRawValue() ^ (uint64(run.sides[0].fights.size()) << 56);
        if (told.insert(key).second)
        {
            Say(Acore::StringFormat("{} isn't fighting: valid target {}, in sight {}, invalid to its AI {}, height {:.1f} "
                                    "against its target's {:.1f}, power {}/{}, state {}",
                                    bot->GetName(), bot->IsValidAttackTarget(target), bot->IsWithinLOSInMap(target),
                                    ai->GetAiObjectContext()->GetValue<bool>("invalid target", "current target")->Get(),
                                    bot->GetPositionZ(), target->GetPositionZ(), bot->GetPower(bot->getPowerType()),
                                    bot->GetMaxPower(bot->getPowerType()), uint32(ai->GetState())));
        }
    }
    bot->SetSelection(target->GetGUID());
    ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Set(target);
    ai->ChangeEngine(BOT_STATE_COMBAT);
    if (bot->GetVictim() != target)
        bot->Attack(target, !PlayerbotAI::IsRanged(bot));
}

// The pack: a side's fight is over when its five are dead, or it is; how long it took is its fight's length
void PackCheck(Player* gm)
{
    for (int side = 0; side < 2; ++side)
    {
        Bot& b = run.sides[side];
        Player* bot = BotOf(side);
        if (b.over || !bot)
            continue;
        bool const alive = std::any_of(b.pack.begin(), b.pack.end(), [gm](ObjectGuid const& guid) {
            Creature* mob = ObjectAccessor::GetCreature(*gm, guid);
            return mob && mob->IsAlive();
        });
        if (alive && bot->IsAlive())
            continue;
        b.over = true;
        b.fight.ms = run.elapsed - ReattackMs;
        b.fight.deaths = bot->IsAlive() ? 0 : 1;
    }
}

void EndFight()
{
    Player* gm = Gm();
    for (int side = 0; side < 2; ++side)
    {
        Bot& b = run.sides[side];
        if (Player* bot = BotOf(side))
        {
            Creature* dummy = gm ? ObjectAccessor::GetCreature(*gm, b.dummy) : nullptr;
            Say(Acore::StringFormat("{}: level {}, {}, {} health, form {}, targeting {}, dummy {} at {:.0f} yards, {} damage "
                                    "({} of it from {} swings), {} heals and auras from outside the run stopped{}",
                                    bot->GetName(), bot->GetLevel(), bot->IsInCombat() ? "in combat" : "out of combat",
                                    bot->IsAlive() ? "alive" : "dead", uint32(bot->GetShapeshiftForm()),
                                    bot->GetTarget().ToString(), dummy ? dummy->GetGUID().ToString() : "missing",
                                    dummy ? bot->GetDistance(dummy) : 0.0f, b.fight.damage, b.white, b.hits, b.outside,
                                    run.scenario == Scenario::Pack ?
                                        Acore::StringFormat(", pack {} in {:.1f} seconds", b.fight.deaths ? "won" : "cleared",
                                                            b.fight.ms / 1000.0f) : ""));
            Say(Acore::StringFormat("{} mana/health: {}", bot->GetName(), b.trace));
            std::vector<std::pair<uint64, uint32>> top;
            for (auto const& [spell, dealt] : b.spells)
                top.emplace_back(dealt, spell);
            std::sort(top.rbegin(), top.rend());
            std::string spells;
            for (size_t i = 0; i < top.size() && i < 5; ++i)
            {
                SpellInfo const* info = sSpellMgr->GetSpellInfo(top[i].second);
                spells += Acore::StringFormat("{}{} {} {}", i ? ", " : "", top[i].second,
                                              info ? info->SpellName[LOCALE_enUS] : "?", top[i].first);
            }
            Say(Acore::StringFormat("{} spells: {}{}", bot->GetName(), spells.empty() ? "none" : spells,
                                    dummy ? Acore::StringFormat("; its dummy level {}, armor {}; {:.1f} yards from its "
                                                                "centre, in melee range {}, facing it {}, behind it {}",
                                                                dummy->GetLevel(), dummy->GetArmor(),
                                                                bot->GetExactDist2d(dummy), bot->IsWithinMeleeRange(dummy),
                                                                bot->HasInArc(float(M_PI), dummy),
                                                                !dummy->HasInArc(float(M_PI), bot)) : ""));
        }
        // (a pack fight's length is how long the side took; a pack still standing at the cap took all of it)
        if (run.scenario != Scenario::Pack || !b.over)
            b.fight.ms = run.warm ? run.fightMs : WarmUpMs;
        if (run.warm)
            b.fights.push_back(b.fight);
        b.fight = {};
        b.trace.clear();
        b.white = 0;
        b.hits = 0;
        b.outside = 0;
        b.intruders.clear();
        b.spells.clear();
        b.over = false;
        if (Player* bot = BotOf(side))
        {
            bot->AttackStop();
            bot->CombatStop(true);
            GET_PLAYERBOT_AI(bot)->ChangeEngine(BOT_STATE_NON_COMBAT);
            Fresh(bot);
        }
        if (gm)
            Clear(gm, b);
    }
}

void Report()
{
    using namespace SubclassBalance;
    Row const row{Acore::Time::TimeToTimestampStr(GameTime::GetGameTime()).substr(0, 10), run.level,
                  SpecName(run.classId, run.plainSpec) + " / " + SpecName(run.classId, run.pathSpec) +
                      (run.swap ? " (sides swapped)" : ""),
                  run.scenario, run.packSize};
    std::string const line = run.scenario == Scenario::Duel ?
                                 TableRow(row, JudgeDuels(run.wins[1], run.wins[0], run.draws)) :
                                 TableRow(row, Compare(run.sides[0].fights, run.sides[1].fights, run.band));
    Say(run.pathName + ": " + line);
    std::string const file = sConfigMgr->GetOption<std::string>("LogsDir", ".") + "/balance-" + run.pathName + ".md";
    bool const fresh = !std::ifstream(file).good();
    std::ofstream out(file, std::ios::app);
    if (fresh)
        out << "# " << run.pathName << " balance\n\n" << SubclassBalance::TableHeader();
    out << line;
    Say("written to " + file);
}

// A duel's end, from the duel hook or the time running out
void DuelTick(uint32 diff)
{
    Player* plain = BotOf(0);
    Player* path = BotOf(1);
    bool const going = plain->duel && plain->duel->State == DUEL_STATE_IN_PROGRESS;
    if (!run.duelOver && !plain->duel && run.elapsed > DuelStartMs)
    {
        Stop("the duel didn't start (is dueling allowed here?)");
        return;
    }
    run.sinceAttack += diff;
    if (going && run.sinceAttack >= DuelRenewMs)
    {
        run.sinceAttack = 0;
        Attack(plain, path);
        Attack(path, plain);
        Trace(plain);
        Trace(path);
    }
    if (!run.duelOver && run.elapsed >= run.fightMs)
    {
        if (plain->duel)
            plain->DuelComplete(DUEL_INTERRUPTED);  // the duel hook counts it a draw
        if (!run.duelOver)
            ++run.draws;
        run.duelOver = true;
    }
    if (!run.duelOver)
        return;
    for (int side = 0; side < 2; ++side)
    {
        Player* bot = BotOf(side);
        Say(Acore::StringFormat("{} mana/health: {}", bot->GetName(), run.sides[side].trace));
        run.sides[side].trace.clear();
        bot->AttackStop();
        bot->CombatStop(true);
        GET_PLAYERBOT_AI(bot)->ChangeEngine(BOT_STATE_NON_COMBAT);
        Fresh(bot);
    }
    for (Player* bot : {plain, path})
        if (Pet* pet = bot->GetPet())
        {
            pet->AttackStop();
            pet->CombatStop(true);
        }
    uint32 const decided = run.wins[0] + run.wins[1];
    Say(Acore::StringFormat("duel {}: plain {} won, path {} won, {} draws", decided + run.draws, run.wins[0], run.wins[1],
                            run.draws));
    if (decided >= run.fightsWanted || decided + run.draws >= 2 * run.fightsWanted)
    {
        Report();
        Stop("done");
        return;
    }
    run.duelOver = false;
    run.phase = Phase::Rest;
    run.elapsed = 0;
}

void Tick(uint32 diff)
{
    if (run.phase == Phase::Idle)
        return;
    Player* gm = Gm();
    if (!gm)
    {
        Stop("the GM left; stopped");
        return;
    }
    run.elapsed += diff;
    switch (run.phase)
    {
        case Phase::Login:
        {
            PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(gm);
            int found = 0;
            for (auto itr = mgr->GetPlayerBotsBegin(); itr != mgr->GetPlayerBotsEnd() && found < 2; ++itr)
            {
                Player* bot = itr->second;
                if (bot->getClass() != run.classId ||
                    std::find(run.before.begin(), run.before.end(), bot->GetGUID()) != run.before.end())
                    continue;
                if (!GET_PLAYERBOT_AI(bot) || !bot->IsInWorld())
                    continue;
                run.sides[run.swap ? 1 - found : found].guid = bot->GetGUID();
                ++found;
            }
            if (found == 2)
            {
                Prepare(BotOf(0), BotOf(1));
                if (run.phase == Phase::Idle)
                    return;
                auto race = [](Player* bot) {
                    static char const* const races[] = {"", "human", "orc", "dwarf", "night elf", "undead",
                                                        "tauren", "gnome", "troll", "", "blood elf", "draenei"};
                    return bot->getRace() < std::size(races) ? races[bot->getRace()] : "";
                };
                Say(Acore::StringFormat("{} ({}, plain) and {} ({}, {}) ready at level {}", BotOf(0)->GetName(),
                                        race(BotOf(0)), BotOf(1)->GetName(), race(BotOf(1)), run.pathName,
                                        run.level));
                if (!Lineup(gm))
                    return;
                run.phase = Phase::Prepare;
                run.elapsed = 0;
            }
            else if (run.elapsed > LoginTimeoutMs)
            {
                for (Bot& side : run.sides)
                    side.guid.Clear();
                Stop("two bots of the class didn't log in (addclass needs free characters of the class)");
            }
            return;
        }
        case Phase::Prepare:
        case Phase::Rest:
            if (run.elapsed < (run.phase == Phase::Prepare ? PrepareMs : RestMs))
                return;
            if (!run.placed)
            {
                if (!Lineup(gm))
                    return;
                run.placed = true;
            }
            // A bot is only where it was put once its teleport lands: a challenge in the same tick reached for where
            // the last duel left it (out of range)
            if (run.scenario == Scenario::Duel && BotOf(0) && BotOf(1) && BotOf(0)->GetDistance(BotOf(1)) > DuelApart)
            {
                if (++run.placeWaits > 200)
                    Stop(Acore::StringFormat("the duel didn't start: the bots stayed {:.0f} yards apart",
                                             BotOf(0)->GetDistance(BotOf(1))));
                return;
            }
            run.placeWaits = 0;
            run.placed = false;
            if (uint32 const cleared = ClearGround(gm))
                Say(Acore::StringFormat("cleared {} wild creatures from around the fight", cleared));
            // Each fight starts fresh: what a bot spent resting (buffs, a pet, a Life Tap to fill up) isn't carried in
            for (int side = 0; side < 2; ++side)
                if (Player* bot = BotOf(side))
                {
                    Fresh(bot);
                    if (std::string const why = Unready(bot, side == 0 ? SubclassPaths::PlainClass : run.pathId);
                        !why.empty())
                    {
                        Stop("not measured, the bots aren't the setup asked for. " + why);
                        return;
                    }
                }
            run.phase = Phase::Fight;
            if (run.scenario == Scenario::Duel)
            {
                run.elapsed = run.sinceAttack = 0;
                if (std::string const why = Challenge(); !why.empty())
                    Stop(why);
                return;
            }
            run.elapsed = run.sinceAttack = ReattackMs;
            if (run.scenario == Scenario::Pack)
                Pull(gm);
            return;
        case Phase::Fight:
        {
            if (!BotOf(0) || !BotOf(1))
            {
                Stop("a bot left mid-fight; stopped");
                return;
            }
            if (run.scenario == Scenario::Duel)
            {
                DuelTick(diff);
                return;
            }
            if (run.scenario == Scenario::Pack)
                PackCheck(gm);
            run.sinceAttack += diff;
            if (run.sinceAttack >= ReattackMs)
            {
                run.sinceAttack = 0;
                for (int side = 0; side < 2; ++side)
                {
                    Player* bot = BotOf(side);
                    Bot const& b = run.sides[side];
                    if (b.over)
                        continue;
                    Trace(bot);
                    if (Creature* dummy = ObjectAccessor::GetCreature(*gm, b.dummy))
                        Attack(bot, dummy);
                    // (a pack keeps a bot in combat: it is only pointed at one of the five when it has no target)
                    else if (run.scenario == Scenario::Pack && !bot->GetVictim())
                        for (ObjectGuid const& guid : b.pack)
                            if (Creature* mob = ObjectAccessor::GetCreature(*gm, guid); mob && mob->IsAlive())
                            {
                                Attack(bot, mob);
                                break;
                            }
                }
            }
            bool const packDone = run.scenario == Scenario::Pack && run.sides[0].over && run.sides[1].over;
            if (!packDone && run.elapsed < (run.warm ? run.fightMs : WarmUpMs) + ReattackMs)
                return;
            if (!run.warm)
            {
                Say(Acore::StringFormat("warm-up: plain {} damage, path {} damage (not counted)", run.sides[0].fight.damage,
                                        run.sides[1].fight.damage));
                EndFight();
                run.warm = true;
                run.phase = Phase::Rest;
                run.elapsed = 0;
                return;
            }
            EndFight();
            uint32 const done = uint32(run.sides[0].fights.size());
            SubclassBalance::Fight const& plainLast = run.sides[0].fights.back();
            SubclassBalance::Fight const& pathLast = run.sides[1].fights.back();
            Say(Acore::StringFormat("fight {} (of at least {}): plain {:.1f} DPS, path {:.1f} DPS{}", done, run.fightsWanted,
                                    SubclassBalance::PerSecond(plainLast.damage, plainLast.ms),
                                    SubclassBalance::PerSecond(pathLast.damage, pathLast.ms),
                                    run.scenario == Scenario::Pack ?
                                        Acore::StringFormat(" (plain {:.1f} s{}, path {:.1f} s{})", plainLast.ms / 1000.0f,
                                                            plainLast.deaths ? " and died" : "", pathLast.ms / 1000.0f,
                                                            pathLast.deaths ? " and died" : "") : ""));
            if (done >= run.fightsWanted)
            {
                if (run.scenario == Scenario::Pack)
                {
                    std::string const unfit = SubclassBalance::PackUnfit(SubclassBalance::Summarize(run.sides[0].fights));
                    if (!unfit.empty())
                    {
                        Stop("not measured, " + unfit);
                        return;
                    }
                }
                // Fights on while the difference is too close to the budget to call, up to the cap
                SubclassBalance::Comparison const c =
                    SubclassBalance::Compare(run.sides[0].fights, run.sides[1].fights, run.band);
                if (SubclassBalance::FightOn(c, done, std::max(run.fightsWanted, SubclassBalance::MaxFights)))
                    Say(Acore::StringFormat("{} ± {:.1f}% is too close to call; another fight",
                                            SubclassBalance::HasBand(run.band) ?
                                                Acore::StringFormat("{:.1f}% of plain", c.sharePct) :
                                                Acore::StringFormat("{:+.1f}%", c.dpsDiffPct),
                                            c.marginPct));
                else
                {
                    Report();
                    Stop("done");
                    return;
                }
            }
            run.phase = Phase::Rest;
            run.elapsed = 0;
            return;
        }
        default:
            return;
    }
}

void MeterSpell(Unit* attacker, uint32 damage, SpellInfo const* spellInfo)
{
    if (run.phase != Phase::Fight || !damage || !attacker || !spellInfo)
        return;
    for (Bot& side : run.sides)
        if (attacker->GetGUID() == side.guid && !side.over)
            side.spells[spellInfo->Id] += damage;
}

// What a bot of the run, or its pet, guardian or totem, deals during a fight; and what they take
void Meter(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType type)
{
    if (run.phase != Phase::Fight || !damage)
        return;
    if (attacker)
        if (Player* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself())
            for (Bot& side : run.sides)
                if (owner->GetGUID() == side.guid && !side.over)
                {
                    side.fight.damage += damage;
                    if (attacker != owner)
                        side.fight.petDamage += damage;
                    else if (type == DIRECT_DAMAGE)
                    {
                        side.white += damage;
                        ++side.hits;
                    }
                }
    if (victim)
        for (Bot& side : run.sides)
            if (victim->GetGUID() == side.guid && !side.over)
                side.fight.taken += damage;
}
}

// Damage into the run from outside it: a guard that takes on a pack mob tanks and kills it for the bot (Northshire's
// guards did, so a pack fight measured the guards). It is stopped, counted, and each intruder said once a fight.
bool Intrusion(Unit* attacker, Unit* victim, uint32 damage)
{
    if (run.phase != Phase::Fight || !damage || !attacker || !victim || attacker == victim || OfRun(attacker) ||
        RunFoe(attacker->GetGUID()))
        return false;
    for (Bot& side : run.sides)
    {
        Player* owner = victim->GetCharmerOrOwnerPlayerOrPlayerItself();
        bool const ours = (owner && owner->GetGUID() == side.guid) || victim->GetGUID() == side.dummy ||
                          std::find(side.pack.begin(), side.pack.end(), victim->GetGUID()) != side.pack.end();
        if (!ours)
            continue;
        ++side.outside;
        if (side.intruders.insert(attacker->GetGUID()).second)
            Say(Acore::StringFormat("{} (entry {}) from outside the run hit {}; stopped", attacker->GetName(),
                                    attacker->GetEntry(), victim->GetName()));
        return true;
    }
    return false;
}

class SubclassBalanceUnitScript : public UnitScript
{
public:
    SubclassBalanceUnitScript() : UnitScript("SubclassBalanceUnitScript", true) {}

    // After absorbs, before the victim's AI: a training dummy zeroes what it takes only after this
    uint32 DealDamage(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType type) override
    {
        if (Intrusion(attacker, victim, damage))
            return 0;
        Meter(attacker, victim, damage, type);
        return damage;
    }

    // A heal from outside the run is the passer-by's, not the bot's
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* /*spellInfo*/) override
    {
        if (run.phase != Phase::Fight || !heal || OfRun(healer))
            return;
        if (Player* owner = target ? target->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
            for (Bot& side : run.sides)
                if (owner->GetGUID() == side.guid)
                {
                    Say(Acore::StringFormat("{}: stopped a heal of {} from {}", target->GetName(), heal,
                                            healer ? healer->GetName() : "nothing"));
                    heal = 0;
                    ++side.outside;
                }
    }

    // Which of a bot's spells its damage came from, direct and over time
    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (damage > 0)
            MeterSpell(attacker, uint32(damage), spellInfo);
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        MeterSpell(attacker, damage, spellInfo);
    }
};

class SubclassBalanceWorldScript : public WorldScript
{
public:
    SubclassBalanceWorldScript() : WorldScript("SubclassBalanceWorldScript", {WORLDHOOK_ON_UPDATE}) {}

    void OnUpdate(uint32 diff) override { Tick(diff); }
};

class SubclassBalancePlayerScript : public PlayerScript
{
public:
    SubclassBalancePlayerScript() : PlayerScript("SubclassBalancePlayerScript", {PLAYERHOOK_ON_DUEL_END}) {}

    // A duel of the run's two bots: who won, or a draw when it was called off
    void OnPlayerDuelEnd(Player* winner, Player* loser, DuelCompleteType type) override
    {
        if (run.scenario != Scenario::Duel || run.phase != Phase::Fight || run.duelOver)
            return;
        auto ours = [](Player* player) {
            return player && (player->GetGUID() == run.sides[0].guid || player->GetGUID() == run.sides[1].guid);
        };
        if (!ours(winner) || !ours(loser))
            return;
        if (type == DUEL_WON)
            ++run.wins[SideOf(winner)];
        else
            ++run.draws;
        run.duelOver = true;
    }
};

class SubclassBalanceCommandScript : public CommandScript
{
public:
    SubclassBalanceCommandScript() : CommandScript("SubclassBalanceCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable balanceTable = {
            {"start", HandleStart, SEC_GAMEMASTER, Console::No},
            {"pack", HandlePack, SEC_GAMEMASTER, Console::No},
            {"duel", HandleDuel, SEC_GAMEMASTER, Console::No},
            {"status", HandleStatus, SEC_GAMEMASTER, Console::No},
            {"stop", HandleStop, SEC_GAMEMASTER, Console::No}};
        static ChatCommandTable commandTable = {{"balance", balanceTable}};
        return commandTable;
    }

    static bool Begin(ChatHandler* handler, Scenario scenario, uint16 pathId, uint8 level, uint8 plainSpec,
                      uint8 pathSpec, uint32 fights, uint32 seconds, bool swap,
                      SubclassBalance::Band band = {}, uint32 packSize = SubclassBalance::PackSize)
    {
        if (SubclassBalance::HasBand(band) && (band.lowPct <= 0 || band.lowPct >= band.highPct || band.highPct > 200))
        {
            handler->SendErrorMessage("A band is two percentages, low then high, of the plain spec's damage.", false);
            return false;
        }
        if (run.phase != Phase::Idle)
        {
            handler->SendErrorMessage("A balance run is going; .balance stop ends it.", false);
            return false;
        }
        Player* gm = handler->GetPlayer();
        SubclassPaths::Path const* path = nullptr;
        for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES && !path; ++cls)
            for (SubclassPaths::Path const* candidate : SubclassPaths::ClassPaths(cls))
                if (candidate->id == pathId)
                    path = candidate;
        if (!path)
        {
            handler->SendErrorMessage(Acore::StringFormat("No path {} (.subclass list shows them).", pathId), false);
            return false;
        }
        uint8 const cls = path->rules.classId;
        if (SpecName(cls, plainSpec).empty() || SpecName(cls, pathSpec).empty())
        {
            handler->SendErrorMessage("Spec numbers are the class's premade specs in playerbots.conf.", false);
            return false;
        }
        if (level < path->rules.unlockLevel || level > DEFAULT_MAX_LEVEL)
        {
            handler->SendErrorMessage(Acore::StringFormat("{} opens at level {}.", path->name, path->rules.unlockLevel),
                                      false);
            return false;
        }
        uint32 packEntry = 0;
        if (scenario == Scenario::Pack)
        {
            // (the five come at the bots, and the GM stands 20 yards from each)
            if (!gm->IsGameMaster())
            {
                handler->SendErrorMessage("The pack needs GM mode on (.gm on): its mobs would come for you too.", false);
                return false;
            }
            packEntry = PackEntry(level);
            if (!packEntry)
            {
                handler->SendErrorMessage(Acore::StringFormat("No plain level {} mob for a pack.", level), false);
                return false;
            }
        }
        PlayerbotMgr* mgr = GET_PLAYERBOT_MGR(gm);
        if (!mgr)
        {
            handler->SendErrorMessage("No bot manager for this character.", false);
            return false;
        }
        run = Run{};
        run.scenario = scenario;
        run.gm = gm->GetGUID();
        run.pathId = path->id;
        run.pathName = path->name;
        run.classId = cls;
        run.level = level;
        run.plainSpec = plainSpec;
        run.pathSpec = pathSpec;
        run.fightsWanted = std::max<uint32>(fights, 1);
        run.fightMs = std::clamp<uint32>(seconds, 30, 900) * IN_MILLISECONDS;
        run.packEntry = packEntry;
        run.packSize = std::clamp<uint32>(packSize, 1, 10);
        run.swap = swap;
        run.band = band;
        run.warm = scenario == Scenario::Duel;  // each duel starts fresh: no warm-up
        for (auto itr = mgr->GetPlayerBotsBegin(); itr != mgr->GetPlayerBotsEnd(); ++itr)
            run.before.push_back(itr->first);
        static char const* const names[MAX_CLASSES] = {"", "warrior", "paladin", "hunter", "rogue", "priest",
                                                       "dk", "shaman", "mage", "warlock", "", "druid"};
        // The command is split in place (strtok): each call gets its own copy
        for (int i = 0; i < 2; ++i)
        {
            std::string add = Acore::StringFormat("addclass {}", names[cls]);
            for (std::string const& reply : mgr->HandlePlayerbotCommand(add.data(), gm))
                LOG_INFO("playerbots", "Balance: {}", reply);
        }
        run.phase = Phase::Login;
        std::string const what =
            scenario == Scenario::Duel ?
                Acore::StringFormat("{} duels decided, a draw after {} seconds", run.fightsWanted, run.fightMs / IN_MILLISECONDS) :
                Acore::StringFormat("{} fights of {}{} seconds{}", run.fightsWanted,
                                    scenario == Scenario::Pack ? "at most " : "", run.fightMs / IN_MILLISECONDS,
                                    scenario == Scenario::Pack ?
                                        Acore::StringFormat(" against {} {}", run.packSize,
                                                            sObjectMgr->GetCreatureTemplate(packEntry)->Name) : "");
        handler->PSendSysMessage("Balance: {} at level {}, {}, {}; logging in two bots.", path->name, level,
                                 SubclassBalance::ScenarioName(scenario), what);
        return true;
    }

    static SubclassBalance::Band BandOf(Optional<uint32> low, Optional<uint32> high)
    {
        return low && high ? SubclassBalance::Band{double(*low), double(*high)} : SubclassBalance::Band{};
    }

    static bool HandleStart(ChatHandler* handler, uint16 pathId, uint8 level, uint8 plainSpec, uint8 pathSpec,
                            Optional<uint32> fights, Optional<uint32> seconds, Optional<uint32> swap,
                            Optional<uint32> low, Optional<uint32> high)
    {
        return Begin(handler, Scenario::Dummy, pathId, level, plainSpec, pathSpec,
                     fights.value_or(SubclassBalance::MinFights), seconds.value_or(180), swap.value_or(0) != 0,
                     BandOf(low, high));
    }

    static bool HandlePack(ChatHandler* handler, uint16 pathId, uint8 level, uint8 plainSpec, uint8 pathSpec,
                           Optional<uint32> fights, Optional<uint32> seconds, Optional<uint32> swap,
                           Optional<uint32> low, Optional<uint32> high, Optional<uint32> size)
    {
        return Begin(handler, Scenario::Pack, pathId, level, plainSpec, pathSpec,
                     fights.value_or(SubclassBalance::MinFights), seconds.value_or(180), swap.value_or(0) != 0,
                     BandOf(low, high), size.value_or(SubclassBalance::PackSize));
    }

    static bool HandleDuel(ChatHandler* handler, uint16 pathId, uint8 level, uint8 plainSpec, uint8 pathSpec,
                           Optional<uint32> duels, Optional<uint32> seconds, Optional<uint32> swap)
    {
        return Begin(handler, Scenario::Duel, pathId, level, plainSpec, pathSpec,
                     std::max(duels.value_or(SubclassBalance::MinDuels), SubclassBalance::MinDuels),
                     seconds.value_or(120), swap.value_or(0) != 0);
    }

    static bool HandleStatus(ChatHandler* handler)
    {
        static char const* const phases[] = {"idle", "logging in", "preparing", "fighting", "resting"};
        uint32 const fought = run.scenario == Scenario::Duel ? run.wins[0] + run.wins[1] + run.draws
                                                              : uint32(run.sides[0].fights.size());
        handler->PSendSysMessage("Balance: {}{}", phases[uint8(run.phase)],
                                 run.phase == Phase::Idle ? std::string()
                                     : Acore::StringFormat(", {} {} at level {}, {} {} of {}", run.pathName,
                                                           SubclassBalance::ScenarioName(run.scenario), run.level,
                                                           run.scenario == Scenario::Duel ? "duel" : "fight",
                                                           fought + 1, run.fightsWanted));
        return true;
    }

    static bool HandleStop(ChatHandler* handler)
    {
        if (run.phase == Phase::Idle)
        {
            handler->SendSysMessage("Balance: no run going.");
            return true;
        }
        Stop("stopped");
        return true;
    }
};

void AddSubclassBalanceScripts()
{
    new SubclassBalanceUnitScript();
    new SubclassBalanceWorldScript();
    new SubclassBalancePlayerScript();
    new SubclassBalanceCommandScript();
}
