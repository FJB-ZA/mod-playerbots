/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HunterTriggers.h"
#include "GenericSpellActions.h"
#include "GenericTriggers.h"
#include "HunterActions.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "TargetValue.h"

// Auto Shot runs beside the global cooldown, so it is started as soon as it isn't already shooting at the current
// target. As the rotations' last default action it only started when every shot above it failed for a moment: a
// pet's actions gave a plain hunter those gaps, but a hunter without a pet whose Steady Shot is always ready (a
// Marksmanship Ranger at 60) never got one and never fired it.
bool AutoShotTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;
    Spell const* repeat = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
    return !repeat || repeat->m_targets.GetUnitTargetGUID() != target->GetGUID();
}

bool KillCommandTrigger::IsActive()
{
    return !botAI->HasAura("kill command", GetTarget());
}

bool BlackArrowTrigger::IsActive()
{
    if (botAI->HasStrategy("trap weave", BOT_STATE_COMBAT))
        return false;

    return DebuffTrigger::IsActive();
}

bool HunterAspectOfTheDragonhawkTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    if (!SpellTrigger::IsActive())
        return false;

    if (botAI->HasAura("aspect of the hawk", target) ||
        botAI->HasAura("aspect of the dragonhawk", target))
        return false;

    if (botAI->HasAura("aspect of the viper", target))
        return AI_VALUE2(uint8, "mana", "self target") >= 60;

    return true;
}

bool HunterNoStingsActiveTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return DebuffTrigger::IsActive() && target &&
           !botAI->HasAura("serpent sting", target, false, true) &&
           !botAI->HasAura("scorpid sting", target, false, true) &&
           !botAI->HasAura("viper sting", target, false, true);
}

bool HuntersPetDeadTrigger::IsActive()
{
    return AI_VALUE(bool, "pet dead") && !AI_VALUE2(bool, "mounted", "self target");
}

bool HuntersPetLowHealthTrigger::IsActive()
{
    Unit* pet = AI_VALUE(Unit*, "pet target");
    return pet && AI_VALUE2(uint8, "health", "pet target") < 40 &&
           !AI_VALUE2(bool, "dead", "pet target") &&
           !AI_VALUE2(bool, "mounted", "self target");
}

bool HuntersPetMediumHealthTrigger::IsActive()
{
    Unit* pet = AI_VALUE(Unit*, "pet target");
    return pet && AI_VALUE2(uint8, "health", "pet target") < sPlayerbotAIConfig.MediumHealth &&
           !AI_VALUE2(bool, "dead", "pet target") && !AI_VALUE2(bool, "mounted", "self target");
}

bool HunterPetNotHappy::IsActive()
{
    return !AI_VALUE(bool, "pet happy") && !AI_VALUE2(bool, "mounted", "self target");
}

bool HunterAspectOfTheViperTrigger::IsActive()
{
    if (botAI->HasStrategy("rnature", BotState::BOT_STATE_COMBAT) ||
        botAI->HasStrategy("rnature", BotState::BOT_STATE_NON_COMBAT) ||
        botAI->HasStrategy("bspeed", BotState::BOT_STATE_COMBAT) ||
        botAI->HasStrategy("bspeed", BotState::BOT_STATE_NON_COMBAT))
        return false;

    return BuffTrigger::IsActive() &&
           AI_VALUE2(uint8, "mana", "self target") < (sPlayerbotAIConfig.LowMana / 2);
}

bool HunterAspectOfThePackTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("aspect of the cheetah", GetTarget());
};

bool HunterLowAmmoTrigger::IsActive()
{
    uint32 ammoCount = AI_VALUE2(uint32, "item count", "ammo");
    return bot->GetGroup() && ammoCount > 0 && ammoCount < 100;
}

bool HunterHasAmmoTrigger::IsActive()
{
    return !AmmoCountTrigger::IsActive();
}

// Valid targets for "Improved Tracking".
// Optional/Utility targets (uncomment for SelfBot).
bool NoTrackTrigger::IsActive()
{
    std::vector<std::string> track_list = {
        "track beasts",
        "track demons",
        "track dragonkin",
        "track elementals",
        "track giants",
        "track humanoids",
        "track undead",
        // "track hidden",
        // "find herbs",
        // "find minerals",
        // "find fish",
        // "find treasure",
    };

    for (auto &track: track_list)
    {
        if (botAI->HasAura(track, bot))
            return false;
    }

    return true;
}

bool SerpentStingOnAttackerTrigger::IsActive()
{
    if (!DebuffOnAttackerTrigger::IsActive())
        return false;

    Unit* target = GetTarget();
    if (!target)
        return false;

    return !botAI->HasAura("scorpid sting", target, false, true) &&
           !botAI->HasAura("viper sting", target, false, true);
}

const std::set<uint32> VolleyChannelCheckTrigger::VOLLEY_SPELL_IDS =
{
    1510,   // Volley Rank 1
    14294,  // Volley Rank 2
    14295,  // Volley Rank 3
    27022,  // Volley Rank 4
    58431,  // Volley Rank 5
    58434   // Volley Rank 6
};

bool VolleyChannelCheckTrigger::IsActive()
{
    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        spell && VOLLEY_SPELL_IDS.count(spell->m_spellInfo->Id))
    {
        return !HasEnoughAoeTargets(botAI, bot->GetPosition(), sPlayerbotAIConfig.SightDistance, minEnemies);
    }

    return false;
}
