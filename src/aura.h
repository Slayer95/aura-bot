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

#include "config.h"
#include "config_bot.h"
#include "config_bnet.h"
#include "config_game.h"

#include <cstdint>
#include <vector>
#include <set>
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
class CIRC;
class CPotentialPlayer;

class CAura
{
public:
  CIRC*                                              m_IRC;                        // IRC server
  CUDPServer*                                        m_UDPServer;                  // a UDP server for incoming traffic, and sending broadcasts, etc. (used with !sendlan)
  CUDPSocket*                                        m_UDPSocket;                  // a UDP socket for proxying UDP traffic
  CGameProtocol*                                     m_GameProtocol;               // class for game protocol
  CGPSProtocol*                                      m_GPSProtocol;                // class for gproxy protocol
  CCRC32*                                            m_CRC;                        // for calculating CRC's
  CSHA1*                                             m_SHA;                        // for calculating SHA1's
  std::vector<CBNET*>                                m_BNETs;                      // all our battle.net connections (there can be more than one)
  CGame*                                             m_CurrentLobby;                // this game is still in the lobby state
  std::vector<CGame*>                                m_Games;                      // these games are in progress
  CAuraDB*                                           m_DB;                         // database
  CMap*                                              m_Map;                        // the currently loaded map
  std::string                                        m_Version;                    // Aura++ version string

  uint32_t                                           m_HostCounter;                // the current host counter (a unique number to identify a game, incremented each time a game is created)
  uint16_t                                           m_HostPort;                   // the port of the last hosted game
  bool                                               m_UDPServerEnabled;           // whether the bot should listen to UDP traffic in port 6112)
  size_t                                             m_MaxGameNameSize;
  bool                                               m_Exiting;                    // set to true to force aura to shutdown next update (used by SignalCatcher)
  bool                                               m_Ready;                      // indicates if there's lacking configuration info so we can quit
  bool                                               m_ScriptsExtracted;           // indicates if there's lacking configuration info so we can quit

  std::map<std::string, std::string>                 m_CachedMaps;       //
  std::unordered_multiset<std::string>               m_BusyMaps;       //

  std::map<uint16_t, CTCPServer*>                    m_GameServers;
  std::map<uint16_t, std::vector<CPotentialPlayer*>> m_IncomingConnections; // (connections that haven't sent a W3GS_REQJOIN packet yet)
  std::string                                        m_SudoUser;
  std::string                                        m_SudoRealm;
  std::string                                        m_SudoAuthCommand;
  std::string                                        m_SudoExecCommand;
  std::vector<std::vector<std::string>>              m_PendingActions;

  uint16_t                                           m_GameRangerLocalPort;
  std::string                                        m_GameRangerLocalAddress;
  uint16_t                                           m_GameRangerRemotePort;
  std::vector<uint8_t>                               m_GameRangerRemoteAddress;

  CBotConfig*                                        m_Config;
  CBNETConfig*                                       m_BNETDefaultConfig;
  CGameConfig*                                       m_GameDefaultConfig;

  explicit CAura(CConfig* CFG, const int argc, const char* argv[]);
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
  void LoadConfigs(CConfig* CFG);
  bool LoadCLI(const int argc, const char* argv[]);
  void LoadBNETs(CConfig* CFG);
  void LoadIRC(CConfig* CFG);
  uint8_t ExtractScripts();
  void LoadIPToCountryData();
  void CacheMapPresets();
  bool CreateGame(CMap* map, uint8_t gameState, std::string gameName, std::string ownerName, std::string ownerServer, std::string creatorName, CBNET* nCreatorServer, bool whisper);
  void CreateMirror(CMap* map, uint8_t gameDisplay, std::string gameName, std::string gameAddress, uint16_t gamePort, uint32_t gameHostCounter, uint32_t gameEntryKey, std::string excludedServer, std::string creatorName, CBNET* creatorServer, bool whisper);
  void SendBroadcast(uint16_t port, const std::vector<uint8_t>& message);
  std::pair<uint8_t, std::string> LoadMap(const std::string& user, const std::string& mapInput, const std::string& observersInput, const std::string& visibilityInput, const std::string& randomHeroInput, const bool& gonnaBeLucky);
  std::vector<std::string> MapFilesMatch(std::string pattern);
  std::vector<std::string> ConfigFilesMatch(std::string pattern);

  uint16_t NextHostPort();
  uint32_t NextHostCounter();

  bool IsIgnoredNotifyPlayer(std::string playerName);
  bool IsIgnoredDatagramSource(std::string sourceIp);
  inline bool GetReady() const { return m_Ready; }

  inline std::string GetSudoAuthCommand(const std::string& Payload, char CommandTrigger) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    // Generate random hex digits
    std::string result;
    result.reserve(27 + Payload.length());
    result += std::string(1, CommandTrigger) + "sudo ";

    for (std::size_t i = 0; i < 20; ++i) {
        const int randomDigit = dis(gen);
        result += (randomDigit < 10) ? (char)('0' + randomDigit) : (char)('a' + (randomDigit - 10));
    }

    result += " " + Payload;
    m_SudoAuthCommand = result;
    return result;
  }
};

#endif // AURA_AURA_H_
