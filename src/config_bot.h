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

  std::string                             m_BindAddress;                // map path
  uint16_t                                m_MinHostPort;                // the min port to host games on
  uint16_t                                m_MaxHostPort;                // the max port to host games on
  bool                                    m_EnableLANBalancer;          // enable to make LAN peers connect to m_LANHostPort
  uint16_t                                m_LANHostPort;                // the port to broadcast over LAN

  std::filesystem::path                   m_GreetingPath;               // the path of the greeting the bot sends to all players joining a game
  std::vector<std::string>                m_Greeting;                   // read from m_GreetingPath

  bool                                    m_UDPInfoStrictMode;          // set to false to send full game info periodically rather than small refresh packets
  bool                                    m_UDPForwardTraffic;          // whether to forward UDP traffic
  std::string                             m_UDPForwardAddress;          // the address to forward UDP traffic to
  uint16_t                                m_UDPForwardPort;             // the port to forward UDP traffic to
  bool                                    m_UDPForwardGameLists;        // whether to forward PvPGN game lists through UDP
  std::set<std::string>                   m_UDPBlockedIPs;              // list of IPs ignored by Aura's UDP server
  bool                                    m_UDPSupportGameRanger;       // enable to send refresh packets compatible with GameRanger - requires m_UDPInfoStrictMode
  std::vector<uint8_t>                    m_UDPGameRangerAddress;       // 
  uint16_t                                m_UDPGameRangerPort;          // 

  bool                                    m_AllowDownloads;             // allow map downloads or not
  uint8_t                                 m_AllowUploads;               // allow map downloads or not
  uint32_t                                m_MaxDownloaders;             // maximum number of map downloaders at the same time
  uint32_t                                m_MaxUploadSize;              // maximum total map size that we may transfer to players in lobbies
  uint32_t                                m_MaxUploadSpeed;             // maximum total map upload speed in KB/sec
  uint32_t                                m_MaxParallelMapPackets;      // map pieces sent in parallel to downloading users
  bool                                    m_RTTPings;                   // use LC style pings (divide actual pings by two
  bool                                    m_HasBufferBloat;
  uint32_t                                m_ReconnectWaitTime;          // the maximum number of minutes to wait for a GProxy++ reliable reconnect

  uint32_t                                m_MinHostCounter;             // defines a subspace for game identifiers
  uint32_t                                m_MaxGames;                   // maximum number of games in progress
  uint32_t                                m_MaxSavedMapSize;            // maximum byte size of maps kept persistently in the m_MapPath folder

  bool                                    m_StrictPaths;                // accept only exact paths (no fuzzy searches) for maps, etc.
  bool                                    m_EnableCFGCache;             // save read map CFGs to disk

  bool                                    m_ExitOnStandby;
  std::optional<bool>                     m_EnableBNET;                 // master switch to enable/disable ALL bnet configs on startup

  explicit CBotConfig(CConfig* CFG);
  ~CBotConfig();
};

#endif
