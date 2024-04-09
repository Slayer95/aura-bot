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

CGameConfig::CGameConfig(CConfig& CFG)
{
  const static string emptyString;

  m_VoteKickPercentage        = CFG.GetInt("hosting.vote_kick.min_percent", 70);
  m_NumPlayersToStartGameOver = CFG.GetInt("hosting.game_over.player_count", 1);
  m_SyncLimit                 = CFG.GetInt("net.start_lag.sync_limit", 10);
  m_SyncLimitSafe             = CFG.GetInt("net.stop_lag.sync_limit", 3);
  m_AutoKickPing              = CFG.GetInt("hosting.high_ping.kick_ms", 300);
  m_WarnHighPing              = CFG.GetInt("hosting.high_ping.warn_ms", 200);
  m_LobbyTimeLimit            = CFG.GetInt("hosting.abandoned_lobby.game_expiry_time", 10);
  m_LobbyNoOwnerTime          = CFG.GetInt("hosting.abandoned_lobby.owner_expiry_time", 2);
  m_Latency                   = CFG.GetUint16("bot.latency", 100);
  m_PerfThreshold             = CFG.GetInt("bot.perf_limit", 150);
  m_LacksMapKickDelay         = CFG.GetInt("hosting.map_missing.kick_delay", 60); // default: 1 minute

  m_CheckJoinable             = CFG.GetBool("monitor.hosting.on_start.check_connectivity", false);
  m_ExtraDiscoveryAddresses   = CFG.GetIPStringSet("net.game_discovery.udp.extra_clients.ip_addresses", ',', {});
  m_ExtraDiscoveryStrict      = CFG.GetBool("net.game_discovery.udp.extra_clients.strict", false);

  m_PrivateCmdToken           = CFG.GetString("hosting.commands.trigger", "!");
  m_BroadcastCmdToken         = CFG.GetString("hosting.commands.broadcast.trigger", emptyString);
  m_EnableBroadcast           = CFG.GetBool("hosting.commands.broadcast.enabled", false);

  if (!m_EnableBroadcast)
    m_BroadcastCmdToken.clear();

  m_IndexVirtualHostName      = CFG.GetString("hosting.index.creator_name", 1, 15, "Aura Bot");
  m_LobbyVirtualHostName      = CFG.GetString("hosting.self.virtual_player.name", 1, 15, "|cFF4080C0Aura");

  m_NotifyJoins               = CFG.GetBool("ui.notify_joins.enabled", false);
  m_IgnoredNotifyJoinPlayers  = CFG.GetSet("ui.notify_joins.exceptions", ',', {});
  m_UDPEnabled                = CFG.GetBool("net.game_discovery.udp.enabled", true);

  if (m_VoteKickPercentage > 100)
    m_VoteKickPercentage = 100;
}

CGameConfig::~CGameConfig() = default;
