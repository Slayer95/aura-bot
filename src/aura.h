/*

   Copyright [2010] [Josko Nikolic]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

 */

#ifndef AURA_AURA_H_
#define AURA_AURA_H_

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <random>

#define NOMINMAX
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#pragma once
#include <windows.h>
#endif

//
// CAura
//

class CUDPServer;
class CUDPSocket;
class CTCPSocket;
class CTCPServer;
class CGameProtocol;
class CGPSProtocol;
class CCRC32;
class CSHA1;
class CBNET;
class CGame;
class CAuraDB;
class CMap;
class CConfig;
class CIRC;
class CPotentialPlayer;

class CAura
{
public:
  CIRC*                    m_IRC;                        // IRC server
  CUDPServer*              m_UDPServer;                  // a UDP server for incoming traffic, and sending broadcasts, etc. (used with !sendlan)
  CUDPSocket*              m_UDPSocket;                  // a UDP socket for proxying UDP traffic
  CGameProtocol*           m_GameProtocol;               // class for game protocol
  CGPSProtocol*            m_GPSProtocol;                // class for gproxy protocol
  CCRC32*                  m_CRC;                        // for calculating CRC's
  CSHA1*                   m_SHA;                        // for calculating SHA1's
  std::vector<CBNET*>      m_BNETs;                      // all our battle.net connections (there can be more than one)
  CGame*                   m_CurrentLobby;                // this game is still in the lobby state
  std::vector<CGame*>      m_Games;                      // these games are in progress
  CAuraDB*                 m_DB;                         // database
  CMap*                    m_Map;                        // the currently loaded map
  std::string              m_Version;                    // Aura++ version string
  std::string              m_MapCFGPath;                 // config value: map cfg path
  std::string              m_MapPath;                    // config value: map path
  std::string              m_IndexVirtualHostName;       // config value: index virtual host name
  std::string              m_LobbyVirtualHostName;       // config value: lobby virtual host name
  std::string              m_LanguageFile;               // config value: language file
  std::string              m_Warcraft3Path;              // config value: Warcraft 3 path
  std::string              m_BindAddress;                // config value: the address to host games on
  std::vector<std::string> m_IgnoredNotifyJoinPlayers;   // config value: list of player names that won't trigger join notifications
  std::vector<std::string> m_IgnoredDatagramSources;     // config value: list of IPs ignored by m_UDPServer
  uint32_t                 m_ReconnectWaitTime;          // config value: the maximum number of minutes to wait for a GProxy++ reliable reconnect
  uint32_t                 m_MaxSavedMapSize;            // config value: maximum byte size of maps kept persistently in the m_MapPath folder
  uint32_t                 m_MaxGames;                   // config value: maximum number of games in progress
  uint32_t                 m_HostCounter;                // the current host counter (a unique number to identify a game, incremented each time a game is created)
  uint16_t                 m_HostPort;                   // the port of the last hosted game
  uint32_t                 m_MinHostCounter;             // config value: defines a subspace for game identifiers
  uint32_t                 m_AllowDownloads;             // config value: allow map downloads or not
  uint32_t                 m_AllowUploads;               // config value: allow map downloads or not
  uint32_t                 m_MaxDownloaders;             // config value: maximum number of map downloaders at the same time
  uint32_t                 m_MaxDownloadSpeed;           // config value: maximum total map download speed in KB/sec
  uint32_t                 m_AutoKickPing;               // config value: auto kick players with ping higher than this
  uint32_t                 m_LobbyTimeLimit;             // config value: auto close the game lobby after this many minutes without any reserved players
  uint32_t                 m_LobbyNoOwnerTime;           // config value: relinquish game ownership after this many minutes
  uint32_t                 m_Latency;                    // config value: the latency (by default)
  uint32_t                 m_SyncLimit;                  // config value: the maximum number of packets a player can fall out of sync before starting the lag screen (by default)
  uint32_t                 m_VoteKickPercentage;         // config value: percentage of players required to vote yes for a votekick to pass
  uint32_t                 m_NumPlayersToStartGameOver;  // config value: when this player count is reached, the game over timer will start
  uint16_t                 m_MinHostPort;                // config value: the min port to host games on
  uint16_t                 m_MaxHostPort;                // config value: the max port to host games on
  bool                     m_EnableLANBalancer;          // config value: enable to make LAN peers connect to m_LANHostPort
  uint16_t                 m_LANHostPort;                // config value: the port to broadcast over LAN
  bool                     m_EnablePvPGNTunnel;          // config value: enable to make peers from pvpgn servers connect to m_PublicHostAddress:m_PublicHostPort
  uint16_t                 m_PublicHostPort;             // config value: the port to broadcast in pvpgn servers
  std::string              m_PublicHostAddress;          // config value: the address to broadcast in pvpgn servers
  bool                     m_ProxyReconnectEnabled;      // config value: whether to listen to GProxy++ reconnects
  uint8_t                  m_LANWar3Version;             // config value: LAN warcraft 3 version
  uint8_t                  m_CommandTrigger;             // config value: the command trigger inside games
  std::string              m_GreetingPath;               // config value: the path of the greeting the bot sends to all players joining a game
  bool                     m_Exiting;                    // set to true to force aura to shutdown next update (used by SignalCatcher)
  bool                     m_Enabled;                    // set to false to prevent new games from being created
  bool                     m_EnabledPublic;              // set to false to allow non-admins to create games
  bool                     m_Ready;                      // indicates if there's lacking configuration info so we can quit
  bool                     m_NotifyJoins;                // whether the bot should beep when a player joins a hosted game
  bool                     m_UDPServerEnabled;           // whether the bot should listen to UDP traffic in port 6112
  bool                     m_UDPInfoStrictMode;          // set to false to send full game info periodically rather than small refresh packets
  uint32_t                 m_UDPForwardTraffic;          // config value: whether to forward UDP traffic
  std::string              m_UDPForwardAddress;          // config value: the address to forward UDP traffic to
  uint16_t                 m_UDPForwardPort;             // config value: the port to forward UDP traffic to
  uint32_t                 m_UDPForwardGameLists;        // config value: whether to forward PvPGN game lists through UDP
  bool                     m_LCPings;                    // config value: use LC style pings (divide actual pings by two)
  bool                     m_ResolveMapToConfig;
  std::vector<std::string> m_Greeting;                   // read from m_GreetingPath
  std::map<std::string, std::string> m_CachedMaps;   //
  std::unordered_multiset<std::string> m_BusyMaps;       //
  std::map<uint16_t, CTCPServer*> m_GameServers;
  std::map<uint16_t, std::vector<CPotentialPlayer*>> m_IncomingConnections; // (connections that haven't sent a W3GS_REQJOIN packet yet)

