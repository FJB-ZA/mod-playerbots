// Nazan heroic bot tactics: a shaman holds Tremor Totem against Bellowing Roar while she is on the ground.

#include "Playerbots.h"
#include "RampActions.h"
#include "RampMultipliers.h"
#include "RampPolicy.h"
#include "RampTriggers.h"
#include "ShamanActions.h"

namespace
{
bool NazanLanded(Unit* nazan) { return HellfireRamparts::NazanLanded(nazan != nullptr, nazan && nazan->IsFlying()); }
}  // namespace

bool NazanBossTremorTotemTrigger::IsActive()
{
    bool const shaman = bot->getClass() == CLASS_SHAMAN;
    bool const heroic = shaman && bot->GetMap()->IsHeroic();
    bool const landed = heroic && NazanLanded(AI_VALUE2(Unit*, "find target", "nazan"));
    return HellfireRamparts::SetsTremorTotem(heroic, shaman, landed,
                                             landed && AI_VALUE2(bool, "has totem", "tremor totem"));
}

bool NazanSetTremorTotemAction::Execute(Event /*event*/)
{
    return !AI_VALUE2(bool, "has totem", "tremor totem") && botAI->CanCastSpell("tremor totem", bot) &&
           botAI->CastSpell("tremor totem", bot);
}

float NazanSetTremorTotemMultiplier::GetValue(Action* action)
{
    bool const inCombat = botAI->GetState() != BOT_STATE_NON_COMBAT;
    bool const shaman = bot->getClass() == CLASS_SHAMAN;
    bool const earthTotem =
        dynamic_cast<CastStrengthOfEarthTotemAction*>(action) || dynamic_cast<CastStoneskinTotemAction*>(action) ||
        dynamic_cast<CastStoneclawTotemAction*>(action) || dynamic_cast<CastEarthbindTotemAction*>(action);
    bool const heroic = inCombat && shaman && earthTotem && bot->GetMap()->IsHeroic();
    bool const landed = heroic && NazanLanded(AI_VALUE2(Unit*, "find target", "nazan"));

    return HellfireRamparts::NazanTremorAllows(heroic, inCombat, shaman, earthTotem, landed) ? 1.0f : 0.0f;
}
