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

#include "realm.h"
#include "command.h"
#include "aura.h"
#include "util.h"
#include "fileutil.h"
#include "config.h"
#include "config_realm.h"
#include "socket.h"
#include "auradb.h"
#include "bncsutilinterface.h"
#include "bnetprotocol.h"
#include "map.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "game.h"
#include "includes.h"
#include "hash.h"

#include <algorithm>
#include <cmath>

using namespace std;

//
// CRealm
//

CRealm::CRealm(CAura* nAura, CRealmConfig* nRealmConfig)
  : m_Aura(nAura),
    m_Config(nRealmConfig),
    m_Socket(nullptr),
    m_Protocol(new CBNETProtocol()),
    m_BNCSUtil(new CBNCSUtilInterface(nRealmConfig->m_UserName, nRealmConfig->m_PassWord)),
    m_HostName(nRealmConfig->m_HostName),
    m_ServerIndex(nRealmConfig->m_ServerIndex),
    m_InternalServerID(nAura->NextServerID()),
    m_PublicServerID(nRealmConfig->m_ServerIndex + 15),
    m_LastDisconnectedTime(0),
    m_LastConnectionAttemptTime(0),
    m_LastGameListTime(0),
    m_LastAdminRefreshTime(GetTime()),
    m_LastBanRefreshTime(GetTime()),
    m_SessionID(0),
    m_NullPacketsSent(0),
    m_Exiting(false),
    m_FirstConnect(true),
    m_ReconnectNextTick(true),
    m_WaitingToConnect(true),
    m_LoggedIn(false),
    m_GamePort(6112),
    m_HadChatActivity(false),
    m_AnyWhisperRejected(false),
    m_ChatQueuedGameAnnouncement(false),
    m_ChatQueueJoinCallback(nullptr),
    m_ChatQueueGameHostWhois(nullptr)
{
  m_ReconnectDelay = GetPvPGN() ? 90 : 240;
}

CRealm::~CRealm()
{
  ResetConnection(false);

  delete m_Config;
  delete m_Socket;
  delete m_Protocol;
  delete m_BNCSUtil;

  for (auto& ctx : m_Aura->m_ActiveContexts) {
    if (ctx->m_SourceRealm == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_SourceRealm = nullptr;
    }
    if (ctx->m_TargetRealm == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_TargetRealm = nullptr;
    }
  }

  if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->MatchesCreatedFrom(GAMESETUP_ORIGIN_REALM, reinterpret_cast<void*>(this))) {
    m_Aura->m_CurrentLobby->RemoveCreator();
  }
  for (auto& game : m_Aura->m_Games) {
    if (game->MatchesCreatedFrom(GAMESETUP_ORIGIN_REALM, reinterpret_cast<void*>(this))) {
      game->RemoveCreator();
    }
  }

  if (m_Aura->m_GameSetup && m_Aura->m_GameSetup->MatchesCreatedFrom(GAMESETUP_ORIGIN_REALM, reinterpret_cast<void*>(this))) {
    m_Aura->m_GameSetup->RemoveCreator();
  }
}

uint32_t CRealm::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  if (m_Socket && !m_Socket->HasError() && !m_Socket->HasFin() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 1;
  }

  return 0;
}

