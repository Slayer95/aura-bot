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
#include "irc.h"
#include "util.h"
#include "fileutil.h"
#include "argh.h"

#include <csignal>
#include <cstdlib>
#include <thread>
#include <fstream>
#include <algorithm>
#include <string>
#include <iterator>
#include <exception>

#define __STORMLIB_SELF__
#include <StormLib.h>

#ifdef WIN32
#define NOMINMAX
#include <ws2tcpip.h>
#include <winsock.h>
#include <process.h>
#endif

#define VERSION "2.0.0.dev"
#define REPOSITORY "https://gitlab.com/ivojulca/aura-bot"

using namespace std;

#undef FD_SETSIZE
#define FD_SETSIZE 512

static CAura* gAura    = nullptr;
bool          gRestart = false;

//
// main
//

int main(const int argc, const char* argv[])
{
  // seed the PRNG

  srand(static_cast<uint32_t>(time(nullptr)));

  // disable sync since we don't use cstdio anyway

  ios_base::sync_with_stdio(false);

  // read config file

  CConfig CFG;
  filesystem::path ExeDirectory = GetExeDirectory();
  filesystem::path ConfigPath = ExeDirectory / filesystem::path("config.ini");
  if (!CFG.Read(ConfigPath)) {
    int FileSize = 0;
    string CFGExample = FileRead(ExeDirectory / filesystem::path("config-example.ini"), &FileSize);
    vector<uint8_t> CFGCopy(CFGExample.begin(), CFGExample.end());
    Print("[AURA] copying config-example.cfg to config.ini...");
    FileWrite(ConfigPath, CFGCopy.data(), CFGCopy.size());
    if (!CFG.Read(ConfigPath)) {
      Print("[AURA] error initializing config.ini");
      return 1;
    }
  }

  signal(SIGINT, [](int32_t) -> void {
    Print("[!!!] caught signal SIGINT, exiting NOW");

    if (gAura)
    {
      if (gAura->m_Exiting)
        exit(1);
      else
        gAura->m_Exiting = true;
    }
    else
      exit(1);
  });

#ifndef WIN32
  // disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

  signal(SIGPIPE, SIG_IGN);
#endif

#ifdef WIN32
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

  // initialize aura

  gAura = new CAura(&CFG, argc, argv);

  // check if it's properly configured

  if (gAura->GetReady())
  {
    // loop

    while (!gAura->Update())
      ;
  }

  // shutdown aura

  Print("[AURA] shutting down");
  delete gAura;

#ifdef WIN32
  // shutdown winsock

  WSACleanup();
#endif

  // restart the program

  if (gRestart)
  {
#ifdef WIN32
    _spawnl(_P_OVERLAY, argv[0], argv[0], nullptr);
#else
    execl(argv[0], argv[0], nullptr);
#endif
  }

  return 0;
}

//
// CAura
//

CAura::CAura(CConfig* CFG, const int argc, const char* argv[])
  : m_GameProtocol(nullptr),
    m_GPSProtocol(new CGPSProtocol()),
    m_CRC(new CCRC32()),
    m_SHA(new CSHA1()),
    m_IRC(nullptr),
    m_Net(nullptr),
    m_CurrentLobby(nullptr),
    m_DB(new CAuraDB(CFG)),
    m_Map(nullptr),
    m_Config(nullptr),
    m_RealmDefaultConfig(nullptr),
    m_GameDefaultConfig(nullptr),
    m_Version(VERSION),
    m_RepositoryURL(REPOSITORY),
    m_MaxSlots(MAX_SLOTS_LEGACY),
    m_HostCounter(0),
    m_LastHostPort(0),
    m_MaxGameNameSize(31),
    m_SudoRealm(nullptr),
    m_SudoGame(nullptr),
    m_SudoIRC(nullptr),
    m_GameVersion(0),
    m_Exiting(false),
    m_Ready(true),
    m_ScriptsExtracted(false)
{
  Print("[AURA] Aura version " + m_Version);

  m_GameProtocol = new CGameProtocol(this);
  m_Net = new CNet(this);
  m_IRC = new CIRC(this);

  m_CRC->Initialize();

  if (!LoadConfigs(CFG)) {
    Print("[CONFIG] Error: Critical errors found in config.ini");
    m_Ready = false;
    return;
  }
  if (LoadCLI(argc, argv)) {
    m_Ready = false;
    return;
  }
  if (m_GameVersion == 0) {
    Print("[CONFIG] Game version and path are missing.");
    m_Ready = false;
    return;
  }
  Print("[AURA] Running game version 1." + to_string(m_GameVersion));

  if (!m_Net->Init()) {
    Print("[AURA] error - close active instances of Warcraft, and/or pause LANViewer to initialize Aura.");
    m_Ready = false;
    return;
  }

  if (m_Net->m_Config->m_UDPEnableCustomPortTCP4) {
    Print("[AURA] Broadcasting games port " + to_string(m_Net->m_Config->m_UDPCustomPortTCP4) + " over LAN");
  }

  if (m_Config->m_EnableBNET.has_value()) {
    if (m_Config->m_EnableBNET.value()) {
      Print("[AURA] All realms forcibly set to ENABLED <bot.toggle_every_realm = on>");
    } else {
      Print("[AURA] All realms forcibly set to DISABLED <bot.toggle_every_realm = off>");
    }
  }
  if (m_Config->m_EnableBNET.value_or(true)) {
    LoadBNETs(CFG);
  }
  vector<string> invalidKeys = CFG->GetInvalidKeys(m_Config->m_EnableBNET.value_or(true));
  if (!invalidKeys.empty()) {
    Print("[CONFIG] warning - some keys are misnamed: " + JoinVector(invalidKeys, false));
  }

  if (m_Realms.empty() && !m_Config->m_EnableBNET.value_or(true))
    Print("[AURA] warning - no enabled battle.net connections configured");
  if (!m_IRC)
    Print("[AURA] warning - no irc connection configured");

  if (m_Realms.empty() && !m_IRC && m_PendingActions.empty()) {
    Print("[AURA] Input disconnected.");
    m_Ready = false;
    return;
  }

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

  // load the iptocountry data
  LoadIPToCountryData();

  // Read CFG->Map links and cache the reverse.
  CacheMapPresets();
}

bool CAura::LoadBNETs(CConfig* CFG)
{
  // load the battle.net connections
  // we're just loading the config data and creating the CRealm classes here, the connections are established later (in the Update function)

  size_t LongestGamePrefixSize = 0;
  bool IsReload = !m_Realms.empty();
  for (uint8_t i = 1; i <= 240; ++i) { // uint8_t wraps around and loops forever
    bool DoResetConnection = false;
    CRealmConfig* ThisConfig = new CRealmConfig(CFG, m_RealmDefaultConfig, i);
    if (m_Config->m_EnableBNET.has_value()) {
      ThisConfig->m_Enabled = m_Config->m_EnableBNET.value();
    }
    if (ThisConfig->m_HostName.empty()) {
      delete ThisConfig;
      continue;
    }
    if (ThisConfig->m_UserName.empty() || ThisConfig->m_PassWord.empty()) {
      if (!IsReload && ThisConfig->m_Enabled) {
        Print("[AURA] server #" + to_string(i) + " ignored [" + ThisConfig->m_UniqueName + "] - username and/or password missing");
      }
      delete ThisConfig;
      continue;
    }
    if (IsReload && i < m_Realms.size()) {
      if (ThisConfig->m_HostName != m_Realms[i - 1]->GetServer()) {
        Print("[AURA] ignoring update for battle.net config #" + to_string(i) + "; host_name mismatch - expected [" + m_Realms[i - 1]->GetServer() + "], but got " + ThisConfig->m_HostName);
        delete ThisConfig;
        continue;
      }
      DoResetConnection = m_Realms[i]->GetLoginName() != ThisConfig->m_UserName || m_Realms[i]->GetEnabled() && !ThisConfig->m_Enabled;
      m_Realms[i]->SetConfig(ThisConfig);
    }

    if (ThisConfig->m_CDKeyROC.length() != 26 || ThisConfig->m_CDKeyTFT.length() != 26)
      Print("[BNET: " + ThisConfig->m_UniqueName + "] warning - your CD keys are not 26 characters long - probably invalid");

    if (ThisConfig->m_GamePrefix.length() > LongestGamePrefixSize)
      LongestGamePrefixSize = ThisConfig->m_GamePrefix.length();

    for (auto& RootAdmin : ThisConfig->m_RootAdmins) {
      m_DB->RootAdminAdd(ThisConfig->m_DataBaseID, RootAdmin);
    }

    if (IsReload && i < m_Realms.size()) {
      Print("[AURA] server #" + to_string(i) + " reloaded [" + ThisConfig->m_UniqueName + "]");
      if (DoResetConnection) {
        m_Realms[i]->ResetConnection(false);
      }
    } else {
      Print("[AURA] server #" + to_string(i) + " found [" + ThisConfig->m_UniqueName + "]");
      m_Realms.push_back(new CRealm(this, ThisConfig));
    }
  }

  m_MaxGameNameSize = 31 - LongestGamePrefixSize;
  return true;
}

