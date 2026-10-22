// Pure rules for the Utgarde Pinnacle bosses: Grauf's freezing clouds, Skadi's whirlwind and Ymiron's Bane.

#ifndef _PLAYERBOTS_UP_POLICY_H
#define _PLAYERBOTS_UP_POLICY_H

namespace UtgardePinnacle
{
// Skadi the Ruthless

// The cloud's hitbox is far smaller than its graphic; the margin keeps bots visibly out of the patches
constexpr float CloudRadius = 3.0f;
constexpr float CloudMargin = 3.0f;

// Grauf breathes down the left or the right side of the hall; the cloud stays on the ground after the breath.
constexpr bool CloudDanger(bool leftBreath, bool rightBreath, bool cloudOnGround)
{
    return leftBreath || rightBreath || cloudOnGround;
}

// Moves once inside the cloud plus most of the margin, out to the full margin; 0 = stay.
constexpr float CloudStepAway(float distanceToCloud)
{
    float clear = CloudRadius + CloudMargin;
    return distanceToCloud < clear - 1.0f ? clear - distanceToCloud : 0.0f;
}

constexpr float WhirlwindRadius = 5.0f;
constexpr float WhirlwindMargin = 2.0f;

// He chases the tank while spinning: the tank pre-moves twice the radius so it doesn't stutter-step.
constexpr float WhirlwindStepAway(bool tank, float distanceToBoss)
{
    if (distanceToBoss >= WhirlwindRadius + WhirlwindMargin)
        return 0.0f;
    return (tank ? 2.0f * WhirlwindRadius : WhirlwindRadius) + WhirlwindMargin - distanceToBoss;
}

struct SkadiAction
{
    bool movement = false;
    bool avoidWhirlwind = false;
    bool attackOnSkadiOrGrauf = false;
};

// Gauntlet: he is in the sky, so kill the adds. Ground: only the whirlwind dodge moves while he spins.
constexpr bool SkadiAllows(bool gauntlet, bool whirlwind, SkadiAction action)
{
    if (gauntlet)
        return !action.attackOnSkadiOrGrauf;
    return !(whirlwind && action.movement && !action.avoidWhirlwind);
}

// King Ymiron

// Bane hits the whole group whenever he takes direct damage.
constexpr bool YmironAllows(bool bane, bool attack) { return !(bane && attack); }
}  // namespace UtgardePinnacle

#endif