bool CRealm::Update(void* fd, void* send_fd)
{
  const int64_t Time = GetTime();

  // we return at the end of each if statement so we don't have to deal with errors related to the order of the if statements
  // that means it might take a few ms longer to complete a task involving multiple steps (in this case, reconnecting) due to blocking or sleeping
  // but it's not a big deal at all, maybe 100ms in the worst possible case (based on a 50ms blocking time)

  if (!m_Socket) {
    m_Socket = new CTCPClient(AF_INET, m_Config->m_HostName);
    m_Socket->SetKeepAlive(true, REALM_TCP_KEEPALIVE_IDLE_TIME);
  }

  if (m_Socket->HasError() || m_Socket->HasFin())
  {
    // the socket has an error, or the server terminated the connection
    ResetConnection(true);
    Print(GetLogPrefix() + "waiting " + to_string(m_ReconnectDelay) + " seconds to reconnect");
    return m_Exiting;
  }

  if (m_Socket->GetConnected())
  {
    // the socket is connected and everything appears to be working properly

    if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {

      // extract as many packets as possible from the socket's receive buffer and process them
      string*              RecvBuffer         = m_Socket->GetBytes();
      std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
      uint32_t             LengthProcessed    = 0;
      bool Abort                              = false;
      CIncomingChatEvent* ChatEvent;

      // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

      while (Bytes.size() >= 4) {
        // bytes 2 and 3 contain the length of the packet
        const uint16_t             Length = ByteArrayToUInt16(Bytes, false, 2);
        if (Length < 4) {
          Abort = true;
          break;
        }
        if (Bytes.size() < Length) break;
        const vector<uint8_t> Data = vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

        // byte 0 is always 255
        if (Bytes[0] == BNET_HEADER_CONSTANT)
        {
          // Any BNET packet is fine to reset app-level inactivity timeout.
          m_NullPacketsSent = 0;

          switch (Bytes[1])
          {
            case CBNETProtocol::SID_NULL:
              // warning: we do not respond to NULL packets with a NULL packet of our own
              // this is because PVPGN servers are programmed to respond to NULL packets so it will create a vicious cycle of useless traffic
              // official battle.net servers do not respond to NULL packets
              break;

            case CBNETProtocol::SID_GETADVLISTEX:
              if (m_Aura->m_Net->m_Config->m_UDPForwardGameLists) {
                std::vector<uint8_t> relayPacket = {W3FW_HEADER_CONSTANT, 0, 0, 0};
                std::vector<uint8_t> War3Version = {m_Aura->m_GameVersion, 0, 0, 0};
                std::string ipString = m_Socket->GetIPString();
                AppendByteArray(relayPacket, ipString, true);
                AppendByteArray(relayPacket, static_cast<uint16_t>(6112u), true);
                AppendByteArray(relayPacket, War3Version);
                AppendByteArrayFast(relayPacket, Data);
                AssignLength(relayPacket);
                if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
                  Print(GetLogPrefix() + "sending game list to " + AddressToString(m_Aura->m_Net->m_Config->m_UDPForwardAddress) + " (" + to_string(relayPacket.size()) + " bytes)");
                }
                m_Aura->m_Net->Send(&(m_Aura->m_Net->m_Config->m_UDPForwardAddress), relayPacket);
              }

              break;

            case CBNETProtocol::SID_ENTERCHAT:
              if (m_Protocol->RECEIVE_SID_ENTERCHAT(Data)) {
                AutoJoinChat();
              }

              break;

            case CBNETProtocol::SID_CHATEVENT:
              ChatEvent = m_Protocol->RECEIVE_SID_CHATEVENT(Data);

              if (ChatEvent)
                ProcessChatEvent(ChatEvent);

              delete ChatEvent;
              break;

            case CBNETProtocol::SID_CHECKAD:
              m_Protocol->RECEIVE_SID_CHECKAD(Data);
              break;

            case CBNETProtocol::SID_STARTADVEX3:
              if (!m_Protocol->RECEIVE_SID_STARTADVEX3(Data)) {
                Print(GetLogPrefix() + "Failed to publish hosted game");
                m_Aura->EventBNETGameRefreshFailed(this);
              }

              break;

            case CBNETProtocol::SID_PING:
              SendAuth(m_Protocol->SEND_SID_PING(m_Protocol->RECEIVE_SID_PING(Data)));
              break;

            case CBNETProtocol::SID_AUTH_INFO: {
              if (!m_Protocol->RECEIVE_SID_AUTH_INFO(Data))
                break;

              bool versionSuccess = m_BNCSUtil->HELP_SID_AUTH_CHECK(m_Aura->m_GameInstallPath, m_Config, m_Protocol->GetValueStringFormulaString(), m_Protocol->GetIX86VerFileNameString(), m_Protocol->GetClientToken(), m_Protocol->GetServerToken(), m_Aura->m_GameVersion);
              if (versionSuccess) {
                if (!m_BNCSUtil->CheckValidEXEVersion()) {
                  m_BNCSUtil->SetEXEVersion(m_BNCSUtil->GetDefaultEXEVersion());
                  Print(GetLogPrefix() + "defaulting to <global_realm.auth_exe_version = " + ByteArrayToDecString(m_BNCSUtil->GetDefaultEXEVersion()) + ">");
                }
                if (!m_BNCSUtil->CheckValidEXEVersionHash()) {
                  m_BNCSUtil->SetEXEVersionHash(m_BNCSUtil->GetDefaultEXEVersionHash());
                  Print(GetLogPrefix() + "defaulting to <global_realm.auth_exe_version_hash = " + ByteArrayToDecString(m_BNCSUtil->GetDefaultEXEVersionHash()) + ">");
                }
                if (!m_BNCSUtil->CheckValidEXEInfo()) {
                  m_BNCSUtil->SetEXEInfo(m_BNCSUtil->GetDefaultEXEInfo());
                  Print(GetLogPrefix() + "defaulting to <global_realm.auth_exe_info = " + m_BNCSUtil->GetDefaultEXEInfo() + ">");
                }

                const vector<uint8_t>& exeVersion = m_BNCSUtil->GetEXEVersion();
                const vector<uint8_t>& exeVersionHash = m_BNCSUtil->GetEXEVersionHash();
                const string& exeInfo = m_BNCSUtil->GetEXEInfo();

                if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
                  Print(
                    GetLogPrefix() + "attempting to auth as WC3: TFT v" +
                    to_string(exeVersion[3]) + "." + to_string(exeVersion[2]) + std::string(1, char(97 + exeVersion[1])) +
                    " (Build " + to_string(exeVersion[0]) + ") - " +
                    "version hash <" + ByteArrayToDecString(exeVersionHash) + ">"
                  );
                }

                SendAuth(m_Protocol->SEND_SID_AUTH_CHECK(m_Protocol->GetClientToken(), exeVersion, exeVersionHash, m_BNCSUtil->GetKeyInfoROC(), m_BNCSUtil->GetKeyInfoTFT(), exeInfo, "Aura"));
                SendAuth(m_Protocol->SEND_SID_NULL());
                SendNetworkConfig();
              } else {
                if (m_Config->m_AuthPasswordHashType == REALM_AUTH_PVPGN) {
                  Print(GetLogPrefix() + "config error - misconfigured <game.install_path>");
                } else {
                  Print(GetLogPrefix() + "config error - misconfigured <game.install_path>, or <realm_" + to_string(m_ServerIndex) + ".cd_key.roc>, or <realm_" + to_string(m_ServerIndex) + ".cd_key.tft>");
                }
                Print(GetLogPrefix() + "bncsutil key hash failed, disconnecting...");
                m_Socket->Disconnect();
              }

              break;
            }

            case CBNETProtocol::SID_AUTH_CHECK:
              if (m_Protocol->RECEIVE_SID_AUTH_CHECK(Data))
              {
                // cd keys accepted
                if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                  Print(GetLogPrefix() + "version OK");
                }
                m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON();
                SendAuth(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGON(m_BNCSUtil->GetClientKey(), m_Config->m_UserName));
              }
              else
              {
                // cd keys not accepted

                switch (ByteArrayToUInt32(m_Protocol->GetKeyState(), false))
                {
                  case CBNETProtocol::KR_ROC_KEY_IN_USE:
                    Print(GetLogPrefix() + "logon failed - ROC CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting...");
                    break;

                  case CBNETProtocol::KR_TFT_KEY_IN_USE:
                    Print(GetLogPrefix() + "logon failed - TFT CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting...");
                    break;

                  case CBNETProtocol::KR_OLD_GAME_VERSION:
                  case CBNETProtocol::KR_INVALID_VERSION:
                    Print(GetLogPrefix() + "config error - rejected <realm_" + to_string(m_ServerIndex) + ".auth_exe_version = " + ByteArrayToDecString(m_BNCSUtil->GetEXEVersion()) + ">");
                    Print(GetLogPrefix() + "config error - rejected <realm_" + to_string(m_ServerIndex) + ".auth_exe_version_hash = " + ByteArrayToDecString(m_BNCSUtil->GetEXEVersionHash()) + ">");
                    Print(GetLogPrefix() + "logon failed - version not supported, or version hash invalid, disconnecting...");
                    break;

                  default:
                    Print(GetLogPrefix() + "logon failed - cd keys not accepted, disconnecting...");
                    break;
                }

                m_Socket->Disconnect();
              }

              break;

            case CBNETProtocol::SID_AUTH_ACCOUNTLOGON:
              if (m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGON(Data))
              {
                if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                  Print(GetLogPrefix() + "username [" + m_Config->m_UserName + "] OK");
                }

                if (m_Config->m_AuthPasswordHashType == REALM_AUTH_PVPGN)
                {
                  // pvpgn logon

                  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                    Print(GetLogPrefix() + "using pvpgn logon type");
                  }
                  m_BNCSUtil->HELP_PvPGNPasswordHash(m_Config->m_PassWord);
                  SendAuth(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetPvPGNPasswordHash()));
                }
                else
                {
                  // battle.net logon

                  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                    Print(GetLogPrefix() + "using battle.net logon type");
                  }
                  m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF(m_Protocol->GetSalt(), m_Protocol->GetServerPublicKey());
                  SendAuth(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetM1()));
                }
              }
              else
              {
                Print(GetLogPrefix() + "logon failed - invalid username, disconnecting");
                m_Socket->Disconnect();
              }

              break;

            case CBNETProtocol::SID_AUTH_ACCOUNTLOGONPROOF:
              if (m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF(Data)) {
                OnLoginOkay();
              } else {
                Print(GetLogPrefix() + "logon failed - invalid password, disconnecting");
                m_Socket->Disconnect();
              }
              break;

            case CBNETProtocol::SID_FRIENDLIST:
              m_Friends = m_Protocol->RECEIVE_SID_FRIENDLIST(Data);
              break;

            case CBNETProtocol::SID_CLANMEMBERLIST:
              m_Clan = m_Protocol->RECEIVE_SID_CLANMEMBERLIST(Data);
              break;
          }
        }

        LengthProcessed += Length;
        Bytes = vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
      }

      if (Abort) {
        RecvBuffer->clear();
      } else if (LengthProcessed > 0) {
        *RecvBuffer = RecvBuffer->substr(LengthProcessed);
      }
    }

    if (m_Socket->HasError() || m_Socket->HasFin()) {
      return m_Exiting;
    }

    if (GetPvPGN() && m_Socket->GetLastRecv() + REALM_APP_KEEPALIVE_IDLE_TIME < Time) {
      // Many PvPGN servers do not implement TCP Keep Alive. However, all PvPGN servers reply to BNET protocol null packets.
      int64_t expectedNullsSent = ((Time - m_Socket->GetLastRecv() - REALM_APP_KEEPALIVE_IDLE_TIME) / REALM_APP_KEEPALIVE_INTERVAL) + 1;
      if (expectedNullsSent > REALM_APP_KEEPALIVE_MAX_MISSED) {
        Print(GetLogPrefix() + "socket inactivity timeout");
        ResetConnection(false);
        return m_Exiting;
      }
      if (m_NullPacketsSent < expectedNullsSent) {
        SendAuth(m_Protocol->SEND_SID_NULL());
        ++m_NullPacketsSent;
        m_Socket->Flush();
      }
    }

    if (m_LoggedIn) {
      bool waitForPriority = false;
      if (m_ChatQueueJoinCallback && GetInChat()) {
        if (m_ChatQueueJoinCallback->GetIsStale()) {
          delete m_ChatQueueJoinCallback;
          m_ChatQueueJoinCallback = nullptr;
        } else {
          if (CheckWithinChatQuota(m_ChatQueueJoinCallback)) {
            if (SendQueuedMessage(m_ChatQueueJoinCallback)) {
              delete m_ChatQueueJoinCallback;
            }
            m_ChatQueueJoinCallback = nullptr;
          } else {
            waitForPriority = true;
          }
        }
      }
      if (m_ChatQueueGameHostWhois) {
        if (m_ChatQueueGameHostWhois->GetIsStale()) {
          delete m_ChatQueueGameHostWhois;
          m_ChatQueueGameHostWhois = nullptr;
        } else {
          if (CheckWithinChatQuota(m_ChatQueueGameHostWhois)) {
            if (SendQueuedMessage(m_ChatQueueGameHostWhois)) {
              delete m_ChatQueueGameHostWhois;
            }
            m_ChatQueueGameHostWhois = nullptr;
          } else {
            waitForPriority = true;
          }
        }
      }
      if (!waitForPriority) {
        while (!m_ChatQueueMain.empty()) {
          CQueuedChatMessage* nextMessage = m_ChatQueueMain.front();
          if (nextMessage->GetIsStale()) {
            delete nextMessage;
            m_ChatQueueMain.pop();
          } else {
            if (CheckWithinChatQuota(nextMessage)) {
              if (SendQueuedMessage(nextMessage)) {
                delete nextMessage;
              }
              m_ChatQueueMain.pop();
            } else {
              break;
            }
          }
        }
      }
    }

    if (Time - m_LastGameListTime >= 90) {
      TrySendGetGamesList();
    }

    m_Socket->DoSend(static_cast<fd_set*>(send_fd));
    return m_Exiting;
  }

  if (m_Config->m_Enabled && !m_Socket->GetConnected() && !m_Socket->GetConnecting() && !m_WaitingToConnect)
  {
    // the socket was disconnected
    ResetConnection(false);
    return m_Exiting;
  }

  if (m_Config->m_Enabled && !m_Socket->GetConnecting() && !m_Socket->GetConnected() && (m_ReconnectNextTick || (Time - m_LastDisconnectedTime >= m_ReconnectDelay)))
  {
    // attempt to connect to battle.net

    if (!m_FirstConnect) {
      Print(GetLogPrefix() + "reconnecting to [" + m_HostName + ":" + to_string(m_Config->m_ServerPort) + "]...");
    } else {
      if (m_Config->m_BindAddress.has_value()) {
        if (m_Aura->MatchLogLevel(LOG_LEVEL_INFO)) {
          Print(GetLogPrefix() + "connecting with local address [" + AddressToString(m_Config->m_BindAddress.value()) + "]...");
        }
      } else {
        if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
          Print(GetLogPrefix() + "connecting...");
        }
      }
    }
    m_FirstConnect = false;
    m_ReconnectNextTick = false;

    sockaddr_storage resolvedAddress;
    if (m_Aura->m_Net->ResolveHostName(resolvedAddress, ACCEPT_ANY, m_Config->m_HostName, m_Config->m_ServerPort)) {
      m_Socket->Connect(m_Config->m_BindAddress, resolvedAddress);
    } else {
      m_Socket->m_HasError = true;
    }
    m_WaitingToConnect          = false;
    m_LastConnectionAttemptTime = Time;
  }

  if (m_Socket->GetConnecting())
  {
    // we are currently attempting to connect to battle.net

    if (m_Socket->CheckConnect())
    {
      // the connection attempt completed

      ++m_SessionID;
      if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
        Print(GetLogPrefix() + "connected to [" + m_HostName + "]");
      }
      SendAuth(m_Protocol->SEND_PROTOCOL_INITIALIZE_SELECTOR());
      uint8_t gameVersion = m_Config->m_AuthWar3Version.has_value() ? m_Config->m_AuthWar3Version.value() : m_Aura->m_GameVersion;
      SendAuth(m_Protocol->SEND_SID_AUTH_INFO(gameVersion, m_Config->m_LocaleID, m_Config->m_CountryShort, m_Config->m_Country));
      m_Socket->DoSend(static_cast<fd_set*>(send_fd));
      m_LastGameListTime       = Time;
      return m_Exiting;
    }
    else if (Time - m_LastConnectionAttemptTime >= 10)
    {
      // the connection attempt timed out (10 seconds)

      Print(GetLogPrefix() + "failed to connect to [" + m_HostName + ":" + to_string(m_Config->m_ServerPort) + "]");
      Print(GetLogPrefix() + "waiting 90 seconds to retry...");
      m_Socket->Reset();
      m_Socket->SetKeepAlive(true, REALM_TCP_KEEPALIVE_IDLE_TIME);
      m_LastDisconnectedTime = Time;
      m_WaitingToConnect     = true;
      return m_Exiting;
    }
  }

  return m_Exiting;
}

