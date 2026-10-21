// Arena playbook actions; each decision is rolled against the bot's hidden skill.

#include "LivingArenaActions.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

#include "Battleground.h"
#include "Group.h"
#include "LivingArenaMgr.h"
#include "LivingBondMgr.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "Timer.h"

namespace
{
uint32 constexpr PVP_TRINKET_SPELL = 42292;
uint32 constexpr EVERY_MAN_FOR_HIMSELF = 59752;
uint8 constexpr ICON_MOON = 4;
uint8 constexpr ICON_SKULL = 7;

std::array<uint32, 6> constexpr HardImmunities = {642, 45438, 19263, 1022, 5599, 10278};
std::array<uint32, 11> constexpr Defensives = {31224, 33206, 47585, 48707, 26669, 22812, 48792, 871, 498, 46924, 47788};

std::vector<char const*> Interrupts(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
            return {"pummel", "shield bash"};
        case CLASS_ROGUE:
            return {"kick"};
        case CLASS_MAGE:
            return {"counterspell"};
        case CLASS_SHAMAN:
            return {"wind shear"};
        case CLASS_DEATH_KNIGHT:
            return {"mind freeze", "strangulate"};
        case CLASS_HUNTER:
            return {"silencing shot"};
        case CLASS_PRIEST:
            return {"silence"};
        case CLASS_PALADIN:
            return {"hammer of justice"};
        case CLASS_WARLOCK:
            return {"spell lock"};
        default:
            return {};
    }
}

// Slows and escapes a ranged player uses on melee in their face, best first.
std::vector<char const*> KiteTools(uint8 cls)
{
    switch (cls)
    {
        case CLASS_MAGE:
            return {"frost nova", "cone of cold", "blast wave"};
        case CLASS_HUNTER:
            return {"disengage", "wing clip", "concussive shot"};
        case CLASS_WARLOCK:
            return {"shadowfury", "curse of exhaustion"};
        case CLASS_SHAMAN:
            return {"thunderstorm", "frost shock"};
        case CLASS_DRUID:
            return {"typhoon", "entangling roots"};
        case CLASS_PRIEST:
            return {"psychic horror"};
        default:
            return {};
    }
}

// Peels that need the attacker within a few yards of the bot itself come first in each list.
std::vector<char const*> Peels(uint8 cls, bool close)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
            return close ? std::vector<char const*>{"intimidating shout", "piercing howl", "hamstring"}
                         : std::vector<char const*>{"intercept"};
        case CLASS_ROGUE:
            return close ? std::vector<char const*>{"gouge", "blind", "kidney shot"} : std::vector<char const*>{"blind"};
        case CLASS_MAGE:
            return close ? std::vector<char const*>{"frost nova", "deep freeze", "polymorph"}
                         : std::vector<char const*>{"deep freeze", "polymorph", "frostbolt"};
        case CLASS_WARLOCK:
            return close ? std::vector<char const*>{"howl of terror", "death coil", "fear"}
                         : std::vector<char const*>{"death coil", "fear"};
        case CLASS_PRIEST:
            return close ? std::vector<char const*>{"psychic scream", "psychic horror"}
                         : std::vector<char const*>{"psychic horror"};
        case CLASS_PALADIN:
            return {"hammer of justice"};
        case CLASS_HUNTER:
            return close ? std::vector<char const*>{"scatter shot", "wing clip", "concussive shot"}
                         : std::vector<char const*>{"scatter shot", "concussive shot"};
        case CLASS_DEATH_KNIGHT:
            return close ? std::vector<char const*>{"hungering cold", "chains of ice", "strangulate"}
                         : std::vector<char const*>{"chains of ice", "strangulate"};
        case CLASS_SHAMAN:
            return {"frost shock", "hex"};
        case CLASS_DRUID:
            return {"cyclone", "entangling roots", "bash", "maim"};
        default:
            return {};
    }
}

