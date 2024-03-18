/*

  Copyright [2024] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

#ifndef AURA_CONFIG_GAME_H_
#define AURA_CONFIG_GAME_H_

#include "config.h"

#include <vector>
#include <set>
#include <string>
#include <map>

//
// CGameConfig
//

class CGameConfig
{
public:
  uint32_t                 m_VoteKickPercentage;         // percentage of players required to vote yes for a votekick to pass
  uint32_t                 m_NumPlayersToStartGameOver;  // when this player count is reached, the game over timer will start
  uint32_t                 m_SyncLimit;                  // the maximum number of packets a player can fall out of sync before starting the lag screen (by default)
  uint32_t                 m_SyncLimitSafe;              // the maximum number of packets a player can fall out of sync before starting the lag screen (by default)
  uint32_t                 m_AutoKickPing;               // auto kick players with ping higher than this
  uint32_t                 m_WarnHighPing;               // auto kick players with ping higher than this
  uint32_t                 m_LobbyTimeLimit;             // auto close the game lobby after this many minutes without any owner
  uint32_t                 m_LobbyNoOwnerTime;           // relinquish game ownership after this many minutes
  uint16_t                 m_Latency;                    // the latency (by default)
  uint32_t                 m_PerfThreshold;              // the max expected delay between updates - if exceeded it means performance is suffering
  uint32_t                 m_LacksMapKickDelay;

  bool                     m_CheckJoinable;
  std::set<std::string>    m_ExtraDiscoveryAddresses;    // list of addresses Aura announces hosted games to through UDP unicast.
  bool                     m_ExtraDiscoveryStrict;

  char                     m_CommandTrigger;             // the command trigger inside games
  std::string              m_IndexVirtualHostName;       // index virtual host name
  std::string              m_LobbyVirtualHostName;       // lobby virtual host name
  bool                     m_NotifyJoins;                // whether the bot should beep when a player joins a hosted game
  std::set<std::string>    m_IgnoredNotifyJoinPlayers;

  bool                     m_UDPEnabled;

  explicit CGameConfig(CConfig* CFG);
  ~CGameConfig();
};

#endif
