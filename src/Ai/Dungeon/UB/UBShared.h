/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_UBSHARED_H
#define PLAYERBOTS_UBSHARED_H

#include "Define.h"
#include "ObjectGuid.h"
#include "UBPolicy.h"

class AiObjectContext;
class Creature;
class Player;
class Unit;

namespace UnderbogHungarfen
{
constexpr uint32 NPC_HUNGARFEN = 17770;
constexpr uint32 NPC_UNDERBOG_MUSHROOM = 17990;
constexpr uint32 NPC_UNDERBAT = 17724;

constexpr uint32 SPELL_FOUL_SPORES = 31673;
constexpr uint32 SPELL_GROW        = 31698;
constexpr uint32 SPELL_SPORE_CLOUD = 34168;

float MaxEffectRadius(uint32 spellId, float fallback);

Unit* HungarfenTarget(AiObjectContext* context);

ObjectGuid FindHungarfenGuid(Player* bot);

bool HungarfenGone(Player* bot, ObjectGuid guid);

float MushroomDangerRange(Player* bot);

GuidVector FindMushroomGuids(Player* bot);

bool IsMushroom(Unit* unit);

bool AnyMushroomAlive(Player* bot, GuidVector const& mushrooms);

Creature* GetNearestDangerousMushroom(Player* bot, GuidVector const& mushrooms, float range);

bool RetreatPathUnsafe(Player* bot, GuidVector const& mushrooms, float destX, float destY);

bool AnyUnderbatInLashRange(Player* bot, GuidVector const& attackers);

Creature* GetNearestUnderbatInLashRange(Player* bot, GuidVector const& attackers);

Creature* GetLashingUnderbat(Player* bot, GuidVector const& attackers);

Unit* UnderbatRallyUnit(Player* bot, GuidVector const& attackers);

bool SpotClearOfUnderbats(Player* bot, GuidVector const& attackers, float x, float y, float z);
}

#endif