std::vector<char const*> CrowdControls(uint8 cls)
{
    switch (cls)
    {
        case CLASS_MAGE:
            return {"polymorph"};
        case CLASS_WARLOCK:
            return {"fear"};
        case CLASS_DRUID:
            return {"cyclone"};
        case CLASS_SHAMAN:
            return {"hex"};
        case CLASS_ROGUE:
            return {"blind"};
        case CLASS_HUNTER:
            return {"scatter shot"};
        case CLASS_PALADIN:
            return {"repentance"};
        default:
            return {};
    }
}

bool IsSelfCentered(char const* spell)
{
    std::string const name = spell;
    return name == "intimidating shout" || name == "piercing howl" || name == "frost nova" ||
           name == "howl of terror" || name == "psychic scream" || name == "hungering cold" ||
           name == "cone of cold" || name == "blast wave" || name == "disengage" || name == "thunderstorm" ||
           name == "typhoon" || name == "shadowfury";
}

bool IsCloth(Player* player)
{
    uint8 const cls = player->getClass();
    return cls == CLASS_MAGE || cls == CLASS_PRIEST || cls == CLASS_WARLOCK;
}
}  // namespace

bool LivingArenaTrigger::IsActive()
{
    return bot->InArena() && bot->GetBattleground() && LivingArenaMgr::instance().IsEnabled() &&
           LivingArenaMgr::IsBot(bot);
}

bool LivingArenaAction::InFight()
{
    Battleground* bg = bot->GetBattleground();
    return bot->InArena() && bg && bg->GetStatus() == STATUS_IN_PROGRESS && bot->IsAlive();
}

std::vector<Player*> LivingArenaAction::Enemies()
{
    std::vector<Player*> enemies;
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return enemies;
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (player && player->IsInWorld() && player->IsAlive() && player->GetMapId() == bot->GetMapId() &&
            player->GetBgTeamId() != bot->GetBgTeamId() && bot->CanSeeOrDetect(player))
            enemies.push_back(player);
    }
    return enemies;
}

std::vector<Player*> LivingArenaAction::Allies()
{
    std::vector<Player*> allies;
    Battleground* bg = bot->GetBattleground();
    if (!bg)
        return allies;
    for (auto const& [guid, player] : bg->GetPlayers())
    {
        if (player && player->IsInWorld() && player->IsAlive() && player->GetMapId() == bot->GetMapId() &&
            player->GetBgTeamId() == bot->GetBgTeamId())
            allies.push_back(player);
    }
    return allies;
}

uint32 LivingArenaAction::Skill() { return LivingArenaMgr::instance().GetSkill(bot); }

bool LivingArenaAction::TryCast(std::vector<char const*> const& spells, Unit* target)
{
    for (char const* spell : spells)
    {
        Unit* castTarget = IsSelfCentered(spell) ? bot : target;
        if (IsSelfCentered(spell) && bot->GetDistance(target) > 8.0f)
            continue;
        if (botAI->CanCastSpell(spell, castTarget) && botAI->CastSpell(spell, castTarget))
            return true;
    }
    return false;
}

bool LivingArenaAction::IsControlled(Unit* unit)
{
    return unit->HasAuraType(SPELL_AURA_MOD_STUN) || unit->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           unit->HasAuraType(SPELL_AURA_MOD_CONFUSE) || unit->HasAuraType(SPELL_AURA_MOD_CHARM) ||
           unit->HasAuraType(SPELL_AURA_MOD_POSSESS);
}

bool LivingArenaAction::IsImmune(Unit* unit)
{
    return std::any_of(HardImmunities.begin(), HardImmunities.end(), [unit](uint32 id) { return unit->HasAura(id); });
}

bool LivingArenaCallAction::isUseful()
{
    return InFight() && bot->GetGroup() && !LivingArenaMgr::instance().HasHumanTeammate(bot);
}

