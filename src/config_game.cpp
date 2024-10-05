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

#define INHERIT(gameConfigKey) this->gameConfigKey = nRootConfig->gameConfigKey;

#define INHERIT_MAP(gameConfigKey, mapDataKey) \
  if ((nMap->mapDataKey).has_value()) { \
    this->gameConfigKey = (nMap->mapDataKey).value(); \
  } else { \
    this->gameConfigKey = nRootConfig->gameConfigKey; \
  }

#define INHERIT_CUSTOM(gameConfigKey, gameSetupKey) \
  if ((nGameSetup->gameSetupKey).has_value()) { \
    this->gameConfigKey = (nGameSetup->gameSetupKey).value(); \
  } else { \
    this->gameConfigKey = nRootConfig->gameConfigKey; \
  }

#define INHERIT_MAP_OR_CUSTOM(gameConfigKey, mapDataKey, gameSetupKey) \
  if ((nGameSetup->gameSetupKey).has_value()) { \
    this->gameConfigKey = (nGameSetup->gameSetupKey).value(); \
  } else if ((nMap->mapDataKey).has_value()) { \
    this->gameConfigKey = (nMap->mapDataKey).value(); \
  } else { \
    this->gameConfigKey = nRootConfig->gameConfigKey; \
  }

using namespace std;

//
// CGameConfig
//

CGameConfig::CGameConfig(CConfig& CFG)
{
  const static string emptyString;

  m_VoteKickPercentage        = CFG.GetInt("hosting.vote_kick.min_percent", 70);
  m_NumPlayersToStartGameOver = CFG.GetUint8("hosting.game_over.player_count", 1);
  m_MaxPlayersLoopback        = CFG.GetUint8("hosting.ip_filter.max_loopback", 8);
  m_MaxPlayersSameIP          = CFG.GetUint8("hosting.ip_filter.max_same_ip", 8);
  m_PlayersReadyMode          = CFG.GetStringIndex("hosting.game_ready.mode", {"fast", "race", "explicit"}, READY_MODE_EXPECT_RACE);
  m_SaveStats                 = CFG.GetBool("db.game_stats.enabled", true);
  m_SyncLimit                 = CFG.GetInt("net.start_lag.sync_limit", 32);
  m_SyncLimitSafe             = CFG.GetInt("net.stop_lag.sync_limit", 8);
  m_SyncNormalize             = CFG.GetBool("net.sync_normalization.enabled", true);
  if (m_SyncLimit <= m_SyncLimitSafe) {
    Print("<net.start_lag.sync_limit> must be larger than <net.stop_lag.sync_limit>");
    CFG.SetFailed();
  }
  m_AutoKickPing              = CFG.GetInt("hosting.high_ping.kick_ms", 250);
  m_WarnHighPing              = CFG.GetInt("hosting.high_ping.warn_ms", 175);
  m_SafeHighPing              = CFG.GetInt("hosting.high_ping.safe_ms", 130);
  m_LobbyTimeout              = CFG.GetInt("hosting.abandoned_lobby.game_expiry_time", 600);
  m_LobbyOwnerTimeout         = CFG.GetInt("hosting.abandoned_lobby.owner_expiry_time", 120);
  m_LobbyCountDownInterval    = CFG.GetInt("hosting.game_start.count_down_interval", 500);
  m_LobbyCountDownStartValue  = CFG.GetInt("hosting.game_start.count_down_ticks", 5);
  m_Latency                   = CFG.GetUint16("bot.latency", 100);
  m_PerfThreshold             = CFG.GetInt("bot.perf_limit", 150);
  m_LacksMapKickDelay         = CFG.GetInt("hosting.map.missing.kick_delay", 60); // default: 1 minute

  m_CheckJoinable             = CFG.GetBool("monitor.hosting.on_start.check_connectivity", false);
  m_ExtraDiscoveryAddresses   = CFG.GetIPStringSet("net.game_discovery.udp.extra_clients.ip_addresses", ',', {});
  m_ExtraDiscoveryStrict      = CFG.GetBool("net.game_discovery.udp.extra_clients.strict", false);

  m_PrivateCmdToken           = CFG.GetString("hosting.commands.trigger", "!");
  m_BroadcastCmdToken         = CFG.GetString("hosting.commands.broadcast.trigger", emptyString);
  m_EnableBroadcast           = CFG.GetBool("hosting.commands.broadcast.enabled", false);

  if (!m_EnableBroadcast)
    m_BroadcastCmdToken.clear();

  m_IndexVirtualHostName      = CFG.GetString("hosting.index.creator_name", 1, 15, emptyString);
  m_LobbyVirtualHostName      = CFG.GetString("hosting.self.virtual_player.name", 1, 15, "|cFF4080C0Aura");

  m_NotifyJoins               = CFG.GetBool("ui.notify_joins.enabled", false);
  m_IgnoredNotifyJoinPlayers  = CFG.GetSet("ui.notify_joins.exceptions", ',', {});
  m_LoggedWords               = CFG.GetSetInsensitive("hosting.log_words", ',', {});
  m_DesyncHandler             = CFG.GetStringIndex("hosting.desync.handler", {"none", "notify", "drop"}, ON_DESYNC_NOTIFY);
  m_IPFloodHandler            = CFG.GetStringIndex("hosting.ip_filter.flood_handler", {"none", "notify", "deny"}, ON_IPFLOOD_DENY);
  m_UDPEnabled                = CFG.GetBool("net.game_discovery.udp.enabled", true);

  set<uint8_t> supportedGameVersions = CFG.GetUint8Set("hosting.crossplay.versions", ',', {});
  m_SupportedGameVersions = vector<uint8_t>(supportedGameVersions.begin(), supportedGameVersions.end());

  if (m_VoteKickPercentage > 100)
    m_VoteKickPercentage = 100;
}

