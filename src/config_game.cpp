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

#include "config_game.h"
#include "util.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CGameConfig
//

CGameConfig::CGameConfig(CConfig* CFG)
{
  m_VoteKickPercentage        = CFG->GetInt("bot_vote_kick_percentage", 70);
  m_NumPlayersToStartGameOver = CFG->GetInt("bot_game_over_player_count", 1);
  m_SyncLimit                 = CFG->GetFloat("net_player_sync_limit", 10.0f);
  m_SyncLimitSafe             = CFG->GetFloat("net_player_sync_limit_safe", 3.0f);
  m_AutoKickPing              = CFG->GetInt("bot_auto_kick_ping", 300);
  m_WarnHighPing              = CFG->GetInt("bot_warnhighping", 200);
  m_LobbyTimeLimit            = CFG->GetInt("bot_lobby_time_limit", 10);
  m_LobbyNoOwnerTime          = CFG->GetInt("bot_lobby_ownerless_time", 2);
  m_Latency                   = static_cast<uint16_t>(CFG->GetInt("bot_latency", 100));
  m_PerfThreshold             = CFG->GetInt("bot_perf_limit", 150);
  m_LacksMapKickDelay         = CFG->GetInt("bot_nomapkickdelay", 60);

  string BotCommandTrigger    = CFG->GetString("bot_command_trigger", "!");
  m_CommandTrigger            = BotCommandTrigger[0];
  m_IndexVirtualHostName      = CFG->GetString("bot_index_virtual_host_name", 1, 15, "Aura Bot");
  m_LobbyVirtualHostName      = CFG->GetString("bot_lobby_virtual_host_name", 1, 15, "|cFF4080C0Aura");

  m_NotifyJoins               = CFG->GetBool("bot_notify_joins", false);
  m_IgnoredNotifyJoinPlayers  = CFG->GetSet("bot_notify_joins_exceptions", ',', {});
  m_LANEnabled                = CFG->GetBool("bot_enablelan", true);

  if (m_VoteKickPercentage > 100)
    m_VoteKickPercentage = 100;
}

CGameConfig::~CGameConfig() = default;