bool LivingArenaCallAction::Execute(Event /*event*/)
{
    LivingArenaMgr& arena = LivingArenaMgr::instance();
    Group* group = bot->GetGroup();
    uint32 const teamSkill = arena.GetTeamSkill(bot);
    LivingArena::Tier const tier = LivingArena::TierFor(teamSkill);
    uint32 const interval = LivingArena::CallInterval(tier);
    // Calls never take the bot's turn, so the rotation keeps running in the same tick.
    if (!interval || !arena.ClaimTeamCall(group->GetGUID(), interval) || !Roll(teamSkill))
        return false;

    std::vector<Player*> const enemies = Enemies();
    std::vector<Player*> const allies = Allies();
    if (enemies.empty())
        return false;

    Player* healer = nullptr;
    for (Player* enemy : enemies)
    {
        if (PlayerbotAI::IsHeal(enemy, true))
            healer = enemy;
    }
    bool const canCc = tier >= LivingArena::Tier::Skilled &&
                       std::any_of(allies.begin(), allies.end(),
                                   [](Player* ally) { return !CrowdControls(ally->getClass()).empty(); });
    bool const ccHealer = healer && canCc && enemies.size() > 1;

    Unit* current = botAI->GetUnit(group->GetTargetIcon(ICON_SKULL));
    Player* best = nullptr;
    float bestScore = -1000.0f;
    for (Player* enemy : enemies)
    {
        float score = 100.0f - enemy->GetHealthPct();
        if (IsImmune(enemy))
            score -= 100.0f;
        // Good teams swap off a big defensive at once; weaker teams keep hitting into it a while.
        else if (std::any_of(Defensives.begin(), Defensives.end(), [enemy](uint32 id) { return enemy->HasAura(id); }))
            score -= tier >= LivingArena::Tier::Skilled ? 45.0f : 20.0f;
        // A spent PvP trinket means the next stun sticks.
        if (tier >= LivingArena::Tier::Skilled && enemy->HasSpellCooldown(PVP_TRINKET_SPELL))
            score += 10.0f;
        if (enemy == healer)
            score += ccHealer ? -60.0f : (tier >= LivingArena::Tier::Skilled ? 35.0f : 15.0f);
        if (IsCloth(enemy))
            score += 8.0f;
        if (std::none_of(allies.begin(), allies.end(),
                         [enemy](Player* ally) { return ally->IsWithinDistInMap(enemy, 40.0f) && ally->IsWithinLOSInMap(enemy); }))
            score -= 20.0f;
        if (enemy == current)
            score += 15.0f;
        // Old grudges show: someone on the team has a score to settle with this player.
        if (std::any_of(allies.begin(), allies.end(),
                        [enemy](Player* ally)
                        {
                            return LivingBondMgr::instance().AttitudeOf(ally->GetGUID().GetCounter(),
                                                                         enemy->GetGUID().GetCounter()) ==
                                   LivingWorld::Attitude::Rival;
                        }))
            score += 10.0f;
        if (score > bestScore)
        {
            bestScore = score;
            best = enemy;
        }
    }

    if (best && best != current)
        group->SetTargetIcon(ICON_SKULL, bot->GetGUID(), best->GetGUID());
    ObjectGuid const moon = ccHealer && healer != best ? healer->GetGUID() : ObjectGuid::Empty;
    if (group->GetTargetIcon(ICON_MOON) != moon)
        group->SetTargetIcon(ICON_MOON, bot->GetGUID(), moon);
    return false;
}

bool LivingArenaTrinketAction::isUseful() { return bot->InArena() && bot->IsAlive() && IsControlled(bot); }