void CRealm::ProcessChatEvent(const CIncomingChatEvent* chatEvent)
{
  CBNETProtocol::IncomingChatEvent Event    = chatEvent->GetChatEvent();
  bool                             Whisper  = (Event == CBNETProtocol::EID_WHISPER);
  string                           User     = chatEvent->GetUser();
  string                           Message  = chatEvent->GetMessage();

  if (!m_Socket->GetConnected()) {
    Print(GetLogPrefix() + "not connected - message from [" + User + "] rejected: [" + Message + "]");
    return;
  }

  // handle spoof checking for current game
  // this case covers whispers - we assume that anyone who sends a whisper to the bot with message "spoofcheck" should be considered spoof checked
  // note that this means you can whisper "spoofcheck" even in a public game to manually spoofcheck if the /whois fails

  if (Event == CBNETProtocol::EID_WHISPER && m_Aura->m_CurrentLobby && !m_Aura->m_CurrentLobby->GetIsMirror()) {
    if (Message == "s" || Message == "sc" || Message == "spoofcheck") {
      CGamePlayer* Player = m_Aura->m_CurrentLobby->GetPlayerFromName(User, true);
      if (Player) m_Aura->m_CurrentLobby->AddToRealmVerified(m_Config->m_HostName, Player, true);
      return;
    }
  }

  if (Event == CBNETProtocol::EID_WHISPER || Event == CBNETProtocol::EID_TALK) {
    m_HadChatActivity = true;
  }

  if (Event == CBNETProtocol::EID_TALK) {
    // Bots on servers with realm_x_mirror = yes ignore commands at channels (but not whispers).
    if (m_Config->m_IsMirror)
      return;
  }

  if (Event == CBNETProtocol::EID_WHISPER || Event == CBNETProtocol::EID_TALK) {
    if (User == GetLoginName()) {
      return;
    }
    if (Whisper)
      Print("[WHISPER: " + m_Config->m_UniqueName + "] [" + User + "] " + Message);
    else
      Print("[CHAT: " + m_Config->m_UniqueName + "] [" + User + "] " + Message);

    // handle bot commands

    if (Message.empty()) {
      return;
    }

    string cmdToken, command, payload;
    uint8_t tokenMatch = ExtractMessageTokensAny(Message, m_Config->m_PrivateCmdToken, m_Config->m_BroadcastCmdToken, cmdToken, command, payload);
    if (tokenMatch == COMMAND_TOKEN_MATCH_NONE) {
      if (Whisper) {
        string tokenName = GetTokenName(m_Config->m_PrivateCmdToken);
        string example = m_Aura->m_Net->m_Config->m_AllowDownloads ? "host wc3maps-8" : "host castle";
        QueueWhisper("Hi, " + User + ". Use " + m_Config->m_PrivateCmdToken + tokenName + " for commands. Example: " + m_Config->m_PrivateCmdToken + example, User);
      }
      return;
    }
    CCommandContext* ctx = new CCommandContext(m_Aura, m_Config->m_CommandCFG, this, User, Whisper, !Whisper && tokenMatch == COMMAND_TOKEN_MATCH_BROADCAST, &std::cout);
    ctx->Run(cmdToken, command, payload);
    m_Aura->UnholdContext(ctx);
  }
  else if (Event == CBNETProtocol::EID_CHANNEL)
  {
    Print(GetLogPrefix() + "joined channel [" + Message + "]");
    m_CurrentChannel = Message;
  } else if (Event == CBNETProtocol::EID_WHISPERSENT) {
    Print(GetLogPrefix() + "whisper sent OK [" + Message + "]");
    if (!m_ChatSentWhispers.empty()) {
      CQueuedChatMessage* oldestWhisper = m_ChatSentWhispers.front();
      if (oldestWhisper->IsProxySent()) {
        CCommandContext* fromCtx = oldestWhisper->GetProxyCtx();
        if (fromCtx->CheckActionMessage(Message) && !fromCtx->GetPartiallyDestroyed()) {
          fromCtx->SendReply("Message sent to " + oldestWhisper->GetReceiver() + ".");
        }
        fromCtx->ClearActionMessage();
      }
      delete oldestWhisper;
      m_ChatSentWhispers.pop();
    }
  } else if (Event == CBNETProtocol::EID_INFO) {
    bool LogInfo = m_HadChatActivity;

    // extract the first word which we hope is the username
    // this is not necessarily true though since info messages also include channel MOTD's and such

    if (m_Aura->m_CurrentLobby && !m_Aura->m_CurrentLobby->GetIsMirror()) {
      string            UserName;
      string::size_type Split = Message.find(' ');

      if (Split != string::npos)
        UserName = Message.substr(0, Split);
      else
        UserName = Message;

      CGamePlayer* AboutPlayer = m_Aura->m_CurrentLobby->GetPlayerFromName(UserName, true);
      if (AboutPlayer && AboutPlayer->GetRealmInternalID() == m_InternalServerID) {
        // handle spoof checking for current game
        // this case covers whois results which are used when hosting a public game (we send out a "/whois [player]" for each player)
        // at all times you can still /w the bot with "spoofcheck" to manually spoof check

        if (Message.find("Throne in game") != string::npos || Message.find("currently in  game") != string::npos || Message.find("currently in private game") != string::npos) {
          // note: if the game is rehosted, bnet will not be aware of the game being renamed
          string GameName = GetPrefixedGameName(m_Aura->m_CurrentLobby->GetGameName());
          string::size_type GameNameSize = GameName.length() + 2;
          string::size_type GameNameFoundPos = Message.find("\"" + GameName + "\"");
          if (GameNameFoundPos != string::npos && GameNameFoundPos + GameNameSize + 1 == Message.length()) {
            m_Aura->m_CurrentLobby->AddToRealmVerified(m_HostName, AboutPlayer, true);
          } else {
            m_Aura->m_CurrentLobby->ReportSpoofed(m_HostName, AboutPlayer);
          }
        } else {
          // [ERROR] Unknown user.
          // [INFO] User was last seen on:
        }
      }
    }
    if (LogInfo) {
      Print("[INFO: " + m_Config->m_UniqueName + "] " + Message);
    }
  } else if (Event == CBNETProtocol::EID_ERROR) {
    // Note that the default English error message <<That user is not logged on.>> is also received in other two circumstances:
    // - When sending /netinfo <USER>, if the bot has admin permissions in this realm.
    // - When sending /ban <USER>, if the bot has admin permissions in this realm.
    //
    // Therefore, abuse of the !SAY command will temporarily break feedback for !TELL/INVITE, until m_ChatSentWhispers is emptied.
    // This is (yet another reason) why !SAY /COMMAND must always be gated behind sudo.
    if (!m_ChatSentWhispers.empty() && Message.length() == m_Config->m_WhisperErrorReply.length() && Message == m_Config->m_WhisperErrorReply) {
      m_AnyWhisperRejected = true;
      CQueuedChatMessage* oldestWhisper = m_ChatSentWhispers.front();
      if (oldestWhisper->IsProxySent()) {
        CCommandContext* fromCtx = oldestWhisper->GetProxyCtx();
        if (!fromCtx->GetPartiallyDestroyed()) {
          fromCtx->SendReply(oldestWhisper->GetReceiver() + " is offline.");
        }
        fromCtx->ClearActionMessage();
      }
      delete oldestWhisper;
      m_ChatSentWhispers.pop();
    }
    Print("[NOTE: " + m_Config->m_UniqueName + "] " + Message);
  }
}

