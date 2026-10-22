// Vazruden and Nazan bot tactics: tank Vazruden mid-platform, skull him, hold fire resistance once Nazan lands.

#include "EncounterHelpers.h"
#include "PaladinActions.h"
#include "Playerbots.h"
#include "RampActions.h"
#include "RampMultipliers.h"
#include "RampPolicy.h"
#include "RampShared.h"
#include "RampTriggers.h"
#include "ShamanActions.h"

namespace
{
bool NazanLanded(Unit* nazan) { return HellfireRamparts::NazanLanded(nazan != nullptr, nazan && nazan->IsFlying()); }
}  // namespace

bool VazrudenTankPositionBossTrigger::IsActive()
{
    if (!PlayerbotAI::IsMainTank(bot))
        return false;

    bool const vazrudenUp = AI_VALUE2(Unit*, "find target", "vazruden") != nullptr;
    return HellfireRamparts::PositionsVazruden(true, vazrudenUp,
                                               vazrudenUp && NazanLanded(AI_VALUE2(Unit*, "find target", "nazan")));
}

bool VazrudenBossIsActiveTrigger::IsActive()
{
    return EncounterHelpers::IsMechanicTrackerBot(bot, RampShared::RAMP_MAP_ID) &&
           AI_VALUE2(Unit*, "find target", "vazruden");
}

bool NazanBossFireResistanceTotemTrigger::IsActive()
{
    bool const shaman = bot->getClass() == CLASS_SHAMAN;
    bool const landed = shaman && NazanLanded(AI_VALUE2(Unit*, "find target", "nazan"));
    return HellfireRamparts::SetsFireResistance(shaman, landed,
                                                landed && AI_VALUE2(bool, "has totem", "fire resistance totem"));
}

bool NazanBossFireResistanceAuraTrigger::IsActive()
{
    bool const paladin = bot->getClass() == CLASS_PALADIN;
    bool const landed = paladin && NazanLanded(AI_VALUE2(Unit*, "find target", "nazan"));
    return HellfireRamparts::SetsFireResistance(paladin, landed, landed && botAI->HasAura("fire resistance aura", bot));
}

bool VazrudenTankPositionBossAction::Execute(Event /*event*/)
{
    Unit* vazruden = AI_VALUE2(Unit*, "find target", "vazruden");
    if (!vazruden)
        return false;

    if (AI_VALUE(Unit*, "current target") != vazruden)
        return Attack(vazruden);

    Position const middle(HellfireRamparts::VazrudenTankX, HellfireRamparts::VazrudenTankY,
                          HellfireRamparts::VazrudenTankZ);
    if (!HellfireRamparts::DragsVazruden(vazruden->GetVictim() == bot, bot->IsWithinMeleeRange(vazruden),
                                         bot->GetHealthPct(), bot->GetExactDist2d(middle)))
        return false;

    float moveX;
    float moveY;
    bool backwards;
    if (!EncounterHelpers::GetStepToPosition(bot, middle, HellfireRamparts::VazrudenArrivalDistance, vazruden, moveX,
                                             moveY, backwards))
        return false;

    return MoveTo(RampShared::RAMP_MAP_ID, moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, backwards);
}

bool VazrudenMarkBossAction::Execute(Event /*event*/)
{
    Unit* vazruden = AI_VALUE2(Unit*, "find target", "vazruden");
    if (!vazruden)
        return false;

    return EncounterHelpers::MarkTargetWithSkull(bot, vazruden);
}

bool NazanSetFireResistanceTotemAction::Execute(Event /*event*/)
{
    return !AI_VALUE2(bool, "has totem", "fire resistance totem") &&
           botAI->CanCastSpell("fire resistance totem", bot) && botAI->CastSpell("fire resistance totem", bot);
}

bool NazanSetFireResistanceAuraAction::Execute(Event /*event*/)
{
    return !botAI->HasAura("fire resistance aura", bot) && botAI->CanCastSpell("fire resistance aura", bot) &&
           botAI->CastSpell("fire resistance aura", bot);
}

// Fire Resistance Totem is a water totem
float NazanSetFireResistanceTotemMultiplier::GetValue(Action* action)
{
    bool const inCombat = botAI->GetState() != BOT_STATE_NON_COMBAT;
    bool const shaman = bot->getClass() == CLASS_SHAMAN;
    bool const waterTotem =
        dynamic_cast<CastCleansingTotemAction*>(action) || dynamic_cast<CastHealingStreamTotemAction*>(action) ||
        dynamic_cast<CastManaSpringTotemAction*>(action) || dynamic_cast<CastManaTideTotemAction*>(action);
    bool const landed = inCombat && shaman && waterTotem && NazanLanded(AI_VALUE2(Unit*, "find target", "nazan"));

    return HellfireRamparts::NazanBuffAllows(inCombat, shaman, waterTotem, landed) ? 1.0f : 0.0f;
}

float NazanSetFireResistanceAuraMultiplier::GetValue(Action* action)
{
    bool const inCombat = botAI->GetState() != BOT_STATE_NON_COMBAT;
    bool const paladin = bot->getClass() == CLASS_PALADIN;
    bool const otherAura =
        dynamic_cast<CastDevotionAuraAction*>(action) || dynamic_cast<CastConcentrationAuraAction*>(action) ||
        dynamic_cast<CastShadowResistanceAuraAction*>(action) || dynamic_cast<CastRetributionAuraAction*>(action) ||
        dynamic_cast<CastFrostResistanceAuraAction*>(action) || dynamic_cast<CastCrusaderAuraAction*>(action) ||
        dynamic_cast<CastSanctityAuraAction*>(action);
    bool const landed = inCombat && paladin && otherAura && NazanLanded(AI_VALUE2(Unit*, "find target", "nazan"));

    return HellfireRamparts::NazanBuffAllows(inCombat, paladin, otherAura, landed) ? 1.0f : 0.0f;
}