bool LivingArenaTrinketAction::Execute(Event /*event*/)
{
    AuraEffect const* control = nullptr;
    for (AuraType type : {SPELL_AURA_MOD_STUN, SPELL_AURA_MOD_FEAR, SPELL_AURA_MOD_CONFUSE, SPELL_AURA_MOD_CHARM})
    {
        for (AuraEffect const* effect : bot->GetAuraEffectsByType(type))
        {
            if (!control || effect->GetBase()->GetDuration() > control->GetBase()->GetDuration())
                control = effect;
        }
    }
    if (!control)
        return false;

    // One look per crowd control: a missed reaction stays missed until the next one lands.
    uint64 const key = (uint64(control->GetId()) << 32) ^ uint64(control->GetBase()->GetApplyTime());
    uint32 const skill = Skill();
    if (key != _decided)
    {
        _decided = key;
        _willing = LivingArena::TierFor(skill) == LivingArena::Tier::Rookie ? urand(1, 100) <= 40 : Roll(skill);
    }
    if (!_willing)
        return false;

    // Better players save the trinket for a crowd control that would cost the game.
    if (LivingArena::TierFor(skill) >= LivingArena::Tier::Skilled)
    {
        bool urgent = bot->GetHealthPct() < 60.0f || PlayerbotAI::IsHeal(bot, true);
        for (Player* ally : Allies())
            urgent = urgent || ally->GetHealthPct() < 45.0f;
        if (!urgent || control->GetBase()->GetDuration() < 2500)
            return false;
    }

    for (uint8 slot : {EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2})
    {
        Item* trinket = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (trinket && trinket->GetTemplate()->Spells[0].SpellId == PVP_TRINKET_SPELL &&
            !bot->HasSpellCooldown(PVP_TRINKET_SPELL))
        {
            bot->CastSpell(bot, PVP_TRINKET_SPELL, false, trinket);
            _willing = false;
            return true;
        }
    }
    if (bot->HasSpell(EVERY_MAN_FOR_HIMSELF) && !bot->HasSpellCooldown(EVERY_MAN_FOR_HIMSELF))
    {
        bot->CastSpell(bot, EVERY_MAN_FOR_HIMSELF, false);
        _willing = false;
        return true;
    }
    return false;
}

bool LivingArenaInterruptAction::isUseful()
{
    return InFight() && !IsControlled(bot) && Tier() >= LivingArena::Tier::Regular &&
           !Interrupts(bot->getClass()).empty();
}

bool LivingArenaInterruptAction::Execute(Event /*event*/)
{
    if (_rolled.size() > 64)
        _rolled.clear();
    uint32 const skill = Skill();
    for (Player* enemy : Enemies())
    {
        if (!bot->IsWithinDistInMap(enemy, 30.0f))
            continue;
        Spell* spell = enemy->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        if (!spell)
            spell = enemy->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        if (!spell || !spell->m_spellInfo)
            continue;
        bool const heal = spell->m_spellInfo->IsPositive();
        bool const control = spell->m_spellInfo->GetAllEffectsMechanicMask() &
                             ((1 << MECHANIC_POLYMORPH) | (1 << MECHANIC_FEAR) | (1 << MECHANIC_CHARM) |
                              (1 << MECHANIC_SLEEP) | (1 << MECHANIC_HORROR) | (1 << MECHANIC_BANISH));
        if (!heal && !control && spell->GetCastTime() < 2000)
            continue;
        auto [found, inserted] = _rolled.try_emplace(reinterpret_cast<uintptr_t>(spell), false);
        if (inserted)
            found->second = Roll(skill);
        if (found->second && TryCast(Interrupts(bot->getClass()), enemy))
            return true;
    }
    return false;
}

bool LivingArenaPeelAction::isUseful()
{
    return InFight() && !IsControlled(bot) && Tier() >= LivingArena::Tier::Skilled;
}

bool LivingArenaPeelAction::Execute(Event /*event*/)
{
    uint32 const now = getMSTime();
    if (getMSTimeDiff(_lastTry, now) < 1000)
        return false;
    _lastTry = now;

    std::vector<Player*> const enemies = Enemies();
    for (Player* ally : Allies())
    {
        bool const healer = PlayerbotAI::IsHeal(ally, true);
        if (!healer && ally->GetHealthPct() > 55.0f)
            continue;
        for (Player* enemy : enemies)
        {
            if (enemy->GetVictim() != ally || !enemy->IsWithinDistInMap(ally, 6.0f) || IsControlled(enemy) ||
                IsImmune(enemy) || !PlayerbotAI::IsMelee(enemy, true))
                continue;
            if (!Roll(Skill()))
                return false;
            if (bot->getClass() == CLASS_PALADIN && ally != bot && botAI->CanCastSpell("hand of protection", ally) &&
                botAI->CastSpell("hand of protection", ally))
                return true;
            if (TryCast(Peels(bot->getClass(), bot->GetDistance(enemy) <= 8.0f), enemy))
                return true;
        }
    }
    return false;
}

