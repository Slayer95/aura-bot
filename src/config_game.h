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
#include "gamesetup.h"
#include "map.h"

#include <vector>
#include <set>
#include <string>
#include <map>

#define ON_DESYNC_NONE 0u
#define ON_DESYNC_NOTIFY 1u
#define ON_DESYNC_DROP 2u

#define ON_IPFLOOD_NONE 0u
#define ON_IPFLOOD_NOTIFY 1u
#define ON_IPFLOOD_DENY 2u

#define READY_MODE_FAST 0u
#define READY_MODE_EXPECT_RACE 1u
#define READY_MODE_EXPLICIT 2u

//
// CGameConfig
//

// Override order
// Default game config
// Default map config (*) map.flags_locked = yes/no
// Game setup

class CGameSetup;
class CMap;

class CGameConfig
{
public:
  uint32_t                 m_VoteKickPercentage;         // percentage of players required to vote yes for a votekick to pass
  uint8_t                  m_NumPlayersToStartGameOver;  // when this player count is reached, the game over timer will start
  uint8_t                  m_MaxPlayersLoopback;
  uint8_t                  m_MaxPlayersSameIP;
  uint8_t                  m_PlayersReadyMode;
  
  uint32_t                 m_SyncLimit;                  // the maximum number of packets a user can fall out of sync before starting the lag screen (by default)
  uint32_t                 m_SyncLimitSafe;              // the maximum number of packets a user can fall out of sync before starting the lag screen (by default)
  bool                     m_SyncNormalize;              // before 3-minute mark, try to keep players in the game
  uint32_t                 m_AutoKickPing;               // auto kick players with ping higher than this
  uint32_t                 m_WarnHighPing;               // announce on chat when players have a ping higher than this value
  uint32_t                 m_SafeHighPing;               // when players ping drops below this value, announce they no longer have high ping
  uint32_t                 m_LobbyTimeout;             // auto close the game lobby after this many minutes without any owner
  uint32_t                 m_LobbyOwnerTimeout;           // relinquish game ownership after this many minutes
  uint32_t                 m_LobbyCountDownInterval;     // ms between each number count down when !start is issued
  uint32_t                 m_LobbyCountDownStartValue;   // number at which !start count down begins
  uint16_t                 m_Latency;                    // the latency (by default)
  uint32_t                 m_PerfThreshold;              // the max expected delay between updates - if exceeded it means performance is suffering
  uint32_t                 m_LacksMapKickDelay;

  bool                     m_CheckJoinable;
  std::set<std::string>    m_ExtraDiscoveryAddresses;    // list of addresses Aura announces hosted games to through UDP unicast.
  bool                     m_ExtraDiscoveryStrict;

  std::string              m_PrivateCmdToken;            // a symbol prefix to identify commands and send a private reply
  std::string              m_BroadcastCmdToken;          // a symbol prefix to identify commands and send the reply to everyone
  bool                     m_EnableBroadcast;
  std::string              m_IndexVirtualHostName;       // index virtual host name
  std::string              m_LobbyVirtualHostName;       // lobby virtual host name
  bool                     m_NotifyJoins;                // whether the bot should beep when a user joins a hosted game
  std::set<std::string>    m_IgnoredNotifyJoinPlayers;
  std::set<std::string>    m_LoggedWords;
  uint8_t                  m_DesyncHandler;
  uint8_t                  m_IPFloodHandler;

  bool                     m_UDPEnabled;                 // whether this game should be listed in "Local Area Network"
  std::vector<uint8_t>     m_SupportedGameVersions;

  explicit CGameConfig(CConfig& CFG);
  explicit CGameConfig(CGameConfig* nRootConfig, CMap* nMap, CGameSetup* nGameSetup);
  ~CGameConfig();
};

#endif