uint8_t CRealm::CountChatQuota()
{
  if (m_ChatQuotaInUse.empty()) return 0;
  int64_t minTicks = GetTicks() - static_cast<int64_t>(m_Config->m_FloodQuotaTime) * 60000 - 300; // 300 ms hardcoded latency
  uint16_t spentQuota = 0;
  for (auto it = begin(m_ChatQuotaInUse); it != end(m_ChatQuotaInUse);) {
    if ((*it).first < minTicks) {
      it = m_ChatQuotaInUse.erase(it);
    } else {
      spentQuota += (*it).second;
      if (0xFF <= spentQuota) break;
      ++it;
    }
  }
  if (0xFF < spentQuota) return 0xFF;
  return static_cast<uint8_t>(spentQuota);
}

bool CRealm::CheckWithinChatQuota(CQueuedChatMessage* message)
{
  if (m_Config->m_FloodImmune) return true;
  uint16_t spentQuota = CountChatQuota();
  if (m_Config->m_FloodQuotaLines <= spentQuota) {
    message->SetWasThrottled(true);
    return false;
  }
  const bool success = message->SelectSize(m_Config->m_VirtualLineLength, m_CurrentChannel) + spentQuota <= m_Config->m_FloodQuotaLines;
  if (!success) message->SetWasThrottled(true);
  return success;
}

