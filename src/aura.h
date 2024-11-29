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

#include "includes.h"
#include "config/config.h"
#include "config/config_bot.h"
#include "config/config_realm.h"
#include "config/config_game.h"
#include "cli.h"
#include "irc.h"
#include "discord.h"
#include "command.h"
#include "net.h"
#include "game_setup.h"

#include <random>
#include <filesystem>

#ifdef _WIN32
#pragma once
#include <windows.h>
#endif

#define AURA_VERSION "3.0.0.dev"
#define AURA_APP_NAME "Aura 3.0.0.dev"
#define AURA_REPOSITORY_URL "https://gitlab.com/ivojulca/aura-bot"
#define AURA_ISSUES_URL "https://gitlab.com/ivojulca/aura-bot/-/issues"


//
// CAura
//

class CAura
{
public:
  uint8_t                                            m_LogLevel;
  CSHA1*                                             m_SHA;                        // for calculating SHA1's
  std::vector<CRealm*>                               m_Realms;                     // all our battle.net clients (there can be more than one)
  CDiscord*                                          m_Discord;                    // Discord client
  CIRC*                                              m_IRC;                        // IRC client
  CNet*                                              m_Net;                        // network manager

  std::filesystem::path                              m_ConfigPath;
  CBotConfig*                                        m_Config;
  CRealmConfig*                                      m_RealmDefaultConfig;
  CGameConfig*                                       m_GameDefaultConfig;
  CCommandConfig*                                    m_CommandDefaultConfig;

  CAuraDB*                                           m_DB;                         // database
  CGameSetup*                                        m_GameSetup;                  // the currently loaded map
  CGameSetup*                                        m_AutoRehostGameSetup;        // game setup to be rehosted whenever free
  std::string                                        m_Version;                    // Aura version string
  std::string                                        m_RepositoryURL;              // Aura repository URL
  std::string                                        m_IssuesURL;                  // Aura issues URL

  uint32_t                                           m_LastServerID;
  uint32_t                                           m_HostCounter;                // the current host counter (a unique number to identify a game, incremented each time a game is created)
  uint32_t                                           m_ReplacingLobbiesCounter;
  uint64_t                                           m_HistoryGameID;
  uint8_t                                            m_GameVersion;
  uint8_t                                            m_MaxSlots;
  size_t                                             m_MaxGameNameSize;

  bool                                               m_ScriptsExtracted;           // indicates if there's lacking configuration info so we can quit
  bool                                               m_Exiting;                    // set to true to force aura to shutdown next update (used by SignalCatcher)
  bool                                               m_ExitingSoon;                // set to true to let aura gracefully stop all services and network traffic, and shutdown once done
  bool                                               m_Ready;                      // indicates if there's lacking configuration info so we can quit
  bool                                               m_AutoReHosted;               // whether our autorehost game setup has been used for one of the active lobbies

  CCommandContext*                                   m_ReloadContext;

  CCommandContext*                                   m_SudoContext;
  std::string                                        m_SudoAuthPayload;
  std::string                                        m_SudoExecCommand;
  std::queue<std::vector<std::string>>               m_PendingActions;

  std::optional<int64_t>                             m_LastGameHostedTicks;
  std::optional<int64_t>                             m_LastGameAutoHostedTicks;
  std::vector<CGame*>                                m_StartedGames;               // all games after they have started
  std::vector<CGame*>                                m_Lobbies;                    // all games before they are started
  std::vector<CGame*>                                m_LobbiesPending;             // vector for just-created lobbies before they get into m_Lobbies
  std::vector<CGame*>                                m_JoinInProgressGames;        // started games that can be joined in-progress (either as observer or player)
  std::map<std::string, std::string>                 m_CachedMaps;
  std::unordered_multiset<std::string>               m_BusyMaps;
  std::map<std::string, std::string>                 m_LastMapSuggestions;
  std::set<CCommandContext*>                         m_ActiveContexts;

  std::filesystem::path                              m_GameInstallPath;

  std::vector<std::string>                           m_RealmsIdentifiers;
  std::map<uint8_t, CRealm*>                         m_RealmsByHostCounter;
  std::map<std::string, CRealm*>                     m_RealmsByInputID;

  explicit CAura(CConfig& CFG, const CCLI& nCLI);
  ~CAura();
  CAura(CAura&) = delete;

  CGame* GetMostRecentLobby(bool allowPending = false) const;
  CGame* GetMostRecentLobbyFromCreator(const std::string& fromName) const;
  CGame* GetLobbyByHostCounter(uint32_t hostCounter) const;
  CGame* GetGameByIdentifier(const uint64_t gameIdentifier) const;

  CRealm* GetRealmByInputId(const std::string& inputId) const;
  CRealm* GetRealmByHostCounter(const uint8_t hostCounter) const;
  CRealm* GetRealmByHostName(const std::string& hostName) const;

  void MergePendingLobbies();
  void TrackGameJoinInProgress(CGame* game);
  void UntrackGameJoinInProgress(CGame* game);

  bool QueueConfigReload(CCommandContext* nCtx);
  void HoldContext(CCommandContext* nCtx);
  void UnholdContext(CCommandContext* nCtx);

  // identifier generators

  uint32_t NextHostCounter();
  uint64_t NextHistoryGameID();
  uint32_t NextServerID();

  std::string GetSudoAuthPayload(const std::string& payload);

  // processing functions

  bool HandleAction(std::vector<std::string> action);
  bool Update();
  inline bool GetReady() const { return m_Ready; }

  bool GetNewGameIsInQuota() const;
  bool GetNewGameIsInQuotaReplace() const;
  bool GetNewGameIsInQuotaConservative() const;
  bool GetNewGameIsInQuotaAutoReHost() const;
  bool CreateGame(CGameSetup* gameSetup);
  inline bool GetIsAutoHostThrottled() { return m_LastGameAutoHostedTicks.has_value() && m_LastGameAutoHostedTicks.value() + static_cast<int64_t>(AUTO_REHOST_COOLDOWN_TICKS) >= GetTicks(); }

  inline bool GetIsAdvertisingGames() { return !m_Lobbies.empty() || !m_JoinInProgressGames.empty(); }
  inline bool GetHasGames() { return !m_StartedGames.empty() || !m_Lobbies.empty(); }

  // events

  void EventBNETGameRefreshSuccess(CRealm* realm);
  void EventBNETGameRefreshError(CRealm* realm);
  void EventGameDeleted(CGame* game);
  void EventGameRemake(CGame* game);
  void EventGameStarted(CGame* game);

  // other functions

  bool ReloadConfigs();
  bool LoadConfigs(CConfig& CFG);
  void OnLoadConfigs();
  bool LoadBNETs(CConfig& CFG, std::bitset<120>& definedConfigs);

  uint8_t ExtractScripts();
  bool CopyScripts();
  bool GetAutoReHostMapHasRefs() const;
  void ClearAutoRehost();

  void LoadMapAliases();
  void LoadIPToCountryData(const CConfig& CFG);
  void InitContextMenu();
  void InitPathVariable();
  void InitSystem();
  void UpdateWindowTitle();
  void UpdateMetaData();

  void CacheMapPresets();
  
  inline bool MatchLogLevel(const uint8_t logLevel) { return logLevel <= m_LogLevel; } // 1: emergency ... 9: trace
  void LogPersistent(const std::string& logText);
  void GracefulExit();
  bool CheckGracefulExit();
};

#endif // AURA_AURA_H_
