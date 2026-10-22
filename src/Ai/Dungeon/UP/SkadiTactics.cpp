/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Skadi the Ruthless bot tactics: dodge Grauf's freezing clouds in the gauntlet and the whirlwind on the ground.

#include "AttackAction.h"
#include "InstanceScript.h"
#include "MovementActions.h"
#include "Playerbots.h"
#include "UPActions.h"
#include "UPMultipliers.h"
#include "UPPolicy.h"
#include "UPTriggers.h"

namespace
{
// Grauf is a passive vehicle that never joins the threat table, so find target can't see him
Creature* Grauf(Player* bot)
{
    // Grauf lives from instance load, so only the gauntlet itself may turn on the cloud scan
    InstanceScript* instance = bot->GetInstanceScript();
    if (!instance || instance->GetData(DATA_SKADI_THE_RUTHLESS) != IN_PROGRESS)
        return nullptr;
    Creature* grauf = instance->GetCreature(DATA_GRAUF);
    return grauf && grauf->IsAlive() ? grauf : nullptr;
}

bool CloudOnGround(PlayerbotAI* botAI, Player* bot)
{
    GuidVector objects = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest hostile npcs")->Get();
    for (ObjectGuid const& guid : objects)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit->GetEntry() != NPC_BREATH_TRIGGER)
            continue;

        for (auto const& [auraId, application] : unit->GetAppliedAuras())
        {
            Aura* aura = application->GetBase();
            if (aura && aura->GetId() == SPELL_FREEZING_CLOUD)
                return true;
        }
    }
    return false;
}
}  // namespace

bool SkadiFreezingCloudTrigger::IsActive()
{
    Creature* bossMount = Grauf(bot);
    if (!bossMount)
        return false;

    // The breath shows at once; the cloud it leaves only becomes visible 3-5 s later, while already hurting
    bool left = bossMount->HasAura(SPELL_FREEZING_CLOUD_BREATH_L);
    bool right = bossMount->HasAura(SPELL_FREEZING_CLOUD_BREATH_R);
    return UtgardePinnacle::CloudDanger(left, right, !left && !right && CloudOnGround(botAI, bot));
}

bool SkadiWhirlwindTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "skadi the ruthless");
    return boss && boss->HasAura(SPELL_SKADI_WHIRLWIND);
}

bool AvoidFreezingCloudAction::Execute(Event /*event*/)
{
    Unit* closestTrigger = nullptr;
    GuidVector objects = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (ObjectGuid const& guid : objects)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit->GetEntry() == NPC_BREATH_TRIGGER &&
            (!closestTrigger || bot->GetExactDist2d(unit) < bot->GetExactDist2d(closestTrigger)))
            closestTrigger = unit;
    }

    if (!closestTrigger)
        return false;

    float step = UtgardePinnacle::CloudStepAway(bot->GetExactDist2d(closestTrigger->GetPosition()));
    return step > 0.0f && MoveAway(closestTrigger, step);
}

bool AvoidSkadiWhirlwindAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "skadi the ruthless");
    if (!boss)
        return false;

    float step = UtgardePinnacle::WhirlwindStepAway(PlayerbotAI::IsTank(bot), bot->GetExactDist2d(boss->GetPosition()));
    return step > 0.0f && MoveAway(boss, step);
}

float SkadiMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "skadi the ruthless");
    if (!boss)
        return 1.0f;

    Creature* bossMount = Grauf(bot);

    UtgardePinnacle::SkadiAction shape;
    shape.movement = dynamic_cast<MovementAction*>(action) != nullptr;
    shape.avoidWhirlwind = dynamic_cast<AvoidSkadiWhirlwindAction*>(action) != nullptr;
    // Bots otherwise keep trying to reach him in the sky instead of killing the adds on the ground
    shape.attackOnSkadiOrGrauf = dynamic_cast<AttackAction*>(action) &&
                                 (action->GetTarget() == boss || (bossMount && action->GetTarget() == bossMount));

    bool whirlwind = boss->HasAura(SPELL_SKADI_WHIRLWIND);
    return UtgardePinnacle::SkadiAllows(bossMount != nullptr, whirlwind, shape) ? 1.0f : 0.0f;
}
