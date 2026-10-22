// Cyanigosa bot tactics: melee and healers stand on her flank, out of her breath and tail.

#include "Playerbots.h"
#include "VHPolicy.h"
#include "VHTriggers.h"

// Healers count: left in front or behind they stand in the breath or the tail
bool CyanigosaPositioningTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "cyanigosa") &&
           VioletHold::StandsOnFlank(PlayerbotAI::IsTank(bot), PlayerbotAI::IsRangedDps(bot));
}