bool CRealm::SendQueuedMessage(CQueuedChatMessage* message)
{
  uint8_t selectType;
  if (m_Aura->MatchLogLevel(LOG_LEVEL_INFO)) {
    if (message->GetWasThrottled()) {
      Print(GetLogPrefix() + "sent (was throttled) <<" + message->GetInnerMessage() + ">>");
    } else {
      Print(GetLogPrefix() + "sent <<" + message->GetInnerMessage() + ">>");
    }
  }
  Send(message->SelectBytes(m_CurrentChannel, selectType));
  if (message->GetSendsEarlyFeedback()) {
    message->SendEarlyFeedback();
  }
  if (selectType == CHAT_RECV_SELECTED_WHISPER) {
    m_ChatSentWhispers.push(message);
    if (m_ChatSentWhispers.size() > 25 && !m_AnyWhisperRejected && m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
      Print(GetLogPrefix() + "warning - " + to_string(m_ChatSentWhispers.size()) + " sent whispers have not been confirmed by the server");
      Print(GetLogPrefix() + "warning - <" + m_Config->m_CFGKeyPrefix + "protocol.whisper.error_reply = " + m_Config->m_WhisperErrorReply + "> may not match the language of this realm's system messages.");
    }
    // If whisper, return false, meaning that caller must not delete the message.
    return false;
  }
  if (!m_Config->m_FloodImmune) {
    uint8_t extraQuota = message->GetVirtualSize(m_Config->m_VirtualLineLength, selectType);
    m_ChatQuotaInUse.push_back(make_pair(GetTicks(), extraQuota));
  }

  switch (message->GetCallback()) {
    case CHAT_CALLBACK_REFRESH_GAME:
      m_ChatQueuedGameAnnouncement = false;
      //assert(m_Aura->m_CurrentLobby != nullptr);
      m_Aura->m_CurrentLobby->AnnounceToRealm(this);
      break;

    default:
      // Do nothing
      break;
  }

  return true;
}

