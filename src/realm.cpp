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
#include "irc.h"
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
    m_FirstChannel(nRealmConfig->m_FirstChannel),
    m_HostName(nRealmConfig->m_HostName),
    m_ServerIndex(nRealmConfig->m_ServerIndex),
    m_InternalServerID(nAura->NextServerID()),
    m_PublicServerID(nRealmConfig->m_ServerIndex + 15),
    m_LastDisconnectedTime(0),
    m_LastConnectionAttemptTime(0),
    m_LastGameListTime(0),
    m_LastOutPacketTicks(0),
    m_LastAdminRefreshTime(GetTime()),
    m_LastBanRefreshTime(GetTime()),
    m_LastOutPacketSize(0),
    m_SessionID(0),
    m_Exiting(false),
    m_FirstConnect(true),
    m_ReconnectNextTick(true),
    m_WaitingToConnect(true),
    m_LoggedIn(false),
    m_InChat(false),
    m_HadChatActivity(false)
{
  m_ReconnectDelay = GetPvPGN() ? 90 : 240;
}

CRealm::~CRealm()
{
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
}

uint32_t CRealm::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  if (m_Socket && !m_Socket->HasError() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 1;
  }

  return 0;
}

bool CRealm::Update(void* fd, void* send_fd)
{
  const int64_t Ticks = GetTicks(), Time = GetTime();

  // we return at the end of each if statement so we don't have to deal with errors related to the order of the if statements
  // that means it might take a few ms longer to complete a task involving multiple steps (in this case, reconnecting) due to blocking or sleeping
  // but it's not a big deal at all, maybe 100ms in the worst possible case (based on a 50ms blocking time)

  if (!m_Socket) {
    m_Socket = new CTCPClient(AF_INET, m_Config->m_HostName);
  }

  if (m_Socket->HasError())
  {
    // the socket has an error
    ResetConnection(true);
    Print("[BNET: " + m_Config->m_UniqueName + "] waiting " + to_string(m_ReconnectDelay) + " seconds to reconnect");
    return m_Exiting;
  }

  if (m_Socket->GetConnected())
  {
    // the socket is connected and everything appears to be working properly

    m_Socket->DoRecv(static_cast<fd_set*>(fd));

    // extract as many packets as possible from the socket's receive buffer and put them in the m_Packets queue

    string*              RecvBuffer      = m_Socket->GetBytes();
    std::vector<uint8_t> Bytes           = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());

    CIncomingChatEvent* ChatEvent;

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while (Bytes.size() >= 4)
    {
      // bytes 2 and 3 contain the length of the packet

      const uint16_t             Length = static_cast<uint16_t>(Bytes[3] << 8 | Bytes[2]);
      if (Length < 4 || Bytes.size() < Length) break;
      const std::vector<uint8_t> Data   = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      // byte 0 is always 255
      if (Bytes[0] == BNET_HEADER_CONSTANT)
      {
        switch (Bytes[1])
        {
          case CBNETProtocol::SID_NULL:
            // warning: we do not respond to NULL packets with a NULL packet of our own
            // this is because PVPGN servers are programmed to respond to NULL packets so it will create a vicious cycle of useless traffic
            // official battle.net servers do not respond to NULL packets

            m_Protocol->RECEIVE_SID_NULL(Data);
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
              //Print("[BNET: " + m_Config->m_UniqueName + "@" + ipString + "] sending game list to " + m_Aura->m_Net->m_Config->m_UDPForwardAddress + ":" + to_string(m_Aura->m_Net->m_Config->m_UDPForwardPort) + " (" + to_string(relayPacket.size()) + " bytes)");
              m_Aura->m_Net->Send(&(m_Aura->m_Net->m_Config->m_UDPForwardAddress), relayPacket);
            }

            break;

          case CBNETProtocol::SID_ENTERCHAT:
            if (m_Protocol->RECEIVE_SID_ENTERCHAT(Data)) {
              //Print("[BNET: " + m_Config->m_UniqueName + "] joining channel [" + m_FirstChannel + "]");
              JoinFirstChannel();
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
            if (m_Protocol->RECEIVE_SID_STARTADVEX3(Data)) {
              m_InChat = false;
            } else {
              Print("[BNET: " + m_Config->m_UniqueName + "] Failed to publish hosted game");
              m_Aura->EventBNETGameRefreshFailed(this);
            }

            break;

          case CBNETProtocol::SID_PING:
            m_Socket->PutBytes(m_Protocol->SEND_SID_PING(m_Protocol->RECEIVE_SID_PING(Data)));
            break;

          case CBNETProtocol::SID_AUTH_INFO: {
            if (!m_Protocol->RECEIVE_SID_AUTH_INFO(Data))
              break;

            bool versionSuccess = m_BNCSUtil->HELP_SID_AUTH_CHECK(m_Aura->m_GameInstallPath, m_Config->m_CDKeyROC, m_Config->m_CDKeyTFT, m_Protocol->GetValueStringFormulaString(), m_Protocol->GetIX86VerFileNameString(), m_Protocol->GetClientToken(), m_Protocol->GetServerToken(), m_Aura->m_GameVersion);
            if (versionSuccess || m_Config->m_AuthSkipVersionCheck) {
              // override the exe information generated by bncsutil if specified in the config file
              if (m_Config->m_AuthExeVersion.has_value()) {
                m_BNCSUtil->SetEXEVersion(m_Config->m_AuthExeVersion.value());
              }
              if (m_Config->m_AuthExeVersionHash.has_value()) {
                m_BNCSUtil->SetEXEVersionHash(m_Config->m_AuthExeVersionHash.value());
              }
              if (!m_Config->m_AuthExeInfo.empty()) {
                m_BNCSUtil->SetEXEInfo(m_Config->m_AuthExeInfo);
              }
              vector<uint8_t> exeVersion = m_BNCSUtil->GetEXEVersion();
              vector<uint8_t> exeVersionHash = m_BNCSUtil->GetEXEVersionHash();
              string exeInfo = m_BNCSUtil->GetEXEInfo();
              if (exeVersion.size() != 4) {
                exeVersion = {173, 1, 27, 1};
                m_BNCSUtil->SetEXEVersion(exeVersion);
                Print("[BNET: " + m_Config->m_UniqueName + "] defaulting to <global_realm.auth_exe_version = " + ByteArrayToDecString(exeVersion) + ">");
              }
              if (exeVersionHash.size() != 4) {
                exeVersionHash = {72, 160, 171, 170};
                m_BNCSUtil->SetEXEVersionHash(exeVersionHash);
                Print("[BNET: " + m_Config->m_UniqueName + "] defaulting to <global_realm.auth_exe_version_hash = " + ByteArrayToDecString(exeVersionHash) + ">");
              }
              if (exeInfo.empty()) {
                exeInfo = "war3.exe 15/03/16 00:00:00 515048";
                m_BNCSUtil->SetEXEInfo(exeInfo);
                Print("[BNET: " + m_Config->m_UniqueName + "] defaulting to <global_realm.auth_exe_info = " + exeInfo + ">");
              }

              Print(
                "[BNET: " + m_Config->m_UniqueName + "] attempting to auth as WC3: TFT v" +
                to_string(exeVersion[3]) + "." + to_string(exeVersion[2]) + std::string(1, char(97 + exeVersion[1])) +
                " (Build " + to_string(exeVersion[0]) + ") - " +
                "version hash <" + ByteArrayToDecString(exeVersionHash) + ">"
              );

              QueuePacket(m_Protocol->SEND_SID_AUTH_CHECK(m_Protocol->GetClientToken(), exeVersion, exeVersionHash, m_BNCSUtil->GetKeyInfoROC(), m_BNCSUtil->GetKeyInfoTFT(), exeInfo, "Aura"), PACKET_TYPE_PRIORITY);
              //QueuePacket(m_Protocol->SEND_SID_NULL());
              SendNetworkConfig();
            } else {
              if (m_Config->m_AuthPasswordHashType == REALM_AUTH_PVPGN) {
                Print("[BNET: " + m_Config->m_UniqueName + "] config error - misconfigured <game.install_path>");
              } else {
                Print("[BNET: " + m_Config->m_UniqueName + "] config error - misconfigured <game.install_path>, or <realm_" + to_string(m_ServerIndex) + ".cd_key.roc>, or <realm_" + to_string(m_ServerIndex) + ".cd_key.tft>");
              }
              Print("[BNET: " + m_Config->m_UniqueName + "] bncsutil key hash failed, disconnecting...");
              m_Socket->Disconnect();
            }

            break;
          }

          case CBNETProtocol::SID_AUTH_CHECK:
            if (m_Protocol->RECEIVE_SID_AUTH_CHECK(Data))
            {
              // cd keys accepted
              //Print("[BNET: " + m_Config->m_UniqueName + "] game OK");
              m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON();
              QueuePacket(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGON(m_BNCSUtil->GetClientKey(), m_Config->m_UserName), PACKET_TYPE_PRIORITY);
            }
            else
            {
              // cd keys not accepted

              switch (ByteArrayToUInt32(m_Protocol->GetKeyState(), false))
              {
                case CBNETProtocol::KR_ROC_KEY_IN_USE:
                  Print("[BNET: " + m_Config->m_UniqueName + "] logon failed - ROC CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting...");
                  break;

                case CBNETProtocol::KR_TFT_KEY_IN_USE:
                  Print("[BNET: " + m_Config->m_UniqueName + "] logon failed - TFT CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting...");
                  break;

                case CBNETProtocol::KR_OLD_GAME_VERSION:
                case CBNETProtocol::KR_INVALID_VERSION:
                  Print("[BNET: " + m_Config->m_UniqueName + "] config error - rejected <realm_" + to_string(m_ServerIndex) + ".auth_exe_version = " + ByteArrayToDecString(m_BNCSUtil->GetEXEVersion()) + ">");
                  Print("[BNET: " + m_Config->m_UniqueName + "] config error - rejected <realm_" + to_string(m_ServerIndex) + ".auth_exe_version_hash = " + ByteArrayToDecString(m_BNCSUtil->GetEXEVersionHash()) + ">");
                  Print("[BNET: " + m_Config->m_UniqueName + "] logon failed - version not supported, or version hash invalid, disconnecting...");
                  break;

                default:
                  Print("[BNET: " + m_Config->m_UniqueName + "] logon failed - cd keys not accepted, disconnecting...");
                  break;
              }

              m_Socket->Disconnect();
            }

            break;

          case CBNETProtocol::SID_AUTH_ACCOUNTLOGON:
            if (m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGON(Data))
            {
              //Print("[BNET: " + m_Config->m_UniqueName + "] username [" + m_Config->m_UserName + "] OK");

              if (m_Config->m_AuthPasswordHashType == REALM_AUTH_PVPGN)
              {
                // pvpgn logon

                //Print("[BNET: " + m_Config->m_UniqueName + "] using pvpgn logon type");
                m_BNCSUtil->HELP_PvPGNPasswordHash(m_Config->m_PassWord);
                QueuePacket(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetPvPGNPasswordHash()), PACKET_TYPE_PRIORITY);
              }
              else
              {
                // battle.net logon

                //Print("[BNET: " + m_Config->m_UniqueName + "] using battle.net logon type");
                m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF(m_Protocol->GetSalt(), m_Protocol->GetServerPublicKey());
                QueuePacket(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetM1()), PACKET_TYPE_PRIORITY);
              }
            }
            else
            {
              Print("[BNET: " + m_Config->m_UniqueName + "] logon failed - invalid username, disconnecting");
              m_Socket->Disconnect();
            }

            break;

          case CBNETProtocol::SID_AUTH_ACCOUNTLOGONPROOF:
            if (m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF(Data))
            {
              // logon successful

              Print("[BNET: " + m_Config->m_UniqueName + "] logged in as [" + m_Config->m_UserName + "]");
              m_LoggedIn = true;
              QueuePacket(m_Protocol->SEND_SID_GETADVLISTEX(), PACKET_TYPE_GAME_LIST | PACKET_TYPE_PRIORITY);
              QueuePacket(m_Protocol->SEND_SID_ENTERCHAT(), PACKET_TYPE_PRIORITY);
              //QueuePacket(m_Protocol->SEND_SID_FRIENDLIST(), PACKET_TYPE_PRIORITY);
              //QueuePacket(m_Protocol->SEND_SID_CLANMEMBERLIST(), PACKET_TYPE_PRIORITY);
            }
            else
            {
              Print("[BNET: " + m_Config->m_UniqueName + "] logon failed - invalid password, disconnecting");
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

      Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
    }

    RecvBuffer->clear();

    // check if at least one packet is waiting to be sent and if we've waited long enough to prevent flooding
    // this formula has changed many times but currently we wait 1 second if the last packet was "small", 3.5 seconds if it was "medium", and 4 seconds if it was "big"

    int64_t WaitTicks;

    if (GetIsFloodImmune())
      WaitTicks = 150;
    else if (m_LastOutPacketSize < 10)
      WaitTicks = 1300;
    else if (m_LastOutPacketSize < 100)
      WaitTicks = 3300;
    else
      WaitTicks = 4300;

    while (!m_OutPackets.empty()) {
      tuple<int64_t, uint32_t, uint8_t, vector<uint8_t>> curPacket = m_OutPackets.front();
      uint8_t packetType = get<2>(curPacket);
      if (!GetIsFloodImmune() && (0 == (packetType & PACKET_TYPE_PRIORITY)) && Ticks < m_LastOutPacketTicks + WaitTicks) {
        break;
      }
      if (get<0>(curPacket) + 30000 < Ticks) {
        // Expired
        m_OutPackets.pop();
        Print(GetLogPrefix() + "packet dropped after 30s in queue.");
        continue;
      }
      if (!GetIsFloodImmune() && m_OutPackets.size() > 20 && (0 == (packetType & PACKET_TYPE_PRIORITY))) {
        m_OutPackets.pop();
        Print(GetLogPrefix() + "too many packets queued (" + to_string(m_OutPackets.size()) + ") - some will be dropped");
        continue;
      }

      bool skipIteration = false;
      switch (packetType) {
        case PACKET_TYPE_GAME_REFRESH: {
          if (!m_Aura->m_CurrentLobby || get<1>(curPacket) != m_Aura->m_CurrentLobby->GetHostCounter()) {
            skipIteration = true;
          }
          break;
        }
        default: {
          if (m_SessionID != get<1>(curPacket)) {
            skipIteration = true;
          }
          break;
        }
      }

      if (skipIteration) {
        m_OutPackets.pop();
        continue;
      }

      if (packetType == PACKET_TYPE_CHAT_BLOCKING && !m_InChat) {
        // Block until we join a chat room.
        break;
      }
      if (packetType == PACKET_TYPE_GAME_REFRESH) {
        m_InChat = false;
      }
      if (packetType == PACKET_TYPE_GAME_LIST) {
        // Game list query
        m_LastGameListTime = Time;
      }
      m_LastOutPacketSize = m_Socket->PutBytes(get<3>(curPacket));
      m_LastOutPacketTicks = Ticks;
      m_OutPackets.pop();
    }

    if (Time - m_LastGameListTime >= 90) {
      m_Socket->PutBytes(m_Protocol->SEND_SID_GETADVLISTEX());
      m_LastGameListTime = Time;
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
      Print("[BNET: " + m_Config->m_UniqueName + "] reconnecting to [" + m_HostName + ":6112]...");
    } else {
      if (m_Config->m_BindAddress.has_value()) {
        Print("[BNET: " + m_Config->m_UniqueName + "] connecting with local address [" + AddressToString(m_Config->m_BindAddress.value()) + "]...");
      }
    }
    m_FirstConnect = false;
    m_ReconnectNextTick = false;

    optional<sockaddr_storage> resolvedAddress = m_Aura->m_Net->ResolveHostName(m_Config->m_HostName);
    if (!resolvedAddress.has_value()) {
      m_Socket->m_HasError = true;
    } else if (resolvedAddress.value().ss_family == AF_INET6) {
      Print(GetLogPrefix() + "Provide IPv4 addresses for realms.");
      m_Socket->m_HasError = true;
    } else {
      m_Socket->Connect(m_Config->m_BindAddress, resolvedAddress.value(), 6112);
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
      Print("[BNET: " + m_Config->m_UniqueName + "] connected to [" + m_HostName + "]");
      QueuePacket(m_Protocol->SEND_PROTOCOL_INITIALIZE_SELECTOR(), PACKET_TYPE_PRIORITY);
      uint8_t gameVersion = m_Config->m_AuthWar3Version.has_value() ? m_Config->m_AuthWar3Version.value() : m_Aura->m_GameVersion;
      QueuePacket(m_Protocol->SEND_SID_AUTH_INFO(gameVersion, m_Config->m_LocaleID, m_Config->m_CountryShort, m_Config->m_Country), PACKET_TYPE_PRIORITY);
      m_Socket->DoSend(static_cast<fd_set*>(send_fd));
      m_LastGameListTime       = Time;
      m_LastOutPacketTicks = Ticks;

      return m_Exiting;
    }
    else if (Time - m_LastConnectionAttemptTime >= 10)
    {
      // the connection attempt timed out (10 seconds)

      Print("[BNET: " + m_Config->m_UniqueName + "] failed to connect to [" + m_HostName + ":6112]");
      Print("[BNET: " + m_Config->m_UniqueName + "] waiting 90 seconds to retry...");
      m_Socket->Reset();
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
    Print("[BNET: " + m_Config->m_UniqueName + "] not connected - message from [" + User + "] rejected: [" + Message + "]");
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

  if (Event == CBNETProtocol::EID_WHISPER || Event == CBNETProtocol::EID_TALK)
  {
    if (Whisper)
      Print("[WHISPER: " + m_Config->m_UniqueName + "] [" + User + "] " + Message);
    else
      Print("[LOCAL: " + m_Config->m_UniqueName + "] [" + User + "] " + Message);

    // handle bot commands

    if (Message.empty()) {
      return;
    }

    if (Message[0] != m_Config->m_CommandTrigger) {
      if (Whisper) {
        string tokenName;
        if (m_Config->m_CommandTrigger == '!') {
          tokenName = " (exclamation mark.)";
        } else if (m_Config->m_CommandTrigger == '?') {
          tokenName = " (question mark.)";
        } else if (m_Config->m_CommandTrigger == '.') {
          tokenName = " (period.)";
        } else if (m_Config->m_CommandTrigger == ',') {
          tokenName = " (comma.)";
        } else if (m_Config->m_CommandTrigger == '~') {
          tokenName = " (tilde.)";
        } else if (m_Config->m_CommandTrigger == '-') {
          tokenName = " (hyphen.)";
        } else if (m_Config->m_CommandTrigger == '#') {
          tokenName = " (hashtag.)";
        } else if (m_Config->m_CommandTrigger == '@') {
          tokenName = " (at.)";
        } else if (m_Config->m_CommandTrigger == '&') {
          tokenName = " (ampersand.)";
        } else if (m_Config->m_CommandTrigger == '$') {
          tokenName = " (dollar.)";
        } else if (m_Config->m_CommandTrigger == '%') {
          tokenName = " (percent.)";
        }
        SendWhisper("Hello, " + User + ". My commands start with " + string(1, m_Config->m_CommandTrigger) + tokenName + " Example: " + string(1, m_Config->m_CommandTrigger) + "host siege", User);
      }
      return;
    }

    char CommandToken = Message[0];
    string            Command, Payload;
    string::size_type PayloadStart = Message.find(' ');

    if (PayloadStart != string::npos) {
      Command = Message.substr(1, PayloadStart - 1);
      Payload = Message.substr(PayloadStart + 1);
    } else {
      Command = Message.substr(1);
    }

    CCommandContext* ctx = new CCommandContext(m_Aura, this, User, Whisper, &std::cout, CommandToken);
    ctx->Run(Command, Payload);
    m_Aura->UnholdContext(ctx);
  }
  else if (Event == CBNETProtocol::EID_CHANNEL)
  {
    // keep track of current channel so we can rejoin it after hosting a game

    Print("[BNET: " + m_Config->m_UniqueName + "] joined channel [" + Message + "]");

    m_InChat = true;
    m_CurrentChannel = Message;
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
    Print("[ERROR: " + m_Config->m_UniqueName + "] " + Message);
  }
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

string CRealm::GetCommandToken() const
{
  return std::string(1, m_Config->m_CommandTrigger);
}

void CRealm::SendGetFriendsList()
{
  if (m_LoggedIn)
    QueuePacket(m_Protocol->SEND_SID_FRIENDLIST());
}

void CRealm::SendGetClanList()
{
  if (m_LoggedIn)
    QueuePacket(m_Protocol->SEND_SID_CLANMEMBERLIST());
}

void CRealm::SendNetworkConfig()
{
  if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetIsMirror() && !m_Config->m_IsMirror) {
    QueuePacket(m_Protocol->SEND_SID_PUBLICHOST(m_Aura->m_CurrentLobby->GetPublicHostAddress(), m_Aura->m_CurrentLobby->GetPublicHostPort()), PACKET_TYPE_PRIORITY);
  } else if (m_Config->m_EnableCustomAddress) {
    uint16_t port = 6112;
    if (m_Config->m_EnableCustomPort) {
      port = m_Config->m_PublicHostPort;
    } else if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetIsLobby()) {
      port = m_Aura->m_CurrentLobby->GetHostPort();
    }
    QueuePacket(m_Protocol->SEND_SID_PUBLICHOST(AddressToIPv4Vector(&(m_Config->m_PublicHostAddress)), port), PACKET_TYPE_PRIORITY);
  } else if (m_Config->m_EnableCustomPort) {
    QueuePacket(m_Protocol->SEND_SID_NETGAMEPORT(m_Config->m_PublicHostPort), PACKET_TYPE_PRIORITY);
  }
}

void CRealm::JoinFirstChannel()
{
  m_Socket->PutBytes(m_Protocol->SEND_SID_JOINCHANNEL(m_FirstChannel));
}

void CRealm::QueuePacket(const vector<uint8_t> message, uint8_t mode)
{
  if (mode == PACKET_TYPE_GAME_REFRESH) {
    if (!m_Aura->m_CurrentLobby) return; // Should never happen.
    m_OutPackets.push(make_tuple(GetTicks(), m_Aura->m_CurrentLobby->GetHostCounter(), mode, message));
  } else {
    m_OutPackets.push(make_tuple(GetTicks(), m_SessionID, mode, message));
  }
}

void CRealm::QueuePacket(const vector<uint8_t> message)
{
  QueuePacket(message, PACKET_TYPE_DEFAULT);
}

void CRealm::QueueEnterChat()
{
  if (m_LoggedIn)
    QueuePacket(m_Protocol->SEND_SID_ENTERCHAT());
}

void CRealm::SendCommand(const string& message)
{
  if (message.empty() || !m_LoggedIn)
    return;

  Print("[BNET: " + m_Config->m_UniqueName + "] Queued \"" + message + "\"");

  size_t MaxMessageSize = GetPvPGN() ? 200 : 255;
  if (message.length() > MaxMessageSize) {
    QueuePacket(m_Protocol->SEND_SID_CHATCOMMAND(message.substr(0, MaxMessageSize)));
  } else {
    QueuePacket(m_Protocol->SEND_SID_CHATCOMMAND(message));
  }

  m_HadChatActivity = true;
}

void CRealm::SendChatChannel(const string& message)
{
  if (message.empty() || !m_LoggedIn)
    return;

  Print("[BNET: " + m_Config->m_UniqueName + "] Queued \"" + message + "\"");

  size_t MaxMessageSize = GetPvPGN() ? 200 : 255;
  if (message.length() > MaxMessageSize) {
    QueuePacket(m_Protocol->SEND_SID_CHATCOMMAND(message.substr(0, MaxMessageSize)), PACKET_TYPE_CHAT_BLOCKING);
  } else {
    QueuePacket(m_Protocol->SEND_SID_CHATCOMMAND(message), PACKET_TYPE_CHAT_BLOCKING);
  }

  m_HadChatActivity = true;
}

void CRealm::SendWhisper(const string& message, const string& user)
{
  SendCommand("/w " + user + " " + message); 
}

void CRealm::SendChatOrWhisper(const string& message, const string& user, bool whisper)
{
  if (whisper) {
    SendWhisper(message, user);
  } else {
    SendChatChannel(message);
  }
}

void CRealm::TrySendChat(const string& message, const string& user, bool isPrivate, ostream* errorLog)
{
  // don't respond to non admins if there are more than 3 messages already in the queue
  // this prevents malicious users from filling up the bot's chat queue and crippling the bot
  // in some cases the queue may be full of legitimate messages but we don't really care if the bot ignores one of these commands once in awhile
  // e.g. when several users join a game at the same time and cause multiple /whois messages to be queued at once

  bool IsQuotaOkay = GetIsFloodImmune() || message.length() <= 200 && (m_OutPackets.size() <= 3 || (GetIsAdmin(user) || GetIsRootAdmin(user) || GetIsSudoer(user)));
  if (IsQuotaOkay) {
    SendChatOrWhisper(message, user, isPrivate);
    return;
  }

  (*errorLog) << GetLogPrefix() + message << std::endl;
  (*errorLog) << "[AURA] Quota exceeded (reply dropped.)" << std::endl;
  if (m_OutPackets.size() <= 5) {
    // TODO(IceSandslash): Throttle these.
    SendWhisper("[Antiflood] Too many messages (reply dropped.)", user);
  }
}

void CRealm::QueueGameCreate(uint8_t displayMode, const string& gameName, CMap* map, uint32_t hostCounter, uint16_t hostPort)
{
  if (!m_LoggedIn || !map)
    return;

  if (!m_CurrentChannel.empty())
    m_FirstChannel = m_CurrentChannel;

  if (m_Config->m_EnableCustomPort) {
    QueuePacket(m_Protocol->SEND_SID_NETGAMEPORT(m_Config->m_PublicHostPort));
  } else {
    QueuePacket(m_Protocol->SEND_SID_NETGAMEPORT(hostPort));
  }
  QueueGameRefresh(displayMode, gameName, map, hostCounter, true);
}

void CRealm::QueueGameMirror(uint8_t displayMode, const string& gameName, CMap* map, uint32_t hostCounter, uint16_t hostPort)
{
  if (!m_LoggedIn || !map)
    return;

  if (!m_CurrentChannel.empty())
    m_FirstChannel = m_CurrentChannel;

  QueuePacket(m_Protocol->SEND_SID_NETGAMEPORT(hostPort));
  QueueGameRefresh(displayMode, gameName, map, hostCounter, false);
}

void CRealm::QueueGameRefresh(uint8_t displayMode, const string& gameName, CMap* map, uint32_t hostCounter, bool useServerNamespace)
{
  if (m_LoggedIn && map)
  {
    // construct a fixed host counter which will be used to identify players from this realm
    // the fixed host counter's highest-order byte will contain a 8 bit ID (0-255)
    // the rest of the fixed host counter will contain the 24 least significant bits of the actual host counter
    // since we're destroying 8 bits of information here the actual host counter should not be greater than 2^24 which is a reasonable assumption
    // when a player joins a game we can obtain the ID from the received host counter
    // note: LAN broadcasts use an ID of 0, battle.net refreshes use IDs of 16-255, the rest are reserved

    uint32_t MapGameType = map->GetMapGameType();
    MapGameType |= MAPGAMETYPE_UNKNOWN0;

    if (displayMode == GAME_PRIVATE)
      MapGameType |= MAPGAMETYPE_PRIVATEGAME;

    QueuePacket(m_Protocol->SEND_SID_STARTADVEX3(
      displayMode, CreateByteArray(MapGameType, false), map->GetMapGameFlags(),
      // use an invalid map width/height to indicate reconnectable games
      m_Aura->m_Net->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : map->GetMapWidth(),
      m_Aura->m_Net->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : map->GetMapHeight(),
      GetPrefixedGameName(gameName), m_Config->m_UserName,
      0, map->GetMapPath(), map->GetMapCRC(), map->GetMapSHA1(),
      hostCounter | (useServerNamespace ? (m_PublicServerID << 24) : 0),
      m_Aura->m_MaxSlots
    ), PACKET_TYPE_GAME_REFRESH);
  }
}

void CRealm::QueueGameUncreate()
{
  if (m_LoggedIn)
    QueuePacket(m_Protocol->SEND_SID_STOPADV());
}

void CRealm::ResetConnection(bool Errored)
{
  if (Errored)
    Print("[BNET: " + m_Config->m_UniqueName + "] disconnected due to socket error");
  else
    Print("[BNET: " + m_Config->m_UniqueName + "] disconnected");
  m_LastDisconnectedTime = GetTime();
  m_BNCSUtil->Reset(m_Config->m_UserName, m_Config->m_PassWord);
  m_Socket->Reset();
  m_LoggedIn         = false;
  m_InChat           = false;
  m_WaitingToConnect = true;
}

bool CRealm::GetIsAdmin(string name) const
{
  transform(begin(name), end(name), begin(name), ::tolower);

  if (m_Aura->m_DB->AdminCheck(m_Config->m_DataBaseID, name))
    return true;

  return false;
}

bool CRealm::GetIsRootAdmin(string name) const
{
  transform(begin(name), end(name), begin(name), ::tolower);

  if (m_Aura->m_DB->RootAdminCheck(m_Config->m_DataBaseID, name))
    return true;

  return false;
}

bool CRealm::GetIsSudoer(string name) const
{
  // Case-sensitive
  return m_Config->m_SudoUsers.find(name) != m_Config->m_SudoUsers.end();
}

bool CRealm::IsBannedName(string name) const
{
  transform(begin(name), end(name), begin(name), ::tolower);
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

