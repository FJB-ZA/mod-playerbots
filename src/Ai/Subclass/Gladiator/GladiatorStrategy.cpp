/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// The Gladiator's rotation: Shield Charge and Shield Slam first, then WoD's cooldowns to interrupt, burst and survive.

#include "GladiatorStrategy.h"

class GladiatorStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GladiatorStrategyActionNodeFactory()
    {
        creators["shield charge"] = &shield_charge;
        creators["charge"] = &charge;
        creators["berserker rage"] = &berserker_rage;
    }

private:
    static ActionNode* shield_charge(PlayerbotAI* /*botAI*/)
    {
        return new ActionNode(
            "shield charge",
            /*P*/ {},
            /*A*/ { NextAction("charge") },
            /*C*/ {}
        );
    }

    static ActionNode* charge(PlayerbotAI* /*botAI*/)
    {
        return new ActionNode(
            "charge",
            /*P*/ {},
            /*A*/ { NextAction("reach melee") },
            /*C*/ {}
        );
    }

    // Berserker Rage works in any stance, and leaving Gladiator Stance in combat would lose it until the fight ends.
    static ActionNode* berserker_rage(PlayerbotAI* /*botAI*/)
    {
        return new ActionNode(
            "berserker rage",
            /*P*/ {},
            /*A*/ {},
            /*C*/ {}
        );
    }
};

GladiatorStrategy::GladiatorStrategy(PlayerbotAI* botAI) : GenericWarriorStrategy(botAI)
{
    actionNodeFactories.Add(new GladiatorStrategyActionNodeFactory());
}

std::vector<NextAction> GladiatorStrategy::getDefaultActions()
{
    return {
        NextAction("shield slam", ACTION_DEFAULT + 0.5f),
        NextAction("revenge", ACTION_DEFAULT + 0.4f),
        NextAction("sunder armor", ACTION_DEFAULT + 0.3f),
        NextAction("melee", ACTION_DEFAULT)
    };
}

void GladiatorStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericWarriorStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "enemy out of melee",
            {
                NextAction("shield charge", ACTION_MOVE + 9)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "medium health",
            {
                NextAction("shield barrier", ACTION_HIGH + 9),
                NextAction("impending victory", ACTION_HIGH + 9)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "low health",
            {
                NextAction("die by the sword", ACTION_MEDIUM_HEAL)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "party member critical health",
            {
                NextAction("rallying cry", ACTION_MEDIUM_HEAL + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "mass spell reflection",
            {
                NextAction("mass spell reflection", ACTION_INTERRUPT + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "storm bolt on enemy healer",
            {
                NextAction("storm bolt on enemy healer", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "storm bolt",
            {
                NextAction("storm bolt", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "bloodbath",
            {
                NextAction("bloodbath", ACTION_HIGH + 2)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "battle shout",
            {
                NextAction("battle shout", ACTION_HIGH + 8)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "ultimatum",
            {
                NextAction("heroic strike", ACTION_HIGH + 7)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "shield charge",
            {
                NextAction("shield charge", ACTION_HIGH + 6)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "shield slam",
            {
                NextAction("shield slam", ACTION_HIGH + 5)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "target critical health",
            {
                NextAction("execute", ACTION_HIGH + 4)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "revenge",
            {
                NextAction("revenge", ACTION_HIGH + 3)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "medium aoe",
            {
                NextAction("dragon roar", ACTION_HIGH + 3),
                NextAction("cleave", ACTION_HIGH + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "avatar",
            {
                NextAction("avatar", ACTION_HIGH + 2)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "dragon roar",
            {
                NextAction("dragon roar", ACTION_HIGH + 2)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "bloodrage",
            {
                NextAction("bloodrage", ACTION_HIGH + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "high rage available",
            {
                NextAction("heroic strike", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "victory rush",
            {
                NextAction("victory rush", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "shield bash on enemy healer",
            {
                NextAction("shield bash on enemy healer", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "shield bash",
            {
                NextAction("shield bash", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "critical health",
            {
                NextAction("last stand", ACTION_EMERGENCY),
                NextAction("rallying cry", ACTION_EMERGENCY - 1)
            }
        )
    );
}
