// Pure rules for Ahn'kahet: Nadox's guardian, Jedoga's volunteers, Shadow Crash and Amanitar's mushrooms.

#ifndef _PLAYERBOTS_AK_POLICY_H
#define _PLAYERBOTS_AK_POLICY_H

namespace AhnKahet
{
// Elder Nadox

// Guardian Aura makes Nadox and his adds immune while the guardian lives: healers keep healing, everyone else kills it.
constexpr bool AttacksNadoxGuardian(bool healer) { return !healer; }

constexpr bool NadoxAllows(bool guardianUp, bool dpsAssist) { return !(guardianUp && dpsAssist); }

// Jedoga Shadowseeker

// A volunteer that reaches her gives her +200% damage: assist would keep dps on her instead.
constexpr bool JedogaAllows(bool volunteerUp, bool dpsAssist) { return !(volunteerUp && dpsAssist); }

// Forgotten One (trash before Herald Volazj)

constexpr float ShadowCrashRadius = 10.0f;
constexpr float ShadowCrashClearance = ShadowCrashRadius + 2.0f;

// How far to step from where the crash lands, or from a ranged neighbour so two crashes don't overlap; 0 = clear.
constexpr float ShadowCrashStepAway(float distance)
{
    return distance < ShadowCrashRadius ? ShadowCrashClearance - distance : 0.0f;
}

constexpr bool SpreadsForShadowCrash(bool melee) { return !melee; }

// A move already under way is probably the dodge: nothing may redirect it.
constexpr bool ForgottenOneAllows(bool moving, bool movement) { return !(moving && movement); }

// Amanitar (heroic)

// A Healthy Mushroom's Potent Fungus cancels Mini on everyone within 4 yards of it when it dies
constexpr float PotentFungusRadius = 4.0f;
constexpr float MushroomApproach = PotentFungusRadius - 1.5f;
// A Poisonous Mushroom clouds 5 yards round itself once a player comes within 3 yards
constexpr float PoisonCloudRadius = 5.0f;
constexpr float MushroomSearchRange = 30.0f;

// Mini cuts damage done by 75% but not healing, and the tank stays on the boss.
constexpr bool SeeksPotentFungus(bool heroic, bool mini, bool tank, bool healer)
{
    return heroic && mini && !tank && !healer;
}

// Walking up to a healthy mushroom must not take the bot into a poisonous one's cloud.
constexpr bool SafeMushroom(float distanceToBot, float distanceToNearestPoisonous)
{
    return distanceToBot <= MushroomSearchRange && distanceToNearestPoisonous > PoisonCloudRadius + MushroomApproach;
}

constexpr bool ApproachesMushroom(float distanceToMushroom) { return distanceToMushroom > MushroomApproach; }

// Stops a yard short so the bot isn't standing inside the mushroom's model
constexpr float MushroomStandOff = 1.0f;

constexpr float MushroomStep(float distanceToMushroom) { return distanceToMushroom - MushroomStandOff; }

// Assist would pull a shrunk bot back onto Amanitar before it kills the mushroom.
constexpr bool AmanitarAllows(bool seekingFungus, bool dpsAssist) { return !(seekingFungus && dpsAssist); }
}  // namespace AhnKahet

#endif
