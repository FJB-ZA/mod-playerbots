/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RangeTriggers.h"
#include "HealerSpotPolicy.h"
#include "MoveSplineInit.h"
#include "MovementActions.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "SharedDefines.h"

bool EnemyTooCloseForSpellTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && (target->GetVictim() != bot || target->isFrozen() || target->HasRootAura()) &&
           target->GetObjectSize() <= 10.0f && target->IsWithinCombatRange(bot, MIN_MELEE_REACH);
    //     Unit* target = AI_VALUE(Unit*, "current target");
    //     if (!target)
    //     {
    //         return false;
    //     }

    //     if (target->GetTarget() == bot->GetGUID() && !bot->GetGroup() && !target->HasUnitState(UNIT_STATE_ROOT) &&
    //     GetSpeedInMotion(target) > GetSpeedInMotion(bot) * 0.65f)
    //         return false;

    //     bool isBoss = false;
    //     bool isRaid = false;
    //     float combatReach = bot->GetCombatReach() + target->GetCombatReach();
    //     float targetDistance = ServerFacade::instance().GetDistance2d(bot, target) + combatReach;
    //     if (target->IsCreature())
    //     {
    //         Creature* creature = botAI->GetCreature(target->GetGUID());
    //             if (creature)
    //             {
    //                 isBoss = creature->isWorldBoss();
    //             }
    //     }

    //     if (bot->GetMap() && bot->GetMap()->IsRaid())
    //         isRaid = true;

    // //    if (isBoss || isRaid)
    // //        return ServerFacade::instance().IsDistanceLessThan(targetDistance, (sPlayerbotAIConfig.TooCloseDistance +
    // combatReach) / 2);

    //     return ServerFacade::instance().IsDistanceLessOrEqualThan(targetDistance, (sPlayerbotAIConfig.TooCloseDistance +
    //     combatReach / 2));
}

bool BackLineOutOfPlaceTrigger::IsActive()
{
    // The back line: healers and ranged damage dealers. Not while a step is under way, and mid-cast (a step cuts the cast
    // short) only out of a mob's front it stands in: a mage casting back to back never stepped out of the pack the tank
    // brought in (solo-wow role check, 2026-11-07)
    bool const backLine = PlayerbotAI::IsHeal(bot) || (PlayerbotAI::IsRanged(bot) && PlayerbotAI::IsDps(bot));
    if (!backLine || PlayerbotAI::IsTank(bot))
        return false;
    if (bot->isMoving())
    {
        // ...unless the step goes somewhere the tank has left: a spot picked seconds before, with the tank since gone
        // after a mob that ran to a damage dealer, kept the healer 30-60 yards off it for a whole fight (Karazhan,
        // 2026-11-09: 206 samples out of the tank's reach in one fight)
        Unit* tank = AI_VALUE(Unit*, "group main tank");
        float x, y, z;
        if (!tank || tank == bot || !tank->IsAlive() || !tank->IsInCombat() || tank->GetMap() != bot->GetMap() ||
            !bot->GetMotionMaster()->GetDestination(x, y, z))
            return false;
        return tank->GetExactDist2d(x, y) > BotMove::HealerNearTank + 4.0f ||
               (bot->GetExactDist2d(tank) > BotMove::HealerNearTank &&
                !tank->IsWithinLOS(x, y, z + bot->GetCollisionHeight()));
    }
    if (bot->IsNonMeleeSpellCast(false))
    {
        // ...or out of the tank's sight, where no heal reaches it: round the Deadmines' corners a healer casting on the
        // others lost the tank for whole pulls (role check, 2026-11-10: healer back 69%)
        Unit* tank = AI_VALUE(Unit*, "group main tank");
        if (PlayerbotAI::IsHeal(bot) && tank && tank != bot && tank->IsAlive() && tank->IsInCombat() &&
            tank->GetMap() == bot->GetMap() && !bot->IsWithinLOSInMap(tank))
            return true;
        for (ObjectGuid const& guid : AI_VALUE(GuidVector, "attackers"))
        {
            Unit* mob = botAI->GetUnit(guid);
            if (!mob || !mob->IsAlive() || !mob->GetVictim() || mob->GetVictim() == bot || mob->isMoving())
                continue;
            BotMove::FacingMob const standing = StandingOf(mob);
            if (BotMove::InFront(standing, bot->GetPositionX(), bot->GetPositionY()))
                return true;
        }
        return false;
    }
    Unit* tank = AI_VALUE(Unit*, "group main tank");
    // In the fight, or not yet but the tank is (out of combat, BackLineStrategy): the healer's combat starts with its
    // first heal, and until then it stood where the pull left it
    if (!bot->IsInCombat() &&
        !(tank && tank != bot && tank->IsAlive() && tank->IsInCombat() && tank->GetMap() == bot->GetMap() &&
          bot->GetExactDist2d(tank) < 60.0f))
        return false;
    // Too far back is as wrong as too close: a healer left with the puller, out of the tank's reach or round a corner
    // from it (in Karazhan a healer stood out of the tank's sight for most of a fight, solo-wow role check 2026-11-06)
    if (tank && tank != bot && tank->IsAlive() && tank->IsInCombat() && tank->GetMap() == bot->GetMap() &&
        (bot->GetExactDist2d(tank) > BotMove::HealerNearTank || !bot->IsWithinLOSInMap(tank)))
        return true;
    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* mob = botAI->GetUnit(guid);
        if (!mob || !mob->IsAlive() || !mob->GetVictim())
            continue;
        // A mob on the healer itself: to the tank, whose taunt takes it back. Standing still, it beat on the healer
        if (mob->GetVictim() == bot)
        {
            if (tank && tank != bot && tank->IsAlive() && bot->GetExactDist2d(tank) > BotMove::HealerToTank &&
                mob->IsWithinMeleeRange(bot) && !tank->IsWithinMeleeRange(mob))
                return true;
            continue;
        }
        // Where it stands, or will once it reaches whoever it runs at (StandingOf): step aside before the pack arrives
        BotMove::FacingMob const standing = StandingOf(mob);
        if (BotMove::FlatDistance(bot->GetPositionX(), bot->GetPositionY(), standing.x, standing.y) <
                BotMove::HealerTooClose ||
            BotMove::InFront(standing, bot->GetPositionX(), bot->GetPositionY()))
            return true;
    }
    return false;
}

bool EnemyTooCloseForAutoShotTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    // hunter move away after casting immolation/explosive trap
    bool trapToCast = bot->getClass() == CLASS_HUNTER;
    uint32 spellId = AI_VALUE2(uint32, "spell id", "immolation trap");
    if (!spellId)
        trapToCast = false;

    if (spellId && bot->HasSpellCooldown(spellId))
        trapToCast = false;

    return !trapToCast && (target->GetVictim() != bot || target->isFrozen() || target->HasRootAura()) &&
           bot->IsWithinMeleeRange(target);

    // if (target->GetTarget() == bot->GetGUID() && !bot->GetGroup() && !target->HasUnitState(UNIT_STATE_ROOT) &&
    // GetSpeedInMotion(target) > GetSpeedInMotion(bot) * 0.65f)
    //     return false;

    // bool isBoss = false;
    // bool isRaid = false;
    // float combatReach = bot->GetCombatReach() + target->GetCombatReach();
    // float targetDistance = ServerFacade::instance().GetDistance2d(bot, target) + combatReach;
    // if (target->IsCreature())
    // {
    //     Creature* creature = botAI->GetCreature(target->GetGUID());
    //     if (creature)
    //     {
    //         isBoss = creature->isWorldBoss();
    //     }
    // }

    // if (bot->GetMap() && bot->GetMap()->IsRaid())
    //     isRaid = true;

    // return ServerFacade::instance().IsDistanceLessOrEqualThan(targetDistance, 5.0f);
}

bool EnemyTooCloseForShootTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    // target->IsWithinCombatRange()

    return target && (target->GetVictim() != bot || target->isFrozen() || target->HasRootAura()) &&
           target->IsWithinCombatRange(bot, MIN_MELEE_REACH);

    //     Unit* target = AI_VALUE(Unit*, "current target");
    //     if (!target)
    //         return false;

    //     if (target->GetTarget() == bot->GetGUID() && !bot->GetGroup() && !target->HasUnitState(UNIT_STATE_ROOT) &&
    //     GetSpeedInMotion(target) > GetSpeedInMotion(bot) * 0.65f)
    //         return false;

    //     bool isBoss = false;
    //     bool isRaid = false;
    //     float combatReach = bot->GetCombatReach() + target->GetCombatReach();
    //     float targetDistance = ServerFacade::instance().GetDistance2d(bot, target) + combatReach;
    //     if (target->IsCreature())
    //     {
    //         Creature* creature = botAI->GetCreature(target->GetGUID());
    //         if (creature)
    //         {
    //             isBoss = creature->isWorldBoss();
    //         }
    //     }

    //     if (bot->GetMap() && bot->GetMap()->IsRaid())
    //         isRaid = true;

    // //    if (isBoss || isRaid)
    // //        return ServerFacade::instance().IsDistanceLessThan(targetDistance, botAI->GetRange("shoot") + combatReach);

    //     return ServerFacade::instance().IsDistanceLessOrEqualThan(targetDistance, (botAI->GetRange("shoot") + combatReach /
    //     2));
}

bool EnemyTooCloseForMeleeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsPlayer())
        return false;

    return target && AI_VALUE2(bool, "inside target", "current target");
}

bool EnemyIsCloseTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && ServerFacade::instance().IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", "current target"),
                                                              sPlayerbotAIConfig.TooCloseDistance);
}

bool EnemyWithinMeleeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return target && bot->IsWithinMeleeRange(target);
}

bool OutOfRangeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, GetTargetName());
    // increase contact distance to prevent calculation error
    float dis = distance + CONTACT_DISTANCE;
    return target &&
           !bot->IsWithinCombatRange(
               target,
               dis);  // ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", GetTargetName()), distance);
}

EnemyOutOfSpellRangeTrigger::EnemyOutOfSpellRangeTrigger(PlayerbotAI* botAI)
    : OutOfRangeTrigger(botAI, "enemy out of spell range", botAI->GetRange("spell"))
{
}

// bool EnemyOutOfSpellRangeTrigger::IsActive()
// {
//     Unit* target = AI_VALUE(Unit*, GetTargetName());
//     if (!target)
//         return false;

//     float combatReach = bot->GetCombatReach() + target->GetCombatReach();
//     return target && (ServerFacade::instance().GetDistance2d(bot, target) > (distance + combatReach +
//     sPlayerbotAIConfig.ContactDistance) || !bot->IsWithinLOSInMap(target));
// }

// bool EnemyOutOfMeleeTrigger::IsActive()
// {
//     Unit* target = AI_VALUE(Unit*, GetTargetName());
//     if (!target)
//         return false;

//     float targetDistance = ServerFacade::instance().GetDistance2d(bot, target);
//     return target && (targetDistance > std::max(5.0f, bot->GetCombatReach() + target->GetCombatReach()) ||
//     (!bot->IsWithinLOSInMap(target) && targetDistance > 5.0f));
// }

bool PartyMemberToHealOutOfSpellRangeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, GetTargetName());
    if (!target)
        return false;

    return target && (ServerFacade::instance().GetDistance2d(bot, target) > (distance + sPlayerbotAIConfig.ContactDistance) ||
                      !bot->IsWithinLOSInMap(target));
}

PartyMemberToHealOutOfSpellRangeTrigger::PartyMemberToHealOutOfSpellRangeTrigger(PlayerbotAI* botAI)
    : OutOfRangeTrigger(botAI, "party member to heal out of spell range", botAI->GetRange("heal") + 1.0f)
{
}

bool FarFromMasterTrigger::IsActive()
{
    return ServerFacade::instance().IsDistanceGreaterThan(AI_VALUE2(float, "distance", "group leader"), distance);
}

bool TooCloseToCreatureTrigger::TooCloseToCreature(uint32 creatureId, float range, bool alive)
{
    Creature* nearestCreature = bot->FindNearestCreature(creatureId, range, alive);
    return nearestCreature != nullptr;
}

bool TooCloseToPlayerWithDebuffTrigger::TooCloseToPlayerWithDebuff(uint32 spellId, float range)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        return false;
    }

    std::vector<Player*> debuffedPlayers;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (player && player->IsAlive() && player->HasAura(spellId))
        {
            debuffedPlayers.push_back(player);
        }
    }

    if (debuffedPlayers.empty())
    {
        return false;
    }

    for (Unit* debuffedPlayer : debuffedPlayers)
    {
        float dist = debuffedPlayer->GetExactDist2d(bot->GetPositionX(), bot->GetPositionY());
        if (dist < range)
        {
            return true;
        }
    }

    return false;
}

bool TooFarFromPlayerWithAuraTrigger::TooFarFromPlayerWithAura(uint32 spellId, float range, bool selfInclude)
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        return false;
    }

    std::vector<Player*> debuffedPlayers;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (player && player->IsAlive() && player->HasAura(spellId) &&
            (selfInclude || (!selfInclude && player->GetGUID() != bot->GetGUID())))
        {
            debuffedPlayers.push_back(player);
        }
    }

    return !debuffedPlayers.empty();

    if (debuffedPlayers.empty())
    {
        return false;
    }

    for (Unit* debuffedPlayer : debuffedPlayers)
    {
        float dist = debuffedPlayer->GetExactDist2d(bot->GetPositionX(), bot->GetPositionY());
        if (dist > range)
        {
            return true;
        }
    }

    return false;
}
