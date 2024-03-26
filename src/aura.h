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
#include "cli.h"
#include "command.h"
#include "net.h"
#include "gamesetup.h"

#include <cstdint>
#include <vector>
#include <queue>
#include <set>
#include <string>
#include <map>
#include <algorithm>
#include <random>
#include <unordered_set>
#include <filesystem>

#define NOMINMAX
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#pragma once
#include <windows.h>
#endif

#define AURA_VERSION "2.0.0.dev"
#define AURA_APP_NAME "Aura 2.0.0.dev"
#define AURA_REPOSITORY_URL "https://gitlab.com/ivojulca/aura-bot"
#define AURA_ISSUES_URL "https://gitlab.com/ivojulca/aura-bot/-/issues"


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
class CGameSetup;
class CIRC;
class CCLI;

class CAura
{
public:
  uint8_t                                            m_LogLevel;
  CGameProtocol*                                     m_GameProtocol;               // class for game protocol
  CGPSProtocol*                                      m_GPSProtocol;                // class for gproxy protocol
  CCRC32*                                            m_CRC;                        // for calculating CRC's
  CSHA1*                                             m_SHA;                        // for calculating SHA1's
  std::vector<CRealm*>                               m_Realms;                     // all our battle.net clients (there can be more than one)
  CIRC*                                              m_IRC;                        // IRC client
  CNet*                                              m_Net;                        // network manager
  CGame*                                             m_CurrentLobby;               // this is the hosted lobby if any

  std::filesystem::path                              m_ConfigPath;
  CBotConfig*                                        m_Config;
  CRealmConfig*                                      m_RealmDefaultConfig;
  CGameConfig*                                       m_GameDefaultConfig;

  CAuraDB*                                           m_DB;                         // database
  CGameSetup*                                        m_GameSetup;                  // the currently loaded map
  std::string                                        m_Version;                    // Aura version string
  std::string                                        m_RepositoryURL;              // Aura repository URL
  std::string                                        m_IssuesURL;                  // Aura issues URL

  uint8_t                                            m_MaxSlots;
  uint32_t                                           m_HostCounter;                // the current host counter (a unique number to identify a game, incremented each time a game is created)
  uint32_t                                           m_LastServerID;
  size_t                                             m_MaxGameNameSize;

  bool                                               m_ScriptsExtracted;           // indicates if there's lacking configuration info so we can quit
  uint8_t                                            m_GameVersion;
  bool                                               m_Exiting;                    // set to true to force aura to shutdown next update (used by SignalCatcher)
  bool                                               m_Ready;                      // indicates if there's lacking configuration info so we can quit

  CCommandContext*                                   m_SudoContext;
  std::string                                        m_SudoAuthPayload;
  std::string                                        m_SudoExecCommand;
  std::queue<std::vector<std::string>>               m_PendingActions;

  std::vector<CGame*>                                m_Games;                      // these games are in progress
  std::map<std::string, std::string>                 m_CachedMaps;
  std::unordered_multiset<std::string>               m_BusyMaps;
  std::set<CCommandContext*>                         m_ActiveContexts;

  std::filesystem::path                              m_GameInstallPath;

  std::vector<std::string>                           m_RealmsIdentifiers;
  std::map<uint8_t, CRealm*>                         m_RealmsByHostCounter;
  std::map<std::string, CRealm*>                     m_RealmsByInputID;

  explicit CAura(CConfig& CFG, const CCLI& nCLI);
  ~CAura();
  CAura(CAura&) = delete;

  CRealm* GetRealmByInputId(const std::string& inputId) const;
  CRealm* GetRealmByHostCounter(const uint8_t hostCounter) const;
  CRealm* GetRealmByHostName(const std::string& hostName) const;
  CTCPServer* GetGameServer(uint16_t, std::string& name);

  void HoldContext(CCommandContext* nCtx);
  void UnholdContext(CCommandContext* nCtx);

  // identifier generators

  uint32_t NextHostCounter();
  uint32_t NextServerID();

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

  // processing functions

  bool HandleAction(std::vector<std::string> action);
  bool Update();
  inline bool GetReady() const { return m_Ready; }
  bool CreateGame(CGameSetup* gameSetup);

  // events

  void EventBNETGameRefreshFailed(CRealm* bnet);
  void EventGameDeleted(CGame* game);

  // other functions

  bool ReloadConfigs();
  bool LoadConfigs(CConfig& CFG);
  void OnLoadConfigs();
  bool LoadBNETs(CConfig& CFG, std::bitset<240>& definedConfigs);

  uint8_t ExtractScripts();
  bool CopyScripts();

  void LoadIPToCountryData(const CConfig& CFG);

  void CacheMapPresets();
  
  bool MatchLogLevel(const uint8_t);
};

#endif // AURA_AURA_H_
