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

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT

 */

#ifndef AURA_AURA_H_
#define AURA_AURA_H_

#include "config.h"
#include "config_bot.h"
#include "config_realm.h"
#include "config_game.h"
#include "command.h"
#include "net.h"

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

class CTCPServer;
class CGameProtocol;
class CGPSProtocol;
class CCRC32;
class CSHA1;
class CRealm;
class CGame;
class CCommandContext;
class CAuraDB;
class CNet;
class CMap;
class CIRC;

struct UDPPkt;

class CAura
{
public:
  CGameProtocol*                                     m_GameProtocol;               // class for game protocol
  CGPSProtocol*                                      m_GPSProtocol;                // class for gproxy protocol
  CCRC32*                                            m_CRC;                        // for calculating CRC's
  CSHA1*                                             m_SHA;                        // for calculating SHA1's
  std::vector<CRealm*>                               m_Realms;                     // all our battle.net clients (there can be more than one)
  CIRC*                                              m_IRC;                        // IRC client
  CNet*                                              m_Net;                        // network manager
  CGame*                                             m_CurrentLobby;               // this is the hosted lobby if any
  std::vector<CGame*>                                m_Games;                      // these games are in progress
  CAuraDB*                                           m_DB;                         // database
  CMap*                                              m_Map;                        // the currently loaded map
  std::string                                        m_Version;                    // Aura version string
  std::string                                        m_RepositoryURL;              // Aura repository URL

  uint8_t                                            m_MaxSlots;
  uint32_t                                           m_HostCounter;                // the current host counter (a unique number to identify a game, incremented each time a game is created)
  uint16_t                                           m_LastHostPort;               // the port of the last hosted game
  size_t                                             m_MaxGameNameSize;
  bool                                               m_Exiting;                    // set to true to force aura to shutdown next update (used by SignalCatcher)
  bool                                               m_Ready;                      // indicates if there's lacking configuration info so we can quit
  bool                                               m_ScriptsExtracted;           // indicates if there's lacking configuration info so we can quit

  std::map<std::string, std::string>                 m_CachedMaps;
  std::unordered_multiset<std::string>               m_BusyMaps;

  std::string                                        m_SudoUser;
  CGame*                                             m_SudoGame;
  CIRC*                                              m_SudoIRC;
  CRealm*                                            m_SudoRealm;
  std::string                                        m_SudoAuthPayload;
  std::string                                        m_SudoExecCommand;
  std::vector<std::vector<std::string>>              m_PendingActions;

  uint8_t                                            m_GameVersion;
  std::filesystem::path                              m_GameInstallPath;

  CBotConfig*                                        m_Config;
  CRealmConfig*                                      m_RealmDefaultConfig;
  CGameConfig*                                       m_GameDefaultConfig;

  explicit CAura(CConfig* CFG, const int argc, const char* argv[]);
  ~CAura();
  CAura(CAura&) = delete;

  // processing functions

  void HandleHealthCheck();
  bool HandleAction(std::vector<std::string>& action);
  void HandleUDP(UDPPkt* pkt);
  bool Update();
  inline bool GetReady() const { return m_Ready; }

  CRealm* GetRealmByInputId(const std::string& inputId);
  CRealm* GetRealmByHostName(const std::string& hostName);
  CRealm* GetRealmByHostCounter(const uint8_t hostCounter);
  CTCPServer* GetGameServer(uint16_t, std::string& name);

  // events

  void EventBNETGameRefreshFailed(CRealm* bnet);
  void EventGameDeleted(CGame* game);

  // other functions

  bool ReloadConfigs();
  bool LoadConfigs(CConfig* CFG);
  bool LoadCLI(const int argc, const char* argv[]);
  bool LoadBNETs(CConfig* CFG);
  bool LoadIRC(CConfig* CFG);

  uint8_t ExtractScripts();
  bool CopyScripts();

  void LoadIPToCountryData();

  void CacheMapPresets();
  bool CreateGame(CMap* map, uint8_t gameState, std::string gameName, std::string ownerName, std::string ownerServer, std::string creatorName, CRealm* nCreatorServer, CCommandContext* ctx);
  bool CreateMirror(CMap* map, uint8_t gameDisplay, std::string gameName, std::vector<uint8_t> gameAddress, uint16_t gamePort, uint32_t gameHostCounter, uint32_t gameEntryKey, std::string excludedServer, std::string creatorName, CRealm* creatorServer, CCommandContext* ctx);
  std::pair<uint8_t, std::string> LoadMap(const std::string& user, const std::string& mapInput, const std::string& observersInput, const std::string& visibilityInput, const std::string& randomHeroInput, const bool& gonnaBeLucky, const bool& allowArbitraryMapPath);
  std::pair<uint8_t, std::string> LoadMapConfig(const std::string& user, const std::string& cfgInput, const std::string& observersInput, const std::string& visibilityInput, const std::string& randomHeroInput, const bool& allowArbitraryMapPath);
  std::vector<std::string> MapFilesMatch(std::string pattern);
  std::vector<std::string> ConfigFilesMatch(std::string pattern);

  uint16_t NextHostPort();
  uint32_t NextHostCounter();

  bool IsIgnoredNotifyPlayer(std::string playerName);
  bool IsIgnoredDatagramSource(std::string sourceIp);

  inline std::string GetSudoAuthPayload(const std::string& Payload) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    // Generate random hex digits
    std::string result;
    result.reserve(21 + Payload.length());

    for (std::size_t i = 0; i < 20; ++i) {
        const int randomDigit = dis(gen);
        result += (randomDigit < 10) ? (char)('0' + randomDigit) : (char)('a' + (randomDigit - 10));
    }

    result += " " + Payload;
    m_SudoAuthPayload = result;
    return result;
  }
};

#endif // AURA_AURA_H_