bool CAura::CopyScripts()
{
  // Try to use manually extracted files already available in bot.map_configs_path
  filesystem::path autoExtractedCommonPath = m_Config->m_MapCFGPath / filesystem::path("common-" + to_string(m_GameVersion) + ".j");
  filesystem::path autoExtractedBlizzardPath = m_Config->m_MapCFGPath / filesystem::path("blizzard-" + to_string(m_GameVersion) + ".j");
  bool commonExists = FileExists(autoExtractedCommonPath);
  bool blizzardExists = FileExists(autoExtractedBlizzardPath);
  if (commonExists && blizzardExists) {
    return true;
  }

  if (!commonExists) {
    filesystem::path manuallyExtractedCommonPath = m_Config->m_MapCFGPath / filesystem::path("common.j");
    try {
      filesystem::copy_file(manuallyExtractedCommonPath, autoExtractedCommonPath, filesystem::copy_options::skip_existing);
    } catch (const exception& e) {
      Print("[AURA] File system error at " + manuallyExtractedCommonPath.string() + ": " + string(e.what()));
      return false;
    }
  }
  if (!blizzardExists) {
    filesystem::path manuallyExtractedBlizzardPath = m_Config->m_MapCFGPath / filesystem::path("blizzard.j");
    try {
      filesystem::copy_file(manuallyExtractedBlizzardPath, autoExtractedBlizzardPath, filesystem::copy_options::skip_existing);
    } catch (const exception& e) {
      Print("[AURA] File system error at " + manuallyExtractedBlizzardPath.string() + ": " + string(e.what()));
      return false;
    }
  }
  return true;
}

CAura::~CAura()
{
  m_SudoGame = nullptr;
  m_SudoIRC = nullptr;
  m_SudoRealm = nullptr;  

  delete m_Config;
  delete m_RealmDefaultConfig;
  delete m_GameDefaultConfig;
  delete m_Net;
  delete m_GameProtocol;
  delete m_GPSProtocol;
  delete m_CRC;
  delete m_SHA;
  delete m_Map;

  for (auto& bnet : m_Realms)
    delete bnet;

  delete m_CurrentLobby;

  for (auto& game : m_Games)
    delete game;

  delete m_DB;
  delete m_IRC;
}

CRealm* CAura::GetRealmByInputId(const string& inputId)
{
  for (auto& bnet : m_Realms) {
    if (bnet->GetInputID() == inputId) {
      return bnet;
    }
  }
  return nullptr;
}

CRealm* CAura::GetRealmByHostName(const string& hostName)
{
  for (auto& bnet : m_Realms) {
    if (bnet->GetServer() == hostName) {
      return bnet;
    }
  }
  return nullptr;
}

