// Pure rules for the Nexus bosses: the commander, Telestra's images, Anomalus's rifts, Ormorok and Keristrasza.

#ifndef _PLAYERBOTS_NEX_POLICY_H
#define _PLAYERBOTS_NEX_POLICY_H

#include <string_view>

namespace Nexus
{
// Faction commander

// The opposing faction's commander: a plain commander on normal, a named one on heroic.
constexpr std::string_view CommanderName(bool alliance, bool heroic)
{
    if (heroic)
        return alliance ? "commander kolurg" : "commander stoutbeard";
    return alliance ? "horde commander" : "alliance commander";
}

// Whirlwind reaches 8 yards
constexpr float WhirlwindClearance = 10.0f;

constexpr float WhirlwindStepAway(float distanceToBoss)
{
    return distanceToBoss < WhirlwindClearance ? WhirlwindClearance - distanceToBoss : 0.0f;
}

// Only the step out may move during a whirlwind, or bots run straight back in.
constexpr bool CommanderAllows(bool whirlwind, bool movement, bool stepOut)
{
    return !(whirlwind && movement && !stepOut);
}

// Grand Magus Telestra

// Firebomb splashes 5 yards round its target
constexpr float FirebombSpacing = 6.0f;

enum class TelestraImage
{
    Arcane,
    Frost,
    Fire,
    None
};

// Arcane first for its Time Stop and Critter, then Frost for its Blizzard, Fire last; -1 = not an image.
constexpr int ImagePriority(TelestraImage image)
{
    switch (image)
    {
        case TelestraImage::Arcane:
            return 0;
        case TelestraImage::Frost:
            return 1;
        case TelestraImage::Fire:
            return 2;
        default:
            return -1;
    }
}

// The images share her name and she stays on the threat table while hidden, so the split is told by the images alone.
constexpr bool SpreadsForFirebomb(bool melee, bool telestraUp, bool imagesUp)
{
    return !melee && telestraUp && !imagesUp;
}

// Assist would pick whichever image is nearest instead of the kill order.
constexpr bool TelestraAllows(bool imagesUp, bool dpsAssist) { return !(imagesUp && dpsAssist); }

// Anomalus

constexpr bool KillsRift(bool healer) { return !healer; }

// While the Rift Shield is up he takes no damage: assist would keep dps on him instead of the rift.
constexpr bool AnomalusAllows(bool riftShield, bool dpsAssist) { return !(riftShield && dpsAssist); }

// Ormorok the Tree-Shaper

// The spikes start a few yards out from him: standing under his feet is the safe spot
constexpr float SpikesSafeDistance = 0.5f;
constexpr float SpikesStepShort = 0.3f;

constexpr bool ShouldStackUnderOrmorok(float distanceToBoss) { return distanceToBoss > SpikesSafeDistance; }

constexpr float StackStep(float distanceToBoss) { return distanceToBoss - SpikesStepShort; }

struct OrmorokAction
{
    bool flee = false;
    bool movement = false;
    bool dodgeSpikes = false;
};

// Ranged flee would ping-pong them out of the safe spot; an engaged tank holds still so he stops shuffling.
constexpr bool OrmorokAllows(OrmorokAction action, bool tankEngaged)
{
    if (action.flee)
        return false;
    return !(tankEngaged && action.movement && !action.dodgeSpikes);
}

// Keristrasza

// Each Intense Cold stack slows casts 10%; moving clears them all
constexpr int IntenseColdNormalStackLimit = 5;

// Her breath and tail are cones: everyone but the tank stands on a flank; ranged dps are far enough out already.
constexpr bool StandsOnFlank(bool tank, bool rangedDps) { return !tank && !rangedDps; }

// Keristrasza (heroic)

// Crystallize roots the whole group for 10 s every 11 s while a stack lands every 2 s: shed them before it hits
constexpr int IntenseColdHeroicStackLimit = 2;

constexpr int IntenseColdStackLimit(bool heroic)
{
    return heroic ? IntenseColdHeroicStackLimit : IntenseColdNormalStackLimit;
}
}  // namespace Nexus

#endif
