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
#define AURA_CONFIG_BOT_H_

#include "config.h"

#include <vector>
#include <set>
#include <string>
#include <map>
#include <unordered_set>

#define MAP_TRANSFERS_NEVER 0
#define MAP_TRANSFERS_AUTOMATIC 1
#define MAP_TRANSFERS_MANUAL 2

//
// CBotConfig
//

class CBotConfig
{
public:

  bool                                    m_Enabled;                    // set to false to prevent new games from being created
  bool                                    m_ProxyReconnectEnabled;      // whether to listen to GProxy++ reconnects
  std::optional<uint8_t>                  m_War3Version;                // warcraft 3 version
  std::optional<std::filesystem::path>    m_Warcraft3Path;              // Warcraft 3 path
  std::filesystem::path                   m_MapCFGPath;                 // map cfg path
  std::filesystem::path                   m_MapPath;                    // map path

  sockaddr_storage                        m_BindAddress4;               // Defaults to 0.0.0.0
  sockaddr_storage                        m_BindAddress6;               // Defaults to ::
  uint16_t                                m_MinHostPort;                // the min port to host games on
  uint16_t                                m_MaxHostPort;                // the max port to host games on

  bool                                    m_UDPEnableCustomPortTCP4;    // enable to make IPv4 peers connect to m_UDPCustomPortTCP4
  uint16_t                                m_UDPCustomPortTCP4;          // the TCP port to broadcast over LAN, or to specific IPv4 clients
  bool                                    m_UDPEnableCustomPortTCP6;    // enable to make IPv6 peers connect to m_UDPCustomPortTCP6
  uint16_t                                m_UDPCustomPortTCP6;          // the TCP port to announce to IPv6 clients

  bool                                    m_EnableTCPWrapUDP;
  bool                                    m_EnableTCPScanUDP;

  uint16_t                                m_UDP6TargetPort;             // the remote UDP port to which we send unicast game discovery messages over IPv6

  std::filesystem::path                   m_GreetingPath;               // the path of the greeting the bot sends to all players joining a game
  std::vector<std::string>                m_Greeting;                   // read from m_GreetingPath

  bool                                    m_UDPBroadcastStrictMode;          // set to false to send full game info periodically rather than small refresh packets
  bool                                    m_UDPForwardTraffic;          // whether to forward UDP traffic
  std::string                             m_UDPForwardAddress;          // the address to forward UDP traffic to
  uint16_t                                m_UDPForwardPort;             // the port to forward UDP traffic to
  bool                                    m_UDPForwardGameLists;        // whether to forward PvPGN game lists through UDP unicast.
  bool                                    m_UDPBroadcastEnabled;        // whether to perform UDP broadcasts to announce hosted games. (unicast is in config_game)
  std::set<std::string>                   m_UDPBlockedIPs;              // list of IPs ignored by Aura's UDP server
  bool                                    m_UDPSupportGameRanger;       // enable to send refresh packets compatible with GameRanger - requires m_UDPBroadcastStrictMode
  std::vector<uint8_t>                    m_UDPGameRangerAddress;       // 
  uint16_t                                m_UDPGameRangerPort;          // 

  bool                                    m_AllowDownloads;             // allow map downloads or not
  uint8_t                                 m_AllowTransfers;             // map transfers mode
  uint32_t                                m_MaxDownloaders;             // maximum number of map downloaders at the same time
  uint32_t                                m_MaxUploadSize;              // maximum total map size that we may transfer to players in lobbies
  uint32_t                                m_MaxUploadSpeed;             // maximum total map upload speed in KB/sec
  uint32_t                                m_MaxParallelMapPackets;      // map pieces sent in parallel to downloading users
  bool                                    m_RTTPings;                   // use LC style pings (divide actual pings by two
  bool                                    m_HasBufferBloat;

  uint8_t                                 m_ReconnectWaitTime;          // the maximum number of minutes to wait for a GProxyDLL reconnect
  uint8_t                                 m_ReconnectWaitTimeLegacy;    // the maximum number of minutes to wait for a GProxy++ reconnect

  bool                                    m_AnnounceGProxy;
  std::string                             m_AnnounceGProxySite;
  bool                                    m_AnnounceIPv6;

  uint32_t                                m_MinHostCounter;             // defines a subspace for game identifiers
  uint32_t                                m_MaxGames;                   // maximum number of games in progress
  uint32_t                                m_MaxSavedMapSize;            // maximum byte size of maps kept persistently in the m_MapPath folder

  bool                                    m_StrictPaths;                // accept only exact paths (no fuzzy searches) for maps, etc.
  bool                                    m_EnableCFGCache;             // save read map CFGs to disk

  bool                                    m_EnableUPnP;
  uint8_t                                 m_PublicIPv4Algorithm;
  std::string                             m_PublicIPv4Value;
  uint8_t                                 m_PublicIPv6Algorithm;
  std::string                             m_PublicIPv6Value;

  bool                                    m_ExitOnStandby;
  std::optional<bool>                     m_EnableBNET;                 // master switch to enable/disable ALL bnet configs on startup

  sockaddr_storage                        m_BroadcastTarget;
  
  explicit CBotConfig(CConfig* CFG);
  ~CBotConfig();
};

#endif