bool LivingArenaCcAction::isUseful()
{
    return InFight() && bot->GetGroup() && !IsControlled(bot) && Tier() >= LivingArena::Tier::Skilled &&
           !CrowdControls(bot->getClass()).empty() && bot->GetGroup()->GetTargetIcon(ICON_MOON);
}

bool LivingArenaCcAction::Execute(Event /*event*/)
{
    uint32 const now = getMSTime();
    if (getMSTimeDiff(_lastTry, now) < 1500)
        return false;
    _lastTry = now;

    Group* group = bot->GetGroup();
    Unit* target = botAI->GetUnit(group->GetTargetIcon(ICON_MOON));
    if (!target || !target->IsAlive() || IsControlled(target) || IsImmune(target) || !bot->IsValidAttackTarget(target))
        return false;
    // Lock the healer down once the kill target is in trouble, not at the gates.
    Unit* kill = botAI->GetUnit(group->GetTargetIcon(ICON_SKULL));
    if (kill && kill->GetHealthPct() > 65.0f)
        return false;

    bool const tracksReturns = Tier() == LivingArena::Tier::Expert;
    std::vector<char const*> usable;
    for (char const* spell : CrowdControls(bot->getClass()))
    {
        uint32 const spellId = AI_VALUE2(uint32, "spell id", spell);
        SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
        if (!info)
            continue;
        DiminishingGroup const returns = GetDiminishingReturnsGroupForSpell(info, false);
        if (tracksReturns && returns != DIMINISHING_NONE && target->GetDiminishing(returns) >= DIMINISHING_LEVEL_3)
            continue;
        usable.push_back(spell);
    }
    if (usable.empty() || !Roll(Skill()))
        return false;
    return TryCast(usable, target);
}

bool LivingArenaLosAction::isUseful()
{
    if (!InFight() || IsControlled(bot) || !botAI->CanMove())
        return false;
    LivingArena::Tier const tier = Tier();
    float const threshold = PlayerbotAI::IsHeal(bot, true) ? 60.0f : 45.0f;
    return (tier == LivingArena::Tier::Expert && bot->GetHealthPct() < threshold) ||
           (tier == LivingArena::Tier::Skilled && bot->GetHealthPct() < 30.0f);
}