  std::vector<std::pair<std::string, std::string>> m_SudoUsers;
  std::string              m_SudoUser;
  std::string              m_SudoRealm;
  std::string              m_SudoAuthCommand;
  std::string              m_SudoExecCommand;

  explicit CAura(CConfig* CFG);
  ~CAura();
  CAura(CAura&) = delete;

  // processing functions

  bool Update();
  CTCPServer* GetGameServer(uint16_t, std::string& name);

  // events

  void EventBNETGameRefreshFailed(CBNET* bnet);
  void EventGameDeleted(CGame* game);

  // other functions

  void ReloadConfigs();
  void SetConfigs(CConfig* CFG);
  void ExtractScripts(const uint8_t War3Version);
  void LoadIPToCountryData();
  void CreateGame(CMap* map, uint8_t gameState, std::string gameName, std::string ownerName, std::string ownerServer, std::string creatorName, CBNET* nCreatorServer, bool whisper);
  std::vector<std::string> MapFilesMatch(std::string pattern);
  std::vector<std::string> ConfigFilesMatch(std::string pattern);

  inline uint16_t NextHostPort()
  {
    m_HostPort++;
    if (m_HostPort > m_MaxHostPort || m_HostPort < m_MinHostPort) {
      m_HostPort = m_MinHostPort;
    }
    return m_HostPort;
  }

  inline uint32_t NextHostCounter()
  {
    m_HostCounter = (m_HostCounter + 1) & 0x0FFFFFFF;
    if (m_HostCounter < m_MinHostCounter) {
      m_HostCounter = m_MinHostCounter;
    }
    return m_HostCounter;
  }

  inline std::string GetSudoAuthCommand(const std::string& Payload, char CommandTrigger) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    // Generate random hex digits
    std::string result;
    result.reserve(27 + Payload.length());
    result += std::string(1, static_cast<char>(CommandTrigger)) + "sudo ";

    for (std::size_t i = 0; i < 20; ++i) {
        const int randomDigit = dis(gen);
        result += (randomDigit < 10) ? (char)('0' + randomDigit) : (char)('a' + (randomDigit - 10));
    }

    result += " " + Payload;
    m_SudoAuthCommand = result;
    return result;
  }

  inline bool GetReady() const
  {
    return m_Ready;
  }

  inline bool GetNotifyJoins() const
  {
    return m_NotifyJoins;
  }

  inline bool GetUDPInfoStrictMode() const
  {
    return m_UDPInfoStrictMode;
  }

  inline bool IsIgnoredNotifyPlayer(std::string str) const
  {
    return std::find(m_IgnoredNotifyJoinPlayers.begin(), m_IgnoredNotifyJoinPlayers.end(), str) != m_IgnoredNotifyJoinPlayers.end();
  }

  inline bool IsIgnoredDatagramSource(std::string str) const
  {
    return std::find(m_IgnoredDatagramSources.begin(), m_IgnoredDatagramSources.end(), str) != m_IgnoredDatagramSources.end();
  }

  inline std::vector<std::string> GetGreeting() const
  {
    return m_Greeting;
  }
};

#endif // AURA_AURA_H_
