// Grand Champions joust bot tactics: take a lance and a mount, then Charge, Thrust, Shield-Breaker and Defend.

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>

#include "ArgentSoldiersValue.h"
#include "AttackersValue.h"
#include "CellImpl.h"
#include "FollowActions.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "TOCActions.h"
#include "TOCMultipliers.h"
#include "TOCPolicy.h"
#include "TOCTriggers.h"
#include "Timer.h"
#include "Vehicle.h"
#include "WipeAction.h"

namespace
{
namespace Toc = TrialOfTheChampion;

Position const ARENA_CENTER = {Toc::ArenaCenterX, Toc::ArenaCenterY, Toc::ArenaCenterZ};

uint32 FactionMountEntry(Player* bot)
{
    return bot->GetTeamId() == TEAM_HORDE ? NPC_ARGENT_BATTLEWORG : NPC_ARGENT_WARHORSE;
}

bool IsJoustMount(uint32 entry) { return entry == NPC_ARGENT_BATTLEWORG || entry == NPC_ARGENT_WARHORSE; }

// The free mount reserved by each rider on foot; bots of one map update together, maps may not
std::mutex keptMountsLock;
std::map<ObjectGuid, ObjectGuid> keptMounts;

ObjectGuid KeptMount(ObjectGuid rider)
{
    std::lock_guard<std::mutex> lock(keptMountsLock);
    auto const it = keptMounts.find(rider);
    return it == keptMounts.end() ? ObjectGuid::Empty : it->second;
}

void KeepMount(ObjectGuid rider, ObjectGuid mount)
{
    std::lock_guard<std::mutex> lock(keptMountsLock);
    if (mount.IsEmpty())
        keptMounts.erase(rider);
    else
        keptMounts[rider] = mount;
}

bool IsEntryIn(std::vector<uint32> const& entries, uint32 entry)
{
    return std::find(entries.begin(), entries.end(), entry) != entries.end();
}

enum JoustSpellSlot : uint8
{
    JOUST_SPELL_DEFEND,
    JOUST_SPELL_CHARGE,
    JOUST_SPELL_THRUST,
    JOUST_SPELL_SHIELD_BREAKER
};

char const* const JOUST_SPELL_NAMES[] = {"Defend", "Charge", "Thrust", "Shield-Breaker"};

uint32 CooldownLeft(Unit* vehicleBase, uint32 spellId)
{
    Creature* creature = vehicleBase->ToCreature();
    return creature ? creature->GetSpellCooldown(spellId) : 0;
}

void ExtendCooldown(Unit* vehicleBase, uint32 spellId, uint32 duration)
{
    if (spellId && duration > CooldownLeft(vehicleBase, spellId))
        vehicleBase->AddSpellCooldown(spellId, 0, duration);
}

// The spell a click on the mount casts: Ride Vehicle (67830 for both joust mounts, from npc_spellclick_spells)
SpellInfo const* RideSpell(Unit* vehicleBase)
{
    SpellClickInfoMapBounds clicks = sObjectMgr->GetSpellClickInfoMapBounds(vehicleBase->GetEntry());
    return clicks.first != clicks.second ? sSpellMgr->GetSpellInfo(clicks.first->second.spellId) : nullptr;
}

bool IsShielded(Unit* unit) { return unit->HasAura(SPELL_BOSS_DEFEND) || unit->HasAura(SPELL_MINIONS_DEFEND); }

uint8 DefendStacks(Unit* unit)
{
    Aura* aura = unit->GetAura(SPELL_DEFEND);
    return aura ? aura->GetStackAmount() : 0;
}

class JoustCreatureCheck
{
public:
    bool operator()(Creature* creature) const
    {
        return IsEntryIn(availableTargets, creature->GetEntry()) && creature->IsAlive() &&
               creature->GetExactDist2d(ARENA_CENTER) <= Toc::ScanRange;
    }
};

// The joust is on while any joust mount is left. Asking for both kinds fails once one kind runs out, and they do over
// wipes: a dismounted champion despawns the battleworg it remounts on (a warhorse in a Horde run) and a dead mount
// never respawns (spawntime 999999). The lance then came off and the mount trigger went quiet, yet the joust
// multiplier, which needs just one free mount, still held the bot on foot with nothing allowed to run.
bool JoustMountNear(Player* bot)
{
    return bot->FindNearestCreature(NPC_ARGENT_WARHORSE, 100.0f) ||
           bot->FindNearestCreature(NPC_ARGENT_BATTLEWORG, 100.0f);
}
}  // namespace

