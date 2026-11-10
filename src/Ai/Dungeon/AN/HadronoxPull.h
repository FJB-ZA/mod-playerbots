/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_HADRONOXPULL_H
#define PLAYERBOTS_HADRONOXPULL_H

class Creature;
class Player;

namespace AzjolNerub
{
bool SkipHadronoxPull(Creature* mob);
bool HadronoxTraffic(Player* bot, float x, float y, float z);
bool WaitForHadronox(Player* tank);
}

#endif
