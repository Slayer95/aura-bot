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

#include "bnet.h"
#include "aura.h"
#include "util.h"
#include "fileutil.h"
#include "config.h"
#include "config_bnet.h"
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
// CBNET
//

CBNET::CBNET(CAura* nAura, CBNETConfig* nBNETConfig)
  : m_Aura(nAura),
    m_Config(nBNETConfig),
    m_Socket(new CTCPClient()),
    m_Protocol(new CBNETProtocol()),
    m_BNCSUtil(new CBNCSUtilInterface(nBNETConfig->m_UserName, nBNETConfig->m_PassWord)),
    m_FirstChannel(nBNETConfig->m_FirstChannel),
    m_HostName(nBNETConfig->m_HostName),
    m_ServerIndex(nBNETConfig->m_ServerIndex),
    m_ServerID(nBNETConfig->m_ServerIndex + 15),
    m_LastDisconnectedTime(0),
    m_LastConnectionAttemptTime(0),
    m_LastGameListTime(0),
    m_LastOutPacketTicks(0),
    m_LastAdminRefreshTime(GetTime()),
    m_LastBanRefreshTime(GetTime()),
    m_LastOutPacketSize(0),
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

CBNET::~CBNET()
{
  delete m_Config;
  delete m_Socket;
  delete m_Protocol;
  delete m_BNCSUtil;
}

uint32_t CBNET::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  if (!m_Socket->HasError() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 1;
  }

  return 0;
}

bool CBNET::Update(void* fd, void* send_fd)
{
  const int64_t Ticks = GetTicks(), Time = GetTime();

  // we return at the end of each if statement so we don't have to deal with errors related to the order of the if statements
  // that means it might take a few ms longer to complete a task involving multiple steps (in this case, reconnecting) due to blocking or sleeping
  // but it's not a big deal at all, maybe 100ms in the worst possible case (based on a 50ms blocking time)

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
            if (m_Aura->m_Config->m_UDPForwardGameLists) {
              std::vector<uint8_t> relayPacket = {W3FW_HEADER_CONSTANT, 0, 0, 0};
              std::vector<uint8_t> IPOctets = m_Socket->GetIP();
              std::vector<uint8_t> ServerPort = {static_cast<uint8_t>(6112 >> 8), static_cast<uint8_t>(6112)};
              std::vector<uint8_t> War3Version = {m_Aura->m_Config->m_War3Version, 0, 0, 0};
              AppendByteArray(relayPacket, IPOctets);
              AppendByteArray(relayPacket, ServerPort);
              AppendByteArray(relayPacket, War3Version);
              AppendByteArrayFast(relayPacket, Data);
              AssignLength(relayPacket);
              //Print("[BNET: " + m_Config->m_UniqueName + "] sending game list to " + m_Aura->m_Config->m_UDPForwardAddress + ":" + to_string(m_Aura->m_Config->m_UDPForwardPort) + " (" + to_string(relayPacket.size()) + " bytes)");
              m_Aura->m_UDPSocket->SendTo(m_Aura->m_Config->m_UDPForwardAddress, m_Aura->m_Config->m_UDPForwardPort, relayPacket);
            }

            break;

          case CBNETProtocol::SID_ENTERCHAT:
            if (m_Protocol->RECEIVE_SID_ENTERCHAT(Data))
            {
              Print("[BNET: " + m_Config->m_UniqueName + "] joining channel [" + m_FirstChannel + "]");
              m_InChat = true;
              m_Socket->PutBytes(m_Protocol->SEND_SID_JOINCHANNEL(m_FirstChannel));
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
            if (m_Protocol->RECEIVE_SID_STARTADVEX3(Data))
            {
              m_InChat = false;
            }
            else
            {
              Print("[BNET: " + m_Config->m_UniqueName + "] startadvex3 failed");
              m_Aura->EventBNETGameRefreshFailed(this);
            }

            break;

          case CBNETProtocol::SID_PING:
            m_Socket->PutBytes(m_Protocol->SEND_SID_PING(m_Protocol->RECEIVE_SID_PING(Data)));
            break;

          case CBNETProtocol::SID_AUTH_INFO:

            if (!m_Protocol->RECEIVE_SID_AUTH_INFO(Data))
              break;

            if (m_BNCSUtil->HELP_SID_AUTH_CHECK(m_Aura->m_Config->m_Warcraft3Path, m_Config->m_CDKeyROC, m_Config->m_CDKeyTFT, m_Protocol->GetValueStringFormulaString(), m_Protocol->GetIX86VerFileNameString(), m_Protocol->GetClientToken(), m_Protocol->GetServerToken(), m_Config->m_AuthWar3Version)) {
              // override the exe information generated by bncsutil if specified in the config file
              // apparently this is useful for pvpgn users

              if (m_Config->m_AuthExeVersion.size() == 4) {
                m_BNCSUtil->SetEXEVersion(m_Config->m_AuthExeVersion);
              }

              if (m_Config->m_AuthExeVersionHash.size() == 4) {
                m_BNCSUtil->SetEXEVersionHash(m_Config->m_AuthExeVersionHash);
              }
              Print(
                "[BNET: " + m_Config->m_UniqueName + "] attempting to auth as WC3: TFT v" +
                to_string(m_Config->m_AuthExeVersion[3]) + "." + to_string(m_Config->m_AuthExeVersion[2]) + std::string(1, char(97 + m_Config->m_AuthExeVersion[1])) +
                " (Build " + to_string(m_Config->m_AuthExeVersion[0]) + ") - " +
                "version hash " + ByteArrayToDecString(m_Config->m_AuthExeVersionHash)
              );

              m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_CHECK(m_Protocol->GetClientToken(), m_BNCSUtil->GetEXEVersion(), m_BNCSUtil->GetEXEVersionHash(), m_BNCSUtil->GetKeyInfoROC(), m_BNCSUtil->GetKeyInfoTFT(), m_BNCSUtil->GetEXEInfo(), "Aura"));
              //m_Socket->PutBytes(m_Protocol->SEND_SID_NULL());
              if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetIsMirror() && !m_Config->m_IsMirror) {
                m_Socket->PutBytes(m_Protocol->SEND_SID_PUBLICHOST(m_Aura->m_CurrentLobby->GetPublicHostAddress(), m_Aura->m_CurrentLobby->GetPublicHostPort()));
              } else if (m_Config->m_EnableTunnel) {
                m_Socket->PutBytes(m_Protocol->SEND_SID_PUBLICHOST(m_Config->m_PublicHostAddress, m_Config->m_PublicHostPort));
              }
            } else {
              if (m_Config->m_AuthPasswordHashType == "pvpgn") {
                Print("[BNET: " + m_Config->m_UniqueName + "] config error - misconfigured bot_war3path");
              } else {
                Print("[BNET: " + m_Config->m_UniqueName + "] config error - misconfigured bot_war3path, or bnet_" + to_string(m_ServerIndex) + "_cdkeyroc, or bnet_" + to_string(m_ServerIndex) + "_cdkeytft");
              }
              Print("[BNET: " + m_Config->m_UniqueName + "] bncsutil key hash failed, disconnecting...");
              m_Socket->Disconnect();
            }

            break;

          case CBNETProtocol::SID_AUTH_CHECK:
            if (m_Protocol->RECEIVE_SID_AUTH_CHECK(Data))
            {
              // cd keys accepted
              Print("[BNET: " + m_Config->m_UniqueName + "] game OK");
              m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON();
              m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGON(m_BNCSUtil->GetClientKey(), m_Config->m_UserName));
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
                  Print("[BNET: " + m_Config->m_UniqueName + "] config error - rejected bnet_" + to_string(m_ServerIndex) + "_auth_exeversion = " + ByteArrayToDecString(m_Config->m_AuthExeVersion));
                  Print("[BNET: " + m_Config->m_UniqueName + "] config error - rejected bnet_" + to_string(m_ServerIndex) + "_auth_exeversionhash = " + ByteArrayToDecString(m_Config->m_AuthExeVersionHash));
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
              Print("[BNET: " + m_Config->m_UniqueName + "] username [" + m_Config->m_UserName + "] OK");

              if (m_Config->m_AuthPasswordHashType == "pvpgn")
              {
                // pvpgn logon

                Print("[BNET: " + m_Config->m_UniqueName + "] using pvpgn logon type");
                m_BNCSUtil->HELP_PvPGNPasswordHash(m_Config->m_PassWord);
                m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetPvPGNPasswordHash()));
              }
              else
              {
                // battle.net logon

                Print("[BNET: " + m_Config->m_UniqueName + "] using battle.net logon type");
                m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF(m_Protocol->GetSalt(), m_Protocol->GetServerPublicKey());
                m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetM1()));
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
              m_Socket->PutBytes(m_Protocol->SEND_SID_GETADVLISTEX());
              m_LastGameListTime = Time;
              m_Socket->PutBytes(m_Protocol->SEND_SID_ENTERCHAT());
              //m_OutPackets.push(m_Protocol->SEND_SID_FRIENDLIST());
              //m_OutPackets.push(m_Protocol->SEND_SID_CLANMEMBERLIST());
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

    if (m_LastOutPacketSize < 10)
      WaitTicks = 1300;
    else if (m_LastOutPacketSize < 100)
      WaitTicks = 3300;
    else
      WaitTicks = 4300;

    if (!m_OutPackets.empty() && Ticks - m_LastOutPacketTicks >= WaitTicks)
    {
      if (m_OutPackets.size() > 7)
        Print("[BNET: " + m_Config->m_UniqueName + "] packet queue warning - there are " + to_string(m_OutPackets.size()) + " packets waiting to be sent");

      m_Socket->PutBytes(m_OutPackets.front());
      m_LastOutPacketSize = m_OutPackets.front().size();
      m_OutPackets.pop();
      m_LastOutPacketTicks = Ticks;
    }

    if (Time - m_LastGameListTime >= 90)
    {
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
      if (!m_Config->m_BindAddress.empty())
        Print("[BNET: " + m_Config->m_UniqueName + "] connecting with local address [" + m_Config->m_BindAddress + "]...");
    }
    m_FirstConnect = false;
    m_ReconnectNextTick = false;

    if (m_ResolvedHostName == m_Config->m_HostName && !m_ResolvedAddress.empty()) {
      // DNS resolution is blocking, so use cache if posible
      m_Socket->Connect(m_Config->m_BindAddress, m_ResolvedAddress, 6112);
    } else {
      m_Socket->Connect(m_Config->m_BindAddress, m_Config->m_HostName, 6112);

      if (!m_Socket->HasError()) {
        m_ResolvedHostName = m_Config->m_HostName;
        m_ResolvedAddress = m_Socket->GetIPString();
        //Print("[BNET: " + m_Config->m_UniqueName + "] resolved and cached server IP address " + m_ResolvedAddress);
      }
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

      Print2("[BNET: " + m_Config->m_UniqueName + "] connected to [" + m_HostName + "]");
      m_Socket->PutBytes(m_Protocol->SEND_PROTOCOL_INITIALIZE_SELECTOR());
      m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_INFO(m_Config->m_AuthWar3Version, m_Config->m_LocaleID, m_Config->m_CountryShort, m_Config->m_Country));
      m_Socket->DoSend(static_cast<fd_set*>(send_fd));
      m_LastGameListTime       = Time;
      m_LastOutPacketTicks = Ticks;

      while (!m_OutPackets.empty())
        m_OutPackets.pop();

      return m_Exiting;
    }
    else if (Time - m_LastConnectionAttemptTime >= 10)
    {
      // the connection attempt timed out (10 seconds)

      Print("[BNET: " + m_Config->m_UniqueName + "] failed to connect to [" + m_HostName + ":6112] (" + m_ResolvedAddress + ")");
      Print("[BNET: " + m_Config->m_UniqueName + "] waiting 90 seconds to retry...");
      m_Socket->Reset();
      m_LastDisconnectedTime = Time;
      m_WaitingToConnect     = true;
      return m_Exiting;
    }
  }

  return m_Exiting;
}