bool ToCLanceTrigger::IsActive()
{
    if (bot->GetVehicle() || !JoustMountNear(bot))
        return false;

    if (!bot->HasItemOrGemWithIdEquipped(ITEM_LANCE, 1))
        return true;

    return !bot->HasItemCount(ITEM_LANCE, 1) && bot->FindNearestGameObject(OBJECT_LANCE_RACK, 100.0f);
}

bool ToCUELanceTrigger::IsActive()
{
    if (bot->GetVehicle() || !bot->HasItemOrGemWithIdEquipped(ITEM_LANCE, 1))
        return false;

    return !JoustMountNear(bot);
}

bool ToCMountedTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return vehicleBase && bot->GetVehicle() && IsJoustMount(vehicleBase->GetEntry());
}

// The same free-mount test as the joust multiplier, so a bot it holds on foot is always one this sends to a mount
bool ToCMountNearTrigger::IsActive() { return !bot->GetVehicle() && ToCMountAction::FindMount(botAI); }

bool ToCLanceAction::Execute(Event /*event*/)
{
    if (bot->HasItemOrGemWithIdEquipped(ITEM_LANCE, 1))
        return false;

    Item* lanceItem = nullptr;
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END && !lanceItem; slot++)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetEntry() == ITEM_LANCE)
            lanceItem = item;
    }

    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END && !lanceItem; bag++)
    {
        Bag* pBag = bot->GetBagByPos(bag);
        if (!pBag)
            continue;

        for (uint8 slot = 0; slot < pBag->GetBagSize() && !lanceItem; slot++)
        {
            Item* item = bot->GetItemByPos(bag, slot);
            if (item && item->GetEntry() == ITEM_LANCE)
                lanceItem = item;
        }
    }

    if (lanceItem)
    {
        if (Item* oldWeapon = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
            bot->SwapItem(oldWeapon->GetPos(), lanceItem->GetPos());
        else
            bot->EquipItem(EQUIPMENT_SLOT_MAINHAND, lanceItem, true);

        return true;
    }

    if (bot->HasItemCount(ITEM_LANCE, 1))
        return false;

    GameObject* lanceRack = bot->FindNearestGameObject(OBJECT_LANCE_RACK, 100.0f);
    if (!lanceRack)
        return false;

    if (!lanceRack->IsWithinDistInMap(bot, INTERACTION_DISTANCE))
        return MoveTo(lanceRack, INTERACTION_DISTANCE);

    botAI->RemoveShapeshift();
    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    lanceRack->Use(bot);
    return false;
}

bool ToCUELanceAction::Execute(Event event)
{
    if (!bot->HasItemOrGemWithIdEquipped(ITEM_LANCE, 1))
        return false;

    EquipUpgradeAction equipUpgradeAction(botAI);
    equipUpgradeAction.Execute(event);
    return false;
}

void ToCMountedAction::ScanArena(std::vector<Unit*>& hostiles, std::vector<Unit*>& walking)
{
    std::vector<Creature*> creatures;
    JoustCreatureCheck check;
    Acore::CreatureListSearcher<JoustCreatureCheck> searcher(bot, creatures, check);
    Cell::VisitObjects(ARENA_CENTER.GetPositionX(), ARENA_CENTER.GetPositionY(), bot->GetMap(), searcher,
                       Toc::ScanRange);

    for (Creature* creature : creatures)
    {
        if (IsEntryIn(availableChampions, creature->GetEntry()) &&
            !creature->GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID) && creature->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            walking.push_back(creature);
        else if (!creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NON_ATTACKABLE_2 |
                                                          UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_NOT_SELECTABLE))
            hostiles.push_back(creature);
    }
}

Unit* ToCMountedAction::FindJoustTarget(std::vector<Unit*>& hostiles)
{
    std::sort(hostiles.begin(), hostiles.end(),
              [](Unit* a, Unit* b)
              {
                  if (a->GetHealth() != b->GetHealth())
                      return a->GetHealth() < b->GetHealth();

                  return a->GetGUID() < b->GetGUID();
              });

    for (Unit* unit : hostiles)
    {
        if (AttackersValue::IsPossibleTarget(unit, bot))
            return unit;
    }

    return nullptr;
}

