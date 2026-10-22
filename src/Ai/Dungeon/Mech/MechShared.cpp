/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MechShared.h"

#include <list>
#include <vector>

#include "Cell.h"
#include "CellImpl.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "MechTriggers.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"

namespace
{
constexpr float FlameScanRadius = 100.0f;

// GetEntry is always the normal entry, so the heroic template (21538) needs no lookup of its own.
void CollectFlames(Player* bot, float radius, std::list<Creature*>& out)
{
    bot->GetCreatureListWithEntryInGrid(out, static_cast<uint32>(MechanarIDs::NPC_RAGING_FLAMES), radius);
}
}  // namespace

namespace MechanarFlames
{
Mechanar::Point PositionOf(WorldObject const* object) { return {object->GetPositionX(), object->GetPositionY()}; }

bool HealerHoldsFire(Player* bot)
{
    if (!Mechanar::HealerMayHoldFire(PlayerbotAI::IsHeal(bot), bot->GetHealthPct()))
        return false;
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    return botAI->GetAiObjectContext()->GetValue<Unit*>("party member to heal")->Get() != nullptr;
}

bool TankMustGrabBoss(Player* bot)
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* boss = GetSepethrea(bot);
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    Player* victimPlayer = victim && victim != bot ? victim->ToPlayer() : nullptr;
    return Mechanar::TankMustGrabBoss(true, boss != nullptr, victimPlayer && PlayerbotAI::IsTank(victimPlayer));
}

void CollectAvoidFlames(Player* bot, std::vector<Mechanar::Point>& out)
{
    std::list<Creature*> flames;
    CollectFlames(bot, FlameScanRadius, flames);
    for (Creature* flame : flames)
        if (flame && flame->IsAlive() && flame->GetVictim() != bot)
            out.push_back(PositionOf(flame));
}

void CollectChasingFlames(Player* bot, std::vector<Creature*>& out)
{
    std::list<Creature*> flames;
    CollectFlames(bot, FlameScanRadius, flames);
    for (Creature* flame : flames)
        if (flame && flame->IsAlive() && flame->GetVictim() == bot)
            out.push_back(flame);
}

Creature* GetNearestFlame(Player* bot, float radius)
{
    std::list<Creature*> flames;
    CollectFlames(bot, radius, flames);

    Creature* best = nullptr;
    float bestDist = radius;
    for (Creature* flame : flames)
    {
        if (!flame || !flame->IsAlive())
            continue;
        float const d = bot->GetDistance2d(flame);
        if (d <= bestDist)
        {
            bestDist = d;
            best = flame;
        }
    }
    return best;
}

ObjectGuid FindFixatingFlameGuid(Player* bot)
{
    std::list<Creature*> flames;
    CollectFlames(bot, FlameScanRadius, flames);
    for (Creature* flame : flames)
        if (flame && flame->IsAlive() && flame->GetVictim() == bot)
            return flame->GetGUID();
    return ObjectGuid::Empty;
}

Unit* GetFixatingFlame(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    ObjectGuid guid = botAI->GetAiObjectContext()->GetValue<ObjectGuid>("sepethrea fixating flame")->Get();
    if (guid.IsEmpty())
        return nullptr;

    Unit* flame = ObjectAccessor::GetUnit(*bot, guid);
    if (!flame || !flame->IsAlive() || flame->GetVictim() != bot)
        return nullptr;
    return flame;
}

void CollectTrailPatches(Player* bot, float scanRadius, std::vector<Mechanar::TrailPatch>& out)
{
    std::vector<WorldObject*> objects;
    Acore::AllWorldObjectsInRange check(bot, scanRadius);
    Acore::WorldObjectListSearcher<Acore::AllWorldObjectsInRange> searcher(bot, objects, check,
                                                                           GRID_MAP_TYPE_MASK_DYNAMICOBJECT);
    Cell::VisitObjects(bot, searcher, scanRadius);

    for (WorldObject* object : objects)
    {
        DynamicObject* patch = object->ToDynObject();
        if (!patch || patch->GetSpellId() != static_cast<uint32>(MechanarIDs::SPELL_RAGING_FLAMES_TRAIL))
            continue;
        float radius = patch->GetRadius();
        if (radius <= 0.0f)
            radius = Mechanar::TrailPatchRadius;
        out.push_back({patch->GetPositionX(), patch->GetPositionY(), radius});
    }
}

bool InTrailDanger(Player* bot)
{
    std::vector<Mechanar::TrailPatch> patches;
    CollectTrailPatches(bot, Mechanar::TrailScan, patches);
    return Mechanar::InTrailDanger(PositionOf(bot), patches);
}

bool InTrailDangerCached(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    return botAI->GetAiObjectContext()->GetValue<bool>("sepethrea trail danger")->Get();
}

bool IsFlameNearCached(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    return botAI->GetAiObjectContext()->GetValue<bool>("sepethrea flame near")->Get();
}

Unit* GetSepethrea(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    return botAI->GetAiObjectContext()->GetValue<Unit*>("find target", "nethermancer sepethrea")->Get();
}
}  // namespace MechanarFlames
