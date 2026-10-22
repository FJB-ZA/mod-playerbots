/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// Kael'thas Sunstrider heroic bot tactics: dps break Shock Barrier so the Pyroblast behind it can be interrupted.

#include "AttackersValue.h"
#include "MgTShared.h"
#include "Playerbots.h"

namespace MagistersTerrace
{
bool IsKaelShielded(Unit* kael) { return kael->HasAura(SPELL_SHOCK_BARRIER); }

Unit* GetShockBarrierKael(Player* bot, Unit* kael)
{
    bool const heroic = bot->GetMap()->IsHeroic();
    bool const barrierUp = heroic && IsKaelShielded(kael);
    bool const attackable = barrierUp && AttackersValue::IsValidTarget(kael, bot);
    return BreaksShockBarrier(heroic, barrierUp, attackable) ? kael : nullptr;
}
}  // namespace MagistersTerrace
