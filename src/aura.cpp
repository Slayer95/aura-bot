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

#include "aura.h"
#include "bncsutilinterface.h"
#include "crc32.h"
#include "sha1.h"
#include "csvparser.h"
#include "config.h"
#include "config_bot.h"
#include "config_realm.h"
#include "config_game.h"
#include "config_irc.h"
#include "socket.h"
#include "auradb.h"
#include "realm.h"
#include "map.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "game.h"
#include "cli.h"
#include "irc.h"
#include "util.h"
#include "fileutil.h"
#include "osutil.h"

#include <csignal>
#include <cstdlib>
#include <thread>
#include <fstream>
#include <algorithm>
#include <string>
#include <bitset>
#include <iterator>
#include <exception>
#include <system_error>
#include <locale>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <ws2tcpip.h>
#include <winsock.h>
#include <process.h>
#endif

using namespace std;

#undef FD_SETSIZE
#define FD_SETSIZE 512

static CAura* gAura    = nullptr;
bool          gRestart = false;

#ifdef _WIN32
static wchar_t* auraHome = nullptr;
static wchar_t* war3Home = nullptr;
#endif

inline void GetAuraHome(const CCLI& cliApp, filesystem::path& homeDir)
{
  if (cliApp.m_HomePath.has_value()) {
    homeDir = cliApp.m_HomePath.value();
    return;
  }
#ifdef _WIN32
  size_t valueSize;
  errno_t err = _wdupenv_s(&auraHome, &valueSize, L"AURA_HOME");
  if (!err && auraHome != nullptr) {
    wstring homeDirString = auraHome;
#else
  const char* envValue = getenv("AURA_HOME");
  if (envValue != nullptr) {
    string homeDirString = envValue;
#endif
    homeDir = filesystem::path(homeDirString);
    NormalizeDirectory(homeDir);
    return;
  }
  if (cliApp.m_CFGPath.has_value()) {
    homeDir = cliApp.m_CFGPath.value().parent_path();
    NormalizeDirectory(homeDir);
    return;
  }

  homeDir = GetExeDirectory();
}

inline filesystem::path GetConfigPath(const CCLI& cliApp, const filesystem::path& homeDir)
{
  if (!cliApp.m_CFGPath.has_value()) return homeDir / "config.ini";
  if (!cliApp.m_UseStandardPaths && (cliApp.m_CFGPath.value() == cliApp.m_CFGPath.value().filename())) {
    return homeDir / cliApp.m_CFGPath.value();
  } else {
    return cliApp.m_CFGPath.value();
  }
}

inline bool LoadConfig(CConfig& CFG, CCLI& cliApp, const filesystem::path& homeDir)
{
  const filesystem::path configPath = GetConfigPath(cliApp, homeDir);
  const bool isCustomConfigFile = cliApp.m_CFGPath.has_value();
  const bool isDirectSuccess = CFG.Read(configPath);
  if (!isDirectSuccess && isCustomConfigFile) {
    filesystem::path cwd;
    try {
      cwd = filesystem::current_path();
    } catch (...) {}
    NormalizeDirectory(cwd);
    Print("[AURA] required config file not found [" + PathToString(configPath) + "]");
    if (!cliApp.m_UseStandardPaths && configPath.parent_path() == homeDir.parent_path() && (cwd.empty() || homeDir.parent_path() != cwd.parent_path())) {
      Print("[HINT] --config was resolved relative to [" + PathToString(homeDir) + "]");
      Print("[HINT] use --stdpaths to read [" + PathToString(cwd / configPath.filename()) + "]");
    }
#ifdef _WIN32
    Print("[HINT] using --config=<FILE> is not recommended, prefer --homedir=<DIR>, or setting %AURA_HOME% instead");
#else
    Print("[HINT] using --config=<FILE> is not recommended, prefer --homedir=<DIR>, or setting $AURA_HOME instead");
#endif
    Print("[HINT] both alternatives auto-initialize \"config.ini\" from \"config-example.ini\" in the same folder");
    return false;
  }
  // TODO: Make sure <bot.home_path.allow_mismatch> is documented in CONFIG.md.
  const bool homePathMatchRequired = CFG.GetBool("bot.home_path.allow_mismatch", false);
  if (isCustomConfigFile) {
    bool pathsMatch = configPath.parent_path() == homeDir.parent_path();
    if (!pathsMatch) {
      try {
        pathsMatch = filesystem::absolute(configPath.parent_path()) == filesystem::absolute(homeDir.parent_path());
      } catch (...) {}
    }
    if (homePathMatchRequired && !pathsMatch) {
      Print("[AURA] error - config file is not located within home dir [" + PathToString(homeDir) + "] - this is not recommended");
      Print("[HINT] to skip this check and execute Aura nevertheless, set <bot.home_path.allow_mismatch = yes> in your config file");
      Print("[HINT] paths in your config file [" + PathToString(configPath) + "] will be resolved relative to the home dir");
      return false;
    } else if (cliApp.m_HomePath.has_value()) {
      Print("[AURA] using --homedir=" + PathToString(homeDir));
    } else {
#ifdef _WIN32
      Print("[AURA] using %AURA_HOME%=" + PathToString(homeDir));
#else
      Print("[AURA] using $AURA_HOME=" + PathToString(homeDir));
#endif
    }
  }

  if (isDirectSuccess) {
    CFG.SetHomeDir(move(homeDir));
    return true;
  }

  const filesystem::path configExamplePath = homeDir / filesystem::path("config-example.ini");

  size_t FileSize = 0;
  const string exampleContents = FileRead(configExamplePath, &FileSize);
  if (exampleContents.empty()) {
    // But Aura can actually work without a config file ;)
    Print("[AURA] config.ini, config-example.ini not found within home dir [" + PathToString(homeDir) + "].");
    Print("[AURA] using automatic configuration");
  } else {
    Print("[AURA] copying config-example.cfg to config.ini...");
    FileWrite(configPath, reinterpret_cast<const uint8_t*>(exampleContents.c_str()), exampleContents.size());
    if (!CFG.Read(configPath)) {
      Print("[AURA] error initializing config.ini");
      return false;
    }
  }

  CFG.SetHomeDir(move(homeDir));
  return true;
}

inline CAura* StartAura(CConfig& CFG, const CCLI& cliApp)
{
  return new CAura(CFG, cliApp);
}

//
// main
//

int main(const int argc, char** argv)
{
  int exitCode = 0;

  // seed the PRNG
  srand(static_cast<uint32_t>(time(nullptr)));

  // disable sync since we don't use cstdio anyway
  ios_base::sync_with_stdio(false);

#ifdef _WIN32
  // print UTF-8 to the console
  SetConsoleOutputCP(CP_UTF8);
#endif

  signal(SIGINT, [](int32_t) -> void {
    Print("[!!!] caught signal SIGINT, exiting NOW");

    if (gAura) {
      if (gAura->m_Exiting)
        exit(1);
      else
        gAura->m_Exiting = true;
    } else {
      exit(1);
    }
  });

#ifndef _WIN32
  // disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

  signal(SIGPIPE, SIG_IGN);
#endif

#ifdef _WIN32
  // initialize winsock

  WSADATA wsadata;

  if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
  {
    Print("[AURA] error starting winsock");
    return 1;
  }

  // increase process priority
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

  if (htons(0xe017) == 0xe017) {
    Print("[AURA] warning - big endian system support is experimental");
  }

  // initialize aura

  {
    // extra scope, so that cliApp can be deallocated
    CCLI cliApp;
    uint8_t cliResult = cliApp.Parse(argc, argv);
    if (cliResult == CLI_EARLY_RETURN) {
      cliApp.RunEarlyOptions();
      exitCode = 0;
    } else if (cliResult != CLI_OK) {
      Print("[AURA] invalid CLI usage - please see CLI.md");
      exitCode = 1;
    } else {
      CConfig CFG;
      filesystem::path homeDir;
      GetAuraHome(cliApp, homeDir);
      if (LoadConfig(CFG, cliApp, homeDir)) {
        gAura = StartAura(CFG, cliApp);
        if (!gAura || !gAura->GetReady()) {
          exitCode = 1;
          Print("[AURA] initialization failure");
        }
      } else {
        Print("[AURA] error loading configuration");
        exitCode = 1;
      }
    }
  }

  if (gAura && gAura->GetReady()) {
    // loop start

    while (!gAura->Update())
      ;

    // loop end - shut down
    Print("[AURA] shutting down");
    delete gAura;
  }


#ifdef _WIN32
  // shutdown winsock

  WSACleanup();
#endif

#ifdef _WIN32
  free(auraHome);
  free(war3Home);
#endif

  // restart the program

  if (gRestart)
  {
#ifdef _WIN32
    _spawnl(_P_OVERLAY, argv[0], argv[0], nullptr);
#else
    execl(argv[0], argv[0], nullptr);
#endif
  }

  return exitCode;
}

//
// CAura
//

CAura::CAura(CConfig& CFG, const CCLI& nCLI)
  : m_LogLevel(LOG_LEVEL_DEBUG),
    m_GameProtocol(nullptr),
    m_GPSProtocol(nullptr),
    m_CRC(new CCRC32()),
    m_SHA(new CSHA1()),
    m_Discord(nullptr),
    m_IRC(nullptr),
    m_Net(nullptr),
    m_CurrentLobby(nullptr),
    m_CanReplaceLobby(false),

    m_ConfigPath(CFG.GetFile()),
    m_Config(nullptr),
    m_RealmDefaultConfig(nullptr),
    m_GameDefaultConfig(nullptr),
    m_CommandDefaultConfig(new CCommandConfig()),

    m_DB(new CAuraDB(CFG)),
    m_GameSetup(nullptr),
    m_AutoRehostGameSetup(nullptr),
    m_Version(AURA_VERSION),
    m_RepositoryURL(AURA_REPOSITORY_URL),
    m_IssuesURL(AURA_ISSUES_URL),
    m_MaxSlots(MAX_SLOTS_LEGACY),
    m_HostCounter(0),
    m_LastServerID(0xF),
    m_MaxGameNameSize(31),

    m_ScriptsExtracted(false),
    m_GameVersion(0),
    m_Exiting(false),
    m_Ready(true),

    m_SudoContext(nullptr)
{
  Print("[AURA] Aura version " + m_Version);

  if (m_DB->HasError()) {
    Print("[CONFIG] Error: Critical errors found in " + PathToString(m_DB->GetFile()) + ".");
    m_Ready = false;
    return;
  }

  m_GameProtocol = new CGameProtocol(this);
  m_GPSProtocol = new CGPSProtocol(this);
  m_Net = new CNet(this);
  m_Discord = new CDiscord(this);
  m_IRC = new CIRC(this);

  m_CRC->Initialize();

  if (!LoadConfigs(CFG)) {
    Print("[CONFIG] Error: Critical errors found in " + PathToString(m_ConfigPath.filename()));
    m_Ready = false;
    return;
  }
  nCLI.OverrideConfig(this);
  OnLoadConfigs();
  if (m_GameVersion == 0) {
    Print("[CONFIG] Game version and path are missing.");
    m_Ready = false;
    return;
  }
  Print("[AURA] running game version 1." + to_string(m_GameVersion));

  if (!m_Net->Init()) {
    Print("[AURA] error - close active instances of Warcraft, and/or pause LANViewer to initialize Aura.");
    m_Ready = false;
    return;
  }

  if (m_Net->m_Config->m_UDPEnableCustomPortTCP4) {
    Print("[AURA] broadcasting games port " + to_string(m_Net->m_Config->m_UDPCustomPortTCP4) + " over LAN");
  }

  m_RealmsIdentifiers.resize(16);
  if (m_Config->m_EnableBNET.has_value()) {
    if (m_Config->m_EnableBNET.value()) {
      Print("[AURA] all realms forcibly set to ENABLED <bot.toggle_every_realm = on>");
    } else {
      Print("[AURA] all realms forcibly set to DISABLED <bot.toggle_every_realm = off>");
    }
  }
  bitset<240> definedRealms;
  if (m_Config->m_EnableBNET.value_or(true)) {
    LoadBNETs(CFG, definedRealms);
  }

  try {
    filesystem::create_directory(m_Config->m_MapPath);
  } catch (...) {
    Print("[AURA] warning - <bot.maps_path> is not a valid directory");
  }

  try {
    filesystem::create_directory(m_Config->m_MapCFGPath);
  } catch (...) {
    Print("[AURA] warning - <bot.map_configs_path> is not a valid directory");
  }

  try {
    filesystem::create_directory(m_Config->m_MapCachePath);
  } catch (...) {
    Print("[AURA] warning - <bot.map_cache_path> is not a valid directory");
  }

  try {
    filesystem::create_directory(m_Config->m_JASSPath);
  } catch (...) {
    Print("[AURA] warning - <bot.jass_path> is not a valid directory");
  }

  if (m_Config->m_ExtractJASS) {
    // extract common.j and blizzard.j from War3Patch.mpq or War3.mpq (depending on version) if we can
    // these two files are necessary for calculating "map_crc" when loading maps so we make sure they are available
    // see CMap :: Load for more information
    m_ScriptsExtracted = ExtractScripts() == 2;
    if (!m_ScriptsExtracted) {
      if (!CopyScripts()) {
        m_Ready = false;
        return;
      }
    }
  }

  if (m_DB->GetIsFirstRun()) {
    LoadMapAliases();
    LoadIPToCountryData(CFG);
    if (nCLI.GetInitSystem().value_or(true)) {
      InitSystem();
    }
  } else if (nCLI.GetInitSystem().value_or(false)) {
    InitSystem();
  }

  if (m_Config->m_EnableCFGCache) {
    CacheMapPresets();
  }

  if (!nCLI.QueueActions(this)) {
    m_Ready = false;
    return;
  }

  vector<string> invalidKeys = CFG.GetInvalidKeys(definedRealms);
  if (!invalidKeys.empty()) {
    Print("[CONFIG] warning - some keys are misnamed: " + JoinVector(invalidKeys, false));
  }

  if (m_Realms.empty() && m_Config->m_EnableBNET.value_or(true))
    Print("[AURA] notice - no enabled battle.net connections configured");
  if (!m_IRC->m_Config->m_Enabled)
    Print("[AURA] notice - no irc connection configured");
  if (!m_Discord->m_Config->m_Enabled)
    Print("[AURA] notice - no discord connection configured");

  if (m_Realms.empty() && !m_IRC->m_Config->m_Enabled && !m_Discord->m_Config->m_Enabled && m_PendingActions.empty()) {
    Print("[AURA] error - no inputs connected");
    m_Ready = false;
    return;
  }
}

bool CAura::LoadBNETs(CConfig& CFG, bitset<240>& definedRealms)
{
  // load the battle.net connections
  // we're just loading the config data and creating the CRealm classes here, the connections are established later (in the Update function)

  bool isInvalidConfig = false;
  map<string, uint8_t> uniqueInputIds;
  map<string, uint8_t> uniqueNames;
  vector<CRealmConfig*> realmConfigs(240, nullptr);
  const bool hasGlobalHostName = CFG.Exists("realm_global.host_name");
  for (uint8_t i = 1; i <= 240; ++i) {
    if (!hasGlobalHostName && !CFG.Exists("realm_" + to_string(i) + ".host_name")) {
      continue;
    }
    CRealmConfig* ThisConfig = new CRealmConfig(CFG, m_RealmDefaultConfig, i);
    if (m_Config->m_EnableBNET.has_value()) {
      ThisConfig->m_Enabled = m_Config->m_EnableBNET.value();
    }
    if (ThisConfig->m_UserName.empty() || ThisConfig->m_PassWord.empty()) {
      ThisConfig->m_Enabled = false;
    }
    if (!ThisConfig->m_Enabled) {
      delete ThisConfig;
    } else if (uniqueNames.find(ThisConfig->m_UniqueName) != uniqueNames.end()) {
      Print("[CONFIG] <realm_" + to_string(uniqueNames.at(ThisConfig->m_UniqueName) + 1) + ".unique_name> must be different from <realm_" + to_string(i) + ".unique_name>");
      isInvalidConfig = true;
      delete ThisConfig;
    } else if (uniqueInputIds.find(ThisConfig->m_InputID) != uniqueInputIds.end()) {
      Print("[CONFIG] <realm_" + to_string(uniqueNames.at(ThisConfig->m_UniqueName) + 1) + ".input_id> must be different from <realm_" + to_string(i) + ".input_id>");
      isInvalidConfig = true;
      delete ThisConfig;
    } else {
      uniqueNames[ThisConfig->m_UniqueName] = i - 1;
      uniqueInputIds[ThisConfig->m_InputID] = i - 1;
      realmConfigs[i - 1] = ThisConfig;
      definedRealms.set(i - 1);
    }
  }

  if (isInvalidConfig) {
    for (auto& realmConfig : realmConfigs) {
      delete realmConfig;
    }
    return false;
  }

  m_RealmsByHostCounter.clear();
  uint8_t i = static_cast<uint8_t>(m_Realms.size());
  while (i--) {
    string inputID = m_Realms[i]->GetInputID();
    if (uniqueInputIds.find(inputID) == uniqueInputIds.end()) {
      delete m_Realms[i];
      m_RealmsByInputID.erase(inputID);
      m_Realms.erase(m_Realms.begin() + i);
    }
  }

  size_t longestGamePrefixSize = 0;
  for (const auto& entry : uniqueInputIds) {
    CRealm* matchingRealm = GetRealmByInputId(entry.first);
    CRealmConfig* realmConfig = realmConfigs[entry.second];
    if (matchingRealm == nullptr) {
      matchingRealm = new CRealm(this, realmConfig);
      m_Realms.push_back(matchingRealm);
      m_RealmsByInputID[entry.first] = matchingRealm;
      m_RealmsIdentifiers.push_back(entry.first);
      // m_RealmsIdentifiers[matchingRealm->GetInternalID()] == matchingRealm->GetInputID();
      if (MatchLogLevel(LOG_LEVEL_DEBUG)) {
        Print("[AURA] server found: " + matchingRealm->GetUniqueDisplayName());
      }
    } else {
      const bool DoResetConnection = (
        matchingRealm->GetServer() != realmConfig->m_HostName ||
        matchingRealm->GetServerPort() != realmConfig->m_ServerPort ||
        matchingRealm->GetLoginName() != realmConfig->m_UserName ||
        (matchingRealm->GetEnabled() && !realmConfig->m_Enabled)
      );
      matchingRealm->SetConfig(realmConfig);
      matchingRealm->SetHostCounter(realmConfig->m_ServerIndex + 15);
      if (DoResetConnection) matchingRealm->ResetConnection(false);
      if (MatchLogLevel(LOG_LEVEL_DEBUG)) {
        Print("[AURA] server reloaded: " + matchingRealm->GetUniqueDisplayName());
      }
    }

    if (realmConfig->m_GamePrefix.length() > longestGamePrefixSize)
      longestGamePrefixSize = realmConfig->m_GamePrefix.length();

    m_RealmsByHostCounter[matchingRealm->GetHostCounterID()] = matchingRealm;
  }

  m_MaxGameNameSize = 31 - longestGamePrefixSize;
  return true;
}

bool CAura::CopyScripts()
{
  // Try to use manually extracted files already available in bot.map_configs_path
  filesystem::path autoExtractedCommonPath = m_Config->m_JASSPath / filesystem::path("common-" + to_string(m_GameVersion) + ".j");
  filesystem::path autoExtractedBlizzardPath = m_Config->m_JASSPath / filesystem::path("blizzard-" + to_string(m_GameVersion) + ".j");
  bool commonExists = FileExists(autoExtractedCommonPath);
  bool blizzardExists = FileExists(autoExtractedBlizzardPath);
  if (commonExists && blizzardExists) {
    return true;
  }

  if (!commonExists) {
    filesystem::path manuallyExtractedCommonPath = m_Config->m_JASSPath / filesystem::path("common.j");
    try {
      filesystem::copy_file(manuallyExtractedCommonPath, autoExtractedCommonPath, filesystem::copy_options::skip_existing);
    } catch (const exception& e) {
      Print("[AURA] " + string(e.what()));
      return false;
    }
  }
  if (!blizzardExists) {
    filesystem::path manuallyExtractedBlizzardPath = m_Config->m_JASSPath / filesystem::path("blizzard.j");
    try {
      filesystem::copy_file(manuallyExtractedBlizzardPath, autoExtractedBlizzardPath, filesystem::copy_options::skip_existing);
    } catch (const exception& e) {
      Print("[AURA] " + string(e.what()));
      return false;
    }
  }
  return true;
}

CAura::~CAura()
{
  UnholdContext(m_SudoContext);
  m_SudoContext = nullptr;

  delete m_Config;
  delete m_RealmDefaultConfig;
  delete m_GameDefaultConfig;
  delete m_CommandDefaultConfig;
  delete m_Net;
  delete m_GameProtocol;
  delete m_GPSProtocol;
  delete m_CRC;
  delete m_SHA;

  if (m_AutoRehostGameSetup) {
    if (m_GameSetup == m_AutoRehostGameSetup) {
      m_GameSetup = nullptr;
    }
    delete m_AutoRehostGameSetup->m_Map;
    m_AutoRehostGameSetup->m_Map = nullptr;
    UnholdContext(m_AutoRehostGameSetup->m_Ctx);
    m_AutoRehostGameSetup->m_Ctx = nullptr;
    delete m_AutoRehostGameSetup;
    m_AutoRehostGameSetup = nullptr;
  }

  if (m_GameSetup) {
    if (m_GameSetup->GetIsDownloading()) {
      // Downloading off-thread. Nullify pointer to CAura.
      m_GameSetup->m_Aura = nullptr;
    } else {
      // Ctrl+C while downloading a map. Prevent crashes.
      delete m_GameSetup;
    }
  }

  for (auto& bnet : m_Realms)
    delete bnet;

  delete m_CurrentLobby;

  for (auto& game : m_Games)
    delete game;

  delete m_DB;
  delete m_IRC;
  delete m_Discord;
}

CRealm* CAura::GetRealmByInputId(const string& inputId) const
{
  auto it = m_RealmsByInputID.find(inputId);
  if (it == m_RealmsByInputID.end()) return nullptr;
  return it->second;
}

CRealm* CAura::GetRealmByHostCounter(const uint8_t hostCounter) const
{
  auto it = m_RealmsByHostCounter.find(hostCounter);
  if (it == m_RealmsByHostCounter.end()) return nullptr;
  return it->second;
}

CRealm* CAura::GetRealmByHostName(const string& hostName) const
{
  for (const auto& realm : m_Realms) {
    if (!realm->GetLoggedIn()) continue;
    if (realm->GetIsMirror()) continue;
    if (realm->GetServer() == hostName) return realm;
  }
  return nullptr;
}

CTCPServer* CAura::GetGameServer(uint16_t inputPort, string& name)
{
  auto it = m_Net->m_GameServers.find(inputPort);
  if (it != m_Net->m_GameServers.end()) {
    Print("[GAME] " + name + " Assigned to port " + to_string(inputPort));
    return it->second;
  }
  CTCPServer* gameServer = new CTCPServer(m_Net->m_SupportTCPOverIPv6 ? AF_INET6 : AF_INET);
  if (!gameServer->Listen(m_Net->m_SupportTCPOverIPv6 ? m_Net->m_Config->m_BindAddress6 : m_Net->m_Config->m_BindAddress4, inputPort, false)) {
    Print("[GAME] " + name + " Error listening on port " + to_string(inputPort));
    return nullptr;
  }
  uint16_t assignedPort = gameServer->GetPort();
  m_Net->m_GameServers[assignedPort] = gameServer;
  vector<CGameConnection*> IncomingConnections;
  m_Net->m_IncomingConnections[assignedPort] = IncomingConnections;

  Print("[GAME] " + name + " Listening on port " + to_string(assignedPort));
  return gameServer;
}

bool CAura::HandleAction(vector<string> action)
{
  if (action[0] == "exec") {
    Print("[AURA] Exec cli unsupported yet");
    return false;
  } else if (action[0] == "host" || action[0] == "rehost") {
    bool success = false;
    if (m_GameSetup && (action[0] != "rehost" || m_GameSetup == m_AutoRehostGameSetup)) {
      success = m_GameSetup->RunHost();
      if (!m_GameSetup->m_LobbyAutoRehosted) {
        UnholdContext(m_GameSetup->m_Ctx);
        m_GameSetup->m_Ctx = nullptr;
        delete m_GameSetup;
      }
      m_GameSetup = nullptr;
    }

    if (!success) {
      // Delete all other pending actions
      return false;
    }
  } else if (action[0] == "lazy") {
    vector<string> lazyAction(action.begin() + 1, action.end());
    m_PendingActions.push(lazyAction);
#ifndef DISABLE_MINIUPNP
  } else if (action[0] == "port-forward") {
    uint16_t externalPort = static_cast<uint16_t>(stoi(action[2]));
    uint16_t internalPort = static_cast<uint16_t>(stoi(action[3]));
    m_Net->RequestUPnP(action[1], externalPort, internalPort, LOG_LEVEL_DEBUG);
#endif
  } else if (!action.empty()) {
    Print("[AURA] Action type " + action[0] + " unsupported");
    return false;
  }
  return true;
}

bool CAura::Update()
{
  // 1. pending actions
  bool skipActions = false;
  while (!m_PendingActions.empty()) {
    if (skipActions || HandleAction(m_PendingActions.front())) {
      m_PendingActions.pop();
    } else {
      skipActions = true;
    }
  }

  if (!m_CurrentLobby && !(m_GameSetup && m_GameSetup->GetIsDownloading())) {
    if (m_AutoRehostGameSetup) {
      m_AutoRehostGameSetup->SetActive();
      vector<string> hostAction{"rehost"};
      m_PendingActions.push(hostAction);
    }
  }

  bool isStandby = (
    !m_CurrentLobby && m_Games.empty() &&
    !m_Net->m_HealthCheckInProgress &&
    !(m_GameSetup && m_GameSetup->GetIsDownloading()) &&
    !m_AutoRehostGameSetup 
  );
  if (isStandby && m_Config->m_ExitOnStandby) {
    return true;
  }

  uint32_t NumFDs = 0;

  // take every socket we own and throw it in one giant select statement so we can block on all sockets

  int32_t nfds = 0;
  fd_set  fd, send_fd;
  FD_ZERO(&fd);
  FD_ZERO(&send_fd);

  // 2. all running game servers

  for (auto& pair : m_Net->m_GameServers) {
    pair.second->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
    ++NumFDs;
  }

  // 3. all unassigned incoming TCP connections

  for (auto& pair : m_Net->m_IncomingConnections) {
    // pair<uint16_t, vector<CGameConnection*>>
    for (auto& connection : pair.second) {
      if (connection->GetSocket()) {
        connection->GetSocket()->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
        ++NumFDs;
      }
    }
  }

  // 4. the current lobby's player sockets

  if (m_CurrentLobby)
    NumFDs += m_CurrentLobby->SetFD(&fd, &send_fd, &nfds);

  // 5. all running games' player sockets

  for (auto& game : m_Games)
    NumFDs += game->SetFD(&fd, &send_fd, &nfds);

  // 6. all battle.net sockets

  for (auto& bnet : m_Realms)
    NumFDs += bnet->SetFD(&fd, &send_fd, &nfds);

  // 7. irc socket
  if (m_IRC)
    NumFDs += m_IRC->SetFD(&fd, &send_fd, &nfds);

  // 8. UDP sockets, outgoing test connections
  NumFDs += m_Net->SetFD(&fd, &send_fd, &nfds);

  if (NumFDs > 40) {
    Print("[AURA] " + to_string(NumFDs) + " file descriptors watched");
  }

  // before we call select we need to determine how long to block for
  // 50 ms is the hard maximum

  int64_t usecBlock = 50000;

  for (auto& game : m_Games)
  {
    if (game->GetNextTimedActionTicks() * 1000 < usecBlock)
      usecBlock = game->GetNextTimedActionTicks() * 1000;
  }

  struct timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = static_cast<long int>(usecBlock);

  struct timeval send_tv;
  send_tv.tv_sec  = 0;
  send_tv.tv_usec = 0;

#ifdef _WIN32
  select(1, &fd, nullptr, nullptr, &tv);
  select(1, nullptr, &send_fd, nullptr, &send_tv);
#else
  select(nfds + 1, &fd, nullptr, nullptr, &tv);
  select(nfds + 1, nullptr, &send_fd, nullptr, &send_tv);
#endif

  if (NumFDs == 0)
  {
    // we don't have any sockets (i.e. we aren't connected to battle.net and irc maybe due to a lost connection and there aren't any games running)
    // select will return immediately and we'll chew up the CPU if we let it loop so just sleep for 200ms to kill some time

    this_thread::sleep_for(chrono::milliseconds(200));
  }

  bool Exit = false;

  // update map downloads
  if (m_GameSetup) {
    if (m_GameSetup->Update()) {
      delete m_GameSetup;
      m_GameSetup = nullptr;
    }
  }

  // if hosting a lobby, accept new connections to its game server

  for (auto& pair : m_Net->m_GameServers) {
    uint16_t localPort = pair.first;
    CStreamIOSocket* socket = pair.second->Accept(static_cast<fd_set*>(&fd));
    if (socket) {
      if (m_Net->m_Config->m_ProxyReconnect > 0) {
        CGameConnection* incomingConnection = new CGameConnection(m_GameProtocol, this, localPort, socket);
        if (MatchLogLevel(LOG_LEVEL_TRACE2)) {
          Print("[AURA] incoming connection from " + incomingConnection->GetIPString());
        }
        m_Net->m_IncomingConnections[localPort].push_back(incomingConnection);
      } else if (!m_CurrentLobby || m_CurrentLobby->GetIsMirror() || localPort != m_CurrentLobby->GetHostPort()) {
        if (MatchLogLevel(LOG_LEVEL_TRACE2)) {
          Print("[AURA] connection to port " + to_string(localPort) + " rejected.");
        }
        delete socket;
      } else {
        CGameConnection* incomingConnection = new CGameConnection(m_GameProtocol, this, localPort, socket);
        if (MatchLogLevel(LOG_LEVEL_TRACE2)) {
          Print("[AURA] incoming connection from " + incomingConnection->GetIPString());
        }
        m_Net->m_IncomingConnections[localPort].push_back(incomingConnection);
      }
    }

    if (pair.second->HasError())
      Exit = true;
  }

  // update unassigned incoming connections

  uint16_t IncomingConnectionsMax = 255;
  uint16_t IncomingConnectionsCount = 0;
  for (auto& pair : m_Net->m_IncomingConnections) {
    for (auto i = begin(pair.second); i != end(pair.second);) {
      // *i is a pointer to a CGameConnection
      uint8_t result = (*i)->Update(&fd, &send_fd);
      if (result == PREPLAYER_CONNECTION_OK) {
        ++i;
        continue;
      }

      if (result == PREPLAYER_CONNECTION_DESTROY) {
        // flush the socket (e.g. in case a rejection message is queued)
        if ((*i)->GetSocket())
          (*i)->GetSocket()->DoSend(static_cast<fd_set*>(&send_fd));

        delete *i;
      }

      i = pair.second.erase(i);
    }
    ++IncomingConnectionsCount;
    if (IncomingConnectionsCount > IncomingConnectionsMax) {
      Print("[AURA] " + to_string(IncomingConnectionsCount) + " connections established at port " + to_string(pair.first));
      break;
    }
  }

  // update current lobby

  if (m_CurrentLobby)
  {
    if (m_CurrentLobby->Update(&fd, &send_fd)) {
      Print("[AURA] deleting current game [" + m_CurrentLobby->GetGameName() + "]");
      if (m_CurrentLobby->GetUDPEnabled())
        m_CurrentLobby->SendGameDiscoveryDecreate();
      delete m_CurrentLobby;
      m_CurrentLobby = nullptr;

      for (auto& bnet : m_Realms) {
        bnet->QueueGameUncreate();
        bnet->SendEnterChat();
      }
    } else if (m_CurrentLobby) {
      m_CurrentLobby->UpdatePost(&send_fd);
    }
  }

  // update running games

  for (auto it = begin(m_Games); it != end(m_Games);) {
    if ((*it)->Update(&fd, &send_fd)) {
      if ((*it)->GetExiting()) {
        Print("[AURA] deleting game [" + (*it)->GetGameName() + "]");
        EventGameDeleted(*it);
        delete *it;
      } else {
        Print("[AURA] remaking game [" + (*it)->GetGameName() + "]");
        EventGameRemake(*it);
      }
      it = m_Games.erase(it);
    } else {
      (*it)->UpdatePost(&send_fd);
      ++it;
    }
  }

  // update battle.net connections

  for (auto& bnet : m_Realms) {
    if (bnet->Update(&fd, &send_fd))
      Exit = true;
  }

  // update irc

  if (m_IRC && m_IRC->Update(&fd, &send_fd))
    Exit = true;

  // update discord
  if (m_Discord && m_Discord->Update())
    Exit = true;

  // update UDP sockets, outgoing test connections
  m_Net->Update(&fd, &send_fd);

  return m_Exiting || Exit;
}

void CAura::EventBNETGameRefreshFailed(CRealm* bnet)
{
  if (m_CurrentLobby)
  {
    // If the game has someone in it, advertise the fail only in the lobby (as it is probably a rehost).
    // Otherwise whisper the game creator that the (re)host failed.

    if (m_CurrentLobby->GetNumHumanPlayers() != 0) {
      m_CurrentLobby->SendAllChat("Unable to create game on server [" + bnet->GetServer() + "]. Try another name");
    } else {
      switch (m_CurrentLobby->GetCreatedFromType()) {
        case GAMESETUP_ORIGIN_REALM:
          reinterpret_cast<CRealm*>(m_CurrentLobby->GetCreatedFrom())->QueueWhisper("Unable to create game on server [" + bnet->GetServer() + "]. Try another name", m_CurrentLobby->GetCreatorName());
          break;
        case GAMESETUP_ORIGIN_IRC:
          reinterpret_cast<CIRC*>(m_CurrentLobby->GetCreatedFrom())->SendUser("Unable to create game on server [" + bnet->GetServer() + "]. Try another name", m_CurrentLobby->GetCreatorName());
          break;
        /*
        // TODO: CAura::EventBNETGameRefreshFailed SendUser()
        case GAMESETUP_ORIGIN_DISCORD:
          reinterpret_cast<CDiscord*>(m_CurrentLobby->GetCreatedFrom())->SendUser("Unable to create game on server [" + bnet->GetServer() + "]. Try another name", m_CurrentLobby->GetCreatorName());
          break;*/
        default:
          break;
      }
    }

    Print("[GAME: " + m_CurrentLobby->GetGameName() + "] Unable to create game on server [" + bnet->GetServer() + "]. Try another name");

    // we take the easy route and simply close the lobby if a refresh fails
    // it's possible at least one refresh succeeded and therefore the game is still joinable on at least one battle.net (plus on the local network) but we don't keep track of that
    // we only close the game if it has no players since we support game rehosting (via !priv and !pub in the lobby)

    if (m_CurrentLobby->GetNumHumanPlayers() == 0)
      m_CurrentLobby->SetExiting(true);

    m_CurrentLobby->SetRefreshError(true);
  }
}

void CAura::EventGameDeleted(CGame* game)
{
  for (auto& realm : m_Realms) {
    if (!realm->GetAnnounceHostToChat()) continue;
    realm->QueueChatChannel("Game ended: " + game->GetDescription());
    if (game->MatchesCreatedFrom(GAMESETUP_ORIGIN_REALM, reinterpret_cast<void*>(this))) {
      realm->QueueWhisper("Game ended: " + game->GetDescription(), game->GetCreatorName());
    }
  }
}

void CAura::EventGameRemake(CGame* game)
{
  for (auto& bnet : m_Realms) {
    bnet->QueueChatChannel("Game remake: " + game->GetDescription());
    if (game->MatchesCreatedFrom(GAMESETUP_ORIGIN_REALM, reinterpret_cast<void*>(this))) {
      bnet->QueueWhisper("Game remake: " + game->GetDescription(), game->GetCreatorName());
    }
  }
}

bool CAura::ReloadConfigs()
{
  bool success = true;
  uint8_t WasVersion = m_GameVersion;
  bool WasCacheEnabled = m_Config->m_EnableCFGCache;
  filesystem::path WasMapPath = m_Config->m_MapPath;
  filesystem::path WasCFGPath = m_Config->m_MapCFGPath;
  filesystem::path WasCachePath = m_Config->m_MapCachePath;
  filesystem::path WasJASSPath = m_Config->m_JASSPath;
  CConfig CFG;
  CFG.Read(m_ConfigPath);
  if (!LoadConfigs(CFG)) {
    Print("[CONFIG] error - bot configuration invalid: not reloaded");
    success = false;
  }
  OnLoadConfigs();
  bitset<240> definedRealms;
  if (!LoadBNETs(CFG, definedRealms)) {
    Print("[CONFIG] error - realms misconfigured: not reloaded");
    success = false;
  }
  vector<string> invalidKeys = CFG.GetInvalidKeys(definedRealms);
  if (!invalidKeys.empty()) {
    Print("[CONFIG] warning - the following keys are invalid/misnamed: " + JoinVector(invalidKeys, false));
  }

  if (m_GameVersion != WasVersion) {
    Print("[AURA] Running game version 1." + to_string(m_GameVersion));
  }

  if (m_Config->m_ExtractJASS) {
    if (!m_ScriptsExtracted || m_GameVersion != WasVersion) {
      m_ScriptsExtracted = ExtractScripts() == 2;
      if (!m_ScriptsExtracted) {
        CopyScripts();
      }
    }
  }

  bool reCachePresets = WasCacheEnabled != m_Config->m_EnableCFGCache;
  if (WasMapPath != m_Config->m_MapPath) {
    try {
      filesystem::create_directory(m_Config->m_MapPath);
    } catch (...) {
      Print("[AURA] warning - <bot.maps_path> is not a valid directory");
    }
    reCachePresets = true;
  }
  if (WasCachePath != m_Config->m_MapCachePath) {
    try {
      filesystem::create_directory(m_Config->m_MapCachePath);
    } catch (...) {
      Print("[AURA] warning - <bot.map_cache_path> is not a valid directory");
    }
    reCachePresets = true;
  }
  if (WasCFGPath != m_Config->m_MapCFGPath) {
    try {
      filesystem::create_directory(m_Config->m_MapCFGPath);
    } catch (...) {
      Print("[AURA] warning - <bot.map_configs_path> is not a valid directory");
    }
  }
  if (WasJASSPath != m_Config->m_JASSPath) {
    try {
      filesystem::create_directory(m_Config->m_JASSPath);
    } catch (...) {
      Print("[AURA] warning - <bot.jass_path> is not a valid directory");
    }
  }

  if (!m_Config->m_EnableCFGCache) {
    m_CachedMaps.clear();
  } else if (reCachePresets) {
    CacheMapPresets();
  }
  m_Net->OnConfigReload();
  return success;
}

bool CAura::LoadConfigs(CConfig& CFG)
{
  CBotConfig* BotConfig = new CBotConfig(CFG);
  CNetConfig* NetConfig = new CNetConfig(CFG);
  CIRCConfig* IRCConfig = new CIRCConfig(CFG);
  CDiscordConfig* DiscordConfig = new CDiscordConfig(CFG);
  CRealmConfig* RealmDefaultConfig = new CRealmConfig(CFG, NetConfig);
  CGameConfig* GameDefaultConfig = new CGameConfig(CFG);

  if (!CFG.GetSuccess()) {
    delete BotConfig;
    delete NetConfig;
    delete IRCConfig;
    delete DiscordConfig;
    delete RealmDefaultConfig;
    delete GameDefaultConfig;
    return false;
  }
  
  delete m_Config;
  delete m_Net->m_Config;
  delete m_IRC->m_Config;
  delete m_Discord->m_Config;
  delete m_RealmDefaultConfig;
  delete m_GameDefaultConfig;

  m_Config = BotConfig;
  m_Net->m_Config = NetConfig;
  m_IRC->m_Config = IRCConfig;
  m_Discord->m_Config = DiscordConfig;
  m_RealmDefaultConfig = RealmDefaultConfig;
  m_GameDefaultConfig = GameDefaultConfig;

  if (m_Config->m_Warcraft3Path.has_value()) {
    m_GameInstallPath = m_Config->m_Warcraft3Path.value();
  } else if (m_GameInstallPath.empty()) {
#ifdef _WIN32
    size_t valueSize;
    errno_t err = _wdupenv_s(&war3Home, &valueSize, L"WAR3_HOME"); 
    if (!err && war3Home != nullptr) {
      wstring war3Path = war3Home;
#else
    const char* envValue = getenv("WAR3_HOME");
    if (envValue != nullptr) {
      string war3Path = envValue;
#endif
      m_GameInstallPath = filesystem::path(war3Path);
    } else {
#ifdef _WIN32
      optional<filesystem::path> maybeInstallPath = MaybeReadRegistryPath(L"SOFTWARE\\Blizzard Entertainment\\Warcraft III", L"InstallPath");
      if (maybeInstallPath.has_value()) {
        m_GameInstallPath = maybeInstallPath.value();
      } else {
        vector<wchar_t*> tryPaths = {
          L"C:\\Program Files (x86)\\Warcraft III\\",
          L"C:\\Program Files\\Warcraft III\\",
          L"C:\\Games\\Warcraft III\\",
          L"C:\\Warcraft III\\",
          L"D:\\Games\\Warcraft III\\",
          L"D:\\Warcraft III\\"
        };
        error_code ec;
        for (const auto& opt : tryPaths) {
          filesystem::path testPath = opt;
          if (filesystem::is_directory(testPath, ec)) {
            m_GameInstallPath = testPath;
          }
        }
      }
#endif
    }
    if (m_GameInstallPath.empty()) {
#ifdef _WIN32
      // Make sure this error message can be looked up.
      Print("[AURA] Registry error loading key 'Warcraft III\\InstallPath'");
#endif
    } else {
      NormalizeDirectory(m_GameInstallPath);
      Print("[AURA] Using <game.install_path = " + PathToString(m_GameInstallPath) + ">");
    }
  }
  return true;
}

void CAura::OnLoadConfigs()
{
  m_LogLevel = m_Config->m_LogLevel;
  if (m_Config->m_War3Version.has_value()) {
    m_GameVersion = m_Config->m_War3Version.value();
  } else if (m_GameVersion == 0 && !m_GameInstallPath.empty() && htons(0xe017) == 0x17e0) {
    optional<uint8_t> AutoVersion = CBNCSUtilInterface::GetGameVersion(m_GameInstallPath);
    if (AutoVersion.has_value()) {
      m_GameVersion = AutoVersion.value();
    }
  }
  m_MaxSlots = m_GameVersion >= 29 ? MAX_SLOTS_MODERN : MAX_SLOTS_LEGACY;
}

uint8_t CAura::ExtractScripts()
{
  if (m_GameInstallPath.empty()) {
    return 0;
  }

  uint8_t FilesExtracted = 0;
  const filesystem::path MPQFilePath = [&]() {
    if (m_GameVersion >= 28)
      return m_GameInstallPath / filesystem::path("War3.mpq");
    else
      return m_GameInstallPath / filesystem::path("War3Patch.mpq");
  }();

  void* MPQ;
  if (OpenMPQArchive(&MPQ, MPQFilePath)) {
    FilesExtracted += ExtractMPQFile(MPQ, R"(Scripts\common.j)", m_Config->m_JASSPath / filesystem::path("common-" + to_string(m_GameVersion) + ".j"));
    FilesExtracted += ExtractMPQFile(MPQ, R"(Scripts\blizzard.j)", m_Config->m_JASSPath / filesystem::path("blizzard-" + to_string(m_GameVersion) + ".j"));
    CloseMPQArchive(MPQ);
  } else {
#ifdef _WIN32
    uint32_t errorCode = (uint32_t)GetLastOSError();
    string errorCodeString = (
      errorCode == 2 ? "Config error: <game.install_path> is not the WC3 directory" : (
      errorCode == 11 ? "File is corrupted." : (
      (errorCode == 3 || errorCode == 15) ? "Config error: <game.install_path> is not a valid directory" : (
      (errorCode == 32 || errorCode == 33) ? "File is currently opened by another process." : (
      "Error code " + to_string(errorCode)
      ))))
    );
#else
    string errorCodeString = "Error code " + to_string(errno);
#endif
    Print("[AURA] warning - unable to load MPQ file [" + PathToString(MPQFilePath) + "] - " + errorCodeString);
  }

  return FilesExtracted;
}

void CAura::LoadMapAliases()
{
  CConfig aliases;
  if (!aliases.Read(m_Config->m_AliasesPath)) {
    return;
  }

  if (!m_DB->Begin()) {
    Print("[AURA] internal database error - map aliases will not be available");
    return;
  }

  for (const auto& entry : aliases.GetEntries()) {
    string normalizedAlias = GetNormalizedAlias(entry.first);
    if (normalizedAlias.empty()) continue;
    m_DB->AliasAdd(normalizedAlias, entry.second);
  }

  if (!m_DB->Commit()) {
    Print("[AURA] internal database error - map aliases will not be available");
  }
}

void CAura::LoadIPToCountryData(const CConfig& CFG)
{
  ifstream in;
  filesystem::path GeoFilePath = CFG.GetHomeDir() / filesystem::path("ip-to-country.csv");
  in.open(GeoFilePath.native().c_str(), ios::in);

  if (in.fail()) {
    Print("[AURA] warning - unable to read file [ip-to-country.csv], geolocalization data not loaded");
    return;
  }
  // the begin and commit statements are optimizations
  // we're about to insert ~4 MB of data into the database so if we allow the database to treat each insert as a transaction it will take a LONG time

  if (!m_DB->Begin()) {
    Print("[AURA] internal database error - geolocalization will not be available");
    in.close();
    return;
  }

  string    Line, Skip, IP1, IP2, Country;
  CSVParser parser;

  in.seekg(0, ios::end);
  in.seekg(0, ios::beg);

  while (!in.eof()) {
    getline(in, Line);

    if (Line.empty())
      continue;

    parser << Line;
    parser >> Skip;
    parser >> Skip;
    parser >> IP1;
    parser >> IP2;
    parser >> Country;
    m_DB->FromAdd(stoul(IP1), stoul(IP2), Country);
  }

  if (!m_DB->Commit()) {
    Print("[AURA] internal database error - geolocalization will not be available");
  }

  in.close();
}

void CAura::InitContextMenu()
{
#ifdef _WIN32
  DeleteUserRegistryKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.w3m");
  DeleteUserRegistryKey(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.w3x");

  wstring openWithAuraCommand = L"\"";
  openWithAuraCommand += GetExePath().wstring();
  openWithAuraCommand += L"\" \"%1\" --stdpaths";

  SetUserRegistryKey(L"Software\\Classes\\WorldEdit.Scenario\\shell\\Host with Aura\\command", L"", openWithAuraCommand.c_str());
  SetUserRegistryKey(L"Software\\Classes\\WorldEdit.ScenarioEx\\shell\\Host with Aura\\command", L"", openWithAuraCommand.c_str());
  Print("[AURA] Installed to context menu.");
#endif
}

void CAura::InitPathVariable()
{
  filesystem::path exeDirectory = GetExeDirectory();
  try {
    filesystem::path exeDirectoryAbsolute = filesystem::absolute(exeDirectory);
    EnsureDirectoryInUserPath(exeDirectoryAbsolute);
  } catch (...) {
  }
}

void CAura::InitSystem()
{
  InitContextMenu();
  InitPathVariable();
}

void CAura::CacheMapPresets()
{
  m_CachedMaps.clear();

  // Preload map_Localpath -> mapcache entries
  const vector<filesystem::path> cacheFiles = FilesMatch(m_Config->m_MapCachePath, FILE_EXTENSIONS_CONFIG);
  for (const auto& cfgName : cacheFiles) {
    string localPathString = CConfig::ReadString(m_Config->m_MapCachePath / cfgName, "map_localpath");
    filesystem::path localPath = localPathString;
    localPath = localPath.lexically_normal();
    try {
      if (localPath == localPath.filename() || filesystem::absolute(localPath.parent_path()) == filesystem::absolute(m_Config->m_MapPath.parent_path())) {
        string mapString = PathToString(localPath.filename());
        string cfgString = PathToString(cfgName);
        if (mapString.empty() || cfgString.empty()) continue;
        m_CachedMaps[mapString] = cfgString;
      }
    } catch (...) {
      // filesystem::absolute may throw errors
    }
  }
}

bool CAura::MatchLogLevel(const uint8_t logLevel)
{
  // 1: emergency ... 9: trace
  return logLevel <= m_LogLevel;
}

bool CAura::CreateGame(CGameSetup* gameSetup)
{
  if (!m_Config->m_Enabled) {
    gameSetup->m_Ctx->ErrorReply("Unable to create game [" + gameSetup->m_Name + "]. The bot is disabled", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (gameSetup->m_Name.size() > m_MaxGameNameSize) {
    gameSetup->m_Ctx->ErrorReply("Unable to create game [" + gameSetup->m_Name + "]. The game name is too long (max " + to_string(m_MaxGameNameSize) + " characters)", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (!gameSetup->m_Map) {
    gameSetup->m_Ctx->ErrorReply("Unable to create game [" + gameSetup->m_Name + "]. The currently loaded game setup is invalid", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }
  if (!gameSetup->m_Map || !gameSetup->m_Map->GetValid()) {
    gameSetup->m_Ctx->ErrorReply("Unable to create game [" + gameSetup->m_Name + "]. The currently loaded map config file is invalid", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (m_CurrentLobby) {
    gameSetup->m_Ctx->ErrorReply("Unable to create game [" + gameSetup->m_Name + "]. Another game lobby [" + m_CurrentLobby->GetDescription() + "] is currently hosted.", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (m_Games.size() >= m_Config->m_MaxGames) {
    gameSetup->m_Ctx->ErrorReply("Unable to create game [" + gameSetup->m_Name + "]. There are too many active games already.", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (gameSetup->GetIsMirror()) {
    Print("[AURA] mirroring game [" + gameSetup->m_Name + "]");
  } else if (gameSetup->m_RestoredGame) {
    Print("[AURA] creating loaded game [" + gameSetup->m_Name + "]");
  } else {
    Print("[AURA] creating game [" + gameSetup->m_Name + "]");
  }

  m_CurrentLobby = new CGame(this, gameSetup);
  m_CanReplaceLobby = gameSetup->m_LobbyReplaceable;
  if (gameSetup->m_LobbyAutoRehosted) {
    m_AutoRehostGameSetup = gameSetup;
  }
  gameSetup->OnGameCreate();

  if (m_CurrentLobby->GetExiting()) {
    delete m_CurrentLobby;
    m_CurrentLobby = nullptr;
    gameSetup->m_Ctx->ErrorReply("Cannot assign a TCP/IP port to game [" + m_CurrentLobby->GetGameName() + "].", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

#ifndef DISABLE_MINIUPNP
  if (m_Net->m_Config->m_EnableUPnP && m_CurrentLobby->GetIsLobby() && m_Games.empty()) {
    // This is a long synchronous network call.
    m_Net->RequestUPnP("TCP", m_CurrentLobby->GetHostPortForDiscoveryInfo(AF_INET), m_CurrentLobby->GetHostPort(), LOG_LEVEL_INFO);
  }
#endif

  if (m_CurrentLobby->GetIsCheckJoinable() && !m_Net->GetIsFetchingIPAddresses()) {
    uint8_t checkMode = HEALTH_CHECK_ALL;
    if (!m_Net->m_SupportTCPOverIPv6) {
      checkMode &= ~HEALTH_CHECK_PUBLIC_IPV6;
      checkMode &= ~HEALTH_CHECK_LOOPBACK_IPV6;
    }
    if (m_CurrentLobby->GetIsVerbose()) {
      checkMode |= HEALTH_CHECK_VERBOSE;
    }
    m_Net->QueryHealthCheck(gameSetup->m_Ctx, checkMode, nullptr, m_CurrentLobby->GetHostPortForDiscoveryInfo(AF_INET));
    m_CurrentLobby->SetIsCheckJoinable(false);
  }

  if (m_CurrentLobby->GetUDPEnabled()) {
    m_CurrentLobby->SendGameDiscoveryCreate();
  }

  for (auto& realm : m_Realms) {
    if (!m_CurrentLobby->GetIsMirror() && !m_CurrentLobby->GetIsRestored()) {
      realm->HoldFriends(m_CurrentLobby);
      realm->HoldClan(m_CurrentLobby);
    }

    if (!realm->GetLoggedIn()) {
      continue;
    }
    if (m_CurrentLobby->GetIsMirror() && realm->GetIsMirror()) {
      // A mirror realm is a realm whose purpose is to mirror games actually hosted by Aura.
      // Do not display external games in those realms.
      continue;
    }
    if (gameSetup->m_RealmsExcluded.find(realm->GetServer()) != gameSetup->m_RealmsExcluded.end()) {
      continue;
    }

    if (m_CurrentLobby->GetDisplayMode() == GAME_PUBLIC && realm->GetAnnounceHostToChat()) {
      realm->QueueGameChatAnnouncement(m_CurrentLobby);
    } else {
      // Send STARTADVEX3
      m_CurrentLobby->AnnounceToRealm(realm);

      // if we're creating a private game we don't need to send any further game refresh messages so we can rejoin the chat immediately
      // unfortunately, this doesn't work on PVPGN servers, because they consider an enterchat message to be a gameuncreate message when in a game
      // so don't rejoin the chat if we're using PVPGN

      if (m_CurrentLobby->GetDisplayMode() == GAME_PRIVATE && !realm->GetPvPGN()) {
        realm->SendEnterChat();
      }
    }
  }

  if (m_CurrentLobby->GetDisplayMode() != GAME_PUBLIC ||
    gameSetup->m_CreatedFromType != GAMESETUP_ORIGIN_REALM ||
    gameSetup->m_Ctx->GetIsWhisper()) {
    gameSetup->m_Ctx->SendPrivateReply(m_CurrentLobby->GetAnnounceText());
  }

  if (m_CurrentLobby->GetDisplayMode() == GAME_PUBLIC) {
    if (m_IRC) {
     m_IRC->SendAllChannels(m_CurrentLobby->GetAnnounceText());
    }
    if (m_Discord) {
      //TODO: Discord game created announcement
      //m_Discord->SendAnnouncementChannels(m_CurrentLobby->GetAnnounceText());
    }
  }

  uint32_t mapSize = ByteArrayToUInt32(m_CurrentLobby->GetMap()->GetMapSize(), false);
  if (m_GameVersion <= 26 && mapSize > 0x800000) {
    Print("[AURA] warning - hosting game beyond 8MB map size limit: [" + m_CurrentLobby->GetMap()->GetServerFileName() + "]");
  }
  if (m_GameVersion < m_CurrentLobby->GetMap()->GetMapMinGameVersion()) {
    Print("[AURA] warning - hosting game that may require version 1." + to_string(m_CurrentLobby->GetMap()->GetMapMinGameVersion()));
  }

  return true;
}

void CAura::HoldContext(CCommandContext* nCtx)
{
  nCtx->Ref();
  m_ActiveContexts.insert(nCtx);
}

void CAura::UnholdContext(CCommandContext* nCtx)
{
  if (nCtx == nullptr)
    return;

  if (nCtx->Unref()) {
    if (MatchLogLevel(LOG_LEVEL_TRACE)) {
      Print("[AURA] deleting ctx for message sent by [" + nCtx->GetSender() + "].");
    }
    m_ActiveContexts.erase(nCtx);
    delete nCtx;
  }
}

uint32_t CAura::NextHostCounter()
{
  m_HostCounter = (m_HostCounter + 1) & 0x00FFFFFF;
  if (m_HostCounter < m_Config->m_MinHostCounter) {
    m_HostCounter = m_Config->m_MinHostCounter;
  }
  return m_HostCounter;
}

uint32_t CAura::NextServerID()
{
  ++m_LastServerID;
  if (m_LastServerID < 0x10) {
    // Ran out of server IDs.
    m_LastServerID = 0;
  }
  return m_LastServerID;
}