CGameConfig::CGameConfig(CGameConfig* nRootConfig, CMap* nMap, CGameSetup* nGameSetup)
{
  INHERIT(m_VoteKickPercentage)
  INHERIT_MAP_OR_CUSTOM(m_NumPlayersToStartGameOver, m_NumPlayersToStartGameOver, m_NumPlayersToStartGameOver)
  INHERIT(m_MaxPlayersLoopback)
  INHERIT(m_MaxPlayersSameIP)
  INHERIT_MAP_OR_CUSTOM(m_PlayersReadyMode, m_PlayersReadyMode, m_PlayersReadyMode)
  INHERIT(m_SaveStats);
  INHERIT_MAP_OR_CUSTOM(m_SyncLimit, m_LatencyMaxFrames, m_LatencyMaxFrames)
  INHERIT_MAP_OR_CUSTOM(m_SyncLimitSafe, m_LatencySafeFrames, m_LatencySafeFrames)
  INHERIT_CUSTOM(m_SyncNormalize, m_SyncNormalize)
  INHERIT_MAP_OR_CUSTOM(m_AutoKickPing, m_AutoKickPing, m_AutoKickPing)
  INHERIT_MAP_OR_CUSTOM(m_WarnHighPing, m_WarnHighPing, m_WarnHighPing)
  INHERIT_MAP_OR_CUSTOM(m_SafeHighPing, m_SafeHighPing, m_SafeHighPing)
  INHERIT_MAP_OR_CUSTOM(m_LobbyTimeout, m_LobbyTimeout, m_LobbyTimeout)
  INHERIT_MAP_OR_CUSTOM(m_LobbyOwnerTimeout, m_LobbyOwnerTimeout, m_LobbyOwnerTimeout)
  INHERIT_MAP_OR_CUSTOM(m_LobbyCountDownInterval, m_LobbyCountDownInterval, m_LobbyCountDownInterval)
  INHERIT_MAP_OR_CUSTOM(m_LobbyCountDownStartValue, m_LobbyCountDownStartValue, m_LobbyCountDownStartValue)
  INHERIT_MAP_OR_CUSTOM(m_Latency, m_Latency, m_LatencyAverage)
  INHERIT(m_PerfThreshold)
  INHERIT(m_LacksMapKickDelay)

  INHERIT_CUSTOM(m_CheckJoinable, m_CheckJoinable)
  INHERIT(m_ExtraDiscoveryAddresses)
  // TODO: Implement m_ExtraDiscoveryStrict
  INHERIT(m_ExtraDiscoveryStrict)

  INHERIT(m_PrivateCmdToken)
  INHERIT(m_BroadcastCmdToken)
  INHERIT(m_EnableBroadcast)

  INHERIT(m_IndexVirtualHostName)
  INHERIT(m_LobbyVirtualHostName)

  INHERIT_CUSTOM(m_NotifyJoins, m_NotifyJoins)
  INHERIT(m_IgnoredNotifyJoinPlayers)
  INHERIT(m_LoggedWords)
  INHERIT(m_DesyncHandler)
  INHERIT_CUSTOM(m_IPFloodHandler, m_IPFloodHandler)
  INHERIT(m_UDPEnabled)

  INHERIT(m_SupportedGameVersions)
  INHERIT(m_VoteKickPercentage)
}

CGameConfig::~CGameConfig() = default;
