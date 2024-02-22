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

#ifndef AURA_CONFIG_BOT_H_
#define AURA_CONFIG_BOT_

#include "config_bot.h"
#include "util.h"
#include "net.h"

#include <utility>
#include <algorithm>
#include <fstream>

using namespace std;

//
// CBotConfig
//

CBotConfig::CBotConfig(CConfig* CFG)
{
  const static string emptyString;

  m_Enabled                = CFG->GetBool("hosting.enabled", true);
  m_ProxyReconnectEnabled  = CFG->GetBool("net.gproxy.enabled", true);
  m_War3Version            = CFG->GetMaybeInt("game.version"/*, 27*/);
  m_Warcraft3Path          = CFG->GetMaybePath("game.install_path"/*, filesystem::path(R"(C:\Program Files\Warcraft III\)")*/);
  m_MapCFGPath             = CFG->GetPath("bot.map_configs_path", filesystem::path());
  m_MapPath                = CFG->GetPath("bot.maps_path", filesystem::path());

  m_BindAddress            = CFG->GetString("net.bind_address", emptyString);
  m_MinHostPort            = static_cast<uint16_t>(CFG->GetInt("net.host_port.min", CFG->GetInt("net.host_port.only", 6112)));
  m_MaxHostPort            = static_cast<uint16_t>(CFG->GetInt("net.host_port.max", m_MinHostPort));
  m_EnableLANBalancer      = CFG->GetBool("net.game_discovery.udp.balancer_port.enabled", false);
  m_LANHostPort            = static_cast<uint16_t>(CFG->GetInt("net.game_discovery.udp.balancer_port.value", 6112));

  /* Make absolute, lexically normal */
  m_GreetingPath           = CFG->GetPath("bot.greeting_path", filesystem::path());
  if (!m_GreetingPath.empty()) {
    ifstream in;
    in.open(m_GreetingPath.string(), ios::in);
    if (!in.fail()) {
      while (!in.eof()) {
        string Line;
        getline(in, Line);
        if (Line.empty()) {
          if (!in.eof())
            m_Greeting.push_back(" ");
        } else {
          m_Greeting.push_back(Line);
        }
      }
      in.close( );
    }
  }

  m_UDPInfoStrictMode            = CFG->GetBool("net.game_discovery.udp.strict", true);
  m_UDPForwardTraffic            = CFG->GetBool("net.udp_redirect.enabled", false);
  m_UDPForwardAddress            = CFG->GetString("net.udp_redirect.ip_address", emptyString);
  m_UDPForwardPort               = static_cast<uint16_t>(CFG->GetInt("net.udp_redirect.port", 6110));
  m_UDPForwardGameLists          = CFG->GetBool("net.udp_redirect.realm_game_lists.enabled", false);
  m_UDPBroadcastEnabled          = CFG->GetBool("net.game_discovery.udp.broadcast.enabled", true);
  m_UDPBlockedIPs                = CFG->GetIPv4Set("net.udp_server.block_list", ',', {});

  m_UDPSupportGameRanger         = CFG->GetBool("net.game_discovery.udp.gameranger.enabled", false);
  m_UDPGameRangerAddress         = CFG->GetIPv4("net.game_discovery.udp.gameranger.ip_address", {255, 255, 255, 255});
  m_UDPGameRangerPort            = static_cast<uint16_t>(CFG->GetInt("net.game_discovery.udp.gameranger.port--but_its_hardcoded", 6112));

  m_AllowDownloads               = CFG->GetBool("bot.allow_downloads", false);
  m_AllowTransfers               = static_cast<uint8_t>(CFG->GetInt("hosting.map_transfers.enabled", MAP_TRANSFERS_AUTOMATIC));
  m_MaxDownloaders               = CFG->GetInt("hosting.map_transfers.max_players", 3);
  m_MaxUploadSize                = CFG->GetInt("hosting.map_transfers.max_size", 8192);
  m_MaxUploadSpeed               = CFG->GetInt("hosting.map_transfers.max_speed", 1024);
  m_MaxParallelMapPackets        = CFG->GetInt("hosting.map_transfers.max_parallel_packets", 1000);
  m_RTTPings                     = CFG->GetBool("metrics.rtt_pings", false);
  m_HasBufferBloat               = CFG->GetBool("net.has_buffer_bloat", false);
  m_ReconnectWaitTime            = static_cast<uint8_t>(CFG->GetInt("net.player_reconnect.wait", 3));

  m_MinHostCounter               = CFG->GetInt("hosting.namepace.first_game_id", 100);
  m_MaxGames                     = CFG->GetInt("hosting.max_games", 20);
  m_MaxSavedMapSize              = CFG->GetInt("bot.max_persistent_size", 0xFFFFFFFF);

  m_StrictPaths                  = CFG->GetBool("bot.load_maps.strict_paths", false);
  m_EnableCFGCache               = CFG->GetBool("bot.load_maps.cache.enabled", true);
  m_EnableUPnP                   = CFG->GetBool("net.port_forwarding.upnp.enabled", true);

  string ipAlgorithm             = CFG->GetString("net.public_ip_address.algorithm", "api");
  if (ipAlgorithm == "manual") {
    m_PublicIPAlgorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL;
  } else if (ipAlgorithm == "api") {
    m_PublicIPAlgorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_API;
  } else if (ipAlgorithm == "none") {
    m_PublicIPAlgorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE;
  } else {
    m_PublicIPAlgorithm = NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE;
  }

  m_PublicIPValue                = CFG->GetString("net.public_ip_address.value",
    m_PublicIPAlgorithm == NET_PUBLIC_IP_ADDRESS_ALGORITHM_API ? "https://api.ipify.org" : ""
  );

  m_ExitOnStandby                = CFG->GetBool("bot.exit_on_standby", false);

  // Master switch mainly intended for CLI. CFG key provided for completeness.
  m_EnableBNET                   = CFG->GetMaybeBool("bot.toggle_every_realm");
  if (m_EnableBNET.has_value()) {
    Print("m_EnableBNET has value");
    if (m_EnableBNET.value()) {
      Print("m_EnableBNET true");
    } else {
      Print("m_EnableBNET false");
    }
  } else {
    Print("m_EnableBNET has no value");
  }
}

CBotConfig::~CBotConfig() = default;
#endif