std::vector<ObjectGuid> ToCMountedAction::MountedRiders(bool tramplersOnly)
{
    std::vector<ObjectGuid> riders;
    uint32 mountEntry = FactionMountEntry(bot);
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->GetMapId() != bot->GetMapId())
                continue;

            if (tramplersOnly && !GET_PLAYERBOT_AI(member))
                continue;

            Unit* base = member->GetVehicleBase();
            if (!base)
                continue;

            uint32 entry = base->GetEntry();
            if (tramplersOnly ? entry != mountEntry : !IsJoustMount(entry))
                continue;

            riders.push_back(member->GetGUID());
        }
    }
    else
        riders.push_back(bot->GetGUID());

    std::sort(riders.begin(), riders.end());
    return riders;
}

// The core only lets the faction's own mount trample a dismounted champion: each such rider takes one.
Unit* ToCMountedAction::FindTrampleTarget(std::vector<Unit*>& walking)
{
    if (walking.empty())
        return nullptr;

    std::sort(walking.begin(), walking.end(), [](Unit* a, Unit* b) { return a->GetGUID() < b->GetGUID(); });

    std::vector<ObjectGuid> riders = MountedRiders(true);
    for (size_t i = 0; i < riders.size() && i < walking.size(); ++i)
    {
        if (riders[i] == bot->GetGUID())
            return walking[i];
    }

    return nullptr;
}

size_t ToCMountedAction::RiderLane(std::vector<ObjectGuid> const& riders)
{
    size_t lane = std::find(riders.begin(), riders.end(), bot->GetGUID()) - riders.begin();
    return lane < riders.size() ? lane : 0;
}

uint32 ToCMountedAction::JoustSpellId(Unit* vehicleBase, uint8 slot)
{
    if (spellIdsBase != vehicleBase->GetGUID())
    {
        spellIdsBase = vehicleBase->GetGUID();
        for (uint8 i = 0; i < 4; ++i)
            spellIds[i] = AI_VALUE2(uint32, "vehicle spell id", JOUST_SPELL_NAMES[i]);
    }

    return spellIds[slot];
}

bool ToCMountedAction::CastJoustSpell(uint32 spellId, Unit* target)
{
    if (!botAI->CanCastVehicleSpell(spellId, target) || !botAI->CastVehicleSpell(spellId, target))
        return false;

    Unit* vehicleBase = bot->GetVehicleBase();
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!vehicleBase || !spellInfo)
        return true;

    if (uint32 category = spellInfo->GetCategory())
    {
        for (uint8 slot = 0; slot < 4; ++slot)
        {
            SpellInfo const* other = sSpellMgr->GetSpellInfo(JoustSpellId(vehicleBase, slot));
            if (other && other->GetCategory() == category)
                ExtendCooldown(vehicleBase, other->Id, spellInfo->CategoryRecoveryTime);
        }
    }

    ExtendCooldown(vehicleBase, spellId, spellInfo->RecoveryTime);
    return true;
}

bool ToCMountedAction::AimAndCast(Unit* vehicleBase, uint32 spellId, Unit* target)
{
    if (!spellId || vehicleBase->HasSpellCooldown(spellId))
        return false;

    if (vehicleBase->HasInArc(CAST_ANGLE_IN_FRONT, target))
        return CastJoustSpell(spellId, target);

    if (!botAI->CanCastVehicleSpell(spellId, target))
        return false;

    vehicleBase->GetMotionMaster()->Clear();
    vehicleBase->StopMoving();
    vehicleBase->SetFacingToObject(target);
    moveMode = JOUST_MOVE_NONE;
    return true;
}

bool ToCMountedAction::DelaysCharge(Unit* vehicleBase, uint32 spellId, uint32 chargeWait)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    SpellInfo const* chargeInfo = sSpellMgr->GetSpellInfo(JoustSpellId(vehicleBase, JOUST_SPELL_CHARGE));
    if (!spellInfo || !chargeInfo || !spellInfo->GetCategory() || spellInfo->GetCategory() != chargeInfo->GetCategory())
        return false;

    return spellInfo->CategoryRecoveryTime > chargeWait;
}

