/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HasTotemValue.h"
#include "Playerbots.h"
#include "SubclassBotMgr.h"

char* strstri(char const* str1, char const* str2);

bool HasTotemValue::Calculate()
{
    for (uint8 i = 0; i < MAX_SUMMON_SLOT; ++i)
    {
        if (!bot->m_SummonSlot[i])
        {
            continue;
        }

        if (Creature* OldTotem = bot->GetMap()->GetCreature(bot->m_SummonSlot[i]))
        {
            if (OldTotem->IsSummon() && OldTotem->GetDistance(bot) <= 30.0f)
            {
                if (strstri(OldTotem->GetName().c_str(), qualifier.c_str()))
                    return true;
                // A path's totem in another body (a Witch Doctor's Serpent Ward) is still the totem it replaces
                uint32 const original = SubclassBotMgr::instance().OriginalCreature(OldTotem->GetEntry());
                if (original != OldTotem->GetEntry())
                    if (CreatureTemplate const* totem = sObjectMgr->GetCreatureTemplate(original))
                        if (strstri(totem->Name.c_str(), qualifier.c_str()))
                            return true;
            }
        }
    }
    return false;
}

// bool HasTotemValue::Calculate()
// {
//     for (uint8 i = 0; i < MAX_SUMMON_SLOT; ++i)
//     {
//         if (!bot->m_SummonSlot[i])
//         {
//             continue;
//         }

//         if (Creature* OldTotem = bot->GetMap()->GetCreature(bot->m_SummonSlot[i]))
//         {
//             if (OldTotem->IsSummon())
//             {
//                 if (strstri(creature->GetName().c_str(), qualifier.c_str()))
//                     return true;
//             }
//         }
//     }

//     GuidVector units = *context->GetValue<GuidVector>("nearest totems");
//     for (ObjectGuid const guid : units)
//     {
//         Unit* unit = botAI->GetUnit(guid);
//         if (!unit)
//             continue;
//         Creature* creature = dynamic_cast<Creature*>(unit);

//         if (creature->GetOwner() != bot)
//         {
//             continue;
//         }

//         if (strstri(creature->GetName().c_str(), qualifier.c_str()))
//             return true;
//     }

//     return false;
// }