bool CRealm::GetEnabled() const
{
  return m_Config->m_Enabled;
}

bool CRealm::GetPvPGN() const
{
  return m_Config->m_AuthPasswordHashType == REALM_AUTH_PVPGN;
}

string CRealm::GetServer() const
{
  return m_Config->m_HostName;
}

uint16_t CRealm::GetServerPort() const
{
  return m_Config->m_ServerPort;
}

string CRealm::GetInputID() const
{
  return m_Config->m_InputID;
}

string CRealm::GetUniqueDisplayName() const
{
  return m_Config->m_UniqueName;
}

string CRealm::GetCanonicalDisplayName() const
{
  return m_Config->m_CanonicalName;
}

string CRealm::GetDataBaseID() const
{
  return m_Config->m_DataBaseID;
}

string CRealm::GetLogPrefix() const
{
  return "[BNET: " + m_Config->m_UniqueName + "] ";
}

string CRealm::GetLoginName() const
{
  return m_Config->m_UserName;
}

bool CRealm::GetIsMirror() const
{
  return m_Config->m_IsMirror;
}

bool CRealm::GetIsVPN() const
{
  return m_Config->m_IsVPN;
}

bool CRealm::GetUsesCustomIPAddress() const
{
  return m_Config->m_EnableCustomAddress;
}

bool CRealm::GetUsesCustomPort() const
{
  return m_Config->m_EnableCustomPort;
}

sockaddr_storage* CRealm::GetPublicHostAddress() const
{
  return &(m_Config->m_PublicHostAddress);
}

uint16_t CRealm::GetPublicHostPort() const
{
  return m_Config->m_PublicHostPort;
}

uint32_t CRealm::GetMaxUploadSize() const
{
  return m_Config->m_MaxUploadSize;
}

bool CRealm::GetIsFloodImmune() const
{
  return m_Config->m_FloodImmune;
}

string CRealm::GetPrefixedGameName(const string& gameName) const
{
  if (gameName.length() + m_Config->m_GamePrefix.length() < 31) {
    // Check again just in case m_GamePrefix was reloaded and became prohibitively large.
    return m_Config->m_GamePrefix + gameName;
  } else {
    return gameName;
  }
}

bool CRealm::GetAnnounceHostToChat() const
{
  return m_Config->m_AnnounceHostToChat;
}

bool CRealm::GetHasEnhancedAntiSpoof() const
{
  return (
    (m_Config->m_CommandCFG->m_Enabled && m_Config->m_UnverifiedRejectCommands) ||
    m_Config->m_UnverifiedCannotStartGame || m_Config->m_UnverifiedAutoKickedFromLobby ||
    m_Config->m_AlwaysSpoofCheckPlayers
  );
}

bool CRealm::GetUnverifiedCannotStartGame() const
{
  return m_Config->m_UnverifiedCannotStartGame;
}

bool CRealm::GetUnverifiedAutoKickedFromLobby() const
{
  return m_Config->m_UnverifiedAutoKickedFromLobby;
}

CCommandConfig* CRealm::GetCommandConfig() const
{
  return m_Config->m_CommandCFG;
}

string CRealm::GetCommandToken() const
{
  return m_Config->m_PrivateCmdToken;
}

void CRealm::SendGetFriendsList()
{
  Send(m_Protocol->SEND_SID_FRIENDLIST());
}

void CRealm::SendGetClanList()
{
  Send(m_Protocol->SEND_SID_CLANMEMBERLIST());
}

void CRealm::SendGetGamesList()
{
  Send(m_Protocol->SEND_SID_GETADVLISTEX());
  m_LastGameListTime = GetTime();
}

void CRealm::TrySendGetGamesList()
{
  if (m_Config->m_QueryGameLists) {
    SendGetGamesList();
  }
}

void CRealm::SendNetworkConfig()
{
  if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetIsMirror() && !m_Config->m_IsMirror) {
    SendAuth(m_Protocol->SEND_SID_PUBLICHOST(m_Aura->m_CurrentLobby->GetPublicHostAddress(), m_Aura->m_CurrentLobby->GetPublicHostPort()));
    m_GamePort = m_Aura->m_CurrentLobby->GetPublicHostPort();
  } else if (m_Config->m_EnableCustomAddress) {
    uint16_t port = 6112;
    if (m_Config->m_EnableCustomPort) {
      port = m_Config->m_PublicHostPort;
    } else if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetIsLobby()) {
      port = m_Aura->m_CurrentLobby->GetHostPort();
    }
    SendAuth(m_Protocol->SEND_SID_PUBLICHOST(AddressToIPv4Vector(&(m_Config->m_PublicHostAddress)), port));
    m_GamePort = port;
  } else if (m_Config->m_EnableCustomPort) {
    SendAuth(m_Protocol->SEND_SID_NETGAMEPORT(m_Config->m_PublicHostPort));
    m_GamePort = m_Config->m_PublicHostPort;
  }
}

void CRealm::AutoJoinChat()
{
  const string& targetChannel = m_AnchorChannel.empty() ? m_Config->m_FirstChannel : m_AnchorChannel;
  if (targetChannel.empty()) return;
  Send(m_Protocol->SEND_SID_JOINCHANNEL(targetChannel));
  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
    Print(GetLogPrefix() + "joining channel [" + targetChannel + "]");
  }
}

