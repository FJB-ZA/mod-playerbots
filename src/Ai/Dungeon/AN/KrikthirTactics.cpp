// Krik'thir the Gatewatcher bot tactics: free webbed players, kill the watchers in order, no swaps or area damage.

#include "ANActions.h"
#include "ANMultipliers.h"
#include "ANPolicy.h"
#include "ANTriggers.h"
#include "ChooseTargetActions.h"
#include "Playerbots.h"

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
}  // namespace

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

    // In-combat adds only: a range search would aggro the packs still waiting nearby
    Unit* best = nullptr;
    int bestPriority = -1;
    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (ObjectGuid const& guid : attackers)
    {
        Unit* npc = botAI->GetUnit(guid);
        if (!npc)
            continue;

        int priority = AzjolNerub::WatcherPriority(WatcherOf(npc->GetEntry()));
        if (priority >= 0 && (!best || priority <= bestPriority))
        {
            best = npc;
            bestPriority = priority;
        }
    }

    // Already on the best target: nothing to send
    return best && currentTarget != best && Attack(best);
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
        boss = boss || unit->GetEntry() == NPC_KRIKTHIR;
        watcher = watcher || WatcherOf(unit->GetEntry()) != AzjolNerub::Watcher::None;
    }

    return AzjolNerub::KrikthirAllows(boss, watcher, assist, aoe) ? 1.0f : 0.0f;
}
