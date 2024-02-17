#ifndef AURA_CONFIG_GAME_H_
#define AURA_CONFIG_GAME_H_

#include "config.h"

#include <vector>
#include <string>
#include <map>
#include <unordered_set>

//
// CGameConfig
//

class CGameConfig
{
public:
  uint32_t                 m_VoteKickPercentage;         // percentage of players required to vote yes for a votekick to pass
  uint32_t                 m_NumPlayersToStartGameOver;  // when this player count is reached, the game over timer will start
  float                    m_SyncLimit;                  // the maximum number of packets a player can fall out of sync before starting the lag screen (by default)
  float                    m_SyncLimitSafe;              // the maximum number of packets a player can fall out of sync before starting the lag screen (by default)
  float                    m_SyncFactor;                 // ratio of clients keepalive period / bot_latency
  uint32_t                 m_AutoKickPing;               // auto kick players with ping higher than this
  uint32_t                 m_WarnHighPing;               // auto kick players with ping higher than this
  uint32_t                 m_LobbyTimeLimit;             // auto close the game lobby after this many minutes without any owner
  uint32_t                 m_LobbyNoOwnerTime;           // relinquish game ownership after this many minutes
  uint16_t                 m_Latency;                    // the latency (by default)
  uint32_t                 m_PerfThreshold;              // the max expected delay between updates - if exceeded it means performance is suffering
  uint32_t                 m_LacksMapKickDelay;

  char                     m_CommandTrigger;             // the command trigger inside games
  std::string              m_IndexVirtualHostName;       // index virtual host name
  std::string              m_LobbyVirtualHostName;       // lobby virtual host name
  bool                     m_NotifyJoins;                // whether the bot should beep when a player joins a hosted game
  std::set<std::string>    m_IgnoredNotifyJoinPlayers;

  bool                     m_LANEnabled;

  explicit CGameConfig(CConfig* CFG);
  ~CGameConfig();
};

#endif