CRealm* CAura::GetRealmByHostCounter(const uint8_t hostCounter)
{
  for (auto& bnet : m_Realms) {
    if (bnet->GetHostCounterID() == hostCounter) {
      return bnet;
    }
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

pair<uint8_t, string> CAura::LoadMap(const string& user, const string& mapInput, const string& observersInput, const string& visibilityInput, const string& randomRaceInput, const string& randomHeroInput, const bool& gonnaBeLucky, const bool& allowArbitraryMapPath)
{
  string SearchInput = mapInput;
  int MapObserversValue = -1;
  int MapVisibilityValue = -1;
  int MapExtraFlags = 0;

  bool errored = false;
  if (!observersInput.empty()) {
    int result = ParseMapObservers(observersInput, errored);
    if (errored) {
      return make_pair(0x10, string("Invalid value passed for map observers: \"" + observersInput + "\""));
    }
    MapObserversValue = result;
  }
  if (!visibilityInput.empty()) {
    int result = ParseMapVisibility(visibilityInput, errored);
    if (errored) {
      return make_pair(0x11, string("Invalid value passed for map visibility: \"" + visibilityInput + "\""));
    }
    MapVisibilityValue = result;
  }
  if (!randomRaceInput.empty()) {
    int result = ParseMapRandomRaces(randomRaceInput, errored);
    if (errored) {
      return make_pair(0x12, string("Invalid value passed for race randomization: \"" + randomRaceInput + "\""));
    }
    MapExtraFlags = result;
  }
  if (!randomHeroInput.empty()) {
    int result = ParseMapRandomHero(randomHeroInput, errored);
    if (errored) {
      return make_pair(0x12, string("Invalid value passed for hero randomization: \"" + randomHeroInput + "\""));
    }
    MapExtraFlags = result;
  }

  if (!FileExists(this->m_Config->m_MapPath)) {
    Print("[AURA] config error - map path doesn't exist");
    return make_pair(0x20, string("Error listing maps - the bot is misconfigured"));
  }

  string MapSiteUri;
  string MapDownloadUri;
  filesystem::path ResolvedCFGPath;
  string ResolvedCFGName;
  filesystem::path FilePath;
  string FileName;
  string FileNameClient;
  bool ResolvedCFGExists = false;

  pair<string, string> NamespacedMap = ParseMapId(SearchInput, m_Config->m_StrictPaths);
  if (NamespacedMap.first == "remote") {
    return make_pair(0x30, string("Download from the specified domain not supported"));
  }
  if (NamespacedMap.second.empty()) {
    return make_pair(0x31, string("Map not valid"));
  }

  CConfig MapCFG;

  // Figure out whether the map pointed at by the URL is already cached locally.
  if (NamespacedMap.first == "epicwar") {
    MapSiteUri = "https://www.epicwar.com/maps/" + NamespacedMap.second;
    // Check that we don't download the same map multiple times.
    ResolvedCFGName = "epicwar-" + NamespacedMap.second + ".cfg";
    ResolvedCFGPath = this->m_Config->m_MapCFGPath / filesystem::path(ResolvedCFGName);
    ResolvedCFGExists = MapCFG.Read(ResolvedCFGPath);
  }

  if (NamespacedMap.first != "local" && !ResolvedCFGExists) {
    // Map config not locally available. Download the map so we can calculate it.
    // But only if we are allowed to.
    if (!this->m_Net->m_Config->m_AllowDownloads) {
      return make_pair(0x21, string("Map downloads are not allowed."));
    }
    if (!this->m_Games.empty() && this->m_Net->m_Config->m_HasBufferBloat) {
      return make_pair(0x40, string("Currently hosting ") + to_string(this->m_Games.size()) + " game(s). Downloads are disabled to prevent high latency.");
    }

    uint8_t DownloadResult = DownloadRemoteMap(NamespacedMap.first, MapSiteUri, MapDownloadUri, FileName, FileNameClient, m_Config->m_MapPath, m_BusyMaps);

    if (DownloadResult == 1) {
      return make_pair(0x50, string("Map not found in EpicWar."));
    } else if (DownloadResult == 2) {
      return make_pair(0x51, string("Downloaded map invalid."));
    } else if (DownloadResult == 3) {
      return make_pair(0x52, string("Download failed - duplicate map name."));
    } else if (DownloadResult == 4) {
      return make_pair(0x53, string("Download failed - unable to write to disk."));
    } else if (DownloadResult != 0) {
      return make_pair(0x54, string("Download failed - code ") + to_string(DownloadResult));
    }
    //SendChatOrWhisper("Downloaded <" + MapSiteUri + "> as [" + Map + "]", User, Whisper, m_IRC);
    Print("[AURA] Downloaded <" + MapSiteUri + "> as [" + FileName + "]");
    SearchInput = FileName;
  }

  if (NamespacedMap.first == "local") {
    SearchInput = NamespacedMap.second;
  }

  if (ResolvedCFGExists) {
    // Exact resolution by map identifier.
    //SendChatOrWhisper("Loading config file [" + ResolvedCFGPath.string() + "]", User, Whisper, m_IRC);
  } else {
    if (m_Config->m_StrictPaths) {
      filesystem::path mapPath = SearchInput;
      if (!allowArbitraryMapPath && mapPath.is_absolute()) {
        return make_pair(0x60, string("Absolute map path not allowed."));
      }
      FilePath = (mapPath.is_absolute() ? mapPath : (this->m_Config->m_MapPath / mapPath)).lexically_normal();
      FileName = FilePath.filename().string();
      if (PathHasNullBytes(FilePath) || !allowArbitraryMapPath && FilePath.parent_path() != this->m_Config->m_MapPath) {
        return make_pair(0x61, string("Forbidden map path."));
      }
    } else {
      // Fuzzy resolution by map name.
      const vector<string> LocalMatches = this->MapFilesMatch(SearchInput);
      if (LocalMatches.empty()) {
        string Suggestions = GetEpicWarSuggestions(SearchInput, 5);
        if (Suggestions.empty()) {
          return make_pair(0x62, string("No matching map found."));
        } else {
          return make_pair(0x63, string("Suggestions: ") + Suggestions);
        }
      }
      if (LocalMatches.size() > 1 && !gonnaBeLucky) {
        string Suggestions = GetEpicWarSuggestions(SearchInput, 5);
        string Results = JoinVector(LocalMatches, !Suggestions.empty()) + Suggestions;
        return make_pair(0x63, string("Suggestions: ") + Results);
      }
      filesystem::path FileNameFragment = LocalMatches.at(0);
      FilePath = this->m_Config->m_MapPath / FileNameFragment;
      FileName = FilePath.filename().string();
    }

    // Load associated CFG if available.
    if (m_Config->m_EnableCFGCache && this->m_CachedMaps.find(FileName) != this->m_CachedMaps.end()) {
      ResolvedCFGName = this->m_CachedMaps[FileName];
      ResolvedCFGPath = this->m_Config->m_MapCFGPath / filesystem::path(ResolvedCFGName);
      ResolvedCFGExists = MapCFG.Read(ResolvedCFGPath);
      if (ResolvedCFGExists) {
        //SendChatOrWhisper("Loading config file [" + ResolvedCFGPath.string() + "]", User, Whisper, m_IRC);
      } else {
        // Oops! CFG file is gone!
        this->m_CachedMaps.erase(FileName);
      }
    }
    if (!ResolvedCFGExists) {
      // CFG nowhere to be found.
      Print("[AURA] Loading map file [" + FileName + "]...");

      MapCFG.SetBool("cfg_partial", true);
      if (FileNameClient.length() > 6 && FileName[FileNameClient.length() - 6] == '~' && isdigit(FileNameClient[FileNameClient.length() - 5])) {
        // IDK if this path is okay. Let's give it a try.
        MapCFG.Set("map_path", R"(Maps\Download\)" + FileNameClient.substr(0, FileNameClient.length() - 6) + FileNameClient.substr(FileNameClient.length() - 4));
      } else {
        MapCFG.Set("map_path", R"(Maps\Download\)" + FileNameClient);
      }
      if (FilePath.parent_path() == this->m_Config->m_MapPath) {
        MapCFG.Set("map_localpath", FileName);
      } else {
        MapCFG.Set("map_localpath", FilePath.string());
      }
      MapCFG.Set("map_site", MapSiteUri);
      MapCFG.Set("map_url", MapDownloadUri);
      if (NamespacedMap.first != "local")
        MapCFG.Set("downloaded_by", user);

      if (FileName.find("DotA") != string::npos)
        MapCFG.Set("map_type", "dota");

      if (ResolvedCFGName.empty()) {
        ResolvedCFGName = "local-" + FileName + ".cfg";
        ResolvedCFGPath = this->m_Config->m_MapCFGPath / filesystem::path(ResolvedCFGName);
      }

      if (m_Config->m_EnableCFGCache) {
        vector<uint8_t> OutputBytes = MapCFG.Export();
        FileWrite(ResolvedCFGPath, OutputBytes.data(), OutputBytes.size());
        this->m_CachedMaps[FileName] = ResolvedCFGName;
      }
    }
  }

  if (MapObserversValue != -1)
    MapCFG.Set("map_observers", to_string(MapObserversValue));
  if (MapVisibilityValue != -1)
    MapCFG.Set("map_visibility", to_string(MapVisibilityValue));

  if (this->m_Map) {
    delete this->m_Map;
  }
  this->m_Map = new CMap(this, &MapCFG, ResolvedCFGExists ? "cfg:" + ResolvedCFGName : MapCFG.GetString("map_localpath", ""));
  if (MapExtraFlags != 0) {
    this->m_Map->AddMapFlags(MapExtraFlags);
  }

  const char* ErrorMessage = this->m_Map->CheckValid();

  if (ErrorMessage) {
    if (!ResolvedCFGPath.empty()) {
      FileDelete(ResolvedCFGPath);
    }
    delete this->m_Map;
    this->m_Map = nullptr;
    return make_pair(0x70, string("Error while loading map: [" + string(ErrorMessage) + "]"));
  }

  if (!ResolvedCFGExists) {
    if (m_Config->m_EnableCFGCache && !MapCFG.GetBool("cfg_partial", false)) {
      // Download and parse successful
      vector<uint8_t> OutputBytes = MapCFG.Export();
      if (FileWrite(ResolvedCFGPath, OutputBytes.data(), OutputBytes.size())) {
        Print("[AURA] Cached map config for [" + FileName + "] as [" + ResolvedCFGPath.string() + "]");
      }
    }
    //SendChatOrWhisper("Map file loaded OK [" + FileName + "]", User, Whisper, m_IRC);
    Print("[AURA] Map file loaded OK [" + FileName + "]");
    return make_pair(0, string("Map file loaded OK [") + FileName + "]");
  } else {
    return make_pair(0, string("Config file loaded OK [") + ResolvedCFGPath.string() + "]");
  }
}

pair<uint8_t, string> CAura::LoadMapConfig(const string& user, const string& cfgInput, const string& observersInput, const string& visibilityInput, const string& randomRaceInput, const string& randomHeroInput, const bool& allowArbitraryMapPath)
{
  CConfig MapCFG;

  filesystem::path cfgPath = cfgInput;
  if (!allowArbitraryMapPath && cfgPath.is_absolute()) {
    return make_pair(0x60, string("Absolute config path not allowed."));
  }
  filesystem::path FilePath = (cfgPath.is_absolute() ? cfgPath : (this->m_Config->m_MapCFGPath / cfgPath)).lexically_normal();
  string FileName = FilePath.filename().string();
  if (PathHasNullBytes(FilePath) || !allowArbitraryMapPath && FilePath.parent_path() != this->m_Config->m_MapCFGPath) {
    return make_pair(0x61, string("Forbidden config path."));
  }

  // TODO(IceSandslash): Use inputs

  int MapExtraFlags = 0;
  bool ResolvedCFGExists = MapCFG.Read(FilePath);
  if (!ResolvedCFGExists) {
    return make_pair(0x62, string("Config file not found."));
  }

  if (this->m_Map) {
    delete this->m_Map;
  }
  this->m_Map = new CMap(this, &MapCFG, "cfg:" + FileName);
  if (MapExtraFlags != 0) {
    this->m_Map->AddMapFlags(MapExtraFlags);
  }

  const char* ErrorMessage = this->m_Map->CheckValid();

  if (ErrorMessage) {
    delete this->m_Map;
    this->m_Map = nullptr;
    return make_pair(0x70, string("Error while loading config: [" + string(ErrorMessage) + "]"));
  }

  return make_pair(0, string("Config file loaded OK [") + FilePath.string() + "]");
}

void CAura::HandleHealthCheck()
{
  bool hasDirectAttempts = false;
  bool anyDirectSuccess = false;
  vector<string> ChatReport;
  vector<uint16_t> FailPorts;
  bool isIPv6Reachable = false;
  for (auto& testConnection : m_Net->m_HealthCheckClients) {
    bool success = false;
    string ResultText;
    if (testConnection->m_Passed.value_or(false)) {
      success = true;
      ResultText = "OK";
    } else if (testConnection->m_CanConnect.value_or(false)) {
      ResultText = "Cannot join";
    } else {
      ResultText = "Cannot connect";
    }
    ChatReport.push_back(testConnection->m_Name + " - " + ResultText);
    Print("[AURA] Game at " + testConnection->m_Name + " - " + ResultText);
    if (0 == (testConnection->m_Type & ~(CONNECTION_TYPE_CUSTOM_PORT))) {
      hasDirectAttempts = true;
      if (success) anyDirectSuccess = true;
    }
    if (0 != (testConnection->m_Type & (CONNECTION_TYPE_IPV6))) {
      if (success) isIPv6Reachable = true;
    }
    if (!success) {
      uint16_t port = testConnection->GetPort();
      if (std::find(FailPorts.begin(), FailPorts.end(), port) == FailPorts.end())
        FailPorts.push_back(port);
    }
  }
  sockaddr_storage* publicIPv4 = m_Net->GetPublicIPv4();
  if (publicIPv4 != nullptr && hasDirectAttempts) {
    string portForwardInstructions;
    if (m_CurrentLobby && m_CurrentLobby->GetIsLobby()) {
      portForwardInstructions = "About port-forwarding: Setup your router to forward external port(s) {" + JoinVector(FailPorts, false) + "} to internal port(s) {" + JoinVector(m_Net->GetPotentialGamePorts(), false) + "}";
    }
    if (anyDirectSuccess) {
      Print("[Network] This bot CAN be reached through the IPv4 Internet. Address: " + AddressToString(*publicIPv4) + ".");
      m_CurrentLobby->SendAllChat("This bot CAN be reached through the IPv4 Internet.");
    } else {
      Print("[Network] This bot is disconnected from the IPv4 Internet, because its public address is unreachable. Address: " + AddressToString(*publicIPv4) + ".");
      Print("[Network] Please setup port-forwarding to allow connections.");
      if (!portForwardInstructions.empty())
        Print("[Network] " + portForwardInstructions);
      Print("[Network] If your router has Universal Plug and Play, the command [upnp] will automatically setup port-forwarding.");
      Print("[Network] Note that you may still play online if you got a VPN, or an active tunnel. See NETWORKING.md for details.");
      Print("[Network] But make sure your firewall allows Aura inbound TCP connections.");
      if (m_CurrentLobby && m_CurrentLobby->GetIsLobby()) {
        m_CurrentLobby->SendAllChat("============= READ IF YOU ARE RUNNING AURA =====================================");
        m_CurrentLobby->SendAllChat("This bot is disconnected from the IPv4 Internet, because its public IPv4 address is unreachable.");
        m_CurrentLobby->SendAllChat("Please setup port-forwarding to allow connections.");
        m_CurrentLobby->SendAllChat(portForwardInstructions);
        m_CurrentLobby->SendAllChat("If your router has Universal Plug and Play, the command [upnp] will automatically setup port-forwarding.");
        m_CurrentLobby->SendAllChat("Note that you may still play online if you got a VPN, or an active tunnel. See NETWORKING.md for details.");
        m_CurrentLobby->SendAllChat("But make sure your firewall allows Aura inbound TCP connections.");
        m_CurrentLobby->SendAllChat("=================================================================================");
      }
    }
  } else {
    Print("Public IPv4 is null");
  }
  sockaddr_storage* publicIPv6 = m_Net->GetPublicIPv6();
  if (publicIPv6 != nullptr) {
    if (isIPv6Reachable) {
      Print("[Network] This bot CAN be reached through the IPv6 Internet. Address: " + AddressToString(*publicIPv6) + ".");
      Print("[Network] See NETWORKING.md for instructions to use IPv6 TCP tunneling.");
      m_CurrentLobby->SendAllChat("This bot CAN be reached through the IPv6 Internet.");
      m_CurrentLobby->SendAllChat("See NETWORKING.md for instructions to use IPv6 TCP tunneling.");
      m_CurrentLobby->SendAllChat("=================================================================================");
    } else {
      Print("[Network] This bot is disconnected from the IPv6 Internet, because its public address is unreachable. Address: " + AddressToString(*publicIPv6) + ".");
      m_CurrentLobby->SendAllChat("This bot is disconnected from the IPv6 Internet, because its public address is unreachable.");
      m_CurrentLobby->SendAllChat("=================================================================================");
    }
  }
  if (m_CurrentLobby && m_CurrentLobby->GetIsLobby()) {
    string LeftMessage = JoinVector(ChatReport, " | ", false);
    while (LeftMessage.length() > 254) {
      m_CurrentLobby->SendAllChat(LeftMessage.substr(0, 254));
      LeftMessage = LeftMessage.substr(254);
    }
    if (LeftMessage.length()) {
      m_CurrentLobby->SendAllChat(LeftMessage);
    }
  }
  m_Net->ResetHealthCheck();
}

bool CAura::HandleAction(vector<string>& action)
{
  if (action[0] == "exec") {
    Print("[AURA] Exec cli unsupported yet");
    return true;
  } else if (action[0] == "hostmap" || action[0] == "hostcfg") {
    string DownloadedBy;
    string SearchInput = action[1];
    string GameName = action[2];
    string ObsInput = action[3];
    string VisInput = action[4];
    string RRInput = action[5];
    string RHInput = action[6];
    string OwnerName = action[7];
    string ExcludedRealm = action[8]; // TODO?
    const bool AllowTraversal = action[9] == "1";
    const bool GonnaBeLucky = true;

    string OwnerRealmName;
    CRealm* OwnerRealm = nullptr;

    size_t OwnerRealmIndex = OwnerName.find('@');
    if (OwnerRealmIndex != string::npos) {
      OwnerName = OwnerName.substr(0, OwnerRealmIndex);
      OwnerRealmName = OwnerName.substr(OwnerRealmIndex + 1);
      transform(begin(OwnerRealmName), end(OwnerRealmName), begin(OwnerRealmName), ::tolower);
      OwnerRealm = GetRealmByInputId(OwnerRealmName);
    }

    pair<uint8_t, string> LoadResult;
    if (action[0] == "hostmap") {
      LoadResult = LoadMap(OwnerName, SearchInput, ObsInput, VisInput, RRInput, RHInput, GonnaBeLucky, AllowTraversal);
    } else if (action[0] == "hostcfg") {
      LoadResult = LoadMapConfig(OwnerName, SearchInput, ObsInput, VisInput, RRInput, RHInput, AllowTraversal);
    }
    if (!LoadResult.second.empty()) {
      Print("[AURA] " + LoadResult.second);
    }

    if (LoadResult.first != 0) {
      return true; // Exit
    }

    if (GameName.empty())
      GameName = "gogogo";

    CCommandContext* ctx = new CCommandContext(this, &cout, '!');
    bool success = CreateGame(
      m_Map, GAME_PUBLIC, GameName,
      OwnerName, OwnerRealm ? OwnerRealm->GetServer() : "",
      OwnerName, OwnerRealm, ctx
    );
    m_Map = nullptr;
    if (!success) {
      return true;
    }
  } else if (action[0] == "mirror") {
    string MapInput = action[1];
    string GameName = action[2];
    string ExcludedRealm = action[3];
    string MirrorTarget = action[4];

    Print("[AURA] Mirror cli unsupported yet");
    return true;
  } else {
    Print("[AURA] Action type unsupported");
    return true;
  }
  return false;
}

void CAura::HandleUDP(UDPPkt* pkt)
{
  std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)pkt->buf, pkt->length);
  // pkt->buf->length at least MIN_UDP_PACKET_SIZE

  if (pkt->sender->ss_family != AF_INET && pkt->sender->ss_family != AF_INET6) {
    return;
  }

  uint16_t remotePort = GetAddressPort(*(pkt->sender));
  string ipAddress = AddressToString(*(pkt->sender));

  if (IsIgnoredDatagramSource(ipAddress)) {
    return;
  }

  if (m_Net->m_Config->m_UDPForwardTraffic) {
    vector<uint8_t> relayPacket = {W3FW_HEADER_CONSTANT, 0, 0, 0};
    AppendByteArray(relayPacket, ipAddress, true);
    size_t portOffset = relayPacket.size();
    relayPacket.resize(portOffset + 6 + pkt->length);
    relayPacket[portOffset] = static_cast<uint8_t>(remotePort >> 8); // Network-byte-order (Big-endian)
    relayPacket[portOffset + 1] = static_cast<uint8_t>(remotePort);
    memset(relayPacket.data() + portOffset + 2, 0, 4); // Game version unknown at this layer.
    memcpy(relayPacket.data() + portOffset + 6, &(pkt->buf), pkt->length);
    AssignLength(relayPacket);
    m_Net->Send(&(m_Net->m_Config->m_UDPForwardAddress), relayPacket);
  }

  if (static_cast<unsigned char>(pkt->buf[0]) != W3GS_HEADER_CONSTANT) {
    return;
  }

  if (!(pkt->length >= 16 && static_cast<unsigned char>(pkt->buf[1]) == CGameProtocol::W3GS_SEARCHGAME)) {
    return;
  }

  uint8_t ExtraDigit = pkt->length == 22 ? 0 : 0xFF;

  if (pkt->buf[8] == m_GameVersion || pkt->buf[8] == 0) {
    if (m_CurrentLobby && m_CurrentLobby->GetUDPEnabled() && !m_CurrentLobby->GetCountDownStarted()) {
      //Print("IP " + ipAddress + " searching from port " + to_string(remotePort) + "...");
      m_CurrentLobby->ReplySearch(pkt->sender, pkt->socket);

      // When we get GAME_SEARCH from a remote port other than 6112, we still announce to port 6112.
      if (remotePort != m_Net->m_UDP4TargetPort && GetInnerIPVersion(pkt->sender) == AF_INET) {
        m_CurrentLobby->AnnounceToAddress(ipAddress);
      }
    }
  }
}

