// Grand Champions joust bot tactics: take a lance and a mount, then Charge, Thrust, Shield-Breaker and Defend.

#include <algorithm>
#include <cmath>

#include "AttackersValue.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
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

bool BothMountsNear(Player* bot)
{
    return bot->FindNearestCreature(NPC_ARGENT_WARHORSE, 100.0f) &&
           bot->FindNearestCreature(NPC_ARGENT_BATTLEWORG, 100.0f);
}
}  // namespace

bool ToCLanceTrigger::IsActive()
{
    if (bot->GetVehicle() || !BothMountsNear(bot))
        return false;

    if (!bot->HasItemOrGemWithIdEquipped(ITEM_LANCE, 1))
        return true;

    return !bot->HasItemCount(ITEM_LANCE, 1) && bot->FindNearestGameObject(OBJECT_LANCE_RACK, 100.0f);
}

bool ToCUELanceTrigger::IsActive()
{
    if (bot->GetVehicle() || !bot->HasItemOrGemWithIdEquipped(ITEM_LANCE, 1))
        return false;

    return !BothMountsNear(bot);
}

bool ToCMountedTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return vehicleBase && bot->GetVehicle() && IsJoustMount(vehicleBase->GetEntry());
}

bool ToCMountNearTrigger::IsActive() { return !bot->GetVehicle() && BothMountsNear(bot); }

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
    float z = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(x, y, z);
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

// Prefers the faction's own mount, the only one that can trample a dismounted champion.
Unit* ToCMountAction::FindMount(PlayerbotAI* botAI)
{
    Player* player = botAI->GetBot();
    Unit* preferredMount = nullptr;
    Unit* otherMount = nullptr;
    uint32 preferredEntry = FactionMountEntry(player);
    GuidVector npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest vehicles")->Get();
    for (ObjectGuid const& guid : npcs)
    {
        Unit* vehicleBase = botAI->GetUnit(guid);
        if (!vehicleBase || vehicleBase->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || !vehicleBase->IsFriendlyTo(player))
            continue;

        if (!vehicleBase->GetVehicleKit() || !vehicleBase->GetVehicleKit()->GetAvailableSeatCount())
            continue;

        uint32 entry = vehicleBase->GetEntry();
        if (!IsJoustMount(entry))
            continue;

        Unit*& slot = entry == preferredEntry ? preferredMount : otherMount;
        if (!slot || player->GetExactDist(slot) > player->GetExactDist(vehicleBase))
            slot = vehicleBase;
    }

    return preferredMount ? preferredMount : otherMount;
}

bool ToCMountAction::EnterVehicle(Unit* vehicleBase, bool moveIfFar)
{
    float dist = bot->GetDistance(vehicleBase);
    if (dist > INTERACTION_DISTANCE && !moveIfFar)
        return false;

    if (dist > INTERACTION_DISTANCE)
        return MoveTo(vehicleBase);

    botAI->RemoveShapeshift();
    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    vehicleBase->HandleSpellClick(bot);

    if (!bot->IsOnVehicle(vehicleBase))
        return false;

    // Bots can enter the vehicle while on their own mount
    WorldPacket emptyPacket;
    bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
    return true;
}

float ToCJoustMultiplier::GetValue(Action* action)
{
    Toc::JoustMoment fight;
    fight.combat = botAI->GetState() == BOT_STATE_COMBAT;
    if (!fight.combat)
        return 1.0f;

    Unit* vehicleBase = bot->GetVehicleBase();
    fight.onVehicle = vehicleBase != nullptr;
    fight.onJoustMount = vehicleBase && IsJoustMount(vehicleBase->GetEntry());

    Toc::JoustAction shape;
    shape.setup = dynamic_cast<ToCMountAction*>(action) || dynamic_cast<ToCLanceAction*>(action) ||
                  dynamic_cast<ToCUELanceAction*>(action) || dynamic_cast<WipeAction*>(action);
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.mountedJoust = dynamic_cast<ToCMountedAction*>(action) != nullptr;

    // The mount scan is cached, so only an unmounted bot that isn't getting ready pays for it
    if (!fight.onVehicle && !shape.setup)
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
