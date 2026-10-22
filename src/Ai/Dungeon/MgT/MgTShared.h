/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MGTSHARED_H
#define PLAYERBOTS_MGTSHARED_H

#include <string>
#include <vector>

#include "Define.h"
#include "MgTPolicy.h"
#include "ObjectGuid.h"

class Action;
class Player;
class PlayerbotAI;
class Unit;

namespace MagistersTerrace
{
constexpr uint32 MAP_MAGISTERS_TERRACE = 585;

// Shared glue (MgTShared.cpp)

void CollectCreaturesByEntry(Player* bot, std::vector<uint32> const& entries, float radius, std::vector<Unit*>& out);
void CollectAttackersByEntry(Player* bot, std::vector<uint32> const& entries, float radius, std::vector<Unit*>& out);
void CollectHazardEscapes(Player* bot, std::vector<Hazard> const& hazards, float anchorGap, EscapeSpots& out);
void CollectRingEscapes(Player* bot, Unit* anchor, std::vector<Hazard> const& hazards, EscapeSpots& out);

uint8 BestCastPriority(Unit* caster, uint8 (*priority)(uint32));
std::string CastInterrupt(PlayerbotAI* botAI, Unit* target, uint8 urgency);

Unit* PickRankedFocus(Player* bot, uint8 (*rank)(uint32), std::vector<Unit*> const& extra, ObjectGuid& latched);
ObjectGuid ResolveFocusOrder(PlayerbotAI* botAI);
FocusOrder ResolveFocusOrderOwner(PlayerbotAI* botAI);
void CollectFocusExclusions(Player* bot, std::vector<ObjectGuid>& out);
bool ActionPullsOffFocus(Player* bot, Action* action);

// Selin Fireheart (SelinTactics.cpp)

Unit* GetSelin(Player* bot);
Unit* GetActiveFelCrystal(Player* bot);

// Trash (TrashTactics.cpp)

void CollectDampeningEscapes(Player* bot, EscapeSpots& out);
Unit* GetGlaiveThrowingMageGuard(Player* bot);
void CollectNovaEscapes(Player* bot, EscapeSpots& out);
Unit* GetInterruptTarget(Player* bot);
uint8 GetInterruptUrgency(Unit* caster);
Unit* GetFocusTarget(Player* bot, ObjectGuid& latched);
Unit* GetEnragedWretched(Player* bot);

// Priestess Delrissa (DelrissaTactics.cpp)

bool ShouldHoldTremorTotem(Player* bot);
Unit* GetDelrissaFocusTarget(Player* bot, ObjectGuid& latched);
void CollectDelrissaInterruptPreference(Player* bot, std::vector<Unit*>& out);
void CollectDelrissaPets(Player* bot, std::vector<ObjectGuid>& out);
uint8 GetRetinueInterruptUrgency(Unit* caster);

// Kael'thas Sunstrider (KaelthasTactics.cpp, KaelthasHeroicTactics.cpp)

Unit* GetKaelthas(Player* bot);
void CollectFlameStrikeEscapes(Player* bot, EscapeSpots& out);
void CollectPhoenixEscapes(Player* bot, EscapeSpots& out);
PhoenixRing GetPhoenixRing(Player* bot);
Unit* GetKaelInterruptTarget(Player* bot);
bool IsKaelUnattackable(Player* bot);
Unit* GetKaelFocusTarget(Player* bot);
bool IsKaelShielded(Unit* kael);
Unit* GetShockBarrierKael(Player* bot, Unit* kael);
bool IsGravityLapseActive(Player* bot);
void CollectMeleePhoenixExclusions(Player* bot, std::vector<ObjectGuid>& out);
}  // namespace MagistersTerrace

#endif