bool CAura::Update()
{
  // 1. pending actions

  for (auto& action : m_PendingActions) {
    if (HandleAction(action)) {
      return true;
    }
  }
  m_PendingActions.clear();

  if (m_Config->m_ExitOnStandby && !m_CurrentLobby && m_Games.empty() && !m_Net->m_HealthCheckInProgress) {
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

#ifdef WIN32
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

  // if hosting a lobby, accept new connections to its game server

  for (auto& pair : m_Net->m_GameServers) {
    uint16_t localPort = pair.first;
    CStreamIOSocket* socket = pair.second->Accept(static_cast<fd_set*>(&fd));
    if (socket) {
      if (m_Net->m_Config->m_ProxyReconnectEnabled) {
        CGameConnection* incomingConnection = new CGameConnection(m_GameProtocol, this, localPort, socket);
        //Print("Incoming connection from " + incomingConnection->GetIPString());
        m_Net->m_IncomingConnections[localPort].push_back(incomingConnection);
      } else if (!m_CurrentLobby || m_CurrentLobby->GetIsMirror() || localPort != m_CurrentLobby->GetHostPort()) {
        delete socket;
      } else {
        CGameConnection* incomingConnection = new CGameConnection(m_GameProtocol, this, localPort, socket);
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

      for (auto& bnet : m_Realms)
      {
        bnet->QueueGameUncreate();
        bnet->QueueEnterChat();
      }
    } else if (m_CurrentLobby)
      m_CurrentLobby->UpdatePost(&send_fd);
  }

  // update running games

  for (auto i = begin(m_Games); i != end(m_Games);)
  {
    if ((*i)->Update(&fd, &send_fd))
    {
      Print("[AURA] deleting game [" + (*i)->GetGameName() + "]");
      EventGameDeleted(*i);
      delete *i;
      i = m_Games.erase(i);
    }
    else
    {
      (*i)->UpdatePost(&send_fd);
      ++i;
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

    if (m_CurrentLobby->GetNumHumanPlayers() != 0)
      m_CurrentLobby->SendAllChat("Unable to create game on server [" + bnet->GetServer() + "]. Try another name");
    else if (m_CurrentLobby->GetCreatorServer()) {
      m_CurrentLobby->GetCreatorServer()->SendWhisper("Unable to create game on server [" + bnet->GetServer() + "]. Try another name", m_CurrentLobby->GetCreatorName());
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
  for (auto& bnet : m_Realms) {
    bnet->SendChatChannel("Game ended: " + game->GetDescription());

    if (bnet == game->GetCreatorServer())
      bnet->SendWhisper("Game ended: " + game->GetDescription(), game->GetCreatorName());
  }
}

bool CAura::ReloadConfigs()
{
  bool success = true;
  uint8_t WasVersion = m_GameVersion;
  filesystem::path WasCFGPath = m_Config->m_MapCFGPath;
  CConfig* CFG = new CConfig();
  CFG->Read("config.ini");
  if (!LoadConfigs(CFG)) {
    Print("[CONFIG] error - bot configuration invalid: not reloaded");
    success = false;
  }
  if (!LoadBNETs(CFG)) {
    Print("[CONFIG] error - realms misconfigured: not reloaded");
    success = false;
  }
  vector<string> invalidKeys = CFG->GetInvalidKeys(true);
  if (!invalidKeys.empty()) {
    Print("[CONFIG] warning - the following keys are invalid/misnamed: " + JoinVector(invalidKeys, false));
  }

  if (m_GameVersion != WasVersion) {
    Print("[AURA] Running game version 1." + to_string(m_GameVersion));
  }
  if (!m_ScriptsExtracted || m_GameVersion != WasVersion) {
    m_ScriptsExtracted = ExtractScripts() == 2;
    if (!m_ScriptsExtracted) {
      CopyScripts();
    }
  }
  if (WasCFGPath != m_Config->m_MapCFGPath) {
    CacheMapPresets();
  }
  m_Net->OnConfigReload();
  return success;
}

bool CAura::LoadConfigs(CConfig* CFG)
{
  CBotConfig* BotConfig = new CBotConfig(CFG);
  CNetConfig* NetConfig = new CNetConfig(CFG);
  CIRCConfig* IRCConfig = new CIRCConfig(CFG);
  CRealmConfig* RealmDefaultConfig = new CRealmConfig(CFG, NetConfig);
  CGameConfig* GameDefaultConfig = new CGameConfig(CFG);

  if (!CFG->GetSuccess()) {
    delete BotConfig;
    delete NetConfig;
    delete IRCConfig;
    delete RealmDefaultConfig;
    delete GameDefaultConfig;
    return false;
  }
  
  delete m_Config;
  delete m_Net->m_Config;
  delete m_IRC->m_Config;
  delete m_RealmDefaultConfig;
  delete m_GameDefaultConfig;

  m_Config = BotConfig;
  m_Net->m_Config = NetConfig;
  m_IRC->m_Config = IRCConfig;
  m_RealmDefaultConfig = RealmDefaultConfig;
  m_GameDefaultConfig = GameDefaultConfig;

  if (m_Config->m_Warcraft3Path.has_value()) {
    m_GameInstallPath = m_Config->m_Warcraft3Path.value();
  } else if (m_GameInstallPath.empty()) {
#ifdef WIN32
    HKEY hKey;
    DWORD dwType, dwSize;
    char szValue[1024];
    bool success = false;

    // Open the desired key
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Blizzard Entertainment\\Warcraft III", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
      // Query the value of the desired registry entry
      dwSize = sizeof(szValue);
      if (RegQueryValueExA(hKey, "InstallPath", nullptr, &dwType, (LPBYTE)szValue, &dwSize) == ERROR_SUCCESS) {
        if (dwType == REG_SZ && 0 < dwSize && dwSize < 1024) {
          success = true;
          string installPath(szValue, dwSize - 1);
          Print("[AURA] Using <game.install_path = " + installPath + ">");
          m_GameInstallPath = installPath;
        }
      }
      // Close the key
      RegCloseKey(hKey);
    }
    if (!success) {
      // Make sure this error message can be looked up.
      Print("[AURA] Registry error loading key 'Warcraft III\\InstallPath'");
    }
#endif
  }

  if (m_Config->m_War3Version.has_value()) {
    m_GameVersion = m_Config->m_War3Version.value();
  } else if (m_GameVersion == 0) {
    optional<uint8_t> AutoVersion = CBNCSUtilInterface::GetGameVersion(m_GameInstallPath);
    if (AutoVersion.has_value()) {
      m_GameVersion = AutoVersion.value();
    }
  }
  m_MaxSlots = m_GameVersion >= 29 ? MAX_SLOTS_MODERN : MAX_SLOTS_LEGACY;
  return true;
}

bool CAura::LoadCLI(const int argc, const char* argv[])
{
  // CLI overrides Config overrides Registry
  argh::parser cmdl({
    // TODO(IceSandslash): Switch to another CLI parser library that properly supports flags
    // Flags
    //"--version", "--help", "--stdpaths",
    //"--lan", "--bnet", "--exit", "--cache",
    //"--no-lan", "--no-bnet", "--no-exit", "--no-cache",

    // Parameters
    "--w3version", "--w3path", "--mapdir", "--cfgdir", "--filetype",
    "--lan-mode",
    "--exclude", "--mirror",
    "--observers", "--visibility", "--random-races", "--random-heroes", "--owner",
    "--exec-as", "--exec-auth", "exec-scope", "--exec"
  });

  cmdl.parse(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

  if (cmdl("--version")) {
    Print("--version");
    return true;
  }
  if (cmdl("help")) {
    Print("Usage:");
    Print("aura DotA.w3x \"come and play\"");
    return true;
  }

  optional<bool> bnet = m_Config->m_EnableBNET;
  bool noexit = !m_Config->m_ExitOnStandby;
  bool nolan = !m_GameDefaultConfig->m_UDPEnabled;
  bool nocache = !m_Config->m_EnableCFGCache;
  bool stdpaths = false;
  
  uint32_t War3Version = m_Config->m_War3Version.has_value() ? m_Config->m_War3Version.value() : 0;
  string War3Path = m_Config->m_Warcraft3Path.has_value() ? m_Config->m_Warcraft3Path.value().string() : string();
  string MapPath = m_Config->m_MapPath.string();
  string MapCFGPath = m_Config->m_MapCFGPath.string();
  string MapFileType = "map";
  string LANMode = m_Net->m_UDPMainServerEnabled ? "strict": "free";

  cmdl("w3version", War3Version) >> War3Version;
  cmdl("w3path", War3Path) >> War3Path;
  cmdl("mapdir", MapPath) >> MapPath;
  cmdl("cfgdir", MapCFGPath) >> MapCFGPath;
  cmdl("filetype", MapFileType) >> MapFileType;
  cmdl("lan-mode", LANMode) >> LANMode;

  cmdl("no-exit", noexit) >> noexit;
  //cmdl("no-bnet", nobnet) >> nobnet;
  cmdl("no-lan", nolan) >> nolan;
  cmdl("no-cache", nocache) >> nocache;
  cmdl("stdpaths") >> stdpaths;

  string rawFirstArgument = cmdl[1];
  if (!rawFirstArgument.empty()) {
    string gameName = cmdl[2].empty() ? "Join and play" : cmdl[2];
    string mirrorTarget, excludedRealm;
    cmdl("mirror") >> mirrorTarget;
    cmdl("exclude") >> excludedRealm;

    string targetMapPath = rawFirstArgument;
    if (stdpaths) {
      filesystem::path mapPath = targetMapPath;
      targetMapPath = filesystem::absolute(mapPath).lexically_normal().string();
    }

    vector<string> Action;
    if (!mirrorTarget.empty()) {
      Action.push_back("mirror");
      Action.push_back(targetMapPath);
      Action.push_back(gameName);
      Action.push_back(excludedRealm);

      Action.push_back(mirrorTarget);
      Action.push_back(stdpaths ? "1" : "0");
    } else if (MapFileType == "map" || MapFileType == "config") {
      string observerMode, visionMode, randomRaces, randomHeroes, owner;
      cmdl("observers") >> observerMode;
      cmdl("visibility") >> visionMode;
      cmdl("random-races") >> randomRaces;
      cmdl("random-heroes") >> randomHeroes;
      cmdl("owner") >> owner;

      if (MapFileType == "map") {
        Action.push_back("hostmap");
      } else {
        Action.push_back("hostcfg");
      }
      Action.push_back(targetMapPath);
      Action.push_back(cmdl[2]);
      Action.push_back(observerMode);

      Action.push_back(visionMode);
      Action.push_back(randomRaces);
      Action.push_back(randomHeroes);
      Action.push_back(owner);
      Action.push_back(excludedRealm);

      Action.push_back(stdpaths ? "1" : "0");
    } else {
      Print("Invalid game hosting parameters. Please see CLI.md");
      return true;
    }

    noexit = false;
    //nocache = cmdl("stdpaths");
    m_PendingActions.push_back(Action);
  }

  m_Config->m_War3Version = War3Version;
  m_Config->m_Warcraft3Path = War3Path;
  m_Config->m_MapPath = MapPath;
  m_Config->m_MapCFGPath = MapCFGPath;

  m_Config->m_ExitOnStandby = !noexit;
  //m_Config->m_EnableBNET = !nobnet;
  m_GameDefaultConfig->m_UDPEnabled = !nolan;
  m_Config->m_EnableCFGCache = !nocache;
  m_Config->m_StrictPaths = stdpaths;

  if (LANMode == "strict" || LANMode == "lax") {
    m_Net->m_UDPMainServerEnabled = true;
  } else if (LANMode == "free") {
    m_Net->m_UDPMainServerEnabled = false;
  } else {
    Print("Bad UDPMode: " + LANMode);
  }

  string execCommand, execAs, execScope, execAuth;
  cmdl("exec", execCommand) >> execCommand;
  cmdl("exec-as", execAs) >> execAs;
  cmdl("exec-scope", execScope) >> execScope;
  cmdl("exec-auth", execAuth) >> execAuth;

  if (!execCommand.empty() && !execAs.empty() && !execAuth.empty()) {
    vector<string> Action;
    Action.push_back("exec");
    Action.push_back(execCommand);
    Action.push_back(execAs);
    Action.push_back(execScope);
    Action.push_back(execAuth);
    m_PendingActions.push_back(Action);
  }

  return false;
}

uint8_t CAura::ExtractScripts()
{
  uint8_t FilesExtracted = 0;
  void*        MPQ;
  const filesystem::path MPQFilePath = [&]() {
    if (m_GameVersion >= 28)
      return m_GameInstallPath / filesystem::path("War3.mpq");
    else
      return m_GameInstallPath / filesystem::path("War3Patch.mpq");
  }();

#ifdef WIN32
  if (SFileOpenArchive(MPQFilePath.wstring().c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | STREAM_FLAG_READ_ONLY, &MPQ))
#else
  if (SFileOpenArchive(MPQFilePath.c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | STREAM_FLAG_READ_ONLY, &MPQ))
#endif
  {
    Print("[AURA] loading MPQ file [" + MPQFilePath.string() + "]");
    void* SubFile;

    // common.j

    if (SFileOpenFileEx(MPQ, R"(Scripts\common.j)", 0, &SubFile))
    {
      const uint32_t FileLength = SFileGetFileSize(SubFile, nullptr);

      if (FileLength > 0 && FileLength != 0xFFFFFFFF)
      {
        auto  SubFileData = new int8_t[FileLength];
        DWORD BytesRead   = 0;

        if (SFileReadFile(SubFile, SubFileData, FileLength, &BytesRead, nullptr))
        {
          filesystem::path commonPath = m_Config->m_MapCFGPath / filesystem::path("common-" + to_string(m_GameVersion) + ".j");
          if (FileWrite(commonPath, reinterpret_cast<uint8_t*>(SubFileData), BytesRead)) {
            Print(R"([AURA] extracted Scripts\common.j to [)" + commonPath.string() + "]");
            ++FilesExtracted;
          } else {
            Print(R"([AURA] warning - unable to save extracted Scripts\common.j to [)" + commonPath.string() + "]");
          }
        }
        else
          Print(R"([AURA] warning - unable to extract Scripts\common.j from MPQ file)");

        delete[] SubFileData;
      }

      SFileCloseFile(SubFile);
    }
    else
      Print(R"([AURA] couldn't find Scripts\common.j in MPQ file)");

    // blizzard.j

    if (SFileOpenFileEx(MPQ, R"(Scripts\blizzard.j)", 0, &SubFile))
    {
      const uint32_t FileLength = SFileGetFileSize(SubFile, nullptr);

      if (FileLength > 0 && FileLength != 0xFFFFFFFF)
      {
        auto  SubFileData = new int8_t[FileLength];
        DWORD BytesRead   = 0;

        if (SFileReadFile(SubFile, SubFileData, FileLength, &BytesRead, nullptr))
        {
          filesystem::path blizzardPath = m_Config->m_MapCFGPath / filesystem::path("blizzard-" + to_string(m_GameVersion) + ".j");
          if (FileWrite(blizzardPath, reinterpret_cast<uint8_t*>(SubFileData), BytesRead)) {
            Print(R"([AURA] extracted Scripts\blizzard.j to [)" + blizzardPath.string() + "]");
            ++FilesExtracted;
          } else {
            Print(R"([AURA] warning - unable to save extracted Scripts\blizzard.j to [)" + blizzardPath.string() + "]");
          }
        }
        else
          Print(R"([AURA] warning - unable to extract Scripts\blizzard.j from MPQ file)");

        delete[] SubFileData;
      }

      SFileCloseFile(SubFile);
    }
    else
      Print(R"([AURA] couldn't find Scripts\blizzard.j in MPQ file)");

    SFileCloseArchive(MPQ);
  }
  else
  {
#ifdef WIN32
    uint32_t ErrorCode = (uint32_t)GetLastError();
    string ErrorCodeString = (
      ErrorCode == 2 ? "Config error: game.install_path is not the WC3 directory" : (
      (ErrorCode == 3 || ErrorCode == 15) ? "Config error: game.install_path is not a valid directory" : (
      (ErrorCode == 32 || ErrorCode == 33) ? "File is currently opened by another process." : (
      "Error code " + to_string(ErrorCode)
      )))
    );
#else
    int32_t ErrorCode = static_cast<int32_t>(GetLastError());
    string ErrorCodeString = "Error code " + to_string(ErrorCode);
#endif
    Print("[AURA] warning - unable to load MPQ file [" + MPQFilePath.string() + "] - " + ErrorCodeString);
  }

  return FilesExtracted;
}

void CAura::LoadIPToCountryData()
{
  ifstream in;
  filesystem::path GeoFilePath = GetExeDirectory() / filesystem::path("ip-to-country.csv");
  in.open(GeoFilePath.string(), ios::in);

  if (in.fail()) {
    Print("[AURA] warning - unable to read file [ip-to-country.csv], geolocalization data not loaded");
  } else {

    // the begin and commit statements are optimizations
    // we're about to insert ~4 MB of data into the database so if we allow the database to treat each insert as a transaction it will take a LONG time

    if (!m_DB->Begin())
      Print("[AURA] warning - failed to begin database transaction, geolocalization data not loaded");
    else
    {
      string    Line, Skip, IP1, IP2, Country;
      CSVParser parser;

      // get length of file for the progress meter

      in.seekg(0, ios::end);
      const uint32_t FileLength = static_cast<uint32_t>(in.tellg());
      in.seekg(0, ios::beg);

      while (!in.eof())
      {
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

      if (!m_DB->Commit())
        Print("[AURA] warning - failed to commit database transaction, geolocalization data not loaded");
    }

    in.close();
  }
}

void CAura::CacheMapPresets()
{
  // Preload map configs
  m_CachedMaps.clear();
  const vector<string> MapConfigFiles = ConfigFilesMatch("");
  for (const auto& MapConfigFile : MapConfigFiles) {
    string MapLocalPath = CConfig::ReadString(m_Config->m_MapCFGPath / filesystem::path(MapConfigFile), "map_localpath");
    if (!MapLocalPath.empty()) {
      m_CachedMaps[MapLocalPath] = MapConfigFile;
    }
  }
}

bool CAura::CreateGame(CMap* map, uint8_t gameDisplay, string gameName, string ownerName, string ownerServer, string creatorName, CRealm* creatorServer, CCommandContext* ctx)
{
  if (!m_Config->m_Enabled) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. The bot is disabled", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (gameName.size() > m_MaxGameNameSize) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. The game name is too long (max " + to_string(m_MaxGameNameSize) + " characters)", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (!map->GetValid()) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. The currently loaded map config file is invalid", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (m_CurrentLobby) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. Another game lobby [" + m_CurrentLobby->GetDescription() + "] is currently hosted.", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (m_Games.size() >= m_Config->m_MaxGames) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. There are too many active games already.", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  Print("[AURA] creating game [" + gameName + "]");

  uint16_t HostPort = NextHostPort();
  m_CurrentLobby = new CGame(this, map, HostPort, gameDisplay, gameName, ownerName, ownerServer, creatorName, creatorServer);
  if (m_CurrentLobby->GetExiting()) {
    delete m_CurrentLobby;
    m_CurrentLobby = nullptr;
    return false;
  }

  if (m_CurrentLobby->GetUDPEnabled())
    m_CurrentLobby->SendGameDiscoveryCreate();

  for (auto& bnet : m_Realms) {
    string AnnounceText = (
      string("[1.") + to_string(m_GameVersion) + ".x] Hosting " + (gameDisplay == GAME_PRIVATE ? "private" : "public") + " game of " + GetFileName(map->GetMapLocalPath()) +
      ". (Started by " + ownerName + ": \"" + bnet->GetPrefixedGameName(gameName) + "\")"
    );
    if (gameDisplay == GAME_PRIVATE) {
      ctx->SendReply(AnnounceText);
    } else if (bnet->GetAnnounceHostToChat()) {
      bnet->SendChatChannel(AnnounceText);
    } else if (bnet == creatorServer) {
      ctx->SendReply(AnnounceText);
    }
    // QueueGameRefresh at QueueGameCreate handles prefix
    bnet->QueueGameCreate(gameDisplay, gameName, map, m_CurrentLobby->GetHostCounter(), m_CurrentLobby->GetHostPort());

    // hold friends and/or clan members

    bnet->HoldFriends(m_CurrentLobby);
    bnet->HoldClan(m_CurrentLobby);

    // if we're creating a private game we don't need to send any game refresh messages so we can rejoin the chat immediately
    // unfortunately this doesn't work on PVPGN servers because they consider an enterchat message to be a gameuncreate message when in a game
    // so don't rejoin the chat if we're using PVPGN

    if (gameDisplay == GAME_PRIVATE && !bnet->GetPvPGN()) {
      bnet->QueueEnterChat();
    }
  }

  return true;
}

bool CAura::CreateMirror(CMap* map, uint8_t gameDisplay, string gameName, vector<uint8_t> gameAddress, uint16_t gamePort, uint32_t gameHostCounter, uint32_t gameEntryKey, string excludedServer, string creatorName, CRealm* creatorServer, CCommandContext* ctx)
{
  if (!m_Config->m_Enabled) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. The bot is disabled", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (gameName.size() > m_MaxGameNameSize) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. The game name is too long (max " + to_string(m_MaxGameNameSize) + " characters)", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (!map->GetValid()) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. The currently loaded map config file is invalid", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (m_CurrentLobby) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. Another game lobby [" + m_CurrentLobby->GetDescription() + "] is currently hosted.", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  if (m_Games.size() >= m_Config->m_MaxGames) {
    ctx->ErrorReply("Unable to create game [" + gameName + "]. There are too many active games already.", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return false;
  }

  Print("[AURA] mirroring game [" + gameName + "]");

  m_CurrentLobby = new CGame(this, map, gameDisplay, gameName, gameAddress, gamePort, gameHostCounter, gameEntryKey, excludedServer);

  for (auto& bnet : m_Realms) {
    if (bnet->GetInputID() == excludedServer || bnet->GetIsMirror())
      continue;

    string AnnounceText = (
      string("[1.") + to_string(m_GameVersion) + ".x] Mirroring " + (gameDisplay == GAME_PRIVATE ? "private" : "public") + " game of " + GetFileName(map->GetMapLocalPath()) +
      ": \"" + bnet->GetPrefixedGameName(gameName) + "\")"
    );
    if (gameDisplay == GAME_PRIVATE) {
      ctx->SendReply(AnnounceText);
    } else if (bnet->GetAnnounceHostToChat()) {
      bnet->SendChatChannel(AnnounceText);
    } else if (bnet == creatorServer) {
      ctx->SendReply(AnnounceText);
    }
    // QueueGameRefresh at QueueGameCreate handles prefix
    bnet->QueueGameMirror(gameDisplay, gameName, map, m_CurrentLobby->GetHostCounter(), m_CurrentLobby->GetPublicHostPort());

    // if we're creating a private game we don't need to send any game refresh messages so we can rejoin the chat immediately
    // unfortunately this doesn't work on PVPGN servers because they consider an enterchat message to be a gameuncreate message when in a game
    // so don't rejoin the chat if we're using PVPGN

    if (gameDisplay == GAME_PRIVATE && !bnet->GetPvPGN()) {
      bnet->QueueEnterChat();
    }
  }

  return true;
}

vector<string> CAura::MapFilesMatch(string rawPattern)
{
  if (IsValidMapName(rawPattern) && FileExists(m_Config->m_MapPath / filesystem::path(rawPattern))) {
    return vector<string>(1, rawPattern);
  }

  string pattern = RemoveNonAlphanumeric(rawPattern);
  transform(begin(pattern), end(pattern), begin(pattern), ::tolower);

  auto TFTMaps = FilesMatch(m_Config->m_MapPath, ".w3x");
  auto ROCMaps = FilesMatch(m_Config->m_MapPath, ".w3m");

  unordered_set<string> MapSet(TFTMaps.begin(), TFTMaps.end());
  MapSet.insert(ROCMaps.begin(), ROCMaps.end());
  for (const auto& pair : m_CachedMaps) {
    MapSet.insert(pair.first);
  }

  vector<string> Matches;

  for (auto& mapName : MapSet)
  {
    string cmpName = RemoveNonAlphanumeric(mapName);
    transform(begin(cmpName), end(cmpName), begin(cmpName), ::tolower);

    if (cmpName.find(pattern) != string::npos) {
      Matches.push_back(mapName);
      if (Matches.size() >= 10) {
        break;
      }
    }
  }

  if (Matches.size() > 0) {
    return Matches;
  }

  if (pattern.find("w3x") == string::npos && pattern.find("w3m") == string::npos) {
    pattern.append("w3x");
  }

  string::size_type maxDistance = 10;
  if (pattern.size() < maxDistance) {
    maxDistance = pattern.size() / 2;
  }

  vector<pair<string, int>> distances;
  vector<pair<string, int>>::size_type i;
  vector<pair<string, int>>::size_type i_max;

  for (auto& mapName : MapSet) {
    string cmpName = RemoveNonAlphanumeric(mapName);
    transform(begin(cmpName), end(cmpName), begin(cmpName), ::tolower);
    if ((pattern.size() <= cmpName.size() + maxDistance) && (cmpName.size() <= maxDistance + pattern.size())) {
      string::size_type distance = GetLevenshteinDistance(pattern, cmpName); // source to target
      if (distance <= maxDistance) {
        distances.emplace_back(mapName, distance);
      }
    }
  }

  partial_sort(
    distances.begin(),
    distances.begin() + min(5, static_cast<int>(distances.size())),
    distances.end(),
    [](const pair<string, int>& a, const pair<string, int>& b) {
        return a.second < b.second;
    }
  );

  string PrioritizedString;
  if (pattern.find("evergreen") != string::npos) {
    PrioritizedString = "evrgrn";
  }

  i_max = distances.size();
  if (i_max > 5)
    i_max = 5;

  for (i = 0; i < i_max; ++i) {
    if (!PrioritizedString.empty() && distances[i].first.find(PrioritizedString) != string::npos) {
      PrioritizedString = "";
      Matches.insert(Matches.begin(), distances[i].first);
    } else {
      Matches.push_back(distances[i].first);
    }
  }

  return Matches;
}

vector<string> CAura::ConfigFilesMatch(string pattern)
{
  transform(begin(pattern), end(pattern), begin(pattern), ::tolower);

  vector<string> ConfigList = FilesMatch(m_Config->m_MapCFGPath, ".cfg");

  vector<string> Matches;

  for (auto& cfgName : ConfigList)
  {
    string lowerCfgName(cfgName);
    transform(begin(lowerCfgName), end(lowerCfgName), begin(lowerCfgName), ::tolower);

    if (lowerCfgName.find(pattern) != string::npos) {
      Matches.push_back(cfgName);
    }
  }

  return Matches;
}

uint16_t CAura::NextHostPort()
{
  ++m_LastHostPort;
  if (m_LastHostPort > m_Net->m_Config->m_MaxHostPort || m_LastHostPort < m_Net->m_Config->m_MinHostPort) {
    m_LastHostPort = m_Net->m_Config->m_MinHostPort;
  }
  return m_LastHostPort;
}

uint32_t CAura::NextHostCounter()
{
  m_HostCounter = (m_HostCounter + 1) & 0x00FFFFFF;
  if (m_HostCounter < m_Config->m_MinHostCounter) {
    m_HostCounter = m_Config->m_MinHostCounter;
  }
  return m_HostCounter;
}

bool CAura::IsIgnoredNotifyPlayer(string playerName)
{
  return m_GameDefaultConfig->m_IgnoredNotifyJoinPlayers.find(playerName) != m_GameDefaultConfig->m_IgnoredNotifyJoinPlayers.end();
}

bool CAura::IsIgnoredDatagramSource(string sourceIp)
{
  string element(sourceIp);
  return m_Net->m_Config->m_UDPBlockedIPs.find(element) != m_Net->m_Config->m_UDPBlockedIPs.end();
}