bool ToCMountedAction::CastDefend(Unit* vehicleBase, uint8 belowStacks, uint32 chargeWait)
{
    uint32 defendId = JoustSpellId(vehicleBase, JOUST_SPELL_DEFEND);
    return Toc::NeedsDefend(std::max(DefendStacks(bot), DefendStacks(vehicleBase)), belowStacks) &&
           !vehicleBase->HasSpellCooldown(defendId) && !DelaysCharge(vehicleBase, defendId, chargeWait) &&
           CastJoustSpell(defendId, vehicleBase);
}

bool ToCMountedAction::RefreshDefend(Unit* vehicleBase, uint32 now)
{
    if (now < defendCheckAt)
        return false;

    defendCheckAt = now + Toc::DefendCheckMs;
    return CastDefend(vehicleBase, Toc::DefendMaxStacks, UINT32_MAX);
}

bool ToCMountedAction::UseFillers(Unit* vehicleBase, Unit* target, float dist, uint32 chargeWait)
{
    if (CastDefend(vehicleBase, 1, chargeWait))
        return true;

    if (Toc::BreaksShield(dist, IsShielded(target)))
    {
        uint32 breakerId = JoustSpellId(vehicleBase, JOUST_SPELL_SHIELD_BREAKER);
        if (!DelaysCharge(vehicleBase, breakerId, chargeWait) && AimAndCast(vehicleBase, breakerId, target))
            return true;
    }

    return CastDefend(vehicleBase, Toc::DefendMaxStacks, chargeWait);
}

void ToCMountedAction::FleeDestination(Unit* target, std::vector<Unit*> const& hostiles, float& x, float& y)
{
    std::vector<Unit*> nearby;
    float cx = 0.0f;
    float cy = 0.0f;
    for (Unit* unit : hostiles)
    {
        if (bot->GetDistance2d(unit) > Toc::ChargeMax)
            continue;

        nearby.push_back(unit);
        cx += unit->GetPositionX();
        cy += unit->GetPositionY();
    }

    if (nearby.empty())
    {
        nearby.push_back(target);
        cx = target->GetPositionX();
        cy = target->GetPositionY();
    }
    else
    {
        cx /= nearby.size();
        cy /= nearby.size();
    }

    Position const centroid(cx, cy);
    float bx = bot->GetPositionX();
    float by = bot->GetPositionY();
    float away;
    if (bot->GetExactDist2d(centroid) > 1.0f)
        away = centroid.GetAngle(bot);
    else if (bot->GetDistance2d(target) > 1.0f)
        away = target->GetAngle(bot);
    else
        away = bot->GetOrientation() + Toc::Pi;

    std::vector<ObjectGuid> riders = MountedRiders(false);
    if (riders.empty())
        riders.push_back(bot->GetGUID());

    away += Toc::FleeLaneOffset(static_cast<int>(RiderLane(riders)), static_cast<int>(riders.size()));

    bool found = false;
    bool insideOnly = false;
    for (int step = 0; step <= 6 && !found; ++step)
    {
        for (int sign = 1; sign >= -1 && !found; sign -= 2)
        {
            if (step == 0 && sign < 0)
                continue;

            float bearing = away + sign * step * Toc::FleeCone;
            float px = bx + std::cos(bearing) * Toc::FleeDistance;
            float py = by + std::sin(bearing) * Toc::FleeDistance;
            if (!Toc::InsideArena(ARENA_CENTER.GetExactDist2d(px, py)))
                continue;

            // The first spot inside the arena is the fallback when every direction is blocked
            if (!insideOnly)
            {
                x = px;
                y = py;
                insideOnly = true;
            }

            bool blocked = false;
            for (Unit* unit : nearby)
            {
                if (Toc::BlocksFlee(bot->GetAngle(unit), bearing, bot->GetDistance2d(unit)))
                {
                    blocked = true;
                    break;
                }
            }

            if (blocked)
                continue;

            x = px;
            y = py;
            found = true;
        }
    }

    if (!found && !insideOnly)
    {
        float bearing = bot->GetAngle(&ARENA_CENTER);
        x = bx + std::cos(bearing) * Toc::FleeDistance;
        y = by + std::sin(bearing) * Toc::FleeDistance;
    }
}