void CRealm::SendEnterChat()
{
  Send(m_Protocol->SEND_SID_ENTERCHAT());
}

void CRealm::TrySendEnterChat()
{
  if (!m_AnchorChannel.empty() || !m_Config->m_FirstChannel.empty()) {
    SendEnterChat();
  }
}

void CRealm::Send(const vector<uint8_t>& packet)
{
  if (m_LoggedIn) {
    SendAuth(packet);
  } else if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
    Print(GetLogPrefix() + "packet not sent (not logged in)");
  }
}

void CRealm::SendAuth(const vector<uint8_t>& packet)
{
  // This function is public only for the login phase.
  // Though it's also privately used by CRealm::Send.
  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
    Print(GetLogPrefix() + "sending packet - " + ByteArrayToHexString(packet));
  }
  m_Socket->PutBytes(packet);
}

void CRealm::OnLoginOkay()
{
  m_LoggedIn = true;
  Print(GetLogPrefix() + "logged in as [" + m_Config->m_UserName + "]");

  TrySendGetGamesList();
  SendGetFriendsList();
  SendGetClanList();

  TrySendEnterChat();
  TryQueueGameChatAnnouncement(m_Aura->m_CurrentLobby);
}

CQueuedChatMessage* CRealm::QueueCommand(const string& message, CCommandContext* fromCtx, const bool isProxy)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  if (!m_Config->m_FloodImmune && message.length() > m_Config->m_MaxLineLength) {
    return nullptr;
  }

  CQueuedChatMessage* entry = new CQueuedChatMessage(this, fromCtx, isProxy);
  entry->SetMessage(message);
  entry->SetReceiver(RECV_SELECTOR_SYSTEM);
  m_ChatQueueMain.push(entry);
  m_HadChatActivity = true;

  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
    Print(GetLogPrefix() + "queued command \"" + entry->GetInnerMessage() + "\"");
  }
  return entry;
}

CQueuedChatMessage* CRealm::QueuePriorityWhois(const string& message)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  if (!m_Config->m_FloodImmune && message.length() > m_Config->m_MaxLineLength) {
    return nullptr;
  }

  if (m_ChatQueueGameHostWhois) {
    delete m_ChatQueueGameHostWhois;
  }
  m_ChatQueueGameHostWhois = new CQueuedChatMessage(this, nullptr, false);
  m_ChatQueueGameHostWhois->SetMessage(message);
  m_ChatQueueGameHostWhois->SetReceiver(RECV_SELECTOR_SYSTEM);
  m_HadChatActivity = true;

  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
    Print(GetLogPrefix() + "queued fast spoofcheck \"" + m_ChatQueueGameHostWhois->GetInnerMessage() + "\"");
  }
  return m_ChatQueueGameHostWhois;
}

CQueuedChatMessage* CRealm::QueueChatChannel(const string& message, CCommandContext* fromCtx, const bool isProxy)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  CQueuedChatMessage* entry = new CQueuedChatMessage(this, fromCtx, isProxy);
  if (!m_Config->m_FloodImmune && m_Config->m_MaxLineLength < message.length()) {
    entry->SetMessage(message.substr(0, m_Config->m_MaxLineLength));
  } else {
    entry->SetMessage(message);
  }
  entry->SetReceiver(RECV_SELECTOR_ONLY_PUBLIC);
  m_ChatQueueMain.push(entry);
  m_HadChatActivity = true;

  Print(GetLogPrefix() + "queued chat message \"" + entry->GetInnerMessage() + "\"");
  return entry;
}

CQueuedChatMessage* CRealm::QueueChatReply(const uint8_t messageValue, const string& message, const string& user, const uint8_t selector, CCommandContext* fromCtx, const bool isProxy)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  CQueuedChatMessage* entry = new CQueuedChatMessage(this, fromCtx, isProxy);
  entry->SetMessage(messageValue, message);
  entry->SetReceiver(selector, user);
  m_ChatQueueMain.push(entry);
  m_HadChatActivity = true;

  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
    Print(GetLogPrefix() + "queued reply to [" + user + "] - \"" + entry->GetInnerMessage() + "\"");
  }
  return entry;
}

CQueuedChatMessage* CRealm::QueueWhisper(const string& message, const string& user, CCommandContext* fromCtx, const bool isProxy)
{
  if (message.empty() || !m_LoggedIn)
    return nullptr;

  CQueuedChatMessage* entry = new CQueuedChatMessage(this, fromCtx, isProxy);
  if (!m_Config->m_FloodImmune && (m_Config->m_MaxLineLength - 20u) < message.length()) {
    entry->SetMessage(message.substr(0, m_Config->m_MaxLineLength - 20u));
  } else {
    entry->SetMessage(message);
  }
  entry->SetReceiver(RECV_SELECTOR_ONLY_WHISPER, user);
  m_ChatQueueMain.push(entry);
  m_HadChatActivity = true;

  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
    Print(GetLogPrefix() + "queued whisper to [" + user + "] - \"" + entry->GetInnerMessage() + "\"");
  }
  return entry;
}

void CRealm::TryQueueGameChatAnnouncement(const CGame* game)
{
  if (!game || !m_LoggedIn) {
    return;
  }

  if (game->GetIsMirror() && GetIsMirror()) {
    // A mirror realm is a realm whose purpose is to mirror games actually hosted by Aura.
    // Do not display external games in those realms.
    return;
  }

  if (game->GetIsRealmExcluded(GetServer())) {
    return;
  }

  if (game->GetDisplayMode() == GAME_PUBLIC && GetAnnounceHostToChat()) {
    QueueGameChatAnnouncement(game);
    return;
  }
}

