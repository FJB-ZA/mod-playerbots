/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UBShared.h"
#include "Creature.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <cmath>
#include <list>

namespace
{
bool IsMushroomDangerous(Creature* mushroom)
{
    if (!mushroom)
        return false;

    // Spore Cloud is a persistent area aura: the mushroom owns the cloud but never carries the aura itself
    bool const sporeCloud = mushroom->GetDynObject(UnderbogHungarfen::SPELL_SPORE_CLOUD) != nullptr;
    Aura* grow = mushroom->GetAura(UnderbogHungarfen::SPELL_GROW);
    return Underbog::MushroomDangerous(mushroom->IsAlive(), sporeCloud, grow ? grow->GetStackAmount() : 0);
}
}

namespace UnderbogHungarfen
{
float MaxEffectRadius(uint32 spellId, float fallback)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return fallback;

    float radius = 0.0f;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        radius = std::max(radius, spellInfo->Effects[i].CalcRadius());

    return radius > 0.0f ? radius : fallback;
}

Unit* HungarfenTarget(AiObjectContext* context)
{
    return AI_VALUE2(Unit*, "find target", "hungarfen");
}

ObjectGuid FindHungarfenGuid(Player* bot)
{
    Creature* hungarfen = bot->FindNearestCreature(NPC_HUNGARFEN, Underbog::MushroomScanRange, false);
    return hungarfen ? hungarfen->GetGUID() : ObjectGuid::Empty;
}

bool HungarfenGone(Player* bot, ObjectGuid guid)
{
    Creature* hungarfen = ObjectAccessor::GetCreature(*bot, guid);
    return !hungarfen || !hungarfen->IsAlive();
}

float MushroomDangerRange(Player* bot)
{
    return Underbog::MushroomDangerRange(MaxEffectRadius(SPELL_SPORE_CLOUD, Underbog::SporeCloudRadiusFallback),
                                         bot->GetCombatReach());
}

GuidVector FindMushroomGuids(Player* bot)
{
    std::list<Creature*> mushrooms;
    bot->GetCreatureListWithEntryInGrid(mushrooms, NPC_UNDERBOG_MUSHROOM, Underbog::MushroomScanRange);

    GuidVector guids;
    for (Creature* mushroom : mushrooms)
    {
        if (mushroom->IsAlive())
            guids.push_back(mushroom->GetGUID());
    }

    return guids;
}

bool IsMushroom(Unit* unit)
{
    return unit && unit->GetEntry() == NPC_UNDERBOG_MUSHROOM;
}

bool AnyMushroomAlive(Player* bot, GuidVector const& mushrooms)
{
    for (ObjectGuid guid : mushrooms)
    {
        Creature* mushroom = ObjectAccessor::GetCreature(*bot, guid);
        if (mushroom && mushroom->IsAlive())
            return true;
    }

    return false;
}

Creature* GetNearestDangerousMushroom(Player* bot, GuidVector const& mushrooms, float range)
{
    Creature* best = nullptr;
    float bestDist = range;
    for (ObjectGuid guid : mushrooms)
    {
        Creature* mushroom = ObjectAccessor::GetCreature(*bot, guid);
        if (!IsMushroomDangerous(mushroom))
            continue;

        float const dist = bot->GetDistance2d(mushroom);
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = mushroom;
        }
    }

    return best;
}

Creature* GetNearestUnderbatInLashRange(Player* bot, GuidVector const& attackers)
{
    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (ObjectGuid guid : attackers)
    {
        Unit* unit = ObjectAccessor::GetUnit(*bot, guid);
        Creature* bat = unit ? unit->ToCreature() : nullptr;
        if (!bat || !bat->IsAlive() || bat->GetEntry() != NPC_UNDERBAT)
            continue;

        if (!bat->IsWithinCombatRange(bot, Underbog::UnderbatLashWindow()))
            continue;

        float const dist = bot->GetDistance2d(bat);
        if (!best || dist < bestDist)
        {
            bestDist = dist;
            best = bat;
        }
    }

    return best;
}

bool AnyUnderbatInLashRange(Player* bot, GuidVector const& attackers)
{
    return GetNearestUnderbatInLashRange(bot, attackers) != nullptr;
}

Creature* GetLashingUnderbat(Player* bot, GuidVector const& attackers)
{
    Creature* best = nullptr;
    float bestDist = 0.0f;
    for (ObjectGuid guid : attackers)
    {
        Unit* unit = ObjectAccessor::GetUnit(*bot, guid);
        Creature* bat = unit ? unit->ToCreature() : nullptr;
        if (!bat || !bat->IsAlive() || bat->GetEntry() != NPC_UNDERBAT)
            continue;

        if (!bat->IsWithinCombatRange(bot, Underbog::UnderbatLashWindow()))
            continue;

        if (bat->HasInArc(Underbog::UnderbatSafeArc, bot))
            continue;

        float const dist = bot->GetDistance2d(bat);
        if (!best || dist < bestDist)
        {
            bestDist = dist;
            best = bat;
        }
    }

    return best;
}

Unit* UnderbatRallyUnit(Player* bot, GuidVector const& attackers)
{
    Creature* bat = GetNearestUnderbatInLashRange(bot, attackers);
    if (!bat)
        return nullptr;

    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive() ||
                member->GetMapId() != bot->GetMapId())
                continue;

            if (!PlayerbotAI::IsTank(member))
                continue;

            if (bot->GetExactDist(member) > Underbog::UnderbatRallyMaxRange)
                continue;

            return member;
        }
    }

    Unit* victim = bat->GetVictim();
    if (victim && victim != bot && bot->GetExactDist(victim) <= Underbog::UnderbatRallyMaxRange)
        return victim;

    return nullptr;
}

bool SpotClearOfUnderbats(Player* bot, GuidVector const& attackers, float x, float y, float z)
{
    Position const spot(x, y, z);
    for (ObjectGuid guid : attackers)
    {
        Unit* unit = ObjectAccessor::GetUnit(*bot, guid);
        Creature* bat = unit ? unit->ToCreature() : nullptr;
        if (!bat || !bat->IsAlive() || bat->GetEntry() != NPC_UNDERBAT)
            continue;

        float const window = Underbog::UnderbatLashWindow() + bat->GetCombatReach() + bot->GetCombatReach();
        if (bat->GetExactDist(x, y, z) > window)
            continue;

        if (!bat->HasInArc(Underbog::UnderbatSafeArc, &spot))
            return false;
    }

    return true;
}

bool RetreatPathUnsafe(Player* bot, GuidVector const& mushrooms, float destX, float destY)
{
    float const dangerRange = MushroomDangerRange(bot);
    float const startX = bot->GetPositionX();
    float const startY = bot->GetPositionY();

    for (ObjectGuid guid : mushrooms)
    {
        Creature* mushroom = ObjectAccessor::GetCreature(*bot, guid);
        if (!mushroom || !mushroom->IsAlive())
            continue;

        float const pathDistance = Underbog::PointSegmentDistance(mushroom->GetPositionX(), mushroom->GetPositionY(),
                                                                  startX, startY, destX, destY);
        if (Underbog::RetreatCrossesMushroom(IsMushroomDangerous(mushroom), mushroom->GetExactDist2d(destX, destY),
                                             pathDistance, dangerRange))
            return true;
    }

    return false;
}
}