void CBNET::ProcessChatEvent(const CIncomingChatEvent* chatEvent)
{
  CBNETProtocol::IncomingChatEvent Event    = chatEvent->GetChatEvent();
  bool                             Whisper  = (Event == CBNETProtocol::EID_WHISPER);
  string                           User     = chatEvent->GetUser();
  string                           Message  = chatEvent->GetMessage();
  bool                             IsMaybeSudoer = false;
  bool                             IsSudoExec = false;
  bool                             IsRootAdmin = GetIsRootAdmin(User);
  bool                             IsAdmin = IsRootAdmin || GetIsAdmin(User);

  if (!m_Socket->GetConnected()) {
    Print("[BNET: " + m_Config->m_UniqueName + "] not connected - message from [" + User + "] rejected: [" + Message + "]");
    return;
  }

  // handle spoof checking for current game
  // this case covers whispers - we assume that anyone who sends a whisper to the bot with message "spoofcheck" should be considered spoof checked
  // note that this means you can whisper "spoofcheck" even in a public game to manually spoofcheck if the /whois fails

  if (Event == CBNETProtocol::EID_WHISPER && m_Aura->m_CurrentLobby && !m_Aura->m_CurrentLobby->GetIsMirror())
  {
    if (Message == "s" || Message == "sc" || Message == "spoofcheck")
    {
      CGamePlayer* Player = m_Aura->m_CurrentLobby->GetPlayerFromName(User, true);
      if (Player) m_Aura->m_CurrentLobby->AddToRealmVerified(m_Config->m_HostName, Player, true);
      return;
    }
  }

  if (Event == CBNETProtocol::EID_IRC)
  {
    // extract the irc channel

    string::size_type MessageStart = Message.find(' ');

    m_IRC   = Message.substr(0, MessageStart);
    Message = Message.substr(MessageStart + 1);
  }
  else
    m_IRC.clear();

  if (Event == CBNETProtocol::EID_WHISPER || Event == CBNETProtocol::EID_TALK) {
    m_HadChatActivity = true;
  }

  if (Event == CBNETProtocol::EID_TALK) {
    // Bots on servers with bnet_x_mirror = yes ignore commands at channels (but not whispers).
    if (m_Config->m_IsMirror)
      return;
  }

  if (Event == CBNETProtocol::EID_WHISPER || Event == CBNETProtocol::EID_TALK || Event == CBNETProtocol::EID_IRC)
  {
    if (Event == CBNETProtocol::EID_WHISPER)
      Print2("[WHISPER: " + m_Config->m_UniqueName + "] [" + User + "] " + Message);
    else
      Print("[LOCAL: " + m_Config->m_UniqueName + "] [" + User + "] " + Message);

    if (Event == CBNETProtocol::EID_IRC) {
      // Too unreliable
      IsAdmin = false;
      IsRootAdmin = false;
    }

    // handle bot commands

    if (Message.empty()) {
      return;
    }

    if (Message[0] != m_Config->m_CommandTrigger) {
      if (Whisper) {
        QueueChatCommand("Hello, " + User + ". My commands start with comma. Example: ,host siege", User, Whisper, m_IRC);
      }
      return;
    }

    if (m_Config->m_SudoUsers.find(User) != m_Config->m_SudoUsers.end()) {
      IsMaybeSudoer = true;
      IsAdmin = true;
    }

    // extract the command trigger, the command, and the payload
    // e.g. "!say hello world" -> command: "say", payload: "hello world"

    string            Command, Payload;
    string::size_type PayloadStart = Message.find(' ');

    if (PayloadStart != string::npos) {
      Command = Message.substr(1, PayloadStart - 1);
      Payload = Message.substr(PayloadStart + 1);
    } else {
      Command = Message.substr(1);
    }

    transform(begin(Command), end(Command), begin(Command), ::tolower);
    uint64_t CommandHash = HashCode(Command);

    if (CommandHash == HashCode("su")) {
      if (!IsMaybeSudoer) {
        Print("[BNET: " + m_Config->m_UniqueName + "] non-sudoer [" + User + "] sent command [" + Message + "]");
      } else {
        m_Aura->m_SudoUser = User;
        m_Aura->m_SudoRealm = m_Config->m_UniqueName;
        m_Aura->m_SudoAuthCommand = m_Aura->GetSudoAuthCommand(Payload, m_Config->m_CommandTrigger);
        m_Aura->m_SudoExecCommand = Payload;
        Print("[AURA] Sudoer " + User + " requests command \"" + Payload + "\"");
        Print("[AURA] Confirm with \"" + m_Aura->m_SudoAuthCommand + "\"");
      }
      return;
    } else if (CommandHash == HashCode("sudo")) {
      if (!IsMaybeSudoer) {
        Print("[BNET: " + m_Config->m_UniqueName + "] non-sudoer [" + User + "] sent command [" + Message + "]");
      } else if (m_Aura->m_SudoUser == User && m_Aura->m_SudoRealm == m_Config->m_UniqueName) {
        if (Message == m_Aura->m_SudoAuthCommand) {
          Print("[AURA] Confirmed " + User + " command \"" + m_Aura->m_SudoExecCommand + "\"");
          PayloadStart = m_Aura->m_SudoExecCommand.find(' ');
          if (PayloadStart != string::npos) {
            Command = m_Aura->m_SudoExecCommand.substr(0, PayloadStart);
            Payload = m_Aura->m_SudoExecCommand.substr(PayloadStart + 1);
          } else {
            Command = m_Aura->m_SudoExecCommand;
            Payload = "";
          }
          transform(begin(Command), end(Command), begin(Command), ::tolower);
          CommandHash = HashCode(Command);

          IsSudoExec = true;
        } else {
          Print("[BNET: " + m_Config->m_UniqueName + "] sudoer [" + User + "] sent unauthenticated command [" + Message + "]");
        }
        m_Aura->m_SudoUser = "";
        m_Aura->m_SudoRealm = "";
        m_Aura->m_SudoAuthCommand = "";
        m_Aura->m_SudoExecCommand = "";
      } else {
        Print("[BNET: " + m_Config->m_UniqueName + "] sudoer [" + User + "] sent unauthenticated command [" + Message + "]");
      }
    }

    if (IsSudoExec) {
      Print("[BNET: " + m_Config->m_UniqueName + "] admin [" + User + "] sent sudo command [" + Message + "]");

      /*****************
       * SUDOER COMMANDS *
       ******************/

      switch (CommandHash)
      {
        // !GETCLAN
        case HashCode("getclan"):
        {
          SendGetClanList();
          QueueChatCommand("Updating the bot's internal clan list from battle.net..", User, Whisper, m_IRC);
          return;
        }

        // !GETFRIENDS
        case HashCode("getfriends"):
        {
          SendGetFriendsList();
          QueueChatCommand("Updating the bot's internal friends list from battle.net..", User, Whisper, m_IRC);
          return;
        }

        case HashCode("lobby"):
        {
          if (!m_Aura->m_CurrentLobby) {
            QueueChatCommand("Not hosting any game.", User, Whisper, m_IRC);
          } else if (m_Aura->m_CurrentLobby->GetIsMirror()) {
            QueueChatCommand("Cannot execute commands in mirrored game.", User, Whisper, m_IRC);
          } else if (Payload.empty()) {
            QueueChatCommand("Command to evaluate not specified.", User, Whisper, m_IRC);
          } else {
            PayloadStart = Payload.find(' ');
            string SubCmd = PayloadStart == string::npos ? Payload : Payload.substr(0, PayloadStart);
            string SubPayload = PayloadStart == string::npos ? string() : Payload.substr(PayloadStart + 1, Payload.length());
            m_Aura->m_CurrentLobby->EventPlayerBotCommand(SubPayload, this, nullptr, SubCmd, m_HostName, User);
          }
          return;
        }

        case HashCode("synccfg"):
        case HashCode("initcfg"):
        {
          std::vector<std::string> AllMaps = FilesMatch(m_Aura->m_Config->m_MapPath, ".w3x");
          std::vector<std::string> ROCMaps = FilesMatch(m_Aura->m_Config->m_MapPath, ".w3m");
          AllMaps.insert(AllMaps.end(), ROCMaps.begin(), ROCMaps.end());
          int counter = 0;
          
          for (const auto& File : AllMaps) {
            if (CommandHash != HashCode("synccfg") && m_Aura->m_CachedMaps.find(File) != m_Aura->m_CachedMaps.end()) {
              continue;
            }
            if (File.find(Payload) == string::npos) {
              continue;
            }
            if (!FileExists(m_Aura->m_Config->m_MapPath + File)) {
              continue;
            }

            CConfig MapCFG;
            MapCFG.Set("cfg_partial", "1");
            MapCFG.Set("map_path", R"(Maps\Download\)" + File);
            MapCFG.Set("map_localpath", File);
            if (File.find("_evrgrn32") != string::npos) {
              MapCFG.Set("map_site", "https://www.hiveworkshop.com/threads/351924/");
            } else {
              MapCFG.Set("map_site", "");
            }
            MapCFG.Set("map_url", "");
            if (File.find("_evrgrn32") != string::npos) {
              MapCFG.Set("map_shortdesc", "This map uses Warcraft 3: Reforged game mechanics.");
            } else {
              MapCFG.Set("map_shortdesc", "");
            }
            MapCFG.Set("downloaded_by", User);

            if (File.find("DotA") != string::npos)
              MapCFG.Set("map_type", "dota");

            CMap* ParsedMap = new CMap(m_Aura, &MapCFG, File);
            delete ParsedMap;

            std::string CFGName = "local-" + File + ".cfg";
            std::string CFGPath = m_Aura->m_Config->m_MapCFGPath + CFGName;

            std::vector<uint8_t> OutputBytes = MapCFG.Export();
            FileWrite(CFGPath, OutputBytes.data(), OutputBytes.size());
            m_Aura->m_CachedMaps[File] = CFGName;
            counter++;
          }
          QueueChatCommand("Initialized " + to_string(counter) + " map config files.", User, Whisper, m_IRC);
          return;
        }

        //
        // !END
        //

        case HashCode("end"):
        {
          if (Payload.empty())
            return;

          // TODO: what if a game ends just as you're typing this command and the numbering changes?

          try
          {
            const uint32_t GameNumber = stoul(Payload) - 1;

            if (GameNumber < m_Aura->m_Games.size())
            {
              // if the game owner is still in the game only allow the root admin to end the game

              if (m_Aura->m_Games[GameNumber]->GetPlayerFromName(m_Aura->m_Games[GameNumber]->GetOwnerName(), false) && !GetIsRootAdmin(User))
                QueueChatCommand("You can't end that game because the game owner [" + m_Aura->m_Games[GameNumber]->GetOwnerName() + "] is still playing", User, Whisper, m_IRC);
              else
              {
                QueueChatCommand("Ending game [" + m_Aura->m_Games[GameNumber]->GetDescription() + "]", User, Whisper, m_IRC);
                Print("[GAME: " + m_Aura->m_Games[GameNumber]->GetGameName() + "] is over (admin ended game)");
                m_Aura->m_Games[GameNumber]->StopPlayers("was disconnected (admin ended game)");
              }
            }
            else
              QueueChatCommand("Game number " + Payload + " doesn't exist", User, Whisper, m_IRC);
          }
          catch (...)
          {
            // do nothing
          }

          return;
        }

        //
        // !COUNTMAPS
        // !COUNTCFGS
        //

        case HashCode("countmap"):
        case HashCode("countmaps"):
        case HashCode("countcfg"):
        case HashCode("countcfgs"):
        {
#ifdef WIN32
          const auto MapCount = FilesMatch(m_Aura->m_Config->m_MapPath, ".").size();
          const auto CFGCount = FilesMatch(m_Aura->m_Config->m_MapCFGPath, ".cfg").size();
#else
          const auto MapCount = FilesMatch(m_Aura->m_Config->m_MapPath, "").size();
          const auto CFGCount = FilesMatch(m_Aura->m_Config->m_MapCFGPath, ".cfg").size();
#endif

          QueueChatCommand(to_string(MapCount) + " maps on disk, " + to_string(CFGCount) + " presets on disk, " + to_string(m_Aura->m_CachedMaps.size()) + " preloaded.", User, Whisper, m_IRC);
          return;
        }

        //
        // !DELETECFG
        // !DELETEMAP
        //

        case HashCode("deletecfg"):
        case HashCode("deletemap"):
        {
          if (Payload.empty())
            return;

          string DeletionType = CommandHash == HashCode("deletecfg") ? "cfg" : "map";
          string Folder = DeletionType == "cfg" ? m_Aura->m_Config->m_MapCFGPath : m_Aura->m_Config->m_MapPath;

          if (DeletionType == "cfg" && !IsValidCFGName(Payload) || DeletionType == "map" &&!IsValidMapName(Payload)) {
            QueueChatCommand("Removal failed", User, Whisper, m_IRC);
            return;
          }

          if (!remove((Folder + Payload).c_str()))
            QueueChatCommand("Deleted [" + Payload + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("Removal failed", User, Whisper, m_IRC);

          return;
        }

        //
        // !SAY (Sudoer version)
        //

        case HashCode("say"):
        {
          if (Payload.empty())
            return;

          QueueChatCommand(Payload);
          return;
        }

        //
        // !SAYGAME
        //

        case HashCode("saygame"):
        //case HashCode("saygameraw"):
        {
          if (Payload.empty())
            return;

          bool IsRaw = CommandHash == HashCode("saygameraw");

          // extract the game number and the message
          // e.g. "3 hello everyone" -> game number: "3", message: "hello everyone"

          uint32_t     GameNumber;
          string       PassedMessage;
          stringstream SS;
          SS << Payload;
          SS >> GameNumber;

          if (SS.fail())
            Print("[BNET: " + m_Config->m_UniqueName + "] bad input #1 to saygame command");
          else
          {
            if (SS.eof())
              Print("[BNET: " + m_Config->m_UniqueName + "] missing input #2 to saygame command");
            else
            {
              getline(SS, PassedMessage);
              string::size_type Start = PassedMessage.find_first_not_of(' ');

              if (Start != string::npos)
                PassedMessage = PassedMessage.substr(Start);

              if (GameNumber - 1 < m_Aura->m_Games.size())
                m_Aura->m_Games[GameNumber - 1]->SendAllChat(IsRaw ? PassedMessage : "ADMIN: " + PassedMessage);
              else
                QueueChatCommand("Game number " + to_string(GameNumber) + " doesn't exist", User, Whisper, m_IRC);
            }
          }

          return;
        }

        //
        // !SAYGAMES
        //

        case HashCode("saygames"):
        //case HashCode("saygamesraw"):
        {
          if (Payload.empty())
            return;

          bool IsRaw = CommandHash == HashCode("saygamesraw");

          if (m_Aura->m_CurrentLobby && !m_Aura->m_CurrentLobby->GetIsMirror())
            m_Aura->m_CurrentLobby->SendAllChat(Payload);

          for (auto& game : m_Aura->m_Games)
            game->SendAllChat(IsRaw ? Payload : "ADMIN: " + Payload);

          return;
        }

        //
        // !DISABLE
        //

        case HashCode("disable"):
        {
          QueueChatCommand("Creation of new games has been disabled (if there is a game in the lobby it will not be automatically unhosted)", User, Whisper, m_IRC);
          m_Aura->m_Config->m_Enabled = false;
          return;
        }

        //
        // !ENABLE
        //

        case HashCode("enable"):
        {
          QueueChatCommand("Creation of new games has been enabled", User, Whisper, m_IRC);
          m_Aura->m_Config->m_Enabled = true;
          return;
        }

        //
        // !DISABLEPUB
        //

        case HashCode("disablepub"):
        {
          QueueChatCommand("Creation of new games has been temporarily disabled for non-admins (if there is a game in the lobby it will not be automatically unhosted)", User, Whisper, m_IRC);
          m_Config->m_EnablePublicCreate = false;
          return;
        }

        //
        // !DISABLEPUB
        //

        case HashCode("enablepub"):
        {
          QueueChatCommand("Creation of new games has been enabled for non-admins", User, Whisper, m_IRC);
          m_Config->m_EnablePublicCreate = true;
          return;
        }

        //
        // !SETDOWNLOADS
        //

        case HashCode("setdownloads"):
        {
          if (Payload.empty())
          {
            if (m_Aura->m_Config->m_AllowUploads == 0)
              QueueChatCommand("Map downloads disabled", User, Whisper, m_IRC);
            else if (m_Aura->m_Config->m_AllowUploads == 1)
              QueueChatCommand("Map downloads enabled", User, Whisper, m_IRC);
            else if (m_Aura->m_Config->m_AllowUploads == 2)
              QueueChatCommand("Conditional map downloads enabled", User, Whisper, m_IRC);

            return;
          }

          try
          {
            const uint32_t Downloads = stoul(Payload);

            if (Downloads == 0)
            {
              QueueChatCommand("Map downloads disabled", User, Whisper, m_IRC);
              m_Aura->m_Config->m_AllowUploads = 0;
            }
            else if (Downloads == 1)
            {
              QueueChatCommand("Map downloads enabled", User, Whisper, m_IRC);
              m_Aura->m_Config->m_AllowUploads = 1;
            }
            else if (Downloads == 2)
            {
              QueueChatCommand("Conditional map downloads enabled", User, Whisper, m_IRC);
              m_Aura->m_Config->m_AllowUploads = 2;
            }
          }
          catch (...)
          {
            // do nothing
          }

          return;
        }

        //
        // !RELOAD
        //

        case HashCode("reload"):
        {
          QueueChatCommand("Reloading configuration files", User, Whisper, m_IRC);
          m_Aura->ReloadConfigs();

          return;
        }

        //
        // !EXIT
        // !QUIT
        //

        case HashCode("exit"):
        case HashCode("quit"):
        {
          if (Payload == "force")
            m_Exiting = true;
          else
          {
            if (m_Aura->m_CurrentLobby || !m_Aura->m_Games.empty())
              QueueChatCommand("At least one game is in the lobby or in progress. Use 'force' to shutdown anyway", User, Whisper, m_IRC);
            else
              m_Exiting = true;
          }

          return;
        }

        //
        // !RESTART
        //

        case HashCode("restart"):
        {
          if ((!m_Aura->m_Games.size() && !m_Aura->m_CurrentLobby) || Payload == "force")
          {
            m_Exiting = true;

            // gRestart is defined in aura.cpp

            extern bool gRestart;
            gRestart = true;
          }
          else
            QueueChatCommand("Games in progress, use !restart force", User, Whisper, m_IRC);

          return;
        }
      }
    }

    /*********************
     * ROOTADMIN COMMANDS *
     *********************/

    if (IsRootAdmin) {
      switch (CommandHash) {
        //
        // !ADDADMIN
        //

        case HashCode("addadmin"):
        {
          if (Payload.empty())
            return;

          if (GetIsAdmin(Payload))
            QueueChatCommand("Error. User [" + Payload + "] is already an admin on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else if (m_Aura->m_DB->AdminAdd(m_Config->m_DataBaseID, Payload))
            QueueChatCommand("Added user [" + Payload + "] to the admin database on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("Error adding user [" + Payload + "] to the admin database on server [" + m_HostName + "]", User, Whisper, m_IRC);

          return;
        }

        //
        // !CHECKADMIN
        //

        case HashCode("checkadmin"):
        {
          if (Payload.empty())
            return;

          if (GetIsAdmin(Payload))
            QueueChatCommand("User [" + Payload + "] is an admin on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("User [" + Payload + "] is not an admin on server [" + m_HostName + "]", User, Whisper, m_IRC);

          return;
        }

        //
        // !COUNTADMINS
        //

        case HashCode("countadmins"):
        {
          uint32_t Count = m_Aura->m_DB->AdminCount(m_Config->m_DataBaseID);

          if (Count == 0)
            QueueChatCommand("There are no admins on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else if (Count == 1)
            QueueChatCommand("There is 1 admin on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("There are " + to_string(Count) + " admins on server [" + m_HostName + "]", User, Whisper, m_IRC);

          return;
        }

        //
        // !DELADMIN
        //

        case HashCode("deladmin"):
        {
          if (Payload.empty())
            return;

          if (!GetIsAdmin(Payload))
            QueueChatCommand("User [" + Payload + "] is not an admin on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else if (m_Aura->m_DB->AdminRemove(m_Config->m_DataBaseID, Payload))
            QueueChatCommand("Deleted user [" + Payload + "] from the admin database on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("Error deleting user [" + Payload + "] from the admin database on server [" + m_HostName + "]", User, Whisper, m_IRC);

          return;
        }

        //
        // !COUNTBANS
        //

        case HashCode("countbans"):
        {
          uint32_t Count = m_Aura->m_DB->BanCount(m_Config->m_DataBaseID);

          if (Count == 0)
            QueueChatCommand("There are no banned users on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else if (Count == 1)
            QueueChatCommand("There is 1 banned user on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("There are " + to_string(Count) + " banned users on server [" + m_HostName + "]", User, Whisper, m_IRC);

          return;
        }
      }
    }

    /*********************
     * ADMIN COMMANDS *
     *********************/

    if (IsAdmin) {
      switch (CommandHash) {
        //
        // !BAN
        //

        case HashCode("ban"):
        {
          if (Payload.empty())
            return;

          // extract the victim and the reason
          // e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

          string       Victim, Reason;
          stringstream SS;
          SS << Payload;
          SS >> Victim;

          if (!SS.eof())
          {
            getline(SS, Reason);
            string::size_type Start = Reason.find_first_not_of(' ');

            if (Start != string::npos)
              Reason = Reason.substr(Start);
          }

          if (GetIsRootAdmin(Victim) || GetIsAdmin(Victim) && !IsRootAdmin) {
            QueueChatCommand("Error. User [" + Victim + "] is an admin on server [" + m_HostName + "]", User, Whisper, m_IRC);
            return;
          }

          CDBBan* Ban = IsBannedName(Victim);

          if (Ban) {
            QueueChatCommand("Error. User [" + Victim + "] is already banned on server [" + m_HostName + "]", User, Whisper, m_IRC);
            delete Ban;
            return;
          }
          if (GetIsAdmin(Victim))
            m_Aura->m_DB->AdminRemove(m_Config->m_DataBaseID, Victim);
          if (m_Aura->m_DB->BanAdd(m_Config->m_DataBaseID, Victim, User, Reason))
            QueueChatCommand("Banned user [" + Victim + "] on server [" + m_HostName + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("Error banning user [" + Victim + "] on server [" + m_HostName + "]", User, Whisper, m_IRC);

          return;
        }

        //
        // !CHECKBAN
        //

        case HashCode("checkban"):
        {
          if (Payload.empty())
            return;

          CDBBan* Ban = IsBannedName(Payload);

          if (Ban)
            QueueChatCommand("User [" + Payload + "] was banned on server [" + m_HostName + "] on " + Ban->GetDate() + " by [" + Ban->GetAdmin() + "] because [" + Ban->GetReason() + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("User [" + Payload + "] is not banned on server [" + m_HostName + "]", User, Whisper, m_IRC);

          delete Ban;
          return;
        }

        //
        // !UNBAN
        //

        case HashCode("unban"):
        {
          if (Payload.empty())
            return;

          if (m_Aura->m_DB->BanRemove(Payload))
            QueueChatCommand("Unbanned user [" + Payload + "] on all realms", User, Whisper, m_IRC);
          else
            QueueChatCommand("Error unbanning user [" + Payload + "] on all realms", User, Whisper, m_IRC);

          return;
        }

        //
        // !UNHOST
        //

        case HashCode("unhost"):
        case HashCode("uh"):
        {
          if (m_Aura->m_CurrentLobby)
          {
            if (m_Aura->m_CurrentLobby->GetCountDownStarted())
              QueueChatCommand("Unable to unhost game [" + m_Aura->m_CurrentLobby->GetDescription() + "]. The countdown has started, just wait a few seconds", User, Whisper, m_IRC);

            // if the game owner is still in the game only allow the root admin to unhost the game

            else if (m_Aura->m_CurrentLobby->HasOwnerInGame() && !GetIsRootAdmin(User))
              QueueChatCommand("You can't unhost that game because the game owner [" + m_Aura->m_CurrentLobby->GetOwnerName() + "] is in the lobby", User, Whisper, m_IRC);
            else
            {
              QueueChatCommand("Unhosting game [" + m_Aura->m_CurrentLobby->GetDescription() + "]", User, Whisper, m_IRC);
              m_Aura->m_CurrentLobby->SetExiting(true);
            }
          }
          else
            QueueChatCommand("Unable to unhost game. There is no game in the lobby", User, Whisper, m_IRC);

          return;
        }

        //
        // !LOAD (load config file)
        //

        case HashCode("load"):
        {
          if (Payload.empty()) {
            if (!m_Aura->m_Map) {
              QueueChatCommand("There is no map/config file loaded.", User, Whisper, m_IRC);
              return;
            }
            QueueChatCommand("The currently loaded map/config file is: [" + m_Aura->m_Map->GetCFGFile() + "]", User, Whisper, m_IRC);
          } else {
            if (!FileExists(m_Aura->m_Config->m_MapCFGPath)) {
              Print("[BNET: " + m_Config->m_UniqueName + "] error listing map configs - map config path doesn't exist");
              QueueChatCommand("Error listing map configs - map config path doesn't exist", User, Whisper, m_IRC);
            } else {
              const vector<string> Matches = m_Aura->ConfigFilesMatch(Payload);

              if (Matches.empty()) {
                QueueChatCommand("No map configs found with that name", User, Whisper, m_IRC);
              } else if (Matches.size() == 1) {
                const string File = Matches.at(0);
                QueueChatCommand("Loading config file [" + m_Aura->m_Config->m_MapCFGPath + File + "]", User, Whisper, m_IRC);
                CConfig MapCFG;
                MapCFG.Read(m_Aura->m_Config->m_MapCFGPath + File);
                if (m_Aura->m_Map)
                  delete m_Aura->m_Map;
                m_Aura->m_Map = new CMap(m_Aura, &MapCFG, m_Aura->m_Config->m_MapCFGPath + File);
              } else {
                string FoundMapConfigs;

                for (const auto& match : Matches)
                  FoundMapConfigs += match + ", ";

                QueueChatCommand("Maps configs: " + FoundMapConfigs.substr(0, FoundMapConfigs.size() - 2), User, Whisper, m_IRC);
              }
            }
          }

          return;
        }

        //
        // !SP
        //

        case HashCode("sp"):
        {
          if (!m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror() || m_Aura->m_CurrentLobby->GetCountDownStarted())
            return;

          if (!m_Aura->m_CurrentLobby->GetLocked())
          {
            m_Aura->m_CurrentLobby->SendAllChat("Shuffling players");
            m_Aura->m_CurrentLobby->ShuffleSlots();
          }
          else
            QueueChatCommand("This command is disabled while the game is locked", User, Whisper, m_IRC);

          return;
        }

        //
        // !START
        // !S
        //

        case HashCode("s"):
        case HashCode("start"):
        {
          if (!m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror() || m_Aura->m_CurrentLobby->GetCountDownStarted())
            return;

          if (m_Aura->m_CurrentLobby->GetNumHumanPlayers() == 0)
            return;

          if (!m_Aura->m_CurrentLobby->GetLocked())
          {
            // if the player sent "!start force" skip the checks and start the countdown
            // otherwise check that the game is ready to start

            if (Payload == "force")
              m_Aura->m_CurrentLobby->StartCountDown(true);
            else
              m_Aura->m_CurrentLobby->StartCountDown(false);
          }
          else
            QueueChatCommand("This command is disabled while the game is locked", User, Whisper, m_IRC);

          return;
        }

        //
        // !SWAP (swap slots)
        //

        case HashCode("swap"):
        {
          if (Payload.empty() || !m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror())
            return;

          if (!m_Aura->m_CurrentLobby->GetLocked())
          {
            uint32_t     SID1, SID2;
            stringstream SS;
            SS << Payload;
            SS >> SID1;

            if (SS.fail())
              Print("[BNET: " + m_Config->m_UniqueName + "] bad input #1 to swap command");
            else
            {
              if (SS.eof())
                Print("[BNET: " + m_Config->m_UniqueName + "] missing input #2 to swap command");
              else
              {
                SS >> SID2;

                if (SS.fail())
                  Print("[BNET: " + m_Config->m_UniqueName + "] bad input #2 to swap command");
                else
                  m_Aura->m_CurrentLobby->SwapSlots(static_cast<uint8_t>(SID1 - 1), static_cast<uint8_t>(SID2 - 1));
              }
            }
          }
          else
            QueueChatCommand("This command is disabled while the game is locked", User, Whisper, m_IRC);

          return;
        }



        //
        // !OPEN (open slot)
        //

        case HashCode("open"):
        {
          if (Payload.empty() || !m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror())
            return;

          if (!m_Aura->m_CurrentLobby->GetLocked())
          {
            // open as many slots as specified, e.g. "5 10" opens slots 5 and 10

            stringstream SS;
            SS << Payload;

            while (!SS.eof())
            {
              uint32_t SID;
              SS >> SID;
              --SID;

              if (SS.fail()) {
                Print("[BNET: " + m_Config->m_UniqueName + "] bad input to open command");
                break;
              } else {
                if (!m_Aura->m_CurrentLobby->DeleteFakePlayer(static_cast<uint8_t>(SID))) {
                  m_Aura->m_CurrentLobby->OpenSlot(static_cast<uint8_t>(SID), false);
                }
              }
            }
          }
          else
            QueueChatCommand("This command is disabled while the game is locked", User, Whisper, m_IRC);

          return;
        }

        //
        // !OPENALL
        //

        case HashCode("openall"):
        {
          if (!m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror())
            return;

          if (!m_Aura->m_CurrentLobby->GetLocked())
            m_Aura->m_CurrentLobby->OpenAllSlots();
          else
            QueueChatCommand("This command is disabled while the game is locked", User, Whisper, m_IRC);

          return;
        }

        //
        // !PRIVBY (host private game by other player)
        //

        case HashCode("privby"):
        {
          if (Payload.empty())
            return;

          if (!m_Aura->m_Map) {
            QueueChatCommand("There is no map/config file loaded.", User, Whisper, m_IRC);
            return;
          }

          // extract the owner and the game name
          // e.g. "Varlock dota 6.54b arem ~~~" -> owner: "Varlock", game name: "dota 6.54b arem ~~~"

          string            Owner, GameName;
          string::size_type GameNameStart = Payload.find(' ');

          if (GameNameStart != string::npos) {
            Owner    = Payload.substr(0, GameNameStart);
            GameName = Payload.substr(GameNameStart + 1);
            
            m_Aura->CreateGame(m_Aura->m_Map, GAME_PRIVATE, GameName, Owner, m_HostName, User, this, Whisper);
            m_Aura->m_Map = nullptr;
          }

          return;
        }

        //
        // !PUBBY (host public game by other player)
        //

        case HashCode("pubby"):
        {
          if (Payload.empty())
            return;

          if (!m_Aura->m_Map) {
            QueueChatCommand("There is no map/config file loaded.", User, Whisper, m_IRC);
            return;
          }
          
          // extract the owner and the game name
          // e.g. "Varlock dota 6.54b arem ~~~" -> owner: "Varlock", game name: "dota 6.54b arem ~~~"

          string            Owner, GameName;
          string::size_type GameNameStart = Payload.find(' ');

          if (GameNameStart != string::npos) {
            Owner    = Payload.substr(0, GameNameStart);
            GameName = Payload.substr(GameNameStart + 1);
            m_Aura->CreateGame(m_Aura->m_Map, GAME_PUBLIC, GameName, Owner, m_HostName, User, this, Whisper);
            m_Aura->m_Map = nullptr;
          }

          return;
        }


        //
        // !CLOSE (close slot)
        //

        case HashCode("close"):
        {
          if (Payload.empty() || !m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror())
            return;

          if (!m_Aura->m_CurrentLobby->GetLocked())
          {
            // close as many slots as specified, e.g. "5 10" closes slots 5 and 10

            stringstream SS;
            SS << Payload;

            while (!SS.eof())
            {
              uint32_t SID;
              SS >> SID;
              --SID;

              if (SS.fail()) {
                Print("[BNET: " + m_Config->m_UniqueName + "] bad input to close command");
                break;
              } else {
                m_Aura->m_CurrentLobby->DeleteFakePlayer(static_cast<uint8_t>(SID));
                m_Aura->m_CurrentLobby->CloseSlot(static_cast<uint8_t>(SID), true);
              }
            }
          }
          else
            QueueChatCommand("This command is disabled while the game is locked", User, Whisper, m_IRC);

          return;
        }

        //
        // !CLOSEALL
        //

        case HashCode("closeall"):
        {
          if (!m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror())
            return;

          if (!m_Aura->m_CurrentLobby->GetLocked())
            m_Aura->m_CurrentLobby->CloseAllSlots();
          else
            QueueChatCommand("This command is disabled while the game is locked", User, Whisper, m_IRC);

          return;
        }


        //
        // !HOLD (hold a slot for someone)
        //

        case HashCode("hold"):
        {
          if (Payload.empty() || !m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror())
            return;

          // hold as many players as specified, e.g. "Varlock Kilranin" holds players "Varlock" and "Kilranin"

          stringstream SS;
          SS << Payload;

          while (!SS.eof())
          {
            string HoldName;
            SS >> HoldName;

            if (SS.fail())
            {
              Print("[BNET: " + m_Config->m_UniqueName + "] bad input to hold command");
              break;
            }
            else
            {
              QueueChatCommand("Added player [" + HoldName + "] to the hold list", User, Whisper, m_IRC);
              m_Aura->m_CurrentLobby->AddToReserved(HoldName);
            }
          }

          return;
        }

        //
        // !GAMERANGER
        //

        case HashCode("gameranger"):
        {
          if (Payload.empty()) {
            QueueChatCommand("GameRanger IP: " + ByteArrayToDecString(m_Aura->m_GameRangerRemoteAddress) + " (proxy at " + to_string(m_Aura->m_GameRangerLocalPort) + ")", User, true, m_IRC);
            return;
          }
          stringstream SS;
          string GameRangerIPString;
          uint16_t GameRangerPort;

          SS << Payload;
          SS >> GameRangerIPString;
          if (SS.fail() || SS.eof())
            return;

          SS >> GameRangerPort;
          if (SS.fail())
            return;
            
          std::vector<uint8_t> GameRangerAddress = ExtractIPv4(GameRangerIPString);
          if (GameRangerAddress.empty())
            return;

          m_Aura->m_GameRangerLocalPort = GameRangerPort;
          m_Aura->m_GameRangerRemoteAddress = GameRangerAddress;
          QueueChatCommand("GameRanger IP set to " + ByteArrayToDecString(m_Aura->m_GameRangerRemoteAddress) + "(proxy at " + to_string(m_Aura->m_GameRangerLocalPort) + ")", User, true, m_IRC);
          return;
        }

        //
        // !SENDLAN
        //

        case HashCode("sendlan"):
        {
          if (Payload.empty() || !m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror() || m_Aura->m_CurrentLobby->GetCountDownStarted())
            return;

          m_Aura->m_CurrentLobby->LANBroadcastGameCreate();
          m_Aura->m_CurrentLobby->LANBroadcastGameRefresh();
          if (!m_Aura->m_UDPServerEnabled)
            m_Aura->m_CurrentLobby->LANBroadcastGameInfo();

          return;
        }

        //
        // !SENDLANINFO
        //

        case HashCode("sendlaninfo"):
        {
          if (Payload.empty() || !m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror() || m_Aura->m_CurrentLobby->GetCountDownStarted())
            return;

          m_Aura->m_CurrentLobby->LANBroadcastGameInfo();
          return;
        }

        //
        // !NETINFO
        //

        case HashCode("netinfo"):
        {
          if (!IsMaybeSudoer) {
            // Common informational command. Let's not do the whole sudo verification,
            // but BNet admins still have no business using this command.
            return;
          }

          QueueChatCommand("/netinfo");
          return;
        }

        //
        // !WHOISANY
        //

        case HashCode("whoisany"):
        {
          if (!IsMaybeSudoer) {
            // Common informational command. Let's not do the whole sudo verification,
            // but BNet admins still have no business using this command.
            return;
          }

          if (Payload.empty())
            return;

            for (auto& bnet : m_Aura->m_BNETs)
              bnet->QueueChatCommand("/whois " + Payload);

          return;
        }

        //
        // !WHOIS
        //

        case HashCode("whois"):
        {
          if (!IsMaybeSudoer) {
            // Common informational command. Let's not do the whole sudo verification,
            // but BNet admins still have no business using this command.
            return;
          }

          if (Payload.empty())
            return;

          QueueChatCommand("/whois " + Payload);
          return;
        }

        //
        // !PRINTGAMES
        //

        case HashCode("printgames"):
        {
          if (!IsMaybeSudoer) {
            // Common informational command. Let's not do the whole sudo verification,
            // but BNet admins still have no business using this command.
            return;
          }

          QueueChatCommand("/games");
          return;
        }

        //
        // !QUERYGAMES
        //

        case HashCode("querygames"):
        {
          if (!IsMaybeSudoer) {
            // Common informational command. Let's not do the whole sudo verification,
            // but BNet admins still have no business using this command.
            return;
          }

          int64_t Time = GetTime();
          if (Time - m_LastGameListTime >= 30) {
            m_Socket->PutBytes(m_Protocol->SEND_SID_GETADVLISTEX());
            m_LastGameListTime = Time;
          }
          return;
        }
      }
    }


    /*********************
     * NON ADMIN COMMANDS *
     *********************/

    // don't respond to non admins if there are more than 3 messages already in the queue
    // this prevents malicious users from filling up the bot's chat queue and crippling the bot
    // in some cases the queue may be full of legitimate messages but we don't really care if the bot ignores one of these commands once in awhile
    // e.g. when several users join a game at the same time and cause multiple /whois messages to be queued at once

    if (IsAdmin || m_OutPackets.size() < 3) {
      switch (CommandHash)
      {
        //
        // !VERSION
        //

        case HashCode("version"):
        {
          QueueChatCommand("Version: Aura " + m_Aura->m_Version, User, Whisper, m_IRC);
          return;
        }

        //
        // !STATUS
        //

        case HashCode("status"):
        {
          string message = "Status: ";

          for (const auto& bnet : m_Aura->m_BNETs)
            message += bnet->GetServer() + (bnet->GetLoggedIn() ? " [online], " : " [offline], ");

          if (m_Aura->m_IRC)
            message += m_Aura->m_IRC->m_Config->m_HostName + (!m_Aura->m_IRC->m_WaitingToConnect ? " [online]" : " [offline]");

          QueueChatCommand(message, User, Whisper, m_IRC);
          return;
        }

        //
        // !CHANNEL (change channel)
        //

        case HashCode("channel"):
        {
          if (Payload.empty())
            return;

          QueueChatCommand("/join " + Payload);
          return;
        }

        //
        // !GETGAME
        // !G
        //

        case HashCode("g"):
        case HashCode("getgame"):
        {
          if (Payload.empty())
          {
            if (m_Aura->m_CurrentLobby) {
              QueueChatCommand("Game [" + m_Aura->m_CurrentLobby->GetDescription() + "] is in the lobby. There are " + to_string(m_Aura->m_Games.size()) + "/" + to_string(m_Aura->m_Config->m_MaxGames) + " other games in progress", User, Whisper, m_IRC);
            } else {
              QueueChatCommand("There is no game in the lobby. There are " + to_string(m_Aura->m_Games.size()) + "/" + to_string(m_Aura->m_Config->m_MaxGames) + " other games in progress", User, Whisper, m_IRC);
            }
            return;
          }

          try
          {
            const uint32_t GameNumber = stoul(Payload) - 1;

            if (GameNumber < m_Aura->m_Games.size())
              QueueChatCommand("Game number " + Payload + " is [" + m_Aura->m_Games[GameNumber]->GetDescription() + "]", User, Whisper, m_IRC);
            else
              QueueChatCommand("Game number " + Payload + " doesn't exist", User, Whisper, m_IRC);
          }
          catch (...)
          {
            // do nothing
          }

          return;
        }

        //
        // !GETGAMES
        // !G
        //

        case HashCode("getgames"):
        {
          if (m_Aura->m_CurrentLobby)
            QueueChatCommand("Game [" + m_Aura->m_CurrentLobby->GetDescription() + "] is in the lobby and there are " + to_string(m_Aura->m_Games.size()) + "/" + to_string(m_Aura->m_Config->m_MaxGames) + " other games in progress", User, Whisper, m_IRC);
          else
            QueueChatCommand("There is no game in the lobby and there are " + to_string(m_Aura->m_Games.size()) + "/" + to_string(m_Aura->m_Config->m_MaxGames) + " other games in progress", User, Whisper, m_IRC);

          return;
        }

        //
        // !SAY (public version)
        //

        case HashCode("say"):
        {
          if (Payload.empty())
            return;

          if (Payload[0] != '/') {
            QueueChatCommand(Payload);
          }

          return;
        }

        //
        // !WANY
        //

        case HashCode("wany"):
        {
          if (Payload.empty())
            return;

          // extract the name and the message
          // e.g. "Varlock hello there!" -> name: "Varlock", message: "hello there!"

          string            Name;
          string            PassedMessage;
          string::size_type MessageStart = Payload.find(' ');

          if (MessageStart != string::npos) {
            Name    = Payload.substr(0, MessageStart);
            PassedMessage = User + " at " + GetCanonicalDisplayName() + " says: " + Payload.substr(MessageStart + 1);

            for (auto& bnet : m_Aura->m_BNETs)
              bnet->QueueChatCommand(PassedMessage, Name, true, string());
          }

          return;
        }

        //
        // !W
        //

        case HashCode("w"):
        {
          if (Payload.empty())
            return;

          // extract the name and the message
          // e.g. "Varlock hello there!" -> name: "Varlock", message: "hello there!"

          string            Name;
          string            PassedMessage;
          string::size_type MessageStart = Payload.find(' ');

          if (MessageStart != string::npos) {
            Name    = Payload.substr(0, MessageStart);
            PassedMessage = User + " says: " + Payload.substr(MessageStart + 1);
            QueueChatCommand(PassedMessage, Name, true, string());
          }

          return;
        }

        //
        // !MAP (load map file)
        //

        case HashCode("hostlan"):
        case HashCode("host"):
        case HashCode("map"):
        {
          if (!m_Config->m_EnablePublicCreate && !GetIsAdmin(User) && !GetIsRootAdmin(User)) {
            QueueChatCommand("Apologies. Game hosting is temporarily unavailable.", User, Whisper, m_IRC);
            return;
          }

          bool IsHostCommand = CommandHash == HashCode("host") || CommandHash == HashCode("hostlan");
          vector<string> Args = SplitArgs(Payload, 1, 5);

          string Map;
          string gameName = "gogogo";
          int MapObserversValue = -1;
          int MapVisibilityValue = -1;
          int MapExtraFlags = 0;

          if (Args.empty() || Args[0].empty()) {
            if (IsHostCommand) {
              QueueChatCommand("Please enter the map in the host command.", User, Whisper, m_IRC);
              return;
            }
            if (!m_Aura->m_Map) {
              QueueChatCommand("There is no map/config file loaded.", User, Whisper, m_IRC);
              return;
            }
            QueueChatCommand("The currently loaded map/config file is: [" + m_Aura->m_Map->GetCFGFile() + "]", User, Whisper, m_IRC);
            return;
          }

          Map = Args[0];

          bool errored = false;
          if (Args.size() >= 2 && !Args[1].empty()) {
            int result = ParseMapObservers(Args[1], errored);
            if (errored) {
              QueueChatCommand("Invalid value passed for map observers: \"" + Args[1] + "\"", User, Whisper, m_IRC);
              return;
            }
            MapObserversValue = result;
          }
          if (Args.size() >= 3 && !Args[2].empty()) {
            int result = ParseMapVisibility(Args[2], errored);
            if (errored) {
              QueueChatCommand("Invalid value passed for map visibility: \"" + Args[2] + "\"", User, Whisper, m_IRC);
              return;
            }
            MapVisibilityValue = result;
          }
          if (Args.size() >= 4 && !Args[3].empty()) {
            int result = ParseMapRandomHero(Args[3], errored);
            if (errored) {
              QueueChatCommand("Invalid value passed for hero randomization: \"" + Args[3] + "\"", User, Whisper, m_IRC);
              return;
            }
            MapExtraFlags = result;
          }
          if (Args.size() >= 5 && !Args[4].empty()) {
            gameName = Args[4];
          }

          if (!FileExists(m_Aura->m_Config->m_MapPath)) {
            Print("[BNET: " + m_Config->m_UniqueName + "] error listing maps - map path doesn't exist");
            QueueChatCommand("Error listing maps - map path doesn't exist", User, Whisper, m_IRC);
            return;
          }

          string MapSiteUri;
          string MapDownloadUri;
          string ResolvedCFGPath;
          string ResolvedCFGName;
          string File;
          bool ResolvedCFGExists = false;

          pair<string, string> NamespacedMap = ParseMapId(Map);
          if (NamespacedMap.first == "remote") {
            QueueChatCommand("Download from the specified domain not supported", User, Whisper, m_IRC);
            return;
          }
          if (NamespacedMap.second.empty()) {
            QueueChatCommand("Map not valid", User, Whisper, m_IRC);
            return;
          }

          CConfig MapCFG;

          // Figure out whether the map pointed at by the URL is already cached locally.
          if (NamespacedMap.first == "epicwar") {
            MapSiteUri = "https://www.epicwar.com/maps/" + NamespacedMap.second;
            // Check that we don't download the same map multiple times.
            ResolvedCFGName = "epicwar-" + NamespacedMap.second + ".cfg";
            ResolvedCFGPath = m_Aura->m_Config->m_MapCFGPath + ResolvedCFGName;
            ResolvedCFGExists = MapCFG.Read(ResolvedCFGPath);
          }

          if (NamespacedMap.first != "local" && !ResolvedCFGExists) {
            // Map config not locally available. Download the map so we can calculate it.
            // But only if we are allowed to.
            if (!m_Aura->m_Config->m_AllowDownloads) {
              QueueChatCommand("Map downloads are not allowed.", User, Whisper, m_IRC);
              return;
            }
            if (!m_Aura->m_Games.empty() && m_Aura->m_Config->m_HasBufferBloat) {
              QueueChatCommand("Currently hosting " + to_string(m_Aura->m_Games.size()) + " game(s). Downloads are disabled to prevent high latency.");
              return;
            }

            string MapDownloadUri;
            uint8_t DownloadResult = DownloadRemoteMap(NamespacedMap.first, MapSiteUri, MapDownloadUri, Map);

            if (DownloadResult == 1) {
              QueueChatCommand("Map not found in EpicWar.", User, Whisper, m_IRC);
            } else if (DownloadResult == 2) {
              QueueChatCommand("Downloaded map invalid.", User, Whisper, m_IRC);
            } else if (DownloadResult == 3) {
              QueueChatCommand("Download failed - duplicate map name.", User, Whisper, m_IRC);
            } else if (DownloadResult == 4) {
              QueueChatCommand("Download failed - unable to write to disk.", User, Whisper, m_IRC);
            } else if (DownloadResult != 0) {
              QueueChatCommand("Download failed - code " + to_string(DownloadResult), User, Whisper, m_IRC);
            }
            if (DownloadResult != 0) {
              return;
            }
            QueueChatCommand("Downloaded <" + MapSiteUri + "> as [" + Map + "]", User, Whisper, m_IRC);
          }

          if (NamespacedMap.first == "local") {
            Map = NamespacedMap.second;
          }

          if (ResolvedCFGExists) {
            // Exact resolution by map identifier.
            QueueChatCommand("Loading config file [" + ResolvedCFGPath + "]", User, Whisper, m_IRC);
          } else {
            // Fuzzy resolution by map name.
            const vector<string> LocalMatches = m_Aura->MapFilesMatch(Map);
            if (LocalMatches.empty()) {
              if (IsHostCommand) {
                QueueChatCommand(Map + " not found. Try " + GetCommandToken() + "host epicwarlink?", User, Whisper, m_IRC);
                return;
              }
              string Results = GetEpicWarSuggestions(Map, 5);
              if (Results.empty()) {
                QueueChatCommand("No matching map found." , User, Whisper, m_IRC);
              } else {
                QueueChatCommand("Maps: " + Results, User, Whisper, m_IRC);
              }
              return;
            }
            if (LocalMatches.size() > 1 && !IsHostCommand) {
              string FoundMaps;
              for (const auto& match : LocalMatches)
                FoundMaps += match + ", ";

              FoundMaps += GetEpicWarSuggestions(Map, 5);

              QueueChatCommand("Maps: " + FoundMaps, User, Whisper, m_IRC);
              return;
            }
            File = LocalMatches.at(0);
            if (m_Aura->m_CachedMaps.find(File) != m_Aura->m_CachedMaps.end()) {
              ResolvedCFGName = m_Aura->m_CachedMaps[File];
              ResolvedCFGPath = m_Aura->m_Config->m_MapCFGPath + ResolvedCFGName;
              ResolvedCFGExists = MapCFG.Read(ResolvedCFGPath);
              if (ResolvedCFGExists) {
                QueueChatCommand("Loading config file [" + ResolvedCFGPath + "]", User, Whisper, m_IRC);
              } else {
                // Oops! CFG file is gone!
                m_Aura->m_CachedMaps.erase(File);
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
                MapCFG.Set("downloaded_by", User);

              if (File.find("DotA") != string::npos)
                MapCFG.Set("map_type", "dota");

              if (ResolvedCFGName.empty()) {
                ResolvedCFGName = "local-" + File + ".cfg";
                ResolvedCFGPath = m_Aura->m_Config->m_MapCFGPath + ResolvedCFGName;
              }

              vector<uint8_t> OutputBytes = MapCFG.Export();
              FileWrite(ResolvedCFGPath, OutputBytes.data(), OutputBytes.size());
              m_Aura->m_CachedMaps[File] = ResolvedCFGName;
            }
          }

          if (MapObserversValue != -1)
            MapCFG.Set("map_observers", to_string(MapObserversValue));
          if (MapVisibilityValue != -1)
            MapCFG.Set("map_visibility", to_string(MapVisibilityValue));

          if (m_Aura->m_Map) {
            delete m_Aura->m_Map;
          }
          m_Aura->m_Map = new CMap(m_Aura, &MapCFG, ResolvedCFGExists ? "cfg:" + ResolvedCFGName : MapCFG.GetString("map_localpath", ""));
          if (MapExtraFlags != 0) {
            m_Aura->m_Map->AddMapFlags(MapExtraFlags);
          }

          const char* ErrorMessage = m_Aura->m_Map->CheckValid();

          if (ErrorMessage) {
            QueueChatCommand("Error while loading map: [" + std::string(ErrorMessage) + "]", User, Whisper, m_IRC);
            if (!ResolvedCFGPath.empty()) {
              FileDelete(ResolvedCFGPath);
            }
            delete m_Aura->m_Map;
            m_Aura->m_Map = nullptr;
            return;
          }

          if (!ResolvedCFGExists) {
            if (MapCFG.GetInt("cfg_partial", 0) == 0) {
              // Download and parse successful
              vector<uint8_t> OutputBytes = MapCFG.Export();
              if (FileWrite(ResolvedCFGPath, OutputBytes.data(), OutputBytes.size())) {
                Print("[AURA] Cached map config for [" + File + "] as [" + ResolvedCFGPath + "]");
              }
            }
            QueueChatCommand("Map file loaded OK [" + File + "]", User, Whisper, m_IRC);
          }
          if (IsHostCommand) {
            m_Aura->CreateGame(
              m_Aura->m_Map, GAME_PUBLIC, gameName,
              User, CommandHash == HashCode("hostlan") ? "" : m_HostName,
              User, this, Whisper
            );
            m_Aura->m_Map = nullptr;
          }

          return;
        }

        //
        // !PUB (host public game)
        //

        case HashCode("pub"):
        {
          if (!m_Config->m_EnablePublicCreate && !GetIsAdmin(User) && !GetIsRootAdmin(User)) {
            QueueChatCommand("Game hosting temporarily disabled.", User, Whisper, m_IRC);
            return;
          }
          if (!m_Aura->m_Map) {
            QueueChatCommand("A map must be loaded with " + GetCommandToken() + "map first.", User, Whisper, m_IRC);
            return;
          }
          if (!Payload.empty())
            m_Aura->CreateGame(m_Aura->m_Map, GAME_PUBLIC, Payload, User, m_HostName, User, this, Whisper);

          return;
        }

        //
        // !PRIV (host private game)
        //

        case HashCode("priv"):
        {
          if (!m_Config->m_EnablePublicCreate && !GetIsAdmin(User) && !GetIsRootAdmin(User)) {
            QueueChatCommand("Game hosting temporarily disabled.", User, Whisper, m_IRC);
            return;
          }
          if (!m_Aura->m_Map) {
            QueueChatCommand("A map must be loaded with " + GetCommandToken() + "map first.", User, Whisper, m_IRC);
            return;
          }
          if (!Payload.empty())
            m_Aura->CreateGame(m_Aura->m_Map, GAME_PRIVATE, Payload, User, m_HostName, User, this, Whisper);

          return;
        }

        //
        // !MIRROR (mirror game from another server)
        //

        case HashCode("mirror"):
        {
          string gameName;
          string gameAddress;
          uint16_t gamePort = 6112;
          uint32_t gameHostCounter = 1;
          uint32_t gameEntryKey = 0;
          string excludedServer;

          stringstream SS(Payload);
          string RawExcludedServer, RawAddress, RawPort, RawHostCounter, RawEntryKey, RawName;
          std::getline(SS, RawExcludedServer, ',');
          if (SS.eof()) {
            Print("Missing arguments");
            return;
          }
          std::getline(SS, RawAddress, ',');
          if (SS.eof()) {
            Print("Missing arguments");
            return;
          }
          std::getline(SS, RawPort, ',');
          if (SS.eof()) {
            Print("Missing arguments");
            return;
          }
          std::getline(SS, RawHostCounter, ',');
          RawHostCounter = TrimString(RawHostCounter);
          if (SS.eof()) {
            Print("Missing arguments");
            return;
          }
          std::getline(SS, RawEntryKey, ',');
          RawEntryKey = TrimString(RawEntryKey);
          if (SS.eof()) {
            Print("Missing arguments");
            return;
          }
          std::getline(SS, RawName, ',');

          excludedServer = TrimString(RawExcludedServer);
          transform(begin(excludedServer), end(excludedServer), begin(excludedServer), ::tolower);

          gameAddress = TrimString(RawAddress);
          if (ExtractIPv4(gameAddress).empty()) {
            Print("Bad IP address");
            return;
          }

          try {
            gamePort = stoul(TrimString(RawPort));
            size_t posId, posKey;
            gameHostCounter = stoul(RawHostCounter, &posId, 16);
            if (posId != RawHostCounter.length())
              return;
            gameEntryKey = stoul(RawEntryKey, &posKey, 16);
            if (posKey != RawEntryKey.length())
              return;
          } catch (...) {
            Print("Bad numbers");
            return;
          }

         gameName = TrimString(RawName);

          if (!m_Aura->m_Map) {
            QueueChatCommand("A map must be loaded with " + GetCommandToken() + "map first.", User, Whisper, m_IRC);
            return;
          }

          m_Aura->CreateMirror(m_Aura->m_Map, GAME_PUBLIC, gameName, gameAddress, gamePort, gameHostCounter, gameEntryKey, excludedServer, User, this, Whisper);
          for (auto& bnet : m_Aura->m_BNETs) {
            if (bnet->GetInputID() != excludedServer && !bnet->GetIsMirror()) {
              bnet->ResetConnection(false);
              bnet->SetReconnectNextTick(true);
            }
          }
          return;
        }

        //
        // !GETPLAYERS
        // !GP
        //

        case HashCode("gplay"):
        case HashCode("getplayers"):
        {
          if (Payload.empty())
            return;

          const int32_t GameNumber = stoi(Payload) - 1;

          if (-1 < GameNumber && GameNumber < static_cast<int32_t>(m_Aura->m_Games.size())) {
            QueueChatCommand("Players in game [" + m_Aura->m_Games[GameNumber]->GetGameName() + "] are: " + m_Aura->m_Games[GameNumber]->GetPlayers(), User, Whisper, m_IRC);
          } else if (GameNumber == -1 && m_Aura->m_CurrentLobby) {
            if (m_Aura->m_CurrentLobby->GetIsMirror())
              QueueChatCommand("Lobby [" + m_Aura->m_CurrentLobby->GetGameName() + "] is mirrored.", User, Whisper, m_IRC);
            else
              QueueChatCommand("Players in lobby [" + m_Aura->m_CurrentLobby->GetGameName() + "] are: " + m_Aura->m_CurrentLobby->GetPlayers(), User, Whisper, m_IRC);
          } else {
            QueueChatCommand("Game number " + Payload + " doesn't exist", User, Whisper, m_IRC);
          }

          return;
        }

        //
        // !GETOBSERVERS
        // !GO
        //

        case HashCode("gobs"):
        case HashCode("getobservers"):
        {
          if (Payload.empty())
            return;

          const int32_t GameNumber = stoi(Payload) - 1;

          if (-1 < GameNumber && GameNumber < static_cast<int32_t>(m_Aura->m_Games.size())) {
            QueueChatCommand("Observers in game [" + m_Aura->m_Games[GameNumber]->GetGameName() + "] are: " + m_Aura->m_Games[GameNumber]->GetObservers(), User, Whisper, m_IRC);
          } else if (GameNumber == -1 && m_Aura->m_CurrentLobby) {
            if (m_Aura->m_CurrentLobby->GetIsMirror())
              QueueChatCommand("Lobby [" + m_Aura->m_CurrentLobby->GetGameName() + "] is mirrored.", User, Whisper, m_IRC);
            else
              QueueChatCommand("Observers in lobby [" + m_Aura->m_CurrentLobby->GetGameName() + "] are: " + m_Aura->m_CurrentLobby->GetObservers(), User, Whisper, m_IRC);
          } else {
            QueueChatCommand("Game number " + Payload + " doesn't exist", User, Whisper, m_IRC);
          }

          return;
        }
      }
    }

    Print("[BNET: " + m_Config->m_UniqueName + "] [" + User + "] sent unhandled command [" + Message + "]");
  }
  else if (Event == CBNETProtocol::EID_CHANNEL)
  {
    // keep track of current channel so we can rejoin it after hosting a game

    Print("[BNET: " + m_Config->m_UniqueName + "] joined channel [" + Message + "]");

    m_CurrentChannel = Message;
  }
  else if (Event == CBNETProtocol::EID_INFO)
  {
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
      if (AboutPlayer && AboutPlayer->GetRealmID() == m_ServerID) {
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
        }
      }
    }
    if (LogInfo) {
      Print("[INFO: " + m_Config->m_UniqueName + "] " + Message);
    }
  }
  else if (Event == CBNETProtocol::EID_ERROR)
  {
    Print("[ERROR: " + m_Config->m_UniqueName + "] " + Message);
  }
}

bool CBNET::GetEnabled() const
{
  return m_Config->m_Enabled;
}

bool CBNET::GetPvPGN() const
{
  return m_Config->m_AuthPasswordHashType == "pvpgn";
}

string CBNET::GetServer() const
{
  return m_Config->m_HostName;
}

string CBNET::GetInputID() const
{
  return m_Config->m_InputID;
}

string CBNET::GetUniqueDisplayName() const
{
  return m_Config->m_UniqueName;
}

string CBNET::GetCanonicalDisplayName() const
{
  return m_Config->m_CanonicalName;
}

string CBNET::GetDataBaseID() const
{
  return m_Config->m_DataBaseID;
}

string CBNET::GetLogPrefix() const
{
  return "[BNET: " + m_Config->m_UniqueName + "] ";
}

uint8_t CBNET::GetHostCounterID() const
{
  return m_ServerID;
}

string CBNET::GetUserName() const
{
  return m_Config->m_UserName;
}

bool CBNET::GetIsMirror() const
{
  return m_Config->m_IsMirror;
}

bool CBNET::GetTunnelEnabled() const
{
  return m_Config->m_EnableTunnel;
}

string CBNET::GetPublicHostAddress() const
{
  return m_Config->m_PublicHostAddress;
}

uint16_t CBNET::GetPublicHostPort() const
{
  return m_Config->m_PublicHostPort;
}

uint32_t CBNET::GetMaxUploadSize() const
{
  return m_Config->m_MaxUploadSize;
}

string CBNET::GetPrefixedGameName(const string& gameName) const
{
  if (gameName.length() + m_Config->m_GamePrefix.length() < 31) {
    // Check again just in case m_GamePrefix was reloaded and became prohibitively large.
    return m_Config->m_GamePrefix + gameName;
  } else {
    return gameName;
  }
}

bool CBNET::GetAnnounceHostToChat() const
{
  return m_Config->m_AnnounceHostToChat;
}

string CBNET::GetCommandToken() const
{
  return std::string(1, m_Config->m_CommandTrigger);
}

void CBNET::SendGetFriendsList()
{
  if (m_LoggedIn)
    m_Socket->PutBytes(m_Protocol->SEND_SID_FRIENDLIST());
}

void CBNET::SendGetClanList()
{
  if (m_LoggedIn)
    m_Socket->PutBytes(m_Protocol->SEND_SID_CLANMEMBERLIST());
}

void CBNET::QueueEnterChat()
{
  if (m_LoggedIn)
    m_OutPackets.push(m_Protocol->SEND_SID_ENTERCHAT());
}

void CBNET::QueueChatCommand(const string& chatCommand)
{
  if (chatCommand.empty() || !m_LoggedIn)
    return;

  if (m_OutPackets.size() > 10) {
    Print("[BNET: " + m_Config->m_UniqueName + "] too many (" + to_string(m_OutPackets.size()) + ") packets queued, discarding \"" + chatCommand + "\"");
    return;
  }

  Print("[BNET: " + m_Config->m_UniqueName + "] Queued \"" + chatCommand + "\"");

  if (GetPvPGN())
    m_OutPackets.push(m_Protocol->SEND_SID_CHATCOMMAND(chatCommand.substr(0, 200)));
  else if (chatCommand.size() > 255)
    m_OutPackets.push(m_Protocol->SEND_SID_CHATCOMMAND(chatCommand.substr(0, 255)));
  else
    m_OutPackets.push(m_Protocol->SEND_SID_CHATCOMMAND(chatCommand));

  m_HadChatActivity = true;
}

void CBNET::QueueChatCommand(const string& chatCommand, const string& user, bool whisper, const string& irc)
{
  if (chatCommand.empty())
    return;

  // if the destination is IRC send it there

  if (!irc.empty())
  {
    m_Aura->m_IRC->SendMessageIRC(chatCommand, irc);
    return;
  }

  // if whisper is true send the chat command as a whisper to user, otherwise just queue the chat command

  if (whisper)
  {
    QueueChatCommand("/w " + user + " " + chatCommand);
  }
  else
    QueueChatCommand(chatCommand);
}

void CBNET::QueueGameCreate(uint8_t state, const string& gameName, CMap* map, uint32_t hostCounter, uint16_t hostPort)
{
  if (!m_LoggedIn || !map)
    return;

  if (!m_CurrentChannel.empty())
    m_FirstChannel = m_CurrentChannel;

  m_InChat = false;

  if (m_Config->m_EnableTunnel) {
    m_OutPackets.push(m_Protocol->SEND_SID_NETGAMEPORT(m_Config->m_PublicHostPort));
  } else {
    m_OutPackets.push(m_Protocol->SEND_SID_NETGAMEPORT(hostPort));
  }
  QueueGameRefresh(state, gameName, map, hostCounter, true);
}

void CBNET::QueueGameMirror(uint8_t state, const string& gameName, CMap* map, uint32_t hostCounter, uint16_t hostPort)
{
  if (!m_LoggedIn || !map)
    return;

  if (!m_CurrentChannel.empty())
    m_FirstChannel = m_CurrentChannel;

  m_InChat = false;

  m_OutPackets.push(m_Protocol->SEND_SID_NETGAMEPORT(hostPort));
  QueueGameRefresh(state, gameName, map, hostCounter, false);
}

void CBNET::QueueGameRefresh(uint8_t state, const string& gameName, CMap* map, uint32_t hostCounter, bool useServerNamespace)
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

    if (state == GAME_PRIVATE)
      MapGameType |= MAPGAMETYPE_PRIVATEGAME;

    m_OutPackets.push(m_Protocol->SEND_SID_STARTADVEX3(
      state, CreateByteArray(MapGameType, false), map->GetMapGameFlags(),
      // use an invalid map width/height to indicate reconnectable games
      m_Aura->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : map->GetMapWidth(),
      m_Aura->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : map->GetMapHeight(),
      GetPrefixedGameName(gameName), m_Config->m_UserName,
      0, map->GetMapPath(), map->GetMapCRC(), map->GetMapSHA1(),
      hostCounter | (useServerNamespace ? (m_ServerID << 24) : 0)
    ));
  }
}

void CBNET::QueueGameUncreate()
{
  if (m_LoggedIn)
    m_OutPackets.push(m_Protocol->SEND_SID_STOPADV());
}

void CBNET::UnqueueGameRefreshes()
{
  queue<std::vector<uint8_t>> Packets;

  while (!m_OutPackets.empty())
  {
    // TODO: it's very inefficient to have to copy all these packets while searching the queue

    std::vector<uint8_t> Packet = m_OutPackets.front();
    m_OutPackets.pop();

    if (Packet.size() >= 2 && Packet[1] != CBNETProtocol::SID_STARTADVEX3)
      Packets.push(Packet);
  }

  m_OutPackets = Packets;
  Print("[BNET: " + m_Config->m_UniqueName + "] unqueued game refresh packets");
}

void CBNET::ResetConnection(bool Errored)
{
  if (Errored)
    Print2("[BNET: " + m_Config->m_UniqueName + "] disconnected from battle.net due to socket error");
  else
    Print2("[BNET: " + m_Config->m_UniqueName + "] disconnected from battle.net");
  m_LastDisconnectedTime = GetTime();
  m_BNCSUtil->Reset(m_Config->m_UserName, m_Config->m_PassWord);
  m_Socket->Reset();
  m_LoggedIn         = false;
  m_InChat           = false;
  m_WaitingToConnect = true;
}

bool CBNET::GetIsAdmin(string name)
{
  transform(begin(name), end(name), begin(name), ::tolower);

  if (m_Aura->m_DB->AdminCheck(m_Config->m_DataBaseID, name))
    return true;

  return false;
}

bool CBNET::GetIsRootAdmin(string name)
{
  transform(begin(name), end(name), begin(name), ::tolower);

  if (m_Aura->m_DB->RootAdminCheck(m_Config->m_DataBaseID, name))
    return true;

  return false;
}

CDBBan* CBNET::IsBannedName(string name)
{
  transform(begin(name), end(name), begin(name), ::tolower);

  if (CDBBan* Ban = m_Aura->m_DB->BanCheck(m_Config->m_DataBaseID, name))
    return Ban;

  return nullptr;
}

void CBNET::HoldFriends(CGame* game)
{
  for (auto& friend_ : m_Friends)
    game->AddToReserved(friend_);
}

void CBNET::HoldClan(CGame* game)
{
  for (auto& clanmate : m_Clan)
    game->AddToReserved(clanmate);
}

string CBNET::EncodeURIComponent(const string & s) const {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (char c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        escaped << c;
    } else if (c == ' ') {
        escaped << '+';
    } else {
        escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
    }
  }

  return escaped.str();
}

string CBNET::DecodeURIComponent(const string & encoded) const {
  std::ostringstream decoded;

  for (std::size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] == '%' && i + 2 < encoded.size() &&
      isxdigit(encoded[i + 1]) && isxdigit(encoded[i + 2])) {
      int hexValue;
      std::istringstream hexStream(encoded.substr(i + 1, 2));
      hexStream >> std::hex >> hexValue;
      decoded << static_cast<char>(hexValue);
      i += 2;
    } else if (encoded[i] == '+') {
      decoded << ' ';
    } else {
      decoded << encoded[i];
    }
  }

  return decoded.str();
}

vector<pair<string, int>> CBNET::ExtractEpicWarMaps(const string &s, const int maxCount) const {
  std::regex pattern(R"(<a href="/maps/(\d+)/"><b>([^<\n]+)</b></a>)");
  std::vector<std::pair<std::string, int>> Output;

  std::sregex_iterator iter(s.begin(), s.end(), pattern);
  std::sregex_iterator end;

  int count = 0;
  while (iter != end && count < maxCount) {
    std::smatch match = *iter;
    Output.emplace_back(std::make_pair(match[2], std::stoi(match[1])));
    ++iter;
    ++count;
  }

  return Output;
}

string CBNET::GetEpicWarSuggestions(string & pattern, int maxCount) const
{
    string SearchUri = "https://www.epicwar.com/maps/search/?go=1&n=" + EncodeURIComponent(pattern) + "&a=&c=0&p=0&pf=0&roc=0&tft=0&order=desc&sort=downloads&page=1";
    Print("Downloading " + SearchUri + "...");
    std::string Suggestions;
    auto response = cpr::Get(cpr::Url{SearchUri});
    if (response.status_code != 200) {
      return Suggestions;
    }

    std::vector<std::pair<std::string, int>> MatchingMaps = ExtractEpicWarMaps(response.text, maxCount);

    for (const auto& element : MatchingMaps) {
      Suggestions += element.first + " (epicwar-" + to_string(element.second) + "), ";
    }

    Suggestions = Suggestions.substr(0, Suggestions.length() - 2);
    return Suggestions;
}

int CBNET::ParseMapObservers(string s, bool & errored) const
{
  transform(begin(s), end(s), begin(s), ::tolower);
  int result = MAPOBS_NONE;
  if (s == "no" || s == "no observers" || s == "no obs" || s == "sin obs" || s == "sin observador" || s == "sin observadores") {
    result = MAPOBS_NONE;
  } else if (s == "referee" || s == "referees" || s == "arbiter" || s == "arbitro" || s == "arbitros" || s == "árbitros") {
    result = MAPOBS_REFEREES;
  } else if (s == "observadores derrotados" || s == "derrotados" || s == "obs derrotados" || s == "obs on defeat" || s == "observers on defeat" || s == "on defeat" || s == "defeat") {
    result = MAPOBS_ONDEFEAT;
  } else if (s == "full observers" || s == "solo observadores") {
    result = MAPOBS_ALLOWED;
  } else {
    errored = true;
    return result;
  }
  errored = false;
  return result;
}

int CBNET::ParseMapVisibility(string s, bool & errored) const
{
  int result = MAPVIS_DEFAULT;
  transform(begin(s), end(s), begin(s), ::tolower);
  if (s == "no" || s == "default" || s == "predeterminado" || s == "fog" || s == "fog of war" || s == "niebla" || s == "niebla de guerra") {
    result = MAPVIS_DEFAULT;
  } else if (s == "hide terrain" || s == "hide" || s == "ocultar terreno" || s == "ocultar") {
    result = MAPVIS_HIDETERRAIN;
  } else if (s == "map explored" || s == "explored" || s == "mapa explorado" || s == "explorado") {
    result = MAPVIS_EXPLORED;
  } else if (s == "always visible" || s == "always" || s == "visible" || s == "todo visible" || s == "todo" || s == "revelar" || s == "todo revelado") {
    result = MAPVIS_ALWAYSVISIBLE;
  } else {
    errored = true;
    return result;
  }
  errored = false;
  return result;
}

int CBNET::ParseMapRandomHero(string s, bool & errored) const
{
  int result = 0;
  transform(begin(s), end(s), begin(s), ::tolower);
  if (s == "random hero" || s == "rh" || s == "yes" || s == "heroe aleatorio" || s == "aleatorio" || s == "héroe aleatorio") {
    result = MAPFLAG_RANDOMHERO;
  } else if (s == "default" || s == "no" || s == "predeterminado") {
    result = 0;
  } else {
    errored = true;
    return result;
  }
  errored = false;
  return result;
}

pair<string, string> CBNET::ParseMapId(const string s) const
{
  string lower = s;
  transform(begin(lower), end(lower), begin(lower), ::tolower);

  // Custom namespace/protocol
  if (lower.substr(0, 8) == "epicwar-" || lower.substr(0, 8) == "epicwar:") {
    return make_pair("epicwar", MaybeBase10(lower.substr(8, lower.length())));
  }
  if (lower.substr(0, 6) == "local-" || lower.substr(0, 6) == "local:") {
    return make_pair("local", s);
  }
  

  bool isUri = false;
  if (lower.substr(0, 7) == "http://") {
    isUri = true;
    lower = lower.substr(7, lower.length());
  } else if (lower.substr(0, 8) == "https://") {
    isUri = true;
    lower = lower.substr(8, lower.length());
  }

  if (lower.substr(0, 12) == "epicwar.com/") {
    string mapCode = lower.substr(17, lower.length());
    return make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
  }
  if (lower.substr(0, 16) == "www.epicwar.com/") {
    string mapCode = lower.substr(21, lower.length());
    return make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
  }
  if (isUri) {
    return make_pair("remote", string());
  } else {
    return make_pair("local", s);
  }
}

uint8_t CBNET::DownloadRemoteMap(const string & siteId, const string & siteUri, string & downloadUri, string & downloadFilename) const
{
  // Proceed with the download
  Print("[AURA] Downloading <" + siteUri + ">...");
  if (siteId == "epicwar") {
    auto response = cpr::Get(cpr::Url{siteUri});
    if (response.status_code != 200) return 5;
    
    size_t DownloadUriStartIndex = response.text.find("<a href=\"/maps/download/");
    if (DownloadUriStartIndex == string::npos) return 5;
    size_t DownloadUriEndIndex = response.text.find("\"", DownloadUriStartIndex + 24);
    if (DownloadUriEndIndex == string::npos) return 5;
    downloadUri = "https://epicwar.com" + response.text.substr(DownloadUriStartIndex + 9, (DownloadUriEndIndex) - (DownloadUriStartIndex + 9));
    size_t LastSlashIndex = downloadUri.rfind("/");
    if (LastSlashIndex == string::npos) return 5;
    string encodedName = downloadUri.substr(LastSlashIndex + 1, downloadUri.length());
    downloadFilename = DecodeURIComponent(encodedName);
  } else {
    return 5;
  }

  if (downloadFilename.empty() || downloadFilename[0] == '.' || downloadFilename.length() > 80 ||
    downloadFilename.find('/') != string::npos || downloadFilename.find('\\') != string::npos ||
    downloadFilename.find('\0') != string::npos) {
    return 2;
  }
  string DownloadFileExt = downloadFilename.substr(downloadFilename.length() - 4, downloadFilename.length());
  if (DownloadFileExt != ".w3m" && DownloadFileExt != ".w3x") {
    return 2;
  }
  string MapSuffix;
  bool FoundAvailableSuffix = false;
  for (int i = 0; i < 10; i++) {
    if (i != 0) {
      MapSuffix = "~" + to_string(i);
    }
    if (FileExists(m_Aura->m_Config->m_MapPath + downloadFilename.substr(0, downloadFilename.length() - 4) + MapSuffix + DownloadFileExt)) {
      // Map already exists.
      // I'd rather directly open the file with wx flags to avoid racing conditions,
      // but there is no standard C++ way to do this, and cstdio isn't very helpful.
      continue;
    }
    if (m_Aura->m_BusyMaps.find(downloadFilename.substr(0, downloadFilename.length() - 4) + MapSuffix + DownloadFileExt) != m_Aura->m_BusyMaps.end()) {
      // Map already hosted.
      continue;
    }
    FoundAvailableSuffix = true;
    break;
  }
  if (!FoundAvailableSuffix) {
    return 3;
  }
  Print("[AURA] Downloading <" + downloadUri + "> as [" + downloadFilename + "]...");
  downloadFilename = downloadFilename.substr(0, downloadFilename.length() - 4) + MapSuffix + DownloadFileExt;
  string DestinationPath = m_Aura->m_Config->m_MapPath + downloadFilename;
  std::ofstream MapFile(DestinationPath, std::ios_base::out | std::ios_base::binary);
  if (!MapFile.is_open()) {
    MapFile.close();
    return 4;
  }
  cpr::Response MapResponse = cpr::Download(MapFile, cpr::Url{downloadUri}, cpr::Header{{"user-agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:109.0) Gecko/20100101 Firefox/115.0"}});
  MapFile.close();
  if (MapResponse.status_code != 200) {
    return 1;
  }

  return 0;
}