CQueuedChatMessage* CRealm::QueueGameChatAnnouncement(const CGame* game, CCommandContext* fromCtx, const bool isProxy)
{
  if (!m_LoggedIn)
    return nullptr;

  m_ChatQueuedGameAnnouncement = true;

  m_ChatQueueJoinCallback = new CQueuedChatMessage(this, fromCtx, isProxy);
  m_ChatQueueJoinCallback->SetMessage(game->GetAnnounceText(this));
  m_ChatQueueJoinCallback->SetReceiver(RECV_SELECTOR_ONLY_PUBLIC);
  m_ChatQueueJoinCallback->SetCallback(CHAT_CALLBACK_REFRESH_GAME);
  m_ChatQueueJoinCallback->SetValidator(CHAT_VALIDATOR_CURRENT_LOBBY);
  m_HadChatActivity = true;

  if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
    Print(GetLogPrefix() + "queued game announcement");
  }
  return m_ChatQueueJoinCallback;
}

void CRealm::TryQueueChat(const string& message, const string& user, bool isPrivate, CCommandContext* fromCtx, const uint8_t ctxFlags)
{
  // TODO(IceSandslash): TryQueueChat ctxFlags
  // don't respond to non admins if there are more than 25 messages already in the queue
  // this prevents malicious users from filling up the bot's chat queue and crippling the bot
  // in some cases the queue may be full of legitimate messages but we don't really care if the bot ignores one of these commands once in awhile
  // e.g. when several users join a game at the same time and cause multiple /whois messages to be queued at once

  if (m_ChatQueueMain.size() >= 25 && !(GetIsFloodImmune() || GetIsModerator(user) || GetIsAdmin(user) || GetIsSudoer(user))) {
    if (m_Aura->MatchLogLevel(LOG_LEVEL_WARNING)) {
      Print(GetLogPrefix() + "warning - " + to_string(m_ChatQueueMain.size()) + " queued messages");
      Print(GetLogPrefix() + message);
      Print("[AURA] Quota exceeded (reply dropped.)");
    }
    return;
  }

  if (isPrivate) {
    QueueWhisper(message, user);
  } else {
    QueueChatChannel(message);
  }
}


void CRealm::SendGameRefresh(const uint8_t displayMode, const CGame* game)
{
  if (!m_LoggedIn)
    return;

  const uint16_t connectPort = GetUsesCustomPort() && !game->GetIsMirror() ? GetPublicHostPort() : game->GetHostPort();
  if (m_GamePort != connectPort) {
    Send(m_Protocol->SEND_SID_NETGAMEPORT(connectPort));
    m_GamePort = connectPort;
  }

  Send(m_Protocol->SEND_SID_STARTADVEX3(
    displayMode,
    game->GetGameType(),
    game->GetMap()->GetMapGameFlags(),
    game->GetAnnounceWidth(),
    game->GetAnnounceHeight(),
    GetPrefixedGameName(game->GetGameName()), m_Config->m_UserName,
    game->GetUptime(),
    game->GetSourceFilePath(),
    game->GetSourceFileHash(),
    game->GetSourceFileSHA1(),

  // construct a fixed host counter which will be used to identify players from this realm
  // the fixed host counter's highest-order byte will contain a 8 bit ID (0-255)
  // the rest of the fixed host counter will contain the 24 least significant bits of the actual host counter
  // since we're destroying 8 bits of information here the actual host counter should not be greater than 2^24 which is a reasonable assumption
  // when a player joins a game we can obtain the ID from the received host counter
  // note: LAN broadcasts use an ID of 0, battle.net refreshes use IDs of 16-255, the rest are reserved

    game->GetHostCounter() | (game->GetIsMirror() ? 0 : (m_PublicServerID << 24)),
    m_Aura->m_MaxSlots
  ));

  if (!m_CurrentChannel.empty()) {
    m_AnchorChannel = m_CurrentChannel;
    m_CurrentChannel.clear();
  }
}

void CRealm::QueueGameUncreate()
{
  ResetGameAnnouncement();
  Send(m_Protocol->SEND_SID_STOPADV());
}

void CRealm::ResetConnection(bool Errored)
{
  m_LastDisconnectedTime = GetTime();
  m_BNCSUtil->Reset(m_Config->m_UserName, m_Config->m_PassWord);

  if (m_Socket) {
    m_Socket->Reset();
    m_Socket->SetKeepAlive(true, REALM_TCP_KEEPALIVE_IDLE_TIME);
    if (Errored) {
      Print(GetLogPrefix() + "disconnected due to socket error");
    } else {
      Print(GetLogPrefix() + "disconnected");
    }
  }

  m_LoggedIn         = false;
  m_CurrentChannel.clear();
  m_AnchorChannel.clear();
  m_WaitingToConnect = true;

  m_ChatQuotaInUse.clear();
  if (m_ChatQueueJoinCallback) {
    delete m_ChatQueueJoinCallback;
    m_ChatQueueJoinCallback = nullptr;
  }
  if (m_ChatQueueGameHostWhois) {
    delete m_ChatQueueGameHostWhois;
    m_ChatQueueGameHostWhois = nullptr;
  }
  while (!m_ChatQueueMain.empty()) {
    delete m_ChatQueueMain.front();
    m_ChatQueueMain.pop();
  }
  while (!m_ChatSentWhispers.empty()) {
    delete m_ChatSentWhispers.front();
    m_ChatSentWhispers.pop();
  }
}

bool CRealm::GetIsModerator(string name) const
{
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });

  if (m_Aura->m_DB->ModeratorCheck(m_Config->m_DataBaseID, name))
    return true;

  return false;
}

bool CRealm::GetIsAdmin(string name) const
{
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  return m_Config->m_Admins.find(name) != m_Config->m_Admins.end();
}

bool CRealm::GetIsSudoer(string name) const
{
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  return m_Config->m_SudoUsers.find(name) != m_Config->m_SudoUsers.end();
}

bool CRealm::IsBannedName(string name) const
{
  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });
  CDBBan* Ban = m_Aura->m_DB->BanCheck(m_Config->m_DataBaseID, name);
  if (!Ban) return false;
  delete Ban;
  return true;
}

void CRealm::HoldFriends(CGame* game)
{
  for (auto& friend_ : m_Friends)
    game->AddToReserved(friend_);
}

void CRealm::HoldClan(CGame* game)
{
  for (auto& clanmate : m_Clan)
    game->AddToReserved(clanmate);
}