bool ToCMountedAction::StartFlee(Unit* target, std::vector<Unit*> const& hostiles, uint32 now)
{
    float x, y;
    FleeDestination(target, hostiles, x, y);
    if (!MoveJoust(JOUST_MOVE_FLEE, x, y, now))
        return false;

    if (!fleeUntil)
        fleeUntil = now + Toc::FleeTimeoutMs;

    return true;
}

void ToCMountedAction::KiteDestination(Unit* target, float& x, float& y)
{
    int lane = static_cast<int>(RiderLane(MountedRiders(false)));
    if (!kiteDir)
        kiteDir = Toc::KiteDirection(lane);

    float radius = Toc::KiteRingRadius(lane);
    float bearing = target->GetExactDist2d(bot) > 0.5f ? target->GetAngle(bot) : bot->GetOrientation() + Toc::Pi;

    for (uint8 attempt = 0; attempt < 2; ++attempt)
    {
        float angle = bearing + kiteDir * Toc::KiteArc / radius;
        x = target->GetPositionX() + std::cos(angle) * radius;
        y = target->GetPositionY() + std::sin(angle) * radius;
        if (ARENA_CENTER.GetExactDist2d(x, y) <= Toc::KiteLimit)
            return;

        kiteDir = -kiteDir;
    }

    float angle = ARENA_CENTER.GetAngle(x, y);
    x = ARENA_CENTER.GetPositionX() + std::cos(angle) * Toc::KiteLimit;
    y = ARENA_CENTER.GetPositionY() + std::sin(angle) * Toc::KiteLimit;
}

bool ToCMountedAction::Kite(Unit* vehicleBase, Unit* target, uint32 now)
{
    if (KeepMoving(vehicleBase, JOUST_MOVE_KITE, now))
        return true;

    float x, y;
    KiteDestination(target, x, y);
    return MoveJoust(JOUST_MOVE_KITE, x, y, now);
}

bool ToCMountedAction::Approach(Unit* vehicleBase, Unit* target, uint32 now)
{
    if (KeepMoving(vehicleBase, JOUST_MOVE_APPROACH, now))
        return true;

    float angle = target->GetAngle(bot);
    return MoveJoust(JOUST_MOVE_APPROACH, target->GetPositionX() + std::cos(angle) * Toc::ChargeIdeal,
                     target->GetPositionY() + std::sin(angle) * Toc::ChargeIdeal, now);
}

bool ToCMountedAction::KeepMoving(Unit* vehicleBase, JoustMove mode, uint32 now)
{
    return moveMode == mode && now < moveRefreshAt && vehicleBase->isMoving() &&
           vehicleBase->GetExactDist2d(&moveDest) > Toc::MoveRefreshDistance;
}

