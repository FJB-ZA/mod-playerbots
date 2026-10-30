/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Gladiator sub-class path's trigger names.

#ifndef PLAYERBOTS_GLADIATORTRIGGERCONTEXT_H
#define PLAYERBOTS_GLADIATORTRIGGERCONTEXT_H

#include "GladiatorTriggers.h"
#include "NamedObjectContext.h"
#include "Trigger.h"

class SubclassGladiatorTriggerContext : public NamedObjectContext<Trigger>
{
public:
    SubclassGladiatorTriggerContext()
    {
        creators["gladiator stance"] = &SubclassGladiatorTriggerContext::gladiator_stance;
        creators["shield charge"] = &SubclassGladiatorTriggerContext::shield_charge;
        creators["shield slam"] = &SubclassGladiatorTriggerContext::shield_slam;
        creators["ultimatum"] = &SubclassGladiatorTriggerContext::ultimatum;
        creators["dragon roar"] = &SubclassGladiatorTriggerContext::dragon_roar;
        creators["avatar"] = &SubclassGladiatorTriggerContext::avatar;
        creators["bloodbath"] = &SubclassGladiatorTriggerContext::bloodbath;
        creators["mass spell reflection"] = &SubclassGladiatorTriggerContext::mass_spell_reflection;
        creators["storm bolt"] = &SubclassGladiatorTriggerContext::storm_bolt;
        creators["storm bolt on enemy healer"] = &SubclassGladiatorTriggerContext::storm_bolt_on_enemy_healer;
    }

private:
    static Trigger* gladiator_stance(PlayerbotAI* botAI) { return new GladiatorStanceTrigger(botAI); }
    static Trigger* shield_charge(PlayerbotAI* botAI) { return new ShieldChargeTrigger(botAI); }
    static Trigger* shield_slam(PlayerbotAI* botAI) { return new ShieldSlamTrigger(botAI); }
    static Trigger* ultimatum(PlayerbotAI* botAI) { return new UltimatumTrigger(botAI); }
    static Trigger* dragon_roar(PlayerbotAI* botAI) { return new DragonRoarTrigger(botAI); }
    static Trigger* avatar(PlayerbotAI* botAI) { return new AvatarTrigger(botAI); }
    static Trigger* bloodbath(PlayerbotAI* botAI) { return new BloodbathTrigger(botAI); }
    static Trigger* mass_spell_reflection(PlayerbotAI* botAI) { return new MassSpellReflectionTrigger(botAI); }
    static Trigger* storm_bolt(PlayerbotAI* botAI) { return new StormBoltInterruptSpellTrigger(botAI); }
    static Trigger* storm_bolt_on_enemy_healer(PlayerbotAI* botAI)
    {
        return new StormBoltInterruptEnemyHealerSpellTrigger(botAI);
    }
};

#endif
