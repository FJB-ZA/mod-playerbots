/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PARTYMEMBERVALUE_H
#define PLAYERBOTS_PARTYMEMBERVALUE_H

#include "Player.h"
#include "Value.h"

class PlayerbotAI;

class FindPlayerPredicate
{
public:
    virtual ~FindPlayerPredicate() = default;
    virtual bool Check(Unit* /*unit*/) = 0;
};

class SpellEntryPredicate
{
public:
    virtual bool Check(SpellInfo const* /*spellInfo*/) = 0;
};

class PartyMemberValue : public UnitCalculatedValue
{
public:
    PartyMemberValue(PlayerbotAI* botAI, std::string const name = "party member", int checkInterval = 1)
        : UnitCalculatedValue(botAI, name, checkInterval)
    {
    }

    bool IsTargetOfSpellCast(Player* target, SpellEntryPredicate& predicate);

protected:
    Unit* FindPartyMember(FindPlayerPredicate& predicate, bool ignoreOutOfGroup = false);
    Unit* FindPartyMember(std::vector<Player*>* party, FindPlayerPredicate& predicate);
    virtual bool Check(Unit* player);
};

class PartyMemberMainTankValue : public PartyMemberValue
{
public:
    PartyMemberMainTankValue(PlayerbotAI* botAI) : PartyMemberValue(botAI, "main tank member", 2 * IN_MILLISECONDS) {}
    virtual Unit* Calculate();
};

// The group's main tank wherever it stands on the bot's map: "main tank" is only the one in sight and spell reach,
// a target for heals and buffs. Standing, assisting and holding threat go by the tank out of sight too: on "main tank" a
// healer round a corner from it never stepped back into its sight, the out-of-sight case BackLineTakeSpotAction is
// written for (solo-wow role check, 2026-11-08)
class GroupMainTankValue : public UnitCalculatedValue
{
public:
    GroupMainTankValue(PlayerbotAI* botAI) : UnitCalculatedValue(botAI, "group main tank", 1) {}
    Unit* Calculate() override;
};

#endif