bool ToCMountedAction::MoveJoust(JoustMove mode, float x, float y, uint32 now)
{
    // A rider's Z includes its seat offset. Obtain the arena floor in world coordinates.
    float const z = bot->GetMap()->GetHeight(bot->GetPhaseMask(), x, y, Toc::ArenaCenterZ + 6.0f, true, 12.0f);
    if (std::abs(z - Toc::ArenaCenterZ) > 6.0f)
        return false;
    AI_VALUE(LastMovement&, "last movement").Set(nullptr);
    if (!MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
        return false;

    moveMode = mode;
    moveDest.Relocate(x, y, z);
    moveRefreshAt = now + Toc::MoveRefreshMs;
    return true;
}

bool ToCMountedAction::ChargeCycle(Unit* vehicleBase, Unit* target, std::vector<Unit*> const& hostiles, float dist,
                                   uint32 now)
{
    if (fleeUntil)
    {
        bool done = Toc::FleeDone(dist, now > fleeUntil, vehicleBase->GetExactDist2d(&moveDest));
        if (!done && ((moveMode == JOUST_MOVE_FLEE && vehicleBase->isMoving()) || StartFlee(target, hostiles, now)))
            return true;

        if (dist < Toc::ChargeMin)
            fleeBlockedUntil = now + Toc::FleeTimeoutMs;

        fleeUntil = 0;
    }

    switch (Toc::ChargeStepAt(dist))
    {
        case Toc::ChargeStep::Approach:
            return Approach(vehicleBase, target, now);
        case Toc::ChargeStep::Charge:
            return AimAndCast(vehicleBase, JoustSpellId(vehicleBase, JOUST_SPELL_CHARGE), target);
        default:
            return now > fleeBlockedUntil && StartFlee(target, hostiles, now);
    }
}

bool ToCMountedAction::RideToSpare(Unit* current, uint32 now)
{
    if (!Toc::NeedsFreshMount(current->GetHealthPct()) || !bot->HasAura(SPELL_LANCE_EQUIPPED))
        return false;
    if (now >= spareCheckAt)
    {
        Unit* candidate = ToCMountAction::FindMount(botAI);
        spare = candidate ? candidate->GetGUID() : ObjectGuid::Empty;
        spareCheckAt = now + Toc::SpareMountCheckMs;
    }
    Unit* next = botAI->GetUnit(spare);
    if (!next || !next->IsAlive() || !next->GetVehicleKit() || !next->GetVehicleKit()->GetAvailableSeatCount())
        return false;
    if (bot->GetDistance(next) > INTERACTION_DISTANCE)
    {
        RefreshDefend(current, now);
        return KeepMoving(current, JOUST_MOVE_SPARE, now) ||
               MoveJoust(JOUST_MOVE_SPARE, next->GetPositionX(), next->GetPositionY(), now);
    }

    // A normal spell click moves the rider between vehicles via Unit::_EnterVehicle.
    // A refused click leaves it on its current mount beside the spare.
    if (SpellInfo const* ride = RideSpell(next); ride && !bot->GetGlobalCooldownMgr().HasGlobalCooldown(ride))
    {
        current->StopMoving();
        next->HandleSpellClick(bot);
        if (bot->IsOnVehicle(next))
        {
            LOG_INFO("playerbots", "ToC mount: {} changed to a fresh mount before being unhorsed", bot->GetName());
            KeepMount(bot->GetGUID(), ObjectGuid::Empty);
            spare.Clear();
            moveMode = JOUST_MOVE_NONE;
            return true;
        }
    }
    return RefreshDefend(current, now);
}

bool ToCMountedAction::Execute(Event /*event*/)
{
    Unit* vehicleBase = bot->GetVehicleBase();
    Vehicle* vehicle = bot->GetVehicle();
    if (!vehicleBase || !vehicle)
        return false;

    if (vehicleBase->HasUnitState(UNIT_STATE_CHARGING))
        return true;

    uint32 now = getMSTime();
    if (now < idleUntil)
        return RefreshDefend(vehicleBase, now);

    std::vector<Unit*> hostiles;
    std::vector<Unit*> walking;
    ScanArena(hostiles, walking);

    if (Unit* champion = FindTrampleTarget(walking))
    {
        fleeUntil = 0;
        if (moveMode != JOUST_MOVE_NONE)
        {
            AI_VALUE(LastMovement&, "last movement").Set(nullptr);
            moveMode = JOUST_MOVE_NONE;
        }

        if (bot->GetDistance2d(champion) > Toc::TrampleRange &&
            !IsWaitingForLastMove(MovementPriority::MOVEMENT_COMBAT))
            MoveTo(champion, 1.0f, MovementPriority::MOVEMENT_COMBAT);

        RefreshDefend(vehicleBase, now);
        return true;
    }

    if (RideToSpare(vehicleBase, now))
        return true;

    Unit* target = FindJoustTarget(hostiles);
    if (!target)
    {
        fleeUntil = 0;
        kiteDir = 0;
        idleUntil = now + Toc::IdleScanMs;
        return RefreshDefend(vehicleBase, now);
    }

    uint32 chargeId = JoustSpellId(vehicleBase, JOUST_SPELL_CHARGE);
    uint32 chargeWait = chargeId ? CooldownLeft(vehicleBase, chargeId) : UINT32_MAX;
    float dist = bot->GetDistance2d(target);

    if (Toc::InThrustRange(dist) && vehicleBase->HasInArc(CAST_ANGLE_IN_FRONT, target) &&
        CastJoustSpell(JoustSpellId(vehicleBase, JOUST_SPELL_THRUST), target))
        return true;

    if (chargeWait)
        fleeUntil = 0;
    else if (ChargeCycle(vehicleBase, target, hostiles, dist, now))
        return true;

    if (UseFillers(vehicleBase, target, dist, chargeWait))
        return true;

    return Kite(vehicleBase, target, now);
}

bool ToCMountAction::Execute(Event /*event*/)
{
    // Already riding: don't switch vehicles
    if (bot->GetVehicle())
        return false;

    Unit* vehicleToEnter = FindMount(botAI);
    if (!vehicleToEnter)
        return false;

    return EnterVehicle(vehicleToEnter, true);
}

// Each bot on foot its own free mount (Toc::AssignedMount), the faction's own preferred: only it can trample a
// dismounted champion.
Unit* ToCMountAction::FindMount(PlayerbotAI* botAI)
{
    Player* player = botAI->GetBot();
    if (InstanceScript* instance = player->GetInstanceScript(); !instance || instance->GetBossState(0) == DONE)
        return nullptr;
    bool const replacing = player->GetVehicle() != nullptr;
    // A rider on foot has first claim. Mounted riders below the replacement threshold
    // reserve among themselves only while every living rider has a seat.
    std::vector<Player*> riders{player};
    if (Group* group = player->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == player || !member->IsAlive() || member->GetMap() != player->GetMap() ||
                !GET_PLAYERBOT_AI(member))
                continue;
            Unit* base = member->GetVehicleBase();
            if (replacing && !base)
                return nullptr;
            if (!base || (replacing && IsJoustMount(base->GetEntry()) && Toc::NeedsFreshMount(base->GetHealthPct())))
                riders.push_back(member);
        }

    auto byGuid = [](Unit* a, Unit* b) { return a->GetGUID() < b->GetGUID(); };
    std::sort(riders.begin(), riders.end(), byGuid);
    size_t const rider = std::find(riders.begin(), riders.end(), player) - riders.begin();

    GuidVector candidates = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest vehicles")->Get();
    GuidVector reservations;
    for (Player* member : riders)
    {
        auto const guid = KeptMount(member->GetGUID());
        reservations.push_back(guid);
        if (guid)
            candidates.push_back(guid);
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    std::vector<Unit*> mounts;
    for (auto guid : candidates)
    {
        Unit* mount = botAI->GetUnit(guid);
        if (mount && mount->IsAlive() && mount->GetMap() == player->GetMap() && IsJoustMount(mount->GetEntry()) &&
            mount->IsFriendlyTo(player) && !mount->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
            mount->GetVehicleKit() && mount->GetVehicleKit()->GetAvailableSeatCount() &&
            (!replacing || Toc::HealthySpare(mount->GetHealthPct())))
            mounts.push_back(mount);
    }
    std::vector<int> kept(riders.size(), -1);
    std::vector<std::vector<float>> distance(riders.size(), std::vector<float>(mounts.size()));
    for (size_t r = 0; r < riders.size(); ++r)
        for (size_t m = 0; m < mounts.size(); ++m)
        {
            if (mounts[m]->GetGUID() == reservations[r])
                kept[r] = static_cast<int>(m);
            bool const otherFaction = mounts[m]->GetEntry() != FactionMountEntry(riders[r]);
            distance[r][m] = Toc::MountCost(riders[r]->GetExactDist(mounts[m]), otherFaction && !riders[r]->IsInCombat());
        }
    int const mine = Toc::AssignedMount(distance, rider, kept);
    KeepMount(player->GetGUID(), mine >= 0 ? mounts[mine]->GetGUID() : ObjectGuid::Empty);
    return mine >= 0 ? mounts[mine] : nullptr;
}

bool ToCMountAction::EnterVehicle(Unit* vehicleBase, bool moveIfFar)
{
    float dist = bot->GetDistance(vehicleBase);
    if (dist > INTERACTION_DISTANCE && !moveIfFar)
        return false;

    // Keep the tick while still on the way, so a heal or a cast the multiplier lets through doesn't halt the run
    if (dist > INTERACTION_DISTANCE)
    {
        if (!bot->isMoving() && botAI->CanMove())
            AI_VALUE(LastMovement&, "last movement").clear();
        auto const priority = bot->IsInCombat() ? MovementPriority::MOVEMENT_COMBAT : MovementPriority::MOVEMENT_NORMAL;
        bool const moving = MoveTo(vehicleBase, INTERACTION_DISTANCE / 2.0f, priority) || bot->isMoving();
        NoteAttempt(vehicleBase, dist, moving ? "on the way" : "can't move to it");
        return moving;
    }

    // The mount turns away a rider without the lance's aura (npc_toc5_player_vehicle::BeforeSpellClick)
    if (!bot->HasAura(SPELL_LANCE_EQUIPPED))
    {
        NoteAttempt(vehicleBase, dist, "no Lance Equipped aura");
        return false;
    }

    // Ride Vehicle is on the global cooldown, so a click in the 1.5 s after a heal fails SPELL_FAILED_NOT_READY. A bot
    // that then heals again restarts it every tick and never boards: hold the tick until the cooldown runs out.
    SpellInfo const* ride = RideSpell(vehicleBase);
    if (ride && bot->GetGlobalCooldownMgr().HasGlobalCooldown(ride))
    {
        NoteAttempt(vehicleBase, dist, "waiting out the global cooldown");
        return true;
    }

    botAI->RemoveShapeshift();
    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    bool const clicked = vehicleBase->HandleSpellClick(bot);

    if (!bot->IsOnVehicle(vehicleBase))
    {
        NoteAttempt(vehicleBase, dist, clicked ? "ride cast failed" : "spell click refused");
        return false;
    }

    KeepMount(bot->GetGUID(), ObjectGuid::Empty);
    NoteAttempt(vehicleBase, dist, "boarded");

    // Bots can enter the vehicle while on their own mount
    WorldPacket emptyPacket;
    bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
    return true;
}

// One line each time the mount or the outcome changes, to show what keeps a bot on foot from boarding mid-fight
void ToCMountAction::NoteAttempt(Unit* vehicleBase, float dist, std::string const& outcome)
{
    if (vehicleBase->GetGUID() == lastMount && outcome == lastOutcome)
        return;

    lastMount = vehicleBase->GetGUID();
    lastOutcome = outcome;
    LOG_INFO("playerbots",
             "ToC mount: {} to {} {} at {:.1f} yd: {}; in combat {}, health {:.0f}%, lance equipped {}, stunned {}",
             bot->GetName(), vehicleBase->GetName(), vehicleBase->GetGUID().GetCounter(), dist, outcome,
             bot->IsInCombat(), bot->GetHealthPct(), bot->HasItemOrGemWithIdEquipped(ITEM_LANCE, 1),
             bot->HasUnitState(UNIT_STATE_STUNNED));
}

float ToCJoustMultiplier::GetValue(Action* action)
{
    Toc::JoustMoment fight;
    fight.combat = botAI->GetState() == BOT_STATE_COMBAT;
    Unit* vehicleBase = bot->GetVehicleBase();
    fight.onVehicle = vehicleBase != nullptr;
    fight.onJoustMount = vehicleBase && IsJoustMount(vehicleBase->GetEntry());
    if (!fight.combat && !fight.onJoustMount)
    {
        // Out of a fight a follow would walk the bot back in among the waiting Argent soldiers (KeepsOffSoldiers)
        if (dynamic_cast<FollowAction*>(action) && !bot->GetVehicle())
        {
            ToCSoldiers const soldiers = AI_VALUE(ToCSoldiers, "toc argent soldiers");
            if (soldiers.nearest &&
                Toc::KeepsOffSoldiers(soldiers.fighting, soldiers.botDistance, soldiers.masterDistance))
                return 0.0f;
        }
        return 1.0f;
    }

    Toc::JoustAction shape;
    shape.setup = dynamic_cast<ToCMountAction*>(action) || dynamic_cast<ToCLanceAction*>(action) ||
                  dynamic_cast<ToCUELanceAction*>(action) || dynamic_cast<WipeAction*>(action);
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.mountedJoust = dynamic_cast<ToCMountedAction*>(action) != nullptr;

    // The mount scan is cached, so only an unmounted bot about to move somewhere else pays for it
    if (!fight.onVehicle && !shape.setup && shape.movement)
        fight.spareMount = SpareMountNearby();

    return Toc::JoustAllows(fight, shape) ? 1.0f : 0.0f;
}

bool ToCJoustMultiplier::SpareMountNearby()
{
    uint32 now = getMSTime();
    if (now >= mountCheckAt)
    {
        spareMount = ToCMountAction::FindMount(botAI) != nullptr;
        mountCheckAt = now + Toc::SpareMountCheckMs;
    }

    return spareMount;
}
