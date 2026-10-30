/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Gladiator sub-class path's action names.

#ifndef PLAYERBOTS_GLADIATORACTIONCONTEXT_H
#define PLAYERBOTS_GLADIATORACTIONCONTEXT_H

#include "Action.h"
#include "GladiatorActions.h"
#include "NamedObjectContext.h"

class SubclassGladiatorActionContext : public NamedObjectContext<Action>
{
public:
    SubclassGladiatorActionContext()
    {
        creators["gladiator stance"] = &SubclassGladiatorActionContext::gladiator_stance;
        creators["shield charge"] = &SubclassGladiatorActionContext::shield_charge;
        creators["dragon roar"] = &SubclassGladiatorActionContext::dragon_roar;
        creators["avatar"] = &SubclassGladiatorActionContext::avatar;
        creators["impending victory"] = &SubclassGladiatorActionContext::impending_victory;
        creators["storm bolt"] = &SubclassGladiatorActionContext::storm_bolt;
        creators["storm bolt on enemy healer"] = &SubclassGladiatorActionContext::storm_bolt_on_enemy_healer;
        creators["bloodbath"] = &SubclassGladiatorActionContext::bloodbath;
        creators["shield barrier"] = &SubclassGladiatorActionContext::shield_barrier;
        creators["die by the sword"] = &SubclassGladiatorActionContext::die_by_the_sword;
        creators["rallying cry"] = &SubclassGladiatorActionContext::rallying_cry;
        creators["mass spell reflection"] = &SubclassGladiatorActionContext::mass_spell_reflection;
    }

private:
    static Action* gladiator_stance(PlayerbotAI* botAI) { return new CastGladiatorStanceAction(botAI); }
    static Action* shield_charge(PlayerbotAI* botAI) { return new CastShieldChargeAction(botAI); }
    static Action* dragon_roar(PlayerbotAI* botAI) { return new CastDragonRoarAction(botAI); }
    static Action* avatar(PlayerbotAI* botAI) { return new CastAvatarAction(botAI); }
    static Action* impending_victory(PlayerbotAI* botAI) { return new CastImpendingVictoryAction(botAI); }
    static Action* storm_bolt(PlayerbotAI* botAI) { return new CastStormBoltAction(botAI); }
    static Action* storm_bolt_on_enemy_healer(PlayerbotAI* botAI)
    {
        return new CastStormBoltOnEnemyHealerAction(botAI);
    }
    static Action* bloodbath(PlayerbotAI* botAI) { return new CastBloodbathAction(botAI); }
    static Action* shield_barrier(PlayerbotAI* botAI) { return new CastShieldBarrierAction(botAI); }
    static Action* die_by_the_sword(PlayerbotAI* botAI) { return new CastDieByTheSwordAction(botAI); }
    static Action* rallying_cry(PlayerbotAI* botAI) { return new CastRallyingCryAction(botAI); }
    static Action* mass_spell_reflection(PlayerbotAI* botAI) { return new CastMassSpellReflectionAction(botAI); }
};

#endif
