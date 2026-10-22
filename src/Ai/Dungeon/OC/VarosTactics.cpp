// Varos Cloudstrider bot trigger: fires while he is on the threat table; no strategy uses it yet.

#include "OCTriggers.h"
#include "Playerbots.h"

bool VarosCloudstriderTrigger::IsActive() { return AI_VALUE2(Unit*, "find target", "varos cloudstrider"); }
