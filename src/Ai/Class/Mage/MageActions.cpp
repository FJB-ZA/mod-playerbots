/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MageActions.h"
#include "BlinkPolicy.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "SharedDefines.h"
#include "UseItemAction.h"
#include <cmath>

std::vector<NextAction> CastMoltenArmorAction::getAlternatives()
{
    if (!botAI->HasSpell("molten armor"))
        return NextAction::merge({ NextAction("mage armor") }, CastBuffSpellAction::getAlternatives());

    return CastBuffSpellAction::getAlternatives();
}

std::vector<NextAction> CastMageArmorAction::getAlternatives()
{
    if (!botAI->HasSpell("mage armor"))
        return NextAction::merge({ NextAction("ice armor") }, CastBuffSpellAction::getAlternatives());

    return CastBuffSpellAction::getAlternatives();
}

bool UseManaSapphireAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(33312, false) > 0;  // Mana Sapphire
}

bool UseManaEmeraldAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(22044, false) > 0;  // Mana Emerald
}

bool UseManaRubyAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(8008, false) > 0;  // Mana Ruby
}

bool UseManaCitrineAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(8007, false) > 0;  // Mana Citrine
}

bool UseManaJadeAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(5513, false) > 0;  // Mana Jade
}

bool UseManaAgateAction::isUseful()
{
    Player* bot = botAI->GetBot();
    return AI_VALUE2(bool, "combat", "self target") && bot->GetItemCount(5514, false) > 0;  // Mana Agate
}

bool CastFrostNovaAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsInWorld() || target->isFrozen() ||
        (target->ToCreature() &&
         target->ToCreature()->HasMechanicTemplateImmunity(1 << (MECHANIC_FREEZE - 1))))
    {
        return false;
    }

    return ServerFacade::instance().IsDistanceLessOrEqualThan(
        AI_VALUE2(float, "distance", GetTargetName()), 10.f);
}

bool CastConeOfColdAction::isUseful()
{
    bool facingTarget = AI_VALUE2(bool, "facing", "current target");
    bool targetClose = ServerFacade::instance().IsDistanceLessOrEqualThan(
        AI_VALUE2(float, "distance", GetTargetName()), 10.f);

    return facingTarget && targetClose;
}

bool CastDragonsBreathAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    bool facingTarget = AI_VALUE2(bool, "facing", "current target");
    bool targetClose = bot->IsWithinCombatRange(target, 10.0f);
    return facingTarget && targetClose;
}

bool CastBlastWaveAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    bool targetClose = bot->IsWithinCombatRange(target, 10.0f);
    return targetClose;
}

Unit* CastFocusMagicOnPartyAction::GetTarget()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Unit* casterDps = nullptr;
    Unit* healer = nullptr;
    Unit* target = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            bot->GetDistance(member) > sPlayerbotAIConfig.SpellDistance || member->HasAura(54646))  // Focus Magic
        {
            continue;
        }

        if (member->getClass() == CLASS_MAGE)
            return member;

        if (!casterDps && PlayerbotAI::IsCaster(member) && PlayerbotAI::IsDps(member))
            casterDps = member;

        if (!healer && PlayerbotAI::IsHeal(member))
            healer = member;

        if (!target)
            target = member;
    }

    if (casterDps)
        return casterDps;

    if (healer)
        return healer;

    return target;
}

bool CastBlinkBackAction::Execute(Event event)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    bot->SetOrientation(bot->GetAngle(target) + M_PI);
    return CastSpellAction::Execute(event);
}

// Blink follows the ground, and off a ledge it drops to the floor below: walked the way the spell does (2-yard steps,
// a step up of at most 2.65 yards), the way back is not one that drops further than a short fall.
bool CastBlinkBackAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    constexpr float Range = 20.0f;
    constexpr float Step = 2.0f;
    constexpr float StepUp = 2.65f;
    constexpr float MaxDrop = 4.0f;
    float const angle = bot->GetAngle(target) + M_PI;
    float z = bot->GetPositionZ();
    for (float d = Step; d <= Range; d += Step)
    {
        float const x = bot->GetPositionX() + d * std::cos(angle);
        float const y = bot->GetPositionY() + d * std::sin(angle);
        float const ground = bot->GetMapHeight(x, y, z + StepUp);
        if (ground <= INVALID_HEIGHT || z - ground > MaxDrop)
            return false;
        z = ground;
    }
    // Nor away from the group (BlinkPolicy.h)
    Unit* tank = AI_VALUE(Unit*, "group main tank");
    bool const hasTank = tank && tank != bot && tank->IsAlive() && tank->GetMap() == bot->GetMap();
    float const landX = bot->GetPositionX() + Range * std::cos(angle);
    float const landY = bot->GetPositionY() + Range * std::sin(angle);
    if (!BotMove::BlinkKeepsGroup(hasTank, hasTank ? bot->GetExactDist2d(tank) : 0.0f,
                                  hasTank ? tank->GetExactDist2d(landX, landY) : 0.0f))
        return false;
    return CastSpellAction::isUseful();
}
