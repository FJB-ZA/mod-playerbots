/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Krik'thir the Gatewatcher bot tactics: free webbed players, kill the watchers in order, no swaps or area damage.

#include "ANActions.h"
#include "ANMultipliers.h"
#include "ANPolicy.h"
#include "ANTriggers.h"
#include "ANStrategy.h"
#include "ChooseTargetActions.h"
#include "KrikthirSwarmValue.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"

#include <algorithm>

namespace
{
AzjolNerub::Watcher WatcherOf(uint32 entry)
{
    switch (entry)
    {
        case NPC_WATCHER_SKIRMISHER:
            return AzjolNerub::Watcher::Skirmisher;
        case NPC_WATCHER_SHADOWCASTER:
            return AzjolNerub::Watcher::Shadowcaster;
        case NPC_WATCHER_SILTHIK:
        case NPC_WATCHER_GASHRA:
        case NPC_WATCHER_NARJIL:
            return AzjolNerub::Watcher::Named;
        case NPC_WATCHER_WARRIOR:
            return AzjolNerub::Watcher::Warrior;
        default:
            return AzjolNerub::Watcher::None;
    }
}

// Web wraps and Krik'thir aren't on the threat table, so find target can't see them
Unit* FindByEntry(PlayerbotAI* botAI, uint32 entry)
{
    GuidVector targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == entry)
            return unit;
    }
    return nullptr;
}

int PriorityOf(Unit* unit)
{
    return unit && unit->IsAlive() ? AzjolNerub::WatcherPriority(WatcherOf(unit->GetEntry())) : -1;
}

// The in-combat watcher earliest in the kill order. In-combat adds only: a range search would aggro the packs still
// waiting nearby
Unit* BestWatcher(PlayerbotAI* botAI, int& bestPriority)
{
    Unit* best = nullptr;
    bestPriority = -1;
    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get())
    {
        Unit* npc = botAI->GetUnit(guid);
        int const priority = PriorityOf(npc);
        if (priority >= 0 && (!best || priority < bestPriority))
        {
            best = npc;
            bestPriority = priority;
        }
    }
    return best;
}

Unit* Skull(PlayerbotAI* botAI)
{
    Group* group = botAI->GetBot()->GetGroup();
    return group ? botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::skullIndex)) : nullptr;
}

// What the skull should be: the watcher the order takes first, the one already marked while none is strictly before
// it; with no watcher in the fight, Krik'thir once he fights. Nothing is a mark the lead's own marking keeps.
Unit* WantedSkull(PlayerbotAI* botAI)
{
    int bestPriority = -1;
    Unit* best = BestWatcher(botAI, bestPriority);
    Unit* skull = Skull(botAI);
    if (best)
        return AzjolNerub::KeepsWatcher(PriorityOf(skull), bestPriority) ? skull : best;
    Unit* boss = FindByEntry(botAI, NPC_KRIKTHIR);
    return boss && boss->IsAlive() && boss->IsInCombat() ? boss : nullptr;
}
}  // namespace

// Keep tanking the boss and watchers while the tank's area attacks collect the swarm.
void WotlkDungeonANStrategy::AppendTargetExclusions(GuidSet& exclusions, TargetValueExclusionType type)
{
    if (type != TargetValueExclusionType::TankTarget)
        return;
    AiObjectContext* context = botAI->GetAiObjectContext();
    auto const& attackers = AI_VALUE_REF(GuidVector, "attackers");
    bool const fightingBoss = std::any_of(attackers.begin(), attackers.end(), [this](ObjectGuid guid)
    {
        Unit* mob = botAI->GetUnit(guid);
        return mob && mob->GetEntry() == NPC_KRIKTHIR && mob->IsAlive() && mob->GetVictim();
    });
    if (!fightingBoss)
        return;
    for (ObjectGuid guid : attackers)
        if (Unit* mob = botAI->GetUnit(guid); mob && AzjolNerub::SwarmAdd(mob->GetEntry()))
            exclusions.insert(guid);
}

// Gather loose swarmers for one area attack, without chasing each add across the room.
bool KrikthirSwarmValue::Calculate()
{
    if (!bot->IsInCombat() || !bot->GetGroup())
        return false;
    Unit* tank = AI_VALUE(Unit*, "group main tank");
    if (!tank || !tank->IsAlive() || tank->GetMap() != bot->GetMap())
        return false;
    bool const pickingUp = tank == bot;
    if (!pickingUp && bot->GetExactDist2d(tank) <= AzjolNerub::SwarmPickupRange)
        return false;

    unsigned count = 0;
    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* mob = botAI->GetUnit(guid);
        if (!mob || !mob->IsAlive() || !AzjolNerub::SwarmAdd(mob->GetEntry()))
            continue;
        Unit* victim = mob->GetVictim();
        Player* attacked = victim ? victim->ToPlayer() : nullptr;
        if (!attacked || !AzjolNerub::CollectSwarmAdd(pickingUp, attacked == bot, bot->IsInSameGroupWith(attacked),
                                                      bot->IsWithinDistInMap(mob, AzjolNerub::SwarmPickupRange)))
            continue;
        if (++count >= AzjolNerub::LooseSwarm)
            return true;
    }
    return false;
}