bool LivingArenaLosAction::Execute(Event /*event*/)
{
    uint32 const now = getMSTime();
    if (getMSTimeDiff(_lastTry, now) < 1500)
        return false;
    _lastTry = now;

    std::vector<Player*> threats;
    for (Player* enemy : Enemies())
    {
        if (PlayerbotAI::IsRanged(enemy, true) && enemy->IsWithinDistInMap(bot, 45.0f) &&
            enemy->IsWithinLOSInMap(bot) && (enemy->GetTarget() == bot->GetGUID() || enemy->GetVictim() == bot))
            threats.push_back(enemy);
    }
    if (threats.empty() || !Roll(Skill()))
        return false;

    bool const healer = PlayerbotAI::IsHeal(bot, true);
    std::vector<Player*> const allies = Allies();
    for (float radius : {5.0f, 8.0f, 12.0f, 16.0f, 20.0f})
    {
        for (int step = 0; step < 16; ++step)
        {
            float const angle = float(M_PI) * 2.0f * step / 16.0f;
            float const x = bot->GetPositionX() + radius * std::cos(angle);
            float const y = bot->GetPositionY() + radius * std::sin(angle);
            float const z = bot->GetMapHeight(x, y, bot->GetPositionZ() + 5.0f);
            if (z < -10000.0f || std::fabs(z - bot->GetPositionZ()) > 6.0f)
                continue;
            if (std::any_of(threats.begin(), threats.end(),
                            [x, y, z](Player* threat) { return threat->IsWithinLOS(x, y, z + 2.0f); }))
                continue;
            // A healer hides from the casters but keeps a teammate in sight.
            if (healer && std::none_of(allies.begin(), allies.end(),
                                       [this, x, y, z](Player* ally)
                                       {
                                           return ally != bot && ally->IsWithinDist3d(x, y, z, 40.0f) &&
                                                  ally->IsWithinLOS(x, y, z + 2.0f);
                                       }))
                continue;
            PathGenerator path(bot);
            path.CalculatePath(x, y, z, false);
            if (path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE))
                continue;
            float length = 0.0f;
            Movement::PointsArray const& points = path.GetPath();
            for (size_t i = 1; i < points.size(); ++i)
                length += (points[i] - points[i - 1]).length();
            if (length > radius * 2.0f + 5.0f)
                continue;
            return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}

bool LivingArenaKiteAction::isUseful()
{
    if (!InFight() || IsControlled(bot) || !botAI->CanMove() || PlayerbotAI::IsHeal(bot, true) ||
        !PlayerbotAI::IsRanged(bot, true))
        return false;
    LivingArena::Tier const tier = Tier();
    return tier == LivingArena::Tier::Expert || (tier == LivingArena::Tier::Skilled && bot->GetHealthPct() < 75.0f);
}

bool LivingArenaKiteAction::Execute(Event /*event*/)
{
    uint32 const now = getMSTime();
    if (getMSTimeDiff(_lastTry, now) < 1500)
        return false;
    _lastTry = now;

    Player* chaser = nullptr;
    for (Player* enemy : Enemies())
    {
        if (enemy->GetVictim() == bot && enemy->IsWithinDistInMap(bot, 7.0f) && !IsControlled(enemy) &&
            PlayerbotAI::IsMelee(enemy, true))
            chaser = enemy;
    }
    if (!chaser || !Roll(Skill()))
        return false;
    TryCast(KiteTools(bot->getClass()), chaser);
    return MoveAway(chaser, 15.0f);
}

bool LivingArenaStealthAction::isUseful()
{
    Battleground* bg = bot->GetBattleground();
    if (!bot->InArena() || !bg || bg->GetStatus() != STATUS_WAIT_JOIN || Tier() < LivingArena::Tier::Skilled ||
        bot->HasStealthAura())
        return false;
    return bot->getClass() == CLASS_ROGUE || (bot->getClass() == CLASS_DRUID && bot->GetShapeshiftForm() == FORM_CAT);
}

bool LivingArenaStealthAction::Execute(Event /*event*/)
{
    char const* spell = bot->getClass() == CLASS_ROGUE ? "stealth" : "prowl";
    return botAI->CanCastSpell(spell, bot) && botAI->CastSpell(spell, bot);
}

bool LivingArenaSkillAction::Execute(Event event)
{
    LivingArenaMgr& arena = LivingArenaMgr::instance();
    std::string const param = event.getParam();
    // Three digits at most, so the conversion can't throw or wrap before the clamp
    if (!param.empty() && param.size() <= 3 &&
        std::all_of(param.begin(), param.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
        arena.SetSkill(bot, std::min<uint32>(std::stoul(param), 100));
    static char const* const names[] = {"rookie", "regular", "skilled", "expert"};
    uint32 const skill = arena.GetSkill(bot);
    botAI->TellMaster("Arena skill " + std::to_string(skill) + " (" +
                      names[static_cast<uint8>(LivingArena::TierFor(skill))] + ")");
    return true;
}

void LivingArenaStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("living arena", {NextAction("living arena call", ACTION_RAID + 5.0f),
                                                        NextAction("living arena trinket", ACTION_EMERGENCY + 6.0f),
                                                        NextAction("living arena interrupt", ACTION_INTERRUPT + 5.0f),
                                                        NextAction("living arena peel", ACTION_INTERRUPT + 3.0f),
                                                        NextAction("living arena los", ACTION_INTERRUPT + 2.0f),
                                                        NextAction("living arena kite", ACTION_MOVE + 8.0f),
                                                        NextAction("living arena cc", ACTION_HIGH + 15.0f),
                                                        NextAction("living arena stealth", ACTION_HIGH + 5.0f)}));
}
