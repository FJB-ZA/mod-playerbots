/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MECHSHARED_H
#define PLAYERBOTS_MECHSHARED_H

#include <vector>

#include "MechPolicy.h"
#include "ObjectGuid.h"

class Creature;
class Player;
class Unit;
class WorldObject;

namespace MechanarFlames
{
Mechanar::Point PositionOf(WorldObject const* object);

void CollectTrailPatches(Player* bot, float scanRadius, std::vector<Mechanar::TrailPatch>& out);

bool InTrailDanger(Player* bot);

bool InTrailDangerCached(Player* bot);

bool IsFlameNearCached(Player* bot);

bool HealerHoldsFire(Player* bot);

bool TankMustGrabBoss(Player* bot);

Creature* GetNearestFlame(Player* bot, float radius);

// Every live flame not chasing this bot.
void CollectAvoidFlames(Player* bot, std::vector<Mechanar::Point>& out);

void CollectChasingFlames(Player* bot, std::vector<Creature*>& out);

ObjectGuid FindFixatingFlameGuid(Player* bot);

Unit* GetFixatingFlame(Player* bot);

Unit* GetSepethrea(Player* bot);
}  // namespace MechanarFlames

#endif
