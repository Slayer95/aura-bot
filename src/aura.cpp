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

#include <csignal>
#include <cstdlib>
#include <thread>
#include <fstream>

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

LPCWSTR StringToLPCWSTR(const std::string& str)
{
    std::wstring wideStr(str.begin(), str.end());
  return wideStr.c_str();
}

int main(const int, const char* argv[])
{
  // seed the PRNG

  srand(static_cast<uint32_t>(time(nullptr)));

  // disable sync since we don't use cstdio anyway

  ios_base::sync_with_stdio(false);

  // read config file

  CConfig CFG;
  CFG.Read("aura.cfg");

  Print("[AURA] starting up");

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

  // print timer resolution

  Print("[AURA] using monotonic timer with resolution " + std::to_string(static_cast<double>(std::chrono::steady_clock::period::num) / std::chrono::steady_clock::period::den * 1e9) + " nanoseconds");

#ifdef WIN32
  // initialize winsock

  Print("[AURA] starting winsock");
  WSADATA wsadata;

  if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
  {
    Print("[AURA] error starting winsock");
    return 1;
  }

  // increase process priority

  Print("[AURA] setting process priority to \"high\"");
  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

  // initialize aura

  gAura = new CAura(&CFG);

  // check if it's properly configured

  if (gAura->GetReady())
  {
    // loop

    while (!gAura->Update())
      ;
  }
  else
    Print("[AURA] check your aura.cfg and configure Aura properly");

  // shutdown aura

  Print("[AURA] shutting down");
  delete gAura;

#ifdef WIN32
  // shutdown winsock

  Print("[AURA] shutting down winsock");
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

CAura::CAura(CConfig* CFG)
  : m_IRC(nullptr),
    m_UDPServer(new CUDPServer()),
    m_UDPSocket(new CUDPSocket()),
    m_GameProtocol(nullptr),
    m_GPSProtocol(new CGPSProtocol()),
    m_CRC(new CCRC32()),
    m_SHA(new CSHA1()),
    m_CurrentLobby(nullptr),
    m_DB(new CAuraDB(CFG)),
    m_Map(nullptr),
    m_Version(VERSION),
    m_HostCounter(0),
    m_HostPort(0),
    m_Exiting(false),
    m_Enabled(true),
    m_EnabledPublic(true),
    m_Ready(true)
{
  Print("[AURA] Aura++ version " + m_Version);

  // get the general configuration variables

  m_UDPServer->SetBroadcastTarget(CFG->GetString("udp_broadcasttarget", string()));
  m_UDPServer->SetDontRoute(CFG->GetInt("udp_dontroute", 0) == 0 ? false : true);
  if (CFG->GetInt("udp_enableserver", 0) == 1) {
    m_UDPServer->Listen(CFG->GetString("bot_bindaddress", "0.0.0.0"), 6112);
  }

  m_UDPSocket->SetBroadcastTarget(CFG->GetString("udp_broadcasttarget", string()));
  m_UDPSocket->SetDontRoute(CFG->GetInt("udp_dontroute", 0) == 0 ? false : true);

  m_GameProtocol = new CGameProtocol(this);
  m_ProxyReconnectEnabled   = CFG->GetInt("bot_enablegproxy", 0) == 1;

  m_CRC->Initialize();

  m_MinHostPort       = CFG->GetInt("bot_minhostport", CFG->GetInt("bot_hostport", 6112));
  m_MaxHostPort       = CFG->GetInt("bot_maxhostport", m_MinHostPort);

  // for load balancers or reverse proxies
  m_EnableLANBalancer = CFG->GetInt("bot_enablelanbalancer", 0) == 1;
  m_LANHostPort       = CFG->GetInt("bot_lanhostport", m_HostPort);
  m_EnablePvPGNTunnel = CFG->GetInt("bot_enablepvpgntunnel", 0) == 1;
  m_PublicHostPort    = CFG->GetInt("bot_publichostport", m_HostPort);
  m_PublicHostAddress = CFG->GetString("bot_publichostaddress", string());
  if (m_EnableLANBalancer) {
    Print("[AURA] Broadcasting games port " + std::to_string(m_LANHostPort) + " over LAN");
  }
  if (m_EnablePvPGNTunnel) {
    Print("[AURA] TCP tunnel enabled at " + m_PublicHostAddress + ":" + std::to_string(m_PublicHostPort));
  }

  m_LANWar3Version    = CFG->GetInt("lan_war3version", 27);
  m_NumPlayersToStartGameOver = CFG->GetInt("bot_gameoverplayernumber", 1);

  // read the rest of the general configuration

  SetConfigs(CFG);

  // get the irc configuration

  string   IRC_Server         = CFG->GetString("irc_server", string());
  string   IRC_NickName       = CFG->GetString("irc_nickname", string());
  string   IRC_UserName       = CFG->GetString("irc_username", string());
  string   IRC_Password       = CFG->GetString("irc_password", string());
  string   IRC_CommandTrigger = CFG->GetString("irc_commandtrigger", "!");
  uint32_t IRC_Port           = CFG->GetInt("irc_port", 6667);

  // get the irc channels and root admins

  vector<string> IRC_Channels, IRC_RootAdmins;

  for (uint32_t i = 1; i <= 10; ++i)
  {
    string Channel, RootAdmin;

    if (i == 1)
    {
      Channel   = CFG->GetString("irc_channel", string());
      RootAdmin = CFG->GetString("irc_rootadmin", string());
    }
    else
    {
      Channel   = CFG->GetString("irc_channel" + to_string(i), string());
      RootAdmin = CFG->GetString("irc_rootadmin" + to_string(i), string());
    }

    if (!Channel.empty())
      IRC_Channels.push_back("#" + Channel);

    if (!RootAdmin.empty())
      IRC_RootAdmins.push_back(RootAdmin);
  }

  if (IRC_Server.empty() || IRC_NickName.empty() || IRC_Port == 0 || IRC_Port >= 65535)
    Print("[AURA] warning - irc connection not found in config file");
  else
    m_IRC = new CIRC(this, IRC_Server, IRC_NickName, IRC_UserName, IRC_Password, IRC_Channels, IRC_RootAdmins, IRC_Port, IRC_CommandTrigger[0]);

  uint8_t HighestWar3Version = 0;

  // load the battle.net connections
  // we're just loading the config data and creating the CBNET classes here, the connections are established later (in the Update function)

  for (uint32_t i = 1; i < 10; ++i)
  {
    string Prefix;

    if (i == 1)
      Prefix = "bnet_";
    else
      Prefix = "bnet" + to_string(i) + "_";

    string   Server        = CFG->GetString(Prefix + "server", string());
    string   ServerAlias   = CFG->GetString(Prefix + "serveralias", string());
    string   CDKeyROC      = CFG->GetString(Prefix + "cdkeyroc", string());
    string   CDKeyTFT      = CFG->GetString(Prefix + "cdkeytft", string());
    string   CountryAbbrev = CFG->GetString(Prefix + "countryabbrev", "DEU");
    string   Country       = CFG->GetString(Prefix + "country", "Germany");
    string   Locale        = CFG->GetString(Prefix + "locale", "system");
    uint32_t LocaleID;

    if (Locale == "system")
      LocaleID = 1031;
    else
      LocaleID = stoul(Locale);

    string UserName     = CFG->GetString(Prefix + "username", string());
    string UserPassword = CFG->GetString(Prefix + "password", string());
    string FirstChannel = CFG->GetString(Prefix + "firstchannel", "The Void");
    string RootAdmins   = CFG->GetString(Prefix + "rootadmins", string());

    // add each root admin to the rootadmin table

    string       User;
    stringstream SS;
    SS << RootAdmins;

    while (!SS.eof())
    {
      SS >> User;
      m_DB->RootAdminAdd(Server, User);
    }

    string               BNETCommandTrigger = CFG->GetString(Prefix + "commandtrigger", "!");
    uint8_t              War3Version        = CFG->GetInt(Prefix + "custom_war3version", 27);
    std::vector<uint8_t> EXEVersion         = ExtractNumbers(CFG->GetString(Prefix + "custom_exeversion", string()), 4);
    std::vector<uint8_t> EXEVersionHash     = ExtractNumbers(CFG->GetString(Prefix + "custom_exeversionhash", string()), 4);
    string               PasswordHashType   = CFG->GetString(Prefix + "custom_passwordhashtype", string());

    HighestWar3Version = (std::max)(HighestWar3Version, War3Version);

    if (Server.empty())
      break;

    Print("[AURA] found battle.net connection #" + to_string(i) + " for server [" + Server + "]");

    if (Locale == "system")
      Print("[AURA] using system locale of " + to_string(LocaleID));

    m_BNETs.push_back(new CBNET(this, Server, ServerAlias, CDKeyROC, CDKeyTFT, CountryAbbrev, Country, LocaleID, UserName, UserPassword, FirstChannel, BNETCommandTrigger[0], War3Version, EXEVersion, EXEVersionHash, PasswordHashType, i));
  }

  if (m_BNETs.empty())
    Print("[AURA] warning - no battle.net connections found in config file");

  if (m_BNETs.empty() && !m_IRC)
  {
    Print("[AURA] error - no battle.net connections and no irc connection specified");
    m_Ready = false;
    return;
  }

  // extract common.j and blizzard.j from War3Patch.mpq or War3.mpq (depending on version) if we can
  // these two files are necessary for calculating "map_crc" when loading maps so we make sure to do it before loading the default map
  // see CMap :: Load for more information

  ExtractScripts(HighestWar3Version);

  // load the iptocountry data

  LoadIPToCountryData();

  // Preload map configs
  const vector<string> MapConfigFiles = ConfigFilesMatch("");
  for (const auto& MapConfigFile : MapConfigFiles) {
    std::string MapLocalPath = CConfig::ReadString(m_MapCFGPath + MapConfigFile, "map_localpath");
    if (!MapLocalPath.empty()) {
      m_CachedMaps[MapLocalPath] = MapConfigFile;
    }
  }
}

CAura::~CAura()
{
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
  if (!m_GameServers[port]->Listen(m_BindAddress, port)) {
    Print2("[GAME] " + name + " Error listening on port " + to_string(port));
    return nullptr;
  }
  Print("[GAME] " + name + " Listening on port " + to_string(port));
  return m_GameServers[port];
}

bool CAura::Update()
{
  uint32_t NumFDs = 0;

  // take every socket we own and throw it in one giant select statement so we can block on all sockets

  int32_t nfds = 0;
  fd_set  fd, send_fd;
  FD_ZERO(&fd);
  FD_ZERO(&send_fd);

  // 1. all running game servers

  for (auto& pair : m_GameServers) {
    pair.second->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
    ++NumFDs;
  }

  // 2. all unassigned incoming TCP connections

  for (auto& pair : m_IncomingConnections) {
    // std::pair<uint16_t, std::vector<CPotentialPlayer*>>
    for (auto& potential : pair.second) {
      if (potential->GetSocket()) {
        potential->GetSocket()->SetFD(static_cast<fd_set*>(&fd), static_cast<fd_set*>(&send_fd), &nfds);
        ++NumFDs;
      }
    }
  }

  // 3. the current lobby's player sockets

  if (m_CurrentLobby)
    NumFDs += m_CurrentLobby->SetFD(&fd, &send_fd, &nfds);

  // 4. all running games' player sockets

  for (auto& game : m_Games)
    NumFDs += game->SetFD(&fd, &send_fd, &nfds);

  // 5. all battle.net sockets

  for (auto& bnet : m_BNETs)
    NumFDs += bnet->SetFD(&fd, &send_fd, &nfds);

  // 6. UDP server
  if (m_UDPServerEnabled) {
    m_UDPServer->SetFD(&fd, &send_fd, &nfds);
    ++NumFDs;
  }

  // 7. irc socket

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
      if (m_ProxyReconnectEnabled) {
        CPotentialPlayer* IncomingConnection = new CPotentialPlayer(m_GameProtocol, this, ConnectPort, NewSocket);
        m_IncomingConnections[ConnectPort].push_back(IncomingConnection);
      } else if (!m_CurrentLobby || ConnectPort != m_CurrentLobby->GetHostPort()) {
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
      char* ipAddress = inet_ntoa(pkt->sender.sin_addr);
      if (!IsIgnoredDatagramSource(ipAddress) && pkt->length >= 2 && static_cast<unsigned char>(pkt->buf[0]) == W3GS_HEADER_CONSTANT) {
        if (m_UDPForwardTraffic) {
          std::vector<uint8_t> relayPacket = {W3FW_HEADER_CONSTANT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
          relayPacket.insert(relayPacket.end(), reinterpret_cast<const uint8_t*>(pkt->buf), reinterpret_cast<const uint8_t*>(pkt->buf + pkt->length));
          const uint16_t Size = static_cast<uint16_t>(relayPacket.size());
          relayPacket[2] = static_cast<uint8_t>(Size);
          relayPacket[3] = static_cast<uint8_t>(Size >> 8);
          std::memcpy(relayPacket.data() + 4, &(pkt->sender.sin_addr.s_addr), sizeof(pkt->sender.sin_addr.s_addr));
          std::memcpy(relayPacket.data() + 8, &(pkt->sender.sin_port), sizeof(pkt->sender.sin_port));
          m_UDPSocket->SendTo(m_UDPForwardAddress, m_UDPForwardPort, relayPacket);
        }

        if (pkt->buf[1] == CGameProtocol::W3GS_SEARCHGAME && pkt->length == 16 && pkt->buf[8] == m_LANWar3Version) {
          if (m_CurrentLobby && !m_CurrentLobby->GetCountDownStarted()) {
            m_CurrentLobby->AnnounceToAddress(ipAddress, 6112);
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
    else
      m_CurrentLobby->GetCreatorServer()->QueueChatCommand("Unable to create game on server [" + bnet->GetServer() + "]. Try another name", m_CurrentLobby->GetCreatorName(), true, string());

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
  CConfig CFG;
  CFG.Read("aura.cfg");
  SetConfigs(&CFG);
}

void CAura::SetConfigs(CConfig* CFG)
{
  // this doesn't set EVERY config value since that would potentially require reconfiguring the battle.net connections
  // it just set the easily reloadable values

  m_Warcraft3Path          = AddPathSeparator(CFG->GetString("bot_war3path", R"(C:\Program Files\Warcraft III\)"));
  m_BindAddress            = CFG->GetString("bot_bindaddress", string());
  m_UDPServerEnabled       = CFG->GetInt("udp_enableserver", 1);
  m_UDPForwardTraffic      = CFG->GetInt("udp_redirenabled", 0);
  m_UDPForwardGameLists    = CFG->GetInt("udp_redirgamelists", 0);
  m_UDPForwardAddress      = CFG->GetString("udp_rediraddress", string());
  m_UDPForwardPort         = CFG->GetInt("udp_redirport", 6110);
  m_ReconnectWaitTime      = CFG->GetInt("bot_reconnectwaittime", 3);
  m_MaxGames               = CFG->GetInt("bot_maxgames", 20);
  m_MinHostCounter         = CFG->GetInt("bot_firstgameid", 100);
  string BotCommandTrigger = CFG->GetString("bot_commandtrigger", "!");
  m_CommandTrigger         = BotCommandTrigger[0];
  m_GreetingPath           = CFG->GetString("bot_greetingpath", string());
  m_MaxSavedMapSize        = CFG->GetInt("bot_maxpersistentsize", 0xFFFFFFFF);

  m_Greeting.clear();
  if (m_GreetingPath.length() > 0) {
    ifstream in;
    in.open(m_GreetingPath);
    if (!in.fail()) {
      while (!in.eof()) {
        string Line;
        getline(in, Line);
        if (Line.empty()) {
          if (!in.eof())
            m_Greeting.push_back(" ");
        } else {
          m_Greeting.push_back(Line);
        }
      }
      in.close( );
    }
  }

  m_MapCFGPath      = AddPathSeparator(CFG->GetString("bot_mapcfgpath", string()));
  m_MapPath         = AddPathSeparator(CFG->GetString("bot_mappath", string()));
  m_IndexVirtualHostName = CFG->GetString("bot_indexvirtualhostname", "Aura Bot");
  m_LobbyVirtualHostName = CFG->GetString("bot_lobbyvirtualhostname", "|cFF4080C0Aura");

  if (m_IndexVirtualHostName.size() > 15) {
    m_IndexVirtualHostName = "Aura Bot";
    Print("[AURA] warning - bot_indexvirtualhostname is longer than 15 characters - using default index virtual host name");
  }

  if (m_LobbyVirtualHostName.size() > 15) {
    m_LobbyVirtualHostName = "|cFF4080C0Aura";
    Print("[AURA] warning - bot_lobbyvirtualhostname is longer than 15 characters - using default lobby virtual host name");
  }

  m_AllowDownloads     = CFG->GetInt("bot_allowdownloads", 0);
  m_AllowUploads       = CFG->GetInt("bot_allowuploads", 0);
  m_MaxDownloaders     = CFG->GetInt("bot_maxdownloaders", 3);
  m_MaxDownloadSpeed   = CFG->GetInt("bot_maxdownloadspeed", 100);
  m_LCPings            = CFG->GetInt("bot_lcpings", 1) == 0 ? false : true;
  m_AutoKickPing       = CFG->GetInt("bot_autokickping", 300);
  m_LobbyTimeLimit     = CFG->GetInt("bot_lobbytimelimit", 10);
  m_LobbyNoOwnerTime   = CFG->GetInt("bot_lobbyownerlesstime", 2);
  m_Latency            = CFG->GetInt("bot_latency", 100);
  m_SyncLimit          = CFG->GetInt("bot_synclimit", 50);
  m_VoteKickPercentage = CFG->GetInt("bot_votekickpercentage", 70);
  m_ResolveMapToConfig = CFG->GetInt("bot_resolvemaptoconfig", 1) == 1;

  if (m_VoteKickPercentage > 100)
    m_VoteKickPercentage = 100;

  m_NotifyJoins       = CFG->GetInt("bot_notifyjoins", 0) == 0 ? false : true;
  m_UDPInfoStrictMode = CFG->GetInt("udp_infostrictmode", 1) == 0 ? false : true;

  stringstream ss(CFG->GetString("bot_notifyjoinsexcept", ""));
  m_IgnoredNotifyJoinPlayers.clear();
  while (ss.good()) {
    string substr;
    getline(ss, substr, ',');
    if (substr.size() > 0) {
      m_IgnoredNotifyJoinPlayers.push_back(substr);
    }
  }

  stringstream ss2(CFG->GetString("udp_blocklist", ""));
  m_IgnoredDatagramSources.clear();
  while (ss2.good()) {
    string substr;
    getline(ss2, substr, ',');
    if (substr.size() > 0) {
      m_IgnoredDatagramSources.push_back(substr);
    }
  }

  stringstream ss3(CFG->GetString("bot_superusers", ""));
  m_SudoUsers.clear();
  while (ss3.good()) {
    string substr;
    getline(ss3, substr, ',');
    int atIndex = substr.find("@");
    if (atIndex != string::npos) {
      m_SudoUsers.emplace_back(std::make_pair(
        substr.substr(0, atIndex),
        substr.substr(atIndex + 1, substr.length())
       ));
    }
  }
}

void CAura::ExtractScripts(const uint8_t War3Version)
{
  void*        MPQ;
  const string MPQFileName = [&]() {
    if (War3Version >= 28)
      return m_Warcraft3Path + "War3.mpq";
    else
      return m_Warcraft3Path + "War3Patch.mpq";
  }();

#ifdef WIN32
  const wstring MPQFileNameW = [&]() {
    if (War3Version >= 28)
      return wstring(begin(m_Warcraft3Path), end(m_Warcraft3Path)) + _T("War3.mpq");
    else
      return wstring(begin(m_Warcraft3Path), end(m_Warcraft3Path)) + _T("War3Patch.mpq");
  }();

  if (SFileOpenArchive(MPQFileNameW.c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1, &MPQ))
#else
  if (SFileOpenArchive(MPQFileName.c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1, &MPQ))
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
          Print(R"([AURA] extracting Scripts\common.j from MPQ file to [)" + m_MapCFGPath + "common.j]");
          FileWrite(m_MapCFGPath + "common.j", reinterpret_cast<uint8_t*>(SubFileData), BytesRead);
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
          Print(R"([AURA] extracting Scripts\blizzard.j from MPQ file to [)" + m_MapCFGPath + "blizzard.j]");
          FileWrite(m_MapCFGPath + "blizzard.j", reinterpret_cast<uint8_t*>(SubFileData), BytesRead);
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
    Print("[AURA] warning - unable to load MPQ file [" + MPQFileName + "] - error code " + to_string((uint32_t)GetLastError()));
#else
    Print("[AURA] warning - unable to load MPQ file [" + MPQFileName + "] - error code " + to_string(static_cast<int32_t>(GetLastError())));
#endif
  }
}

void CAura::LoadIPToCountryData()
{
  ifstream in;
  in.open("ip-to-country.csv");

  if (in.fail()) {
    Print("[AURA] warning - unable to read file [ip-to-country.csv], iptocountry data not loaded");
  } else {
    Print("[AURA] loading [ip-to-country.csv]");

    // the begin and commit statements are optimizations
    // we're about to insert ~4 MB of data into the database so if we allow the database to treat each insert as a transaction it will take a LONG time

    if (!m_DB->Begin())
      Print("[AURA] warning - failed to begin database transaction, iptocountry data not loaded");
    else
    {
      uint8_t   Percent = 0;
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
        Print("[AURA] warning - failed to commit database transaction, iptocountry data not loaded");
      else
        Print("[AURA] finished loading [ip-to-country.csv]");
    }

    in.close();
  }
}

void CAura::CreateGame(CMap* map, uint8_t gameState, string gameName, string ownerName, string ownerServer, string creatorName, CBNET* creatorServer, bool whisper)
{
  if (!m_Enabled)
  {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. The bot is disabled", creatorName, whisper, string());
    return;
  }

  if (gameName.size() > 31)
  {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. The game name is too long (the maximum is 31 characters)", creatorName, whisper, string());
    return;
  }

  if (!map->GetValid())
  {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. The currently loaded map config file is invalid", creatorName, whisper, string());
    return;
  }

  if (m_CurrentLobby)
  {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. Another game [" + m_CurrentLobby->GetDescription() + "] is in the lobby", creatorName, whisper, string());
    return;
  }

  if (m_Games.size() >= m_MaxGames)
  {
    creatorServer->QueueChatCommand("Unable to create game [" + gameName + "]. The maximum number of simultaneous games (" + to_string(m_MaxGames) + ") has been reached", creatorName, whisper, string());
    return;
  }

  Print2("[AURA] creating game [" + gameName + "]");

  uint16_t HostPort = NextHostPort();
  m_CurrentLobby = new CGame(this, map, HostPort, m_EnableLANBalancer ? m_LANHostPort : HostPort, gameState, gameName, ownerName, ownerServer, creatorName, creatorServer);

  for (auto& bnet : m_BNETs) {
    if (whisper && bnet == creatorServer) {
      bnet->QueueChatCommand(std::string("Creating ") + (gameState == GAME_PRIVATE ? "private" : "public") + " game of " + map->GetMapLocalPath() + ". (Started by " + ownerName + ": \"" + gameName + "\")", creatorName, whisper, string());
    }
    bnet->QueueChatCommand((gameState == GAME_PRIVATE ? std::string("Private") : std::string("Public")) + " game of " + map->GetMapLocalPath() + " created. (Started by " + ownerName + ": \"" + gameName + "\").");
    bnet->QueueGameCreate(gameState, gameName, map, m_CurrentLobby->GetHostCounter());

    // hold friends and/or clan members

    bnet->HoldFriends(m_CurrentLobby);
    bnet->HoldClan(m_CurrentLobby);

    // if we're creating a private game we don't need to send any game refresh messages so we can rejoin the chat immediately
    // unfortunately this doesn't work on PVPGN servers because they consider an enterchat message to be a gameuncreate message when in a game
    // so don't rejoin the chat if we're using PVPGN

    if (gameState == GAME_PRIVATE && !bnet->GetPvPGN())
      bnet->QueueEnterChat();
  }
}

vector<string> CAura::MapFilesMatch(string rawPattern)
{
  if (IsValidMapName(rawPattern) && FileExists(m_MapPath + rawPattern)) {
    return std::vector<std::string>(1, rawPattern);
  }

  string pattern = RemoveNonAlphanumeric(rawPattern);
  transform(begin(pattern), end(pattern), begin(pattern), ::tolower);

  auto TFTMaps = FilesMatch(m_MapPath, ".w3x");
  auto ROCMaps = FilesMatch(m_MapPath, ".w3m");

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

  vector<string> ConfigList = FilesMatch(m_MapCFGPath, ".cfg");

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