bool KrikthirSwarmTrigger::IsActive()
{
    return (AI_VALUE(Unit*, "group main tank") == bot) == _pickup && AI_VALUE(bool, "krik'thir loose swarm");
}

bool KrikthirBringSwarmAction::Execute(Event /*event*/)
{
    Unit* tank = AI_VALUE(Unit*, "group main tank");
    if (!tank || tank == bot || !tank->IsAlive() || tank->GetMap() != bot->GetMap() ||
        bot->GetExactDist2d(tank) <= AzjolNerub::SwarmPickupRange || bot->IsNonMeleeSpellCast(false, false, true))
        return false;
    return MoveTo(tank, AzjolNerub::SwarmPickupRange / 2.0f, MovementPriority::MOVEMENT_COMBAT);
}

bool KrikthirMarkTrigger::IsActive()
{
    return PlayerbotAI::IsTank(bot) && bot->IsInCombat() && bot->GetGroup() && FindByEntry(botAI, NPC_KRIKTHIR);
}

// Each damage dealer held the watcher order on its own while the tank's target and the lead's skull were picked by
// other measures: the role check counted them off the tank's target for 60% of the samples at Krik'thir (heroic
// Azjol-Nerub, 2026-11-09). One skull, on the watcher the order wants, is the target all of them share.
bool KrikthirMarkAction::isUseful()
{
    Unit* wanted = WantedSkull(botAI);
    return wanted && wanted != Skull(botAI);
}

bool KrikthirMarkAction::Execute(Event /*event*/)
{
    Unit* wanted = WantedSkull(botAI);
    if (!wanted)
        return false;
    bot->GetGroup()->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), wanted->GetGUID());
    return true;
}

bool KrikthirWebWrapTrigger::IsActive() { return PlayerbotAI::IsDps(bot) && FindByEntry(botAI, NPC_WEB_WRAP); }

bool KrikthirWatchersTrigger::IsActive() { return PlayerbotAI::IsDps(bot) && FindByEntry(botAI, NPC_KRIKTHIR); }

bool AttackWebWrapAction::isUseful() { return AzjolNerub::AttacksWebWrap(PlayerbotAI::IsHeal(bot)); }

bool AttackWebWrapAction::Execute(Event /*event*/)
{
    Unit* webWrap = FindByEntry(botAI, NPC_WEB_WRAP);
    if (!webWrap || AI_VALUE(Unit*, "current target") == webWrap)
        return false;
    return Attack(webWrap);
}

bool WatchersTargetAction::isUseful() { return !PlayerbotAI::IsHeal(bot); }

bool WatchersTargetAction::Execute(Event /*event*/)
{
    // Web wraps always come first
    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (currentTarget && currentTarget->GetEntry() == NPC_WEB_WRAP)
        return false;

    int bestPriority = -1;
    Unit* best = BestWatcher(botAI, bestPriority);
    if (!best)
        return false;

    // The skull among the equals first: it does not reorder, and it is the one the whole group is on
    Unit* skull = Skull(botAI);
    GuidVector const attackers = AI_VALUE(GuidVector, "attackers");
    if (skull && PriorityOf(skull) == bestPriority &&
        std::find(attackers.begin(), attackers.end(), skull->GetGUID()) != attackers.end())
        return currentTarget == skull ? false : Attack(skull);

    // Else already on a watcher as good as any: keep it. Taking the last of equals in a list that reorders, the bots
    // switched target every tick and finished no cast (role check, heroic Azjol-Nerub, 2026-11-09)
    if (AzjolNerub::KeepsWatcher(PriorityOf(currentTarget), bestPriority))
        return false;
    return Attack(best);
}

float KrikthirMultiplier::GetValue(Action* action)
{
    if (!PlayerbotAI::IsDps(bot))
        return 1.0f;

    bool assist = dynamic_cast<DpsAssistAction*>(action) != nullptr;
    bool aoe = action->getThreatType() == Action::ActionThreatType::Aoe;
    if (!assist && !aoe)
        return 1.0f;

    bool boss = false;
    bool watcher = false;
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (ObjectGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;
        // Only while he waits: in combat he has brought in the whole room and calls swarms of 50 and more Skittering
        // Swarmers, which held to single targets killed the group at every try (role check, 2026-11-09)
        boss = boss || (unit->GetEntry() == NPC_KRIKTHIR && !unit->IsInCombat());
        watcher = watcher || WatcherOf(unit->GetEntry()) != AzjolNerub::Watcher::None;
    }

    // On a watcher the watcher order picks the target: an assist turning to the tank's mob undid it every other tick
    Unit* current = AI_VALUE(Unit*, "current target");
    bool const onWatcher = current && current->IsAlive() && WatcherOf(current->GetEntry()) != AzjolNerub::Watcher::None;
    if (assist && onWatcher)
        return 0.0f;
    return AzjolNerub::KrikthirAllows(boss, watcher, assist, aoe) ? 1.0f : 0.0f;
}
