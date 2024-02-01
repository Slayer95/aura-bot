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

#include "aura.h"
#include "crc32.h"
#include "sha1.h"
#include "csvparser.h"
#include "config.h"
#include "config_bot.h"
#include "config_bnet.h"
#include "config_game.h"
#include "config_irc.h"
#include "socket.h"
#include "auradb.h"
#include "bnet.h"
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

#define __STORMLIB_SELF__
#include <StormLib.h>

#ifdef WIN32
#define NOMINMAX
#include <ws2tcpip.h>
#include <winsock.h>
#include <process.h>
#endif

#define VERSION "1.34.dev"

using namespace std;

#undef FD_SETSIZE
#define FD_SETSIZE 512

static CAura* gAura    = nullptr;
bool          gRestart = false;

void Print2(const string& message)
{
  Print(message);

  if (gAura->m_IRC)
    gAura->m_IRC->SendMessageIRC(message, string());
}

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
  if (!CFG.Read("aura.cfg")) {
    int FileSize = 0;
    string CFGExample = FileRead("aura-example.cfg", &FileSize);
    vector<uint8_t> CFGCopy(CFGExample.begin(), CFGExample.end());
    Print("[AURA] copying aura-example.cfg to aura.cfg...");
    FileWrite("aura.cfg", CFGCopy.data(), CFGCopy.size());
    if (!CFG.Read("aura.cfg")) {
      Print("[AURA] error initializing aura.cfg");
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
  : m_IRC(nullptr),
    m_UDPServer(nullptr),
    m_UDPSocket(new CUDPSocket()),
    m_GameProtocol(nullptr),
    m_GPSProtocol(new CGPSProtocol()),
    m_CRC(new CCRC32()),
    m_SHA(new CSHA1()),
    m_CurrentLobby(nullptr),
    m_DB(new CAuraDB(CFG)),
    m_Map(nullptr),
    m_Config(nullptr),
    m_BNETDefaultConfig(nullptr),
    m_GameDefaultConfig(nullptr),
    m_Version(VERSION),
    m_HostCounter(0),
    m_HostPort(0),
    m_UDPServerEnabled(false),
    m_MaxGameNameSize(31),
    m_GameRangerLocalPort(0),
    m_GameRangerLocalAddress("255.255.255.255"),
    m_GameRangerRemotePort(0),
    m_GameRangerRemoteAddress({0xFF, 0xFF, 0xFF, 0xFF}),
    m_Exiting(false),
    m_Ready(true),
    m_ScriptsExtracted(false)
{
  Print("[AURA] Aura++ version " + m_Version);

  // get the general configuration variables
  m_UDPServerEnabled = CFG->GetBool("udp_enableserver", false);
  if (m_UDPServerEnabled) {
    m_UDPServer = new CUDPServer();
    m_UDPServer->SetBroadcastTarget(CFG->GetString("udp_broadcasttarget", string()));
    m_UDPServer->SetDontRoute(CFG->GetBool("udp_dontroute", false));
    if (!m_UDPServer->Listen(CFG->GetString("bot_bindaddress", "0.0.0.0"), 6112, false)) {
      Print("[AURA] waiting for active instances of Warcraft to close... Please release port 6112.");
      std::this_thread::sleep_for(std::chrono::milliseconds(5000));
      if (!m_UDPServer->Listen(CFG->GetString("bot_bindaddress", "0.0.0.0"), 6112, true)) {
        Print("[AURA] error - close active instances of Warcraft, and/or pause LANViewer to initialize Aura.");
        m_Ready = false;
        return;
      }
    }
  }

  m_UDPSocket->SetBroadcastTarget(CFG->GetString("udp_broadcasttarget", string()));
  m_UDPSocket->SetDontRoute(CFG->GetBool("udp_dontroute", false));

  m_GameProtocol = new CGameProtocol(this);
  m_CRC->Initialize();

  LoadConfigs(CFG);
  if (LoadCLI(argc, argv)) {
    m_Ready = false;
    return;
  }

  if (m_Config->m_EnableLANBalancer) {
    Print("[AURA] Broadcasting games port " + std::to_string(m_Config->m_LANHostPort) + " over LAN");
  }

  if (m_Config->m_EnableBNET) {
    LoadBNETs(CFG);
  }
  LoadIRC(CFG);

  if (m_BNETs.empty()) {
    Print("[AURA] warning - no battle.net connections found in aura.cfg");

    if (!m_IRC) {
      Print("[AURA] warning - no irc connection found in aura.cfg");
      Print("[AURA] error - no connections found in aura.cfg");
      m_Ready = false;
      return;
    }
  } else if (!m_IRC) {
    Print("[AURA] warning - no irc connection specified");
  }

  // extract common.j and blizzard.j from War3Patch.mpq or War3.mpq (depending on version) if we can
  // these two files are necessary for calculating "map_crc" when loading maps so we make sure they are available
  // see CMap :: Load for more information
  m_ScriptsExtracted = ExtractScripts() == 2;
  if (!m_ScriptsExtracted && (!FileExists(m_Config->m_MapCFGPath + "common.j") || !FileExists(m_Config->m_MapCFGPath + "blizzard.j"))) {
    Print("[AURA] error - close active instances of Warcraft and/or WorldEdit to initialize Aura.");
    m_Ready = false;
    return;
  }

  // load the iptocountry data
  LoadIPToCountryData();

  // Read CFG->Map links and cache the reverse.
  CacheMapPresets();
}

void CAura::LoadBNETs(CConfig* CFG)
{
  // load the battle.net connections
  // we're just loading the config data and creating the CBNET classes here, the connections are established later (in the Update function)

  size_t LongestGamePrefixSize = 0;
  bool IsReload = !m_BNETs.empty();
  for (uint8_t i = 1; i <= 240; ++i) { // uint8_t wraps around and loops forever
    bool DoResetConnection = false;
    CBNETConfig* ThisConfig = new CBNETConfig(CFG, m_BNETDefaultConfig, i);
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
    if (IsReload && i < m_BNETs.size()) {
      if (ThisConfig->m_HostName != m_BNETs[i]->GetServer()) {
        Print("[AURA] ignoring update for battle.net config #" + to_string(i) + "; hostname mismatch - expected [" + m_BNETs[i]->GetServer() + "], but got " + ThisConfig->m_HostName);
        delete ThisConfig;
        continue;
      }
      DoResetConnection = m_BNETs[i]->GetLoginName() != ThisConfig->m_UserName || m_BNETs[i]->GetEnabled() && !ThisConfig->m_Enabled;
      m_BNETs[i]->SetConfig(ThisConfig);
    }

    if (ThisConfig->m_CDKeyROC.length() != 26 || ThisConfig->m_CDKeyTFT.length() != 26)
      Print("[BNET: " + ThisConfig->m_UniqueName + "] warning - your CD keys are not 26 characters long - probably invalid");

    // TODO(IceSandslash): Warn on password reuse

    if (ThisConfig->m_GamePrefix.length() > LongestGamePrefixSize)
      LongestGamePrefixSize = ThisConfig->m_GamePrefix.length();

    for (auto RootAdmin : ThisConfig->m_RootAdmins) {
      m_DB->RootAdminAdd(ThisConfig->m_DataBaseID, RootAdmin);
    }

    if (IsReload && i < m_BNETs.size()) {
      Print("[AURA] server #" + to_string(i) + " reloaded [" + ThisConfig->m_UniqueName + "]");
      if (DoResetConnection) {
        m_BNETs[i]->ResetConnection(false);
      }
    } else {
      Print("[AURA] server #" + to_string(i) + " found [" + ThisConfig->m_UniqueName + "]");
      m_BNETs.push_back(new CBNET(this, ThisConfig));
    }
  }

  m_MaxGameNameSize = 31 - LongestGamePrefixSize;
}

void CAura::LoadIRC(CConfig* CFG)
{
  delete m_IRC;
  m_IRC = nullptr;

  CIRCConfig* IRCConfig = new CIRCConfig(CFG);
  if (IRCConfig->m_Enabled && (IRCConfig->m_HostName.empty() || IRCConfig->m_NickName.empty() || IRCConfig->m_Port == 0)) {
    Print("[AURA] config error - irc connection misconfigured");
  } else {
    m_IRC = new CIRC(this, IRCConfig);
  }
}

CAura::~CAura()
{
  delete m_Config;
  delete m_BNETDefaultConfig;
  delete m_GameDefaultConfig;
  delete m_UDPServer;
  delete m_UDPSocket;
  delete m_GameProtocol;
  delete m_GPSProtocol;
  delete m_CRC;
  delete m_SHA;
  delete m_Map;

  for (auto& bnet : m_BNETs)
    delete bnet;

  delete m_CurrentLobby;

  for (auto& game : m_Games)
    delete game;

  delete m_DB;
  delete m_IRC;
}

CTCPServer* CAura::GetGameServer(uint16_t port, string& name)
{
  auto it = m_GameServers.find(port);
  if (it != m_GameServers.end()) {
    Print("[GAME] " + name + " Assigned to port " + to_string(port));
    return it->second;
  }
  m_GameServers[port] = new CTCPServer();
  std::vector<CPotentialPlayer*> IncomingConnections;
  m_IncomingConnections[port] = IncomingConnections;
  if (!m_GameServers[port]->Listen(m_Config->m_BindAddress, port, false)) {
    Print2("[GAME] " + name + " Error listening on port " + to_string(port));
    return nullptr;
  }
  Print("[GAME] " + name + " Listening on port " + to_string(port));
  return m_GameServers[port];
}

pair<uint8_t, string> CAura::LoadMap(const string& user, const string& mapInput, const string& observersInput, const string& visibilityInput, const string& randomHeroInput, const bool& gonnaBeLucky)
{
  string Map = mapInput;
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
  string ResolvedCFGPath;
  string ResolvedCFGName;
  string File;
  bool ResolvedCFGExists = false;

  pair<string, string> NamespacedMap = ParseMapId(Map);
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
    ResolvedCFGPath = this->m_Config->m_MapCFGPath + ResolvedCFGName;
    ResolvedCFGExists = MapCFG.Read(ResolvedCFGPath);
  }

  if (NamespacedMap.first != "local" && !ResolvedCFGExists) {
    // Map config not locally available. Download the map so we can calculate it.
    // But only if we are allowed to.
    if (!this->m_Config->m_AllowDownloads) {
      return make_pair(0x21, string("Map downloads are not allowed."));
    }
    if (!this->m_Games.empty() && this->m_Config->m_HasBufferBloat) {
      return make_pair(0x40, string("Currently hosting ") + to_string(this->m_Games.size()) + " game(s). Downloads are disabled to prevent high latency.");
    }

    uint8_t DownloadResult = DownloadRemoteMap(NamespacedMap.first, MapSiteUri, MapDownloadUri, Map, m_Config->m_MapPath, m_BusyMaps);

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
    //QueueChatCommand("Downloaded <" + MapSiteUri + "> as [" + Map + "]", User, Whisper, m_IRC);
    Print("[AURA] Downloaded <" + MapSiteUri + "> as [" + Map + "]");
  }

  if (NamespacedMap.first == "local") {
    Map = NamespacedMap.second;
  }

  if (ResolvedCFGExists) {
    // Exact resolution by map identifier.
    //QueueChatCommand("Loading config file [" + ResolvedCFGPath + "]", User, Whisper, m_IRC);
  } else {
    // Fuzzy resolution by map name.
    const vector<string> LocalMatches = this->MapFilesMatch(Map);
    if (LocalMatches.empty()) {
      string Suggestions = GetEpicWarSuggestions(Map, 5);
      if (Suggestions.empty()) {
        return make_pair(0x61, string("No matching map found."));
      } else {
        return make_pair(0x62, string("Suggestions: ") + Suggestions);
      }
    }
    if (LocalMatches.size() > 1 && !gonnaBeLucky) {
      string Suggestions = GetEpicWarSuggestions(Map, 5);
      string Results = JoinVector(LocalMatches, !Suggestions.empty()) + Suggestions;
      return make_pair(0x63, string("Suggestions: ") + Results);
    }
    File = LocalMatches.at(0);
    if (this->m_CachedMaps.find(File) != this->m_CachedMaps.end()) {
      ResolvedCFGName = this->m_CachedMaps[File];
      ResolvedCFGPath = this->m_Config->m_MapCFGPath + ResolvedCFGName;
      ResolvedCFGExists = MapCFG.Read(ResolvedCFGPath);
      if (ResolvedCFGExists) {
        //QueueChatCommand("Loading config file [" + ResolvedCFGPath + "]", User, Whisper, m_IRC);
      } else {
        // Oops! CFG file is gone!
        this->m_CachedMaps.erase(File);
      }
    }
    if (!ResolvedCFGExists) {
      // CFG nowhere to be found.
      Print("[AURA] Loading map file [" + File + "]...");

      MapCFG.Set("cfg_partial", "1");
      if (File.length() > 6 && File[File.length() - 6] == '~' && isdigit(File[File.length() - 5])) {
        // IDK if this path is okay. Let's give it a try.
        MapCFG.Set("map_path", R"(Maps\Download\)" + File.substr(0, File.length() - 6) + File.substr(File.length() - 4, File.length()));
      } else {
        MapCFG.Set("map_path", R"(Maps\Download\)" + File);
      }
      MapCFG.Set("map_localpath", File);
      MapCFG.Set("map_site", MapSiteUri);
      MapCFG.Set("map_url", MapDownloadUri);
      if (NamespacedMap.first != "local")
        MapCFG.Set("downloaded_by", user);

      if (File.find("DotA") != string::npos)
        MapCFG.Set("map_type", "dota");

      if (ResolvedCFGName.empty()) {
        ResolvedCFGName = "local-" + File + ".cfg";
        ResolvedCFGPath = this->m_Config->m_MapCFGPath + ResolvedCFGName;
      }

      vector<uint8_t> OutputBytes = MapCFG.Export();
      FileWrite(ResolvedCFGPath, OutputBytes.data(), OutputBytes.size());
      this->m_CachedMaps[File] = ResolvedCFGName;
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
    return make_pair(0x70, string("Error while loading map: [" + std::string(ErrorMessage) + "]"));
  }

  if (!ResolvedCFGExists) {
    if (MapCFG.GetInt("cfg_partial", 0) == 0) {
      // Download and parse successful
      vector<uint8_t> OutputBytes = MapCFG.Export();
      if (FileWrite(ResolvedCFGPath, OutputBytes.data(), OutputBytes.size())) {
        Print("[AURA] Cached map config for [" + File + "] as [" + ResolvedCFGPath + "]");
      }
    }
    //QueueChatCommand("Map file loaded OK [" + File + "]", User, Whisper, m_IRC);
    Print("[AURA] Map file loaded OK [" + File + "]");
    return make_pair(0, string("Map file loaded OK [") + File + "]");
  } else {
    return make_pair(0, string("Config file loaded OK [") + ResolvedCFGPath + "]");
  }
}

bool CAura::Update()
{
  // 1. pending actions
  for (auto& action : m_PendingActions) {
    if (action[0] == "exec") {
      Print("[AURA] Exec cli unsupported yet");
      return true;
    } else if (action[0] == "host") {
      string DownloadedBy;
      string MapInput = action[1];
      string GameName = action[2];
      string ObsInput = action[3];
      string VisInput = action[4];
      string RHInput = action[5];
      string OwnerName = action[6];
      string ExcludedRealm = action[7]; // TODO?

      string OwnerRealmName;
      CBNET* OwnerRealm = nullptr;

      size_t OwnerRealmIndex = OwnerName.find('@');
      if (OwnerRealmIndex != string::npos) {
        OwnerName = OwnerName.substr(0, OwnerRealmIndex);
        OwnerRealmName = OwnerName.substr(OwnerRealmIndex + 1, OwnerName.length());
        transform(begin(OwnerRealmName), end(OwnerRealmName), begin(OwnerRealmName), ::tolower);
        for (auto& bnet : m_BNETs) {
          if (bnet->GetInputID() == OwnerRealmName) {
            if (!bnet->GetIsMirror()) {
              OwnerRealm = bnet;
              break;
            } else if (OwnerRealm == nullptr) {
              OwnerRealm = bnet;
            }
          }
        }
      }

      pair<uint8_t, string> LoadResult = LoadMap(OwnerName, MapInput, ObsInput, VisInput, RHInput, true /* I'm gonna be lucky */);
      if (!LoadResult.second.empty()) {
        Print("[AURA] " + LoadResult.second);
      }

      if (LoadResult.first != 0) {
        return true; // Exit
      }

      if (GameName.empty())
        GameName = "gogogo";

      bool success = CreateGame(
        m_Map, GAME_PUBLIC, GameName,
        OwnerName, OwnerRealm ? OwnerRealm->GetServer() : "",
        OwnerName, OwnerRealm, false
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
  }

  m_PendingActions.clear();

  if (m_Config->m_ExitOnStandby && !m_CurrentLobby && m_Games.empty()) {
    return true;
  }

  uint32_t NumFDs = 0;

  // take every socket we own and throw it in one giant select statement so we can block on all sockets

  int32_t nfds = 0;
  fd_set  fd, send_fd;
  FD_ZERO(&fd);
  FD_ZERO(&send_fd);

  // 2. all running game servers

  for (auto& pair : m_GameServers) {
    pair.second->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
    ++NumFDs;
  }

  // 3. all unassigned incoming TCP connections

  for (auto& pair : m_IncomingConnections) {
    // std::pair<uint16_t, std::vector<CPotentialPlayer*>>
    for (auto& potential : pair.second) {
      if (potential->GetSocket()) {
        potential->GetSocket()->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
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

  for (auto& bnet : m_BNETs)
    NumFDs += bnet->SetFD(&fd, &send_fd, &nfds);

  // 7. UDP server
  if (m_UDPServerEnabled) {
    m_UDPServer->SetFD(&fd, &send_fd, &nfds);
    ++NumFDs;
  }

  // 8. irc socket

  if (m_IRC)
    NumFDs += m_IRC->SetFD(&fd, &send_fd, &nfds);

  if (NumFDs > 25) {
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

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  bool Exit = false;

  // if hosting a lobby, accept new connections to its game server

  for (auto& pair : m_GameServers) {
    uint16_t ConnectPort = pair.first;
    CTCPSocket* NewSocket = pair.second->Accept(static_cast<fd_set*>(&fd));
    if (NewSocket) {
      if (m_Config->m_ProxyReconnectEnabled) {
        CPotentialPlayer* IncomingConnection = new CPotentialPlayer(m_GameProtocol, this, ConnectPort, NewSocket);
        m_IncomingConnections[ConnectPort].push_back(IncomingConnection);
      } else if (!m_CurrentLobby || m_CurrentLobby->GetIsMirror() || ConnectPort != m_CurrentLobby->GetHostPort()) {
        delete NewSocket;
      } else {
        CPotentialPlayer* IncomingConnection = new CPotentialPlayer(m_GameProtocol, this, ConnectPort, NewSocket);
        m_IncomingConnections[ConnectPort].push_back(IncomingConnection);
      }
    }

    if (pair.second->HasError())
      Exit = true;
  }

  // update unassigned incoming connections

  uint16_t IncomingConnectionsMax = 255;
  uint16_t IncomingConnectionsCount = 0;
  for (auto& pair : m_IncomingConnections) {
    for (auto i = begin(pair.second); i != end(pair.second);) {
      // *i is a pointer to a CPotentialPlayer
      uint8_t result = (*i)->Update(&fd, &send_fd);
      if (result == 0) {
        ++i;
      } else {
        if (result == 1) {
          // flush the socket (e.g. in case a rejection message is queued)
          if ((*i)->GetSocket())
            (*i)->GetSocket()->DoSend(static_cast<fd_set*>(&send_fd));

          delete *i;
        }

        i = pair.second.erase(i);
      }
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
    if (m_CurrentLobby->Update(&fd, &send_fd))
    {
      Print2("[AURA] deleting current game [" + m_CurrentLobby->GetGameName() + "]");
      if (m_CurrentLobby->GetLANEnabled())
        m_CurrentLobby->LANBroadcastGameDecreate();
      delete m_CurrentLobby;
      m_CurrentLobby = nullptr;

      for (auto& bnet : m_BNETs)
      {
        bnet->QueueGameUncreate();
        bnet->QueueEnterChat();
      }
    }
    else if (m_CurrentLobby)
      m_CurrentLobby->UpdatePost(&send_fd);
  }

  // update running games

  for (auto i = begin(m_Games); i != end(m_Games);)
  {
    if ((*i)->Update(&fd, &send_fd))
    {
      Print2("[AURA] deleting game [" + (*i)->GetGameName() + "]");
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

  for (auto& bnet : m_BNETs)
  {
    if (bnet->Update(&fd, &send_fd))
      Exit = true;
  }

  // update UDP server

  if (m_UDPServerEnabled) {
    UDPPkt* pkt = m_UDPServer->Accept(&fd);
    if (pkt != nullptr) {
      std::string ipAddress(inet_ntoa(pkt->sender.sin_addr));
      uint16_t remotePort = pkt->sender.sin_port;
      if (pkt->length > 0 && !IsIgnoredDatagramSource(ipAddress)) {
        if (m_Config->m_UDPForwardTraffic) {
          std::vector<uint8_t> relayPacket = {W3FW_HEADER_CONSTANT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 14 bytes
          relayPacket.insert(relayPacket.end(), reinterpret_cast<const uint8_t*>(pkt->buf), reinterpret_cast<const uint8_t*>(pkt->buf + pkt->length));
          const uint16_t Size = static_cast<uint16_t>(relayPacket.size());
          relayPacket[2] = static_cast<uint8_t>(Size);
          relayPacket[3] = static_cast<uint8_t>(Size >> 8);
          std::memcpy(relayPacket.data() + 4, &(pkt->sender.sin_addr.s_addr), sizeof(pkt->sender.sin_addr.s_addr));
          std::memcpy(relayPacket.data() + 8, &(pkt->sender.sin_port), sizeof(pkt->sender.sin_port));
          m_UDPSocket->SendTo(m_Config->m_UDPForwardAddress, m_Config->m_UDPForwardPort, relayPacket);
        }

        if (pkt->length >= 2 && static_cast<unsigned char>(pkt->buf[0]) == W3GS_HEADER_CONSTANT/* && pkt->buf[8] == m_Config->m_War3Version */) {
          if (pkt->length >= 16 && static_cast<unsigned char>(pkt->buf[1]) == CGameProtocol::W3GS_SEARCHGAME) {
            bool FromGameRanger = false;
            if (m_Config->m_UDPSupportGameRanger) {
              FromGameRanger = (
                static_cast<unsigned char>(pkt->buf[2]) == 0x10 && (pkt->length == 22 || pkt->length == 23) &&
                !(pkt->buf[16] == static_cast<unsigned char>(W3GS_HEADER_CONSTANT) && pkt->buf[17] != static_cast<unsigned char>(W3GS_HEADER_CONSTANT))
              );
            }
            uint8_t ExtraDigit = pkt->length == 22 ? 0 : 0xFF;
            if (FromGameRanger) {
              vector<uint8_t> GameRangerAddress(pkt->buf + 16, pkt->buf + 20);
              vector<uint8_t> GameRangerPort(pkt->buf + 20, pkt->buf + 22);
              if (ExtraDigit == 0xFF && static_cast<unsigned char>(pkt->buf[22]) == 0) {
                // OK; 0xFF signals delete
              } else {
                // ?
              }
              m_GameRangerLocalPort = remotePort;
              m_GameRangerLocalAddress = ipAddress;
              m_GameRangerRemotePort = GameRangerPort[0] << 8 + GameRangerPort[1];
              m_GameRangerRemoteAddress = GameRangerAddress;

              Print("[AURA] GameRanger client at " + ipAddress + ":" + to_string(remotePort) + " searching for game");
              Print("[AURA] GameRanger local address: " + m_GameRangerLocalAddress + ":" + to_string(m_GameRangerLocalPort));
              Print("[AURA] GameRanger remote address: " + ByteArrayToDecString(m_GameRangerRemoteAddress) + ":" + to_string(m_GameRangerRemotePort));
            }

            if (pkt->buf[8] == m_Config->m_War3Version) {
              if (m_CurrentLobby && !m_CurrentLobby->GetIsMirror() && !m_CurrentLobby->GetCountDownStarted()) {
                if (FromGameRanger) {
                  m_CurrentLobby->AnnounceToAddressForGameRanger(ipAddress, remotePort, m_GameRangerRemoteAddress, m_GameRangerRemotePort, ExtraDigit);
                } else {
                  m_CurrentLobby->AnnounceToAddress(ipAddress, 6112);
                }
              }
            }
          }
        }
      }
    }

    delete pkt;
  }

  // update irc

  if (m_IRC && m_IRC->Update(&fd, &send_fd))
    Exit = true;

  return m_Exiting || Exit;
}

void CAura::EventBNETGameRefreshFailed(CBNET* bnet)
{
  if (m_CurrentLobby)
  {
    // If the game has someone in it, advertise the fail only in the lobby (as it is probably a rehost).
    // Otherwise whisper the game creator that the (re)host failed.

    if (m_CurrentLobby->GetNumHumanPlayers() != 0)
      m_CurrentLobby->SendAllChat("Unable to create game on server [" + bnet->GetServer() + "]. Try another name");
    else if (m_CurrentLobby->GetCreatorServer()) {
      m_CurrentLobby->GetCreatorServer()->QueueChatCommand("Unable to create game on server [" + bnet->GetServer() + "]. Try another name", m_CurrentLobby->GetCreatorName(), true, string());
    }

    Print2("[GAME: " + m_CurrentLobby->GetGameName() + "] Unable to create game on server [" + bnet->GetServer() + "]. Try another name");

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
  for (auto& bnet : m_BNETs)
  {
    bnet->QueueChatCommand("Game [" + game->GetDescription() + "] is over");

    if (bnet == game->GetCreatorServer())
      bnet->QueueChatCommand("Game [" + game->GetDescription() + "] is over", game->GetCreatorName(), true, string());
  }
}

void CAura::ReloadConfigs()
{
  uint8_t WasVersion = m_Config->m_War3Version;
  string WasCFGPath  = m_Config->m_MapCFGPath;
  CConfig* CFG = new CConfig();
  CFG->Read("aura.cfg");
  LoadConfigs(CFG);
  LoadBNETs(CFG);
  LoadIRC(CFG);

  if (!m_ScriptsExtracted || m_Config->m_War3Version != WasVersion) {
    m_ScriptsExtracted = ExtractScripts() == 2;
  }
  if (WasCFGPath != m_Config->m_MapCFGPath) {
    CacheMapPresets();
  }
}

void CAura::LoadConfigs(CConfig* CFG)
{
  // this doesn't set EVERY config value since that would potentially require reconfiguring the battle.net connections
  // it just set the easily reloadable values

  delete m_Config;
  delete m_BNETDefaultConfig;

  m_Config = new CBotConfig(CFG);

  m_BNETDefaultConfig = new CBNETConfig(CFG, m_Config);
  m_GameDefaultConfig = new CGameConfig(CFG);
}

bool CAura::LoadCLI(const int argc, const char* argv[])
{
  argh::parser cmdl({
    /*
      aura
      ! --version
      ! --help
       --w3version <DIRECTORY> --w3path <DIRECTORY> --mapdir <DIRECTORY> --cfgdir <DIRECTORY>
      ! --lan <BOOLEAN> --bnet <BOOLEAN> --exit <BOOLEAN> --nolan <BOOLEAN> --nobnet <BOOLEAN> --noexit <BOOLEAN>
      ! --udp <MODE:(strict|lax|free)>
       <MAP> <NAME> --exclude <SERVER> --mirror <IP:PORT#ID>
       <MAP> <NAME> --obs <OBSERVER> --visibility <VISIBILITY> --rh <RANDOM> --owner <USER@SERVER>
      ! --exec-as <USER@SERVER> --exec-auth <yes|no|auto>
      ! --exec <COMMAND1> --exec <COMMAND2> --exec <COMMAND3>
    */

    // Flags
    //"--version", "--help",
    //"--lan", "--bnet", "--exit",
    //"--nolan", "--nobnet", "--noexit",

    // Parameters
    "--w3version", "--w3path", "--mapdir", "--cfgdir",
    "--udp",
    "--exclude", "--mirror",
    "--obs", "--visibility", "--rh", "--owner",
    "--exec-as", "--exec-auth", "--exec"
  });

  cmdl.parse(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

  /* TODO: Switch to another CLI parser library that properly supports flags */
  if (cmdl("--version")) {
    Print("--version");
    return true;
  }
  if (cmdl("help")) {
    Print("Usage:");
    Print("aura DotA.w3x \"come and play\"");
    return true;
  }

  bool noexit = !m_Config->m_ExitOnStandby;
  bool nobnet = !m_Config->m_EnableBNET;
  bool nolan = !m_GameDefaultConfig->m_LANEnabled;

  if (!cmdl[1].empty() && !cmdl[2].empty()) {
    string MirrorTarget, ExcludedRealm;
    cmdl("mirror") >> MirrorTarget;
    cmdl("exclude") >> ExcludedRealm;
    vector<string> Action;
    if (!MirrorTarget.empty()) {
      Action.push_back("mirror");
      Action.push_back(cmdl[1]);
      Action.push_back(cmdl[2]);
      Action.push_back(ExcludedRealm);
      Action.push_back(MirrorTarget);
    } else {
      string Obs, Vision, RandomHeroes, Owner;
      cmdl("obs") >> Obs;
      cmdl("visibility") >> Vision;
      cmdl("rh") >> RandomHeroes;
      cmdl("owner") >> Owner;
      Action.push_back("host");
      Action.push_back(cmdl[1]);
      Action.push_back(cmdl[2]);
      Action.push_back(Obs);
      Action.push_back(Vision);
      Action.push_back(RandomHeroes);
      Action.push_back(Owner);
      Action.push_back(ExcludedRealm);
    }
    m_PendingActions.push_back(Action);
    noexit = false;
  }

  
  uint32_t War3Version = m_Config->m_War3Version;
  string War3Path = m_Config->m_Warcraft3Path;
  string MapPath = m_Config->m_MapPath;
  string MapCFGPath = m_Config->m_MapCFGPath;

  cmdl("w3version", War3Version) >> War3Version;
  cmdl("w3path", War3Path) >> War3Path;
  cmdl("mapdir", MapPath) >> MapPath;
  cmdl("cfgdir", MapCFGPath) >> MapCFGPath;

  cmdl("noexit", noexit) >> noexit;
  cmdl("nobnet", nobnet) >> nobnet;
  cmdl("nolan", nolan) >> nolan;

  m_Config->m_War3Version = War3Version;
  m_Config->m_Warcraft3Path = War3Path;
  m_Config->m_MapPath = MapPath;
  m_Config->m_MapCFGPath = MapCFGPath;

  m_Config->m_ExitOnStandby = !noexit;
  m_Config->m_EnableBNET = !nobnet;
  m_GameDefaultConfig->m_LANEnabled = !nolan;

  string ExecCommand, ExecAs, ExecAuth;
  cmdl("exec", ExecCommand) >> ExecCommand;
  cmdl("exec-as", ExecAs) >> ExecAs;
  cmdl("exec-auth", ExecAuth) >> ExecAuth;

  if (!ExecCommand.empty() && !ExecAs.empty() && !ExecAuth.empty()) {
    vector<string> Action;
    Action.push_back("exec");
    Action.push_back(ExecCommand);
    Action.push_back(ExecAs);
    Action.push_back(ExecAuth);
    m_PendingActions.push_back(Action);
  }
  /*
    aura
  --version
  --help
  --w3version <DIRECTORY> --w3path <DIRECTORY> --mapdir <DIRECTORY> --cfgdir <DIRECTORY>
  * --lan <BOOLEAN> --bnet <BOOLEAN> --exit <BOOLEAN> --nolan <BOOLEAN> --nobnet <BOOLEAN> --noexit <BOOLEAN>
  * --udp <MODE:(strict|lax|free)>
  <MAP> <NAME> --exclude <SERVER> --mirror <IP:PORT#ID>
  <MAP> <NAME> --obs <OBSERVER> --visibility <VISIBILITY> --rh <RANDOM> --owner <USER@SERVER>
  * --exec-as <USER@SERVER> --exec-auth <yes|no|auto>
  * --exec <COMMAND1> --exec <COMMAND2> --exec <COMMAND3>
  */

  return false;
}

uint8_t CAura::ExtractScripts()
{
  uint8_t FilesExtracted = 0;
  void*        MPQ;
  const string MPQFileName = [&]() {
    if (m_Config->m_War3Version >= 28)
      return m_Config->m_Warcraft3Path + "War3.mpq";
    else
      return m_Config->m_Warcraft3Path + "War3Patch.mpq";
  }();

#ifdef WIN32
  const wstring MPQFileNameW = [&]() {
    if (m_Config->m_War3Version >= 28)
      return wstring(begin(m_Config->m_Warcraft3Path), end(m_Config->m_Warcraft3Path)) + _T("War3.mpq");
    else
      return wstring(begin(m_Config->m_Warcraft3Path), end(m_Config->m_Warcraft3Path)) + _T("War3Patch.mpq");
  }();

  if (SFileOpenArchive(MPQFileNameW.c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | STREAM_FLAG_READ_ONLY, &MPQ))
#else
  if (SFileOpenArchive(MPQFileName.c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | STREAM_FLAG_READ_ONLY, &MPQ))
#endif
  {
    Print("[AURA] loading MPQ file [" + MPQFileName + "]");
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
          if (FileWrite(m_Config->m_MapCFGPath + "common.j", reinterpret_cast<uint8_t*>(SubFileData), BytesRead)) {
            Print(R"([AURA] extracted Scripts\common.j to [)" + m_Config->m_MapCFGPath + "common.j]");
            ++FilesExtracted;
          } else {
            Print(R"([AURA] warning - unable to save extracted Scripts\common.j to [)" + m_Config->m_MapCFGPath + "common.j]");
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
          if (FileWrite(m_Config->m_MapCFGPath + "blizzard.j", reinterpret_cast<uint8_t*>(SubFileData), BytesRead)) {
            Print(R"([AURA] extracted Scripts\blizzard.j to [)" + m_Config->m_MapCFGPath + "blizzard.j]");
            ++FilesExtracted;
          } else {
            Print(R"([AURA] warning - unable to save extracted Scripts\blizzard.j to [)" + m_Config->m_MapCFGPath + "blizzard.j]");
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
      ErrorCode == 2 ? "Config error: bot_war3path is not the WC3 directory" : (
      (ErrorCode == 3 || ErrorCode == 15) ? "Config error: bot_war3path is not a valid directory" : (
      (ErrorCode == 32 || ErrorCode == 33) ? "File is currently opened by another process." : (
      "Error code " + to_string(ErrorCode)
      )))
    );
#else
    int32_t ErrorCode = static_cast<int32_t>(GetLastError());
    string ErrorCodeString = "Error code " + to_string(ErrorCode);
#endif
    Print("[AURA] warning - unable to load MPQ file [" + MPQFileName + "] - " + ErrorCodeString);
  }

  return FilesExtracted;
}

void CAura::LoadIPToCountryData()
{
  ifstream in;
  in.open("ip-to-country.csv", ios::in);

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
      const uint32_t FileLength = in.tellg();
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
    std::string MapLocalPath = CConfig::ReadString(m_Config->m_MapCFGPath + MapConfigFile, "map_localpath");
    if (!MapLocalPath.empty()) {
      m_CachedMaps[MapLocalPath] = MapConfigFile;
    }
  }
}

bool CAura::CreateGame(CMap* map, uint8_t gameDisplay, string gameName, string ownerName, string ownerServer, string creatorName, CBNET* creatorServer, bool whisper)
{
  if (!m_Config->m_Enabled) {
    if (creatorServer) creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. The bot is disabled", creatorName, whisper, string());
    Print("Unable to create game [" + gameName + "]. The bot is disabled");
    return false;
  }

  if (gameName.size() > m_MaxGameNameSize) {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. The game name is too long (the maximum is " + to_string(m_MaxGameNameSize) + " characters)", creatorName, whisper, string());
    Print("Unable to create game [" + gameName + "]. The game name is too long (the maximum is " + to_string(m_MaxGameNameSize) + " characters)");
    return false;
  }

  if (!map->GetValid()) {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. The currently loaded map config file is invalid", creatorName, whisper, string());
    Print("Unable to create game [" + gameName + "]. The currently loaded map config file is invalid");
    return false;
  }

  if (m_CurrentLobby) {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. Another game [" + m_CurrentLobby->GetDescription() + "] is in the lobby", creatorName, whisper, string());
    Print("Unable to create game [" + gameName + "]. Another game [" + m_CurrentLobby->GetDescription() + "] is in the lobby");
    return false;
  }

  if (m_Games.size() >= m_Config->m_MaxGames) {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. The maximum number of simultaneous games (" + to_string(m_Config->m_MaxGames) + ") has been reached", creatorName, whisper, string());
    Print("Unable to create game [" + gameName + "]. The maximum number of simultaneous games (" + to_string(m_Config->m_MaxGames) + ") has been reached");
    return false;
  }

  Print2("[AURA] creating game [" + gameName + "]");

  uint16_t HostPort = NextHostPort();
  m_CurrentLobby = new CGame(this, map, HostPort, m_Config->m_EnableLANBalancer ? m_Config->m_LANHostPort : HostPort, gameDisplay, gameName, ownerName, ownerServer, creatorName, creatorServer);
  if (m_CurrentLobby->GetExiting()) {
    delete m_CurrentLobby;
    m_CurrentLobby = nullptr;
    return false;
  }

  m_CurrentLobby->LANBroadcastGameCreate();

  for (auto& bnet : m_BNETs) {
    std::string AnnounceText = (
      std::string("[1.") + to_string(m_Config->m_War3Version) + ".x] Hosting " + (gameDisplay == GAME_PRIVATE ? "private" : "public") + " game of " + map->GetMapLocalPath() +
      ". (Started by " + ownerName + ": \"" + bnet->GetPrefixedGameName(gameName) + "\")"
    );
    if (whisper && bnet == creatorServer) {
      bnet->QueueChatCommand(AnnounceText, creatorName, whisper, string());
    }
    if (bnet->GetAnnounceHostToChat()) {
      bnet->QueueChatCommand(AnnounceText);
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

void CAura::CreateMirror(CMap* map, uint8_t gameDisplay, string gameName, string gameAddress, uint16_t gamePort, uint32_t gameHostCounter, uint32_t gameEntryKey, string excludedServer, string creatorName, CBNET* creatorServer, bool whisper)
{
  if (!m_Config->m_Enabled) {
    creatorServer->QueueChatCommand("Unable to mirror game [" + gameName + "]. The bot is disabled", creatorName, whisper, string());
    return;
  }

  if (gameName.size() > m_MaxGameNameSize) {
    creatorServer->QueueChatCommand("Unable to mirror game [" + gameName + "]. The game name is too long (the maximum is " + to_string(m_MaxGameNameSize) + " characters)", creatorName, whisper, string());
    return;
  }

  if (!map->GetValid()) {
    creatorServer->QueueChatCommand("Unable to mirror game [" + gameName + "]. The currently loaded map config file is invalid", creatorName, whisper, string());
    return;
  }

  if (m_CurrentLobby) {
    creatorServer->QueueChatCommand("Unable to mirror game [" + gameName + "]. Another game [" + m_CurrentLobby->GetDescription() + "] is in the lobby", creatorName, whisper, string());
    return;
  }

  if (m_Games.size() >= m_Config->m_MaxGames) {
    creatorServer->QueueChatCommand("Unable to mirror game [" + gameName + "]. The maximum number of simultaneous games (" + to_string(m_Config->m_MaxGames) + ") has been reached", creatorName, whisper, string());
    return;
  }

  Print2("[AURA] mirroring game [" + gameName + "]");

  m_CurrentLobby = new CGame(this, map, gameDisplay, gameName, gameAddress, gamePort, gameHostCounter, gameEntryKey, excludedServer);

  for (auto& bnet : m_BNETs) {
    if (bnet->GetInputID() == excludedServer || bnet->GetIsMirror())
      continue;

    std::string AnnounceText = (
      std::string("[1.") + to_string(m_Config->m_War3Version) + ".x] Mirroring " + (gameDisplay == GAME_PRIVATE ? "private" : "public") + " game of " + map->GetMapLocalPath() +
      ": \"" + bnet->GetPrefixedGameName(gameName) + "\")"
    );
    if (whisper && bnet == creatorServer) {
      bnet->QueueChatCommand(AnnounceText, creatorName, whisper, string());
    }
    if (bnet->GetAnnounceHostToChat()) {
      bnet->QueueChatCommand(AnnounceText);
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
}

void CAura::SendBroadcast(uint16_t port, const vector<uint8_t>& message)
{
  if (m_UDPServerEnabled && m_UDPServer->Broadcast(port, message)) {
    return;
  }
  m_UDPSocket->Broadcast(port, message);
}

vector<string> CAura::MapFilesMatch(string rawPattern)
{
  if (IsValidMapName(rawPattern) && FileExists(m_Config->m_MapPath + rawPattern)) {
    return std::vector<std::string>(1, rawPattern);
  }

  string pattern = RemoveNonAlphanumeric(rawPattern);
  transform(begin(pattern), end(pattern), begin(pattern), ::tolower);

  auto TFTMaps = FilesMatch(m_Config->m_MapPath, ".w3x");
  auto ROCMaps = FilesMatch(m_Config->m_MapPath, ".w3m");

  std::unordered_set<std::string> MapSet(TFTMaps.begin(), TFTMaps.end());
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

  std::string::size_type maxDistance = 10;
  if (pattern.size() < maxDistance) {
    maxDistance = pattern.size() / 2;
  }

  std::vector<std::pair<std::string, int>> distances;
  std::vector<std::pair<std::string, int>>::size_type i;
  std::vector<std::pair<std::string, int>>::size_type i_max;

  for (auto& mapName : MapSet) {
    string cmpName = RemoveNonAlphanumeric(mapName);
    transform(begin(cmpName), end(cmpName), begin(cmpName), ::tolower);
    if ((pattern.size() <= cmpName.size() + maxDistance) && (cmpName.size() <= maxDistance + pattern.size())) {
      std::string::size_type distance = GetLevenshteinDistance(pattern, cmpName); // source to target
      if (distance <= maxDistance) {
        distances.emplace_back(mapName, distance);
      }
    }
  }

  std::partial_sort(
    distances.begin(),
    distances.begin() + std::min(5, static_cast<int>(distances.size())),
    distances.end(),
    [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
        return a.second < b.second;
    }
  );

  std::string PrioritizedString;
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
  ++m_HostPort;
  if (m_HostPort > m_Config->m_MaxHostPort || m_HostPort < m_Config->m_MinHostPort) {
    m_HostPort = m_Config->m_MinHostPort;
  }
  return m_HostPort;
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
  return m_Config->m_UDPBlockedIPs.find(element) != m_Config->m_UDPBlockedIPs.end();
}