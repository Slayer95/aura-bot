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
#include "socket.h"
#include "auradb.h"
#include "bncsutilinterface.h"
#include "bnetprotocol.h"
#include "map.h"
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

CBNET::CBNET(CAura* nAura, string nServer, const string& nServerAlias, const string& nCDKeyROC, const string& nCDKeyTFT, string nCountryAbbrev, string nCountry, uint32_t nLocaleID, const string& nUserName, const string& nUserPassword, string nFirstChannel, char nCommandTrigger, uint8_t nWar3Version, std::vector<uint8_t> nEXEVersion, std::vector<uint8_t> nEXEVersionHash, string nPasswordHashType, uint32_t nHostCounterID)
  : m_Aura(nAura),
    m_Socket(new CTCPClient()),
    m_Protocol(new CBNETProtocol()),
    m_BNCSUtil(new CBNCSUtilInterface(nUserName, nUserPassword)),
    m_EXEVersion(move(nEXEVersion)),
    m_EXEVersionHash(move(nEXEVersionHash)),
    m_Server(move(nServer)),
    m_CDKeyROC(nCDKeyROC),
    m_CDKeyTFT(nCDKeyTFT),
    m_CountryAbbrev(move(nCountryAbbrev)),
    m_Country(move(nCountry)),
    m_UserName(nUserName),
    m_UserPassword(nUserPassword),
    m_FirstChannel(move(nFirstChannel)),
    m_PasswordHashType(move(nPasswordHashType)),
    m_LastDisconnectedTime(0),
    m_LastConnectionAttemptTime(0),
    m_LastGameListTime(0),
    m_LastOutPacketTicks(0),
    m_LastAdminRefreshTime(GetTime()),
    m_LastBanRefreshTime(GetTime()),
    m_LastOutPacketSize(0),
    m_LocaleID(nLocaleID),
    m_HostCounterID(nHostCounterID),
    m_War3Version(nWar3Version),
    m_CommandTrigger(nCommandTrigger),
    m_Exiting(false),
    m_FirstConnect(true),
    m_WaitingToConnect(true),
    m_LoggedIn(false),
    m_InChat(false)
{
  if (m_PasswordHashType == "pvpgn" || m_EXEVersion.size() == 4 || m_EXEVersionHash.size() == 4)
  {
    m_PvPGN          = true;
    m_ReconnectDelay = 90;
  }
  else
  {
    m_PvPGN          = false;
    m_ReconnectDelay = 240;
  }

  if (!nServerAlias.empty())
    m_ServerAlias = nServerAlias;
  else
    m_ServerAlias = m_Server;

  m_CDKeyROC = nCDKeyROC;
  m_CDKeyTFT = nCDKeyTFT;

  // remove dashes and spaces from CD keys and convert to uppercase

  m_CDKeyROC.erase(remove(begin(m_CDKeyROC), end(m_CDKeyROC), '-'), end(m_CDKeyROC));
  m_CDKeyTFT.erase(remove(begin(m_CDKeyTFT), end(m_CDKeyTFT), '-'), end(m_CDKeyTFT));
  m_CDKeyROC.erase(remove(begin(m_CDKeyROC), end(m_CDKeyROC), ' '), end(m_CDKeyROC));
  m_CDKeyTFT.erase(remove(begin(m_CDKeyTFT), end(m_CDKeyTFT), ' '), end(m_CDKeyTFT));
  transform(begin(m_CDKeyROC), end(m_CDKeyROC), begin(m_CDKeyROC), ::toupper);
  transform(begin(m_CDKeyTFT), end(m_CDKeyTFT), begin(m_CDKeyTFT), ::toupper);

  if (m_CDKeyROC.size() != 26)
    Print("[BNET: " + m_ServerAlias + "] warning - your ROC CD key is not 26 characters long and is probably invalid");

  if (m_CDKeyTFT.size() != 26)
    Print("[BNET: " + m_ServerAlias + "] warning - your TFT CD key is not 26 characters long and is probably invalid");
}

CBNET::~CBNET()
{
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

    Print2("[BNET: " + m_ServerAlias + "] disconnected from battle.net due to socket error");
    Print("[BNET: " + m_ServerAlias + "] waiting " + to_string(m_ReconnectDelay) + " seconds to reconnect");
    m_BNCSUtil->Reset(m_UserName, m_UserPassword);
    m_Socket->Reset();
    m_LastDisconnectedTime = Time;
    m_LoggedIn             = false;
    m_InChat               = false;
    m_WaitingToConnect     = true;
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
            if (m_Aura->m_UDPForwardGameLists) {
              std::vector<uint8_t> relayPacket = {W3FW_HEADER_CONSTANT, 0, 0, 0};
              std::vector<uint8_t> IPOctets = m_Socket->GetIP();
              std::vector<uint8_t> ServerPort = {static_cast<uint8_t>(6112 >> 8), static_cast<uint8_t>(6112)};
              std::vector<uint8_t> War3Version = {m_War3Version, 0, 0, 0};
              AppendByteArray(relayPacket, IPOctets);
              AppendByteArray(relayPacket, ServerPort);
              AppendByteArray(relayPacket, War3Version);
              AppendByteArrayFast(relayPacket, Data);
              AssignLength(relayPacket);
              m_Aura->m_UDPSocket->SendTo(m_Aura->m_UDPForwardAddress, m_Aura->m_UDPForwardPort, relayPacket);
            }

            break;

          case CBNETProtocol::SID_ENTERCHAT:
            if (m_Protocol->RECEIVE_SID_ENTERCHAT(Data))
            {
              Print("[BNET: " + m_ServerAlias + "] joining channel [" + m_FirstChannel + "]");
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
              Print("[BNET: " + m_ServerAlias + "] startadvex3 failed");
              m_Aura->EventBNETGameRefreshFailed(this);
            }

            break;

          case CBNETProtocol::SID_PING:
            m_Socket->PutBytes(m_Protocol->SEND_SID_PING(m_Protocol->RECEIVE_SID_PING(Data)));
            break;

          case CBNETProtocol::SID_AUTH_INFO:

            if (m_Protocol->RECEIVE_SID_AUTH_INFO(Data))
            {
              if (m_BNCSUtil->HELP_SID_AUTH_CHECK(m_Aura->m_Warcraft3Path, m_CDKeyROC, m_CDKeyTFT, m_Protocol->GetValueStringFormulaString(), m_Protocol->GetIX86VerFileNameString(), m_Protocol->GetClientToken(), m_Protocol->GetServerToken(), m_War3Version))
              {
                // override the exe information generated by bncsutil if specified in the config file
                // apparently this is useful for pvpgn users

                if (m_EXEVersion.size() == 4)
                {
                  Print("[BNET: " + m_ServerAlias + "] using custom exe version bnet_custom_exeversion = " + to_string(m_EXEVersion[0]) + " " + to_string(m_EXEVersion[1]) + " " + to_string(m_EXEVersion[2]) + " " + to_string(m_EXEVersion[3]));
                  m_BNCSUtil->SetEXEVersion(m_EXEVersion);
                }

                if (m_EXEVersionHash.size() == 4)
                {
                  Print("[BNET: " + m_ServerAlias + "] using custom exe version hash bnet_custom_exeversionhash = " + to_string(m_EXEVersionHash[0]) + " " + to_string(m_EXEVersionHash[1]) + " " + to_string(m_EXEVersionHash[2]) + " " + to_string(m_EXEVersionHash[3]));
                  m_BNCSUtil->SetEXEVersionHash(m_EXEVersionHash);
                }

                Print("[BNET: " + m_ServerAlias + "] attempting to auth as Warcraft III: The Frozen Throne");

                m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_CHECK(m_Protocol->GetClientToken(), m_BNCSUtil->GetEXEVersion(), m_BNCSUtil->GetEXEVersionHash(), m_BNCSUtil->GetKeyInfoROC(), m_BNCSUtil->GetKeyInfoTFT(), m_BNCSUtil->GetEXEInfo(), "Aura"));
                if (m_Aura->m_EnablePvPGNTunnel) {
                  m_Socket->PutBytes(m_Protocol->SEND_SID_NULL());
                  m_Socket->PutBytes(m_Protocol->SEND_SID_PUBLICHOST(m_Aura->m_PublicHostAddress, m_Aura->m_PublicHostPort));
                }
              }
              else
              {
                Print("[BNET: " + m_ServerAlias + "] logon failed - bncsutil key hash failed (check your Warcraft 3 path and cd keys), disconnecting");
                m_Socket->Disconnect();
              }
            }

            break;

          case CBNETProtocol::SID_AUTH_CHECK:
            if (m_Protocol->RECEIVE_SID_AUTH_CHECK(Data))
            {
              // cd keys accepted

              Print("[BNET: " + m_ServerAlias + "] cd keys accepted");
              m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGON();
              m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGON(m_BNCSUtil->GetClientKey(), m_UserName));
            }
            else
            {
              // cd keys not accepted

              switch (ByteArrayToUInt32(m_Protocol->GetKeyState(), false))
              {
                case CBNETProtocol::KR_ROC_KEY_IN_USE:
                  Print("[BNET: " + m_ServerAlias + "] logon failed - ROC CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting");
                  break;

                case CBNETProtocol::KR_TFT_KEY_IN_USE:
                  Print("[BNET: " + m_ServerAlias + "] logon failed - TFT CD key in use by user [" + m_Protocol->GetKeyStateDescription() + "], disconnecting");
                  break;

                case CBNETProtocol::KR_OLD_GAME_VERSION:
                  Print("[BNET: " + m_ServerAlias + "] logon failed - game version is too old, disconnecting");
                  break;

                case CBNETProtocol::KR_INVALID_VERSION:
                  Print("[BNET: " + m_ServerAlias + "] logon failed - game version is invalid, disconnecting");
                  break;

                default:
                  Print("[BNET: " + m_ServerAlias + "] logon failed - cd keys not accepted, disconnecting");
                  break;
              }

              m_Socket->Disconnect();
            }

            break;

          case CBNETProtocol::SID_AUTH_ACCOUNTLOGON:
            if (m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGON(Data))
            {
              Print("[BNET: " + m_ServerAlias + "] username [" + m_UserName + "] accepted");

              if (m_PasswordHashType == "pvpgn")
              {
                // pvpgn logon

                Print("[BNET: " + m_ServerAlias + "] using pvpgn logon type (for pvpgn servers only)");
                m_BNCSUtil->HELP_PvPGNPasswordHash(m_UserPassword);
                m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetPvPGNPasswordHash()));
              }
              else
              {
                // battle.net logon

                Print("[BNET: " + m_ServerAlias + "] using battle.net logon type (for official battle.net servers only)");
                m_BNCSUtil->HELP_SID_AUTH_ACCOUNTLOGONPROOF(m_Protocol->GetSalt(), m_Protocol->GetServerPublicKey());
                m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_ACCOUNTLOGONPROOF(m_BNCSUtil->GetM1()));
              }
            }
            else
            {
              Print("[BNET: " + m_ServerAlias + "] logon failed - invalid username, disconnecting");
              m_Socket->Disconnect();
            }

            break;

          case CBNETProtocol::SID_AUTH_ACCOUNTLOGONPROOF:
            if (m_Protocol->RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF(Data))
            {
              // logon successful

              Print("[BNET: " + m_ServerAlias + "] logon successful");
              m_LoggedIn = true;
              m_Socket->PutBytes(m_Protocol->SEND_SID_NETGAMEPORT(m_Aura->m_PublicHostPort));
              m_Socket->PutBytes(m_Protocol->SEND_SID_ENTERCHAT());
              m_Socket->PutBytes(m_Protocol->SEND_SID_FRIENDLIST());
              m_Socket->PutBytes(m_Protocol->SEND_SID_CLANMEMBERLIST());
              m_Socket->PutBytes(m_Protocol->SEND_SID_GETADVLISTEX());
            }
            else
            {
              Print("[BNET: " + m_ServerAlias + "] logon failed - invalid password, disconnecting");

              // try to figure out if the user might be using the wrong logon type since too many people are confused by this

              string Server = m_Server;
              transform(begin(Server), end(Server), begin(Server), ::tolower);

              if (m_PvPGN && (Server == "useast.battle.net" || Server == "uswest.battle.net" || Server == "asia.battle.net" || Server == "europe.battle.net"))
                Print("[BNET: " + m_ServerAlias + R"(] it looks like you're trying to connect to a battle.net server using a pvpgn logon type, check your config file's "battle.net custom data" section)");
              else if (!m_PvPGN && (Server != "useast.battle.net" && Server != "uswest.battle.net" && Server != "asia.battle.net" && Server != "europe.battle.net"))
                Print("[BNET: " + m_ServerAlias + R"(] it looks like you're trying to connect to a pvpgn server using a battle.net logon type, check your config file's "battle.net custom data" section)");

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
        Print("[BNET: " + m_ServerAlias + "] packet queue warning - there are " + to_string(m_OutPackets.size()) + " packets waiting to be sent");

      m_Socket->PutBytes(m_OutPackets.front());
      m_LastOutPacketSize = m_OutPackets.front().size();
      m_OutPackets.pop();
      m_LastOutPacketTicks = Ticks;
    }

    if (Time - m_LastGameListTime >= 60)
    {
      m_Socket->PutBytes(m_Protocol->SEND_SID_GETADVLISTEX());
      m_LastGameListTime = Time;
    }

    m_Socket->DoSend(static_cast<fd_set*>(send_fd));
    return m_Exiting;
  }

  if (!m_Socket->GetConnected() && !m_Socket->GetConnecting() && !m_WaitingToConnect)
  {
    // the socket was disconnected

    Print2("[BNET: " + m_ServerAlias + "] disconnected from battle.net");
    m_LastDisconnectedTime = Time;
    m_BNCSUtil->Reset(m_UserName, m_UserPassword);
    m_Socket->Reset();
    m_LoggedIn         = false;
    m_InChat           = false;
    m_WaitingToConnect = true;
    return m_Exiting;
  }

  if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && (m_FirstConnect || (Time - m_LastDisconnectedTime >= m_ReconnectDelay)))
  {
    // attempt to connect to battle.net

    m_FirstConnect = false;
    Print("[BNET: " + m_ServerAlias + "] connecting to server [" + m_Server + "] on port 6112");

    if (!m_Aura->m_BindAddress.empty())
      Print("[BNET: " + m_ServerAlias + "] attempting to bind to address [" + m_Aura->m_BindAddress + "]");

    if (m_ServerIP.empty())
    {
      m_Socket->Connect(m_Aura->m_BindAddress, m_Server, 6112);

      if (!m_Socket->HasError())
      {
        m_ServerIP = m_Socket->GetIPString();
        Print("[BNET: " + m_ServerAlias + "] resolved and cached server IP address " + m_ServerIP);
      }
    }
    else
    {
      // use cached server IP address since resolving takes time and is blocking

      Print("[BNET: " + m_ServerAlias + "] using cached server IP address " + m_ServerIP);
      m_Socket->Connect(m_Aura->m_BindAddress, m_ServerIP, 6112);
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

      Print2("[BNET: " + m_ServerAlias + "] connected");
      m_Socket->PutBytes(m_Protocol->SEND_PROTOCOL_INITIALIZE_SELECTOR());
      m_Socket->PutBytes(m_Protocol->SEND_SID_AUTH_INFO(m_War3Version, m_LocaleID, m_CountryAbbrev, m_Country));
      m_Socket->DoSend(static_cast<fd_set*>(send_fd));
      m_LastGameListTime       = Time;
      m_LastOutPacketTicks = Ticks;

      while (!m_OutPackets.empty())
        m_OutPackets.pop();

      return m_Exiting;
    }
    else if (Time - m_LastConnectionAttemptTime >= 15)
    {
      // the connection attempt timed out (15 seconds)

      Print("[BNET: " + m_ServerAlias + "] connect timed out");
      Print("[BNET: " + m_ServerAlias + "] waiting 90 seconds to reconnect");
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

  // handle spoof checking for current game
  // this case covers whispers - we assume that anyone who sends a whisper to the bot with message "spoofcheck" should be considered spoof checked
  // note that this means you can whisper "spoofcheck" even in a public game to manually spoofcheck if the /whois fails

  if (Event == CBNETProtocol::EID_WHISPER && m_Aura->m_CurrentLobby)
  {
    if (Message == "s" || Message == "sc" || Message == "spoofcheck")
    {
      m_Aura->m_CurrentLobby->AddToRealmVerified(m_Server, User, true);
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

  if (Event == CBNETProtocol::EID_WHISPER || Event == CBNETProtocol::EID_TALK || Event == CBNETProtocol::EID_IRC)
  {
    if (Event == CBNETProtocol::EID_WHISPER)
      Print2("[WHISPER: " + m_ServerAlias + "] [" + User + "] " + Message);
    else
      Print("[LOCAL: " + m_ServerAlias + "] [" + User + "] " + Message);

    if (Event == CBNETProtocol::EID_IRC) {
      // Too unreliable
      IsAdmin = false;
      IsRootAdmin = false;
    }

    // handle bot commands

    if (Message.empty()) {
      return;
    }

    if (Message[0] != m_CommandTrigger) {
      if (Whisper) {
        QueueChatCommand("Hello, " + User + ". My commands start with comma. Example: ,host siege", User, Whisper, m_IRC);
      }
      return;
    }

    for (const auto& match : m_Aura->m_SudoUsers) {
      if (User == match.first && match.second == m_ServerAlias) {
        // Using the name of a sudoer grants basic admin access (no root).
        // This nickname has been verified with this server, but NOT necessarily with the bot.
        // FIXME(IceSandslash): That's what I wished.
        // In fact, nicknames can be spoofed through IRC, bypassing the server.
        IsMaybeSudoer = true;
        IsAdmin = true;
        break;
      }
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
        Print("[BNET: " + m_ServerAlias + "] non-sudoer [" + User + "] sent command [" + Message + "]");
      } else {
        m_Aura->m_SudoUser = User;
        m_Aura->m_SudoRealm = m_ServerAlias;
        m_Aura->m_SudoAuthCommand = m_Aura->GetSudoAuthCommand(Payload, m_CommandTrigger);
        m_Aura->m_SudoExecCommand = Payload;
        Print("[AURA] Sudoer " + User + " requests command \"" + Payload + "\"");
        Print("[AURA] Confirm with \"" + m_Aura->m_SudoAuthCommand + "\"");
      }
      return;
    } else if (CommandHash == HashCode("sudo")) {
      if (!IsMaybeSudoer) {
        Print("[BNET: " + m_ServerAlias + "] non-sudoer [" + User + "] sent command [" + Message + "]");
      } else if (m_Aura->m_SudoUser == User && m_Aura->m_SudoRealm == m_ServerAlias) {
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
          Print("[BNET: " + m_ServerAlias + "] sudoer [" + User + "] sent unauthenticated command [" + Message + "]");
        }
        m_Aura->m_SudoUser = "";
        m_Aura->m_SudoRealm = "";
        m_Aura->m_SudoAuthCommand = "";
        m_Aura->m_SudoExecCommand = "";
      } else {
        Print("[BNET: " + m_ServerAlias + "] sudoer [" + User + "] sent unauthenticated command [" + Message + "]");
      }
    }

    if (IsSudoExec) {
      Print("[BNET: " + m_ServerAlias + "] admin [" + User + "] sent sudo command [" + Message + "]");

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
          } else if (Payload.empty()) {
            QueueChatCommand("Command to evaluate not specified.", User, Whisper, m_IRC);
          } else {
            PayloadStart = Payload.find(' ');
            if (PayloadStart == string::npos) {
              m_Aura->m_CurrentLobby->EventPlayerBotCommand(string(), this, nullptr, Payload, m_Server, User);
            } else {
              m_Aura->m_CurrentLobby->EventPlayerBotCommand(Payload.substr(PayloadStart + 1, Payload.length()), this, nullptr, Payload.substr(0, PayloadStart), m_Server, User);
            }
          }
          return;
        }

        case HashCode("synccfg"):
        case HashCode("initcfg"):
        {
          std::vector<std::string> AllMaps = FilesMatch(m_Aura->m_MapPath, ".w3x");
          std::vector<std::string> ROCMaps = FilesMatch(m_Aura->m_MapPath, ".w3m");
          AllMaps.insert(AllMaps.end(), ROCMaps.begin(), ROCMaps.end());
          int counter = 0;
          
          for (const auto& File : AllMaps) {
            if (CommandHash != HashCode("synccfg") && m_Aura->m_CachedMaps.find(File) != m_Aura->m_CachedMaps.end()) {
              continue;
            }
            if (File.find(Payload) == string::npos) {
              continue;
            }
            if (!FileExists(m_Aura->m_MapPath + File)) {
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
            std::string CFGPath = m_Aura->m_MapCFGPath + CFGName;

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
          const auto MapCount = FilesMatch(m_Aura->m_MapPath, ".").size();
          const auto CFGCount = FilesMatch(m_Aura->m_MapCFGPath, ".cfg").size();
#else
          const auto MapCount = FilesMatch(m_Aura->m_MapPath, "").size();
          const auto CFGCount = FilesMatch(m_Aura->m_MapCFGPath, ".cfg").size();
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
          string Folder = DeletionType == "cfg" ? m_Aura->m_MapCFGPath : m_Aura->m_MapPath;

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
          string       Message;
          stringstream SS;
          SS << Payload;
          SS >> GameNumber;

          if (SS.fail())
            Print("[BNET: " + m_ServerAlias + "] bad input #1 to saygame command");
          else
          {
            if (SS.eof())
              Print("[BNET: " + m_ServerAlias + "] missing input #2 to saygame command");
            else
            {
              getline(SS, Message);
              string::size_type Start = Message.find_first_not_of(' ');

              if (Start != string::npos)
                Message = Message.substr(Start);

              if (GameNumber - 1 < m_Aura->m_Games.size())
                m_Aura->m_Games[GameNumber - 1]->SendAllChat(IsRaw ? Message : "ADMIN: " + Message);
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

          if (m_Aura->m_CurrentLobby)
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
          m_Aura->m_Enabled = false;
          return;
        }

        //
        // !ENABLE
        //

        case HashCode("enable"):
        {
          QueueChatCommand("Creation of new games has been enabled", User, Whisper, m_IRC);
          m_Aura->m_Enabled = true;
          return;
        }

        //
        // !DISABLEPUB
        //

        case HashCode("disablepub"):
        {
          QueueChatCommand("Creation of new games has been temporarily disabled for non-admins (if there is a game in the lobby it will not be automatically unhosted)", User, Whisper, m_IRC);
          m_Aura->m_EnabledPublic = false;
          return;
        }

        //
        // !DISABLEPUB
        //

        case HashCode("enablepub"):
        {
          QueueChatCommand("Creation of new games has been enabled for non-admins", User, Whisper, m_IRC);
          m_Aura->m_EnabledPublic = true;
          return;
        }

        //
        // !SETDOWNLOADS
        //

        case HashCode("setdownloads"):
        {
          if (Payload.empty())
          {
            if (m_Aura->m_AllowUploads == 0)
              QueueChatCommand("Map downloads disabled", User, Whisper, m_IRC);
            else if (m_Aura->m_AllowUploads == 1)
              QueueChatCommand("Map downloads enabled", User, Whisper, m_IRC);
            else if (m_Aura->m_AllowUploads == 2)
              QueueChatCommand("Conditional map downloads enabled", User, Whisper, m_IRC);

            return;
          }

          try
          {
            const uint32_t Downloads = stoul(Payload);

            if (Downloads == 0)
            {
              QueueChatCommand("Map downloads disabled", User, Whisper, m_IRC);
              m_Aura->m_AllowUploads = 0;
            }
            else if (Downloads == 1)
            {
              QueueChatCommand("Map downloads enabled", User, Whisper, m_IRC);
              m_Aura->m_AllowUploads = 1;
            }
            else if (Downloads == 2)
            {
              QueueChatCommand("Conditional map downloads enabled", User, Whisper, m_IRC);
              m_Aura->m_AllowUploads = 2;
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
            QueueChatCommand("Error. User [" + Payload + "] is already an admin on server [" + m_Server + "]", User, Whisper, m_IRC);
          else if (m_Aura->m_DB->AdminAdd(m_Server, Payload))
            QueueChatCommand("Added user [" + Payload + "] to the admin database on server [" + m_Server + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("Error adding user [" + Payload + "] to the admin database on server [" + m_Server + "]", User, Whisper, m_IRC);

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
            QueueChatCommand("User [" + Payload + "] is an admin on server [" + m_Server + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("User [" + Payload + "] is not an admin on server [" + m_Server + "]", User, Whisper, m_IRC);

          return;
        }

        //
        // !COUNTADMINS
        //

        case HashCode("countadmins"):
        {
          uint32_t Count = m_Aura->m_DB->AdminCount(m_Server);

          if (Count == 0)
            QueueChatCommand("There are no admins on server [" + m_Server + "]", User, Whisper, m_IRC);
          else if (Count == 1)
            QueueChatCommand("There is 1 admin on server [" + m_Server + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("There are " + to_string(Count) + " admins on server [" + m_Server + "]", User, Whisper, m_IRC);

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
            QueueChatCommand("User [" + Payload + "] is not an admin on server [" + m_Server + "]", User, Whisper, m_IRC);
          else if (m_Aura->m_DB->AdminRemove(m_Server, Payload))
            QueueChatCommand("Deleted user [" + Payload + "] from the admin database on server [" + m_Server + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("Error deleting user [" + Payload + "] from the admin database on server [" + m_Server + "]", User, Whisper, m_IRC);

          return;
        }

        //
        // !COUNTBANS
        //

        case HashCode("countbans"):
        {
          uint32_t Count = m_Aura->m_DB->BanCount(m_Server);

          if (Count == 0)
            QueueChatCommand("There are no banned users on server [" + m_Server + "]", User, Whisper, m_IRC);
          else if (Count == 1)
            QueueChatCommand("There is 1 banned user on server [" + m_Server + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("There are " + to_string(Count) + " banned users on server [" + m_Server + "]", User, Whisper, m_IRC);

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
            QueueChatCommand("Error. User [" + Victim + "] is an admin on server [" + m_Server + "]", User, Whisper, m_IRC);
            return;
          }

          CDBBan* Ban = IsBannedName(Victim);

          if (Ban) {
            QueueChatCommand("Error. User [" + Victim + "] is already banned on server [" + m_Server + "]", User, Whisper, m_IRC);
            delete Ban;
            return;
          }
          if (GetIsAdmin(Victim))
            m_Aura->m_DB->AdminRemove(m_Server, Victim);
          if (m_Aura->m_DB->BanAdd(m_Server, Victim, User, Reason))
            QueueChatCommand("Banned user [" + Victim + "] on server [" + m_Server + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("Error banning user [" + Victim + "] on server [" + m_Server + "]", User, Whisper, m_IRC);

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
            QueueChatCommand("User [" + Payload + "] was banned on server [" + m_Server + "] on " + Ban->GetDate() + " by [" + Ban->GetAdmin() + "] because [" + Ban->GetReason() + "]", User, Whisper, m_IRC);
          else
            QueueChatCommand("User [" + Payload + "] is not banned on server [" + m_Server + "]", User, Whisper, m_IRC);

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

            else if (m_Aura->m_CurrentLobby->GetPlayerFromName(m_Aura->m_CurrentLobby->GetOwnerName(), false) && !GetIsRootAdmin(User))
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
            if (!FileExists(m_Aura->m_MapCFGPath)) {
              Print("[BNET: " + m_ServerAlias + "] error listing map configs - map config path doesn't exist");
              QueueChatCommand("Error listing map configs - map config path doesn't exist", User, Whisper, m_IRC);
            } else {
              const vector<string> Matches = m_Aura->ConfigFilesMatch(Payload);

              if (Matches.empty()) {
                QueueChatCommand("No map configs found with that name", User, Whisper, m_IRC);
              } else if (Matches.size() == 1) {
                const string File = Matches.at(0);
                QueueChatCommand("Loading config file [" + m_Aura->m_MapCFGPath + File + "]", User, Whisper, m_IRC);
                CConfig MapCFG;
                MapCFG.Read(m_Aura->m_MapCFGPath + File);
                if (m_Aura->m_Map)
                  delete m_Aura->m_Map;
                m_Aura->m_Map = new CMap(m_Aura, &MapCFG, m_Aura->m_MapCFGPath + File);
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
          if (!m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetCountDownStarted())
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
          if (!m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetCountDownStarted() || m_Aura->m_CurrentLobby->GetNumHumanPlayers() == 0)
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
          if (Payload.empty() || !m_Aura->m_CurrentLobby)
            return;

          if (!m_Aura->m_CurrentLobby->GetLocked())
          {
            uint32_t     SID1, SID2;
            stringstream SS;
            SS << Payload;
            SS >> SID1;

            if (SS.fail())
              Print("[BNET: " + m_ServerAlias + "] bad input #1 to swap command");
            else
            {
              if (SS.eof())
                Print("[BNET: " + m_ServerAlias + "] missing input #2 to swap command");
              else
              {
                SS >> SID2;

                if (SS.fail())
                  Print("[BNET: " + m_ServerAlias + "] bad input #2 to swap command");
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
          if (Payload.empty() || !m_Aura->m_CurrentLobby)
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
                Print("[BNET: " + m_ServerAlias + "] bad input to open command");
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
          if (!m_Aura->m_CurrentLobby)
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
            
            m_Aura->CreateGame(m_Aura->m_Map, GAME_PRIVATE, GameName, Owner, m_Server, User, this, Whisper);
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
            m_Aura->CreateGame(m_Aura->m_Map, GAME_PUBLIC, GameName, Owner, m_Server, User, this, Whisper);
            m_Aura->m_Map = nullptr;
          }

          return;
        }


        //
        // !CLOSE (close slot)
        //

        case HashCode("close"):
        {
          if (Payload.empty() || !m_Aura->m_CurrentLobby)
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
                Print("[BNET: " + m_ServerAlias + "] bad input to close command");
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
          if (!m_Aura->m_CurrentLobby)
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
          if (Payload.empty() || !m_Aura->m_CurrentLobby)
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
              Print("[BNET: " + m_ServerAlias + "] bad input to hold command");
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
        // !SENDLAN
        //

        case HashCode("sendlan"):
        {
          if (Payload.empty() || !m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetCountDownStarted())
            return;

          m_Aura->m_CurrentLobby->LANBroadcastGameInfo();
          return;
        }

        //
        // !WHOIS
        //

        case HashCode("whois"):
        {
          if (!IsMaybeSudoer)
            // Common informational command. Let's not do the whole sudo verification,
            // but BNet admins still have no business using this command.
            return;

          if (Payload.empty())
            return;

            for (auto& bnet : m_Aura->m_BNETs)
              bnet->QueueChatCommand("/whois " + Payload);

          return;
        }

        //
        // !PRINTGAMES
        //

        case HashCode("printgames"):
        {
          if (!IsMaybeSudoer)
            // Common informational command. Let's not do the whole sudo verification,
            // but BNet admins still have no business using this command.
            return;

          QueueChatCommand("/games");
          return;
        }

        //
        // !QUERYGAMES
        //

        case HashCode("querygames"):
        {
          if (!IsMaybeSudoer)
            // Common informational command. Let's not do the whole sudo verification,
            // but BNet admins still have no business using this command.
            return;

          m_Socket->PutBytes(m_Protocol->SEND_SID_GETADVLISTEX());
          m_LastGameListTime = GetTime();
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
            message += m_Aura->m_IRC->m_Server + (!m_Aura->m_IRC->m_WaitingToConnect ? " [online]" : " [offline]");

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
            if (m_Aura->m_CurrentLobby)
              QueueChatCommand("Game [" + m_Aura->m_CurrentLobby->GetDescription() + "] is in the lobby and there are " + to_string(m_Aura->m_Games.size()) + "/" + to_string(m_Aura->m_MaxGames) + " other games in progress", User, Whisper, m_IRC);
            else
              QueueChatCommand("There is no game in the lobby and there are " + to_string(m_Aura->m_Games.size()) + "/" + to_string(m_Aura->m_MaxGames) + " other games in progress", User, Whisper, m_IRC);
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
            QueueChatCommand("Game [" + m_Aura->m_CurrentLobby->GetDescription() + "] is in the lobby and there are " + to_string(m_Aura->m_Games.size()) + "/" + to_string(m_Aura->m_MaxGames) + " other games in progress", User, Whisper, m_IRC);
          else
            QueueChatCommand("There is no game in the lobby and there are " + to_string(m_Aura->m_Games.size()) + "/" + to_string(m_Aura->m_MaxGames) + " other games in progress", User, Whisper, m_IRC);

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
        // !W
        //

        case HashCode("w"):
        {
          if (Payload.empty())
            return;

          // extract the name and the message
          // e.g. "Varlock hello there!" -> name: "Varlock", message: "hello there!"

          string            Name;
          string            Message;
          string::size_type MessageStart = Payload.find(' ');

          if (MessageStart != string::npos) {
            Name    = Payload.substr(0, MessageStart);
            Message = User + " at " + m_ServerAlias + " says: " + Payload.substr(MessageStart + 1);

            for (auto& bnet : m_Aura->m_BNETs)
              bnet->QueueChatCommand(Message, Name, true, string());
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
          if (!m_Aura->m_EnabledPublic && !GetIsAdmin(User) && !GetIsRootAdmin(User)) {
            QueueChatCommand("Apologies. Game hosting is temporarily unavailable.", User, Whisper, m_IRC);
            return;
          }

          bool IsHostCommand = CommandHash == HashCode("host") || CommandHash == HashCode("hostlan");

          stringstream SS(Payload);
          string Map, MapObservers, MapVisibility, MapRandomHero, MapGameName;
          std::string gameName = "gogogo";
          std::getline(SS, Map, ',');
          int MapObserversValue = -1;
          int MapVisibilityValue = -1;
          int MapExtraFlags = 0;
          if (!SS.eof()) {
            getline(SS, MapObservers, ',');
            MapObservers = TrimString(MapObservers);
            transform(begin(MapObservers), end(MapObservers), begin(MapObservers), ::tolower);
            if (MapObservers == "no" || MapObservers == "no observers" || MapObservers == "no obs" || MapObservers == "sin obs" || MapObservers == "sin observador" || MapObservers == "sin observadores") {
              MapObserversValue = MAPOBS_NONE;
            } else if (MapObservers == "referee" || MapObservers == "referees" || MapObservers == "arbiter" || MapObservers == "arbitro" || MapObservers == "arbitros" || MapObservers == "árbitros") {
              MapObserversValue = MAPOBS_REFEREES;
            } else if (MapObservers == "observadores derrotados" || MapObservers == "derrotados" || MapObservers == "obs derrotados" || MapObservers == "obs on defeat" || MapObservers == "observers on defeat" || MapObservers == "on defeat" || MapObservers == "defeat") {
              MapObserversValue = MAPOBS_ONDEFEAT;
            } else if (MapObservers == "full observers" || MapObservers == "solo observadores") {
              MapObserversValue = MAPOBS_ALLOWED;
            } else {
              QueueChatCommand("Opción de observadores \"" + MapObservers + "\" no admitida.", User, Whisper, m_IRC);
              return;
            }
            if (!SS.eof()) {
              getline(SS, MapVisibility, ',');
              MapVisibility = TrimString(MapVisibility);
              transform(begin(MapVisibility), end(MapVisibility), begin(MapVisibility), ::tolower);
              if (MapVisibility == "no" || MapVisibility == "default" || MapVisibility == "predeterminado" || MapVisibility == "fog" || MapVisibility == "fog of war" || MapVisibility == "niebla" || MapVisibility == "niebla de guerra") {
                MapVisibilityValue = MAPVIS_DEFAULT;
              } else if (MapVisibility == "hide terrain" || MapVisibility == "hide" || MapVisibility == "ocultar terreno" || MapVisibility == "ocultar") {
                MapVisibilityValue = MAPVIS_HIDETERRAIN;
              } else if (MapVisibility == "map explored" || MapVisibility == "explored" || MapVisibility == "mapa explorado" || MapVisibility == "explorado") {
                MapVisibilityValue = MAPVIS_EXPLORED;
              } else if (MapVisibility == "always visible" || MapVisibility == "always" || MapVisibility == "visible" || MapVisibility == "todo visible" || MapVisibility == "todo" || MapVisibility == "revelar" || MapVisibility == "todo revelado") {
                MapVisibilityValue = MAPVIS_ALWAYSVISIBLE;
              } else {
                QueueChatCommand("Opción de visibilidad \"" + MapVisibility + "\" no admitida.", User, Whisper, m_IRC);
                return;
              }
              if (!SS.eof()) {
                getline(SS, MapRandomHero, ',');
                MapRandomHero = TrimString(MapRandomHero);
                transform(begin(MapRandomHero), end(MapRandomHero), begin(MapRandomHero), ::tolower);
                if (MapRandomHero == "random hero" || MapRandomHero == "rh" || MapRandomHero == "heroe aleatorio" || MapRandomHero == "aleatorio" || MapRandomHero == "héroe aleatorio") {
                  MapExtraFlags |= MAPFLAG_RANDOMHERO;
                } else if (MapRandomHero == "default" || MapRandomHero == "no" || MapRandomHero == "predeterminado") {
                  MapExtraFlags |= 0;
                } else {
                  QueueChatCommand("Opción de héroe aleatorio \"" + MapRandomHero + "\" no admitida.", User, Whisper, m_IRC);
                  return;
                }
                if (!SS.eof()) {
                  getline(SS, MapGameName, ',');
                  MapGameName = TrimString(MapGameName);
                  if (MapGameName.length() > 0) {
                    gameName = MapGameName;
                  }
                }
              }
            }
          }

          if (Map.empty()) {
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

          string MapSiteUri;
          string MapDownloadUri;
          string ResolvedCFGPath;
          string ResolvedCFGName;
          bool ResolvedCFGExists = false;
          if (!FileExists(m_Aura->m_MapPath)) {
            Print("[BNET: " + m_ServerAlias + "] error listing maps - map path doesn't exist");
            QueueChatCommand("Error listing maps - map path doesn't exist", User, Whisper, m_IRC);
          } else {
            if (IsHostCommand) {
              // Parse autocfg for URLs
              if (Map.substr(0, 8) == "epicwar-") {
                Map = "https://www.epicwar.com/maps/" + Map.substr(8, Map.length());
              }
              // Parse URL shortcuts
              if (Map.substr(0, 12) == "epicwar.com/") {
                Map = "https://www." + MapSiteUri;
              } else if (MapSiteUri.substr(0, 16) == "www.epicwar.com/") {
                Map = "https://" + MapSiteUri;
              }
              // Only download through HTTPS
              if (Map.substr(0, 7) == "http://") {
                Map = "https://" + MapSiteUri.substr(7, MapSiteUri.length());
              }
              if (Map.substr(0, 8) == "https://") {
                MapSiteUri = Map;
                if (!m_Aura->m_AllowDownloads) {
                  QueueChatCommand("Hosting from websites disabled.", User, Whisper, m_IRC);
                  return;
                }
                if (MapSiteUri.substr(8, 12) == "epicwar.com/") {
                  MapSiteUri = "https://www.epicwar.com/" + MapSiteUri.substr(20, MapSiteUri.length());
                }
                if (MapSiteUri.substr(8, 16) == "www.epicwar.com/") {
                  if (m_Aura->m_Games.size() > 0) {
                    QueueChatCommand("Currently hosting " + to_string(m_Aura->m_Games.size()) + " game(s). Downloads are disabled to prevent high latency.");
                    return;
                  }
                  string EpicWarId = MapSiteUri.substr(29, MapSiteUri.length());
                  int TrailingSlashIndex = EpicWarId.find("/");
                  if (TrailingSlashIndex != string::npos) {
                    EpicWarId = EpicWarId.substr(0, TrailingSlashIndex);
                  }
                  if (EpicWarId.length() == 0 || EpicWarId.find(".") != string::npos || EpicWarId.find("\\") != string::npos) {
                    QueueChatCommand("Map not found in EpicWar.", User, Whisper, m_IRC);
                    return;
                  }
                  // Check that we don't download the same map multiple times.
                  ResolvedCFGName = "epicwar-" + EpicWarId + ".cfg";
                  ResolvedCFGPath = m_Aura->m_MapCFGPath + ResolvedCFGName;
                  Print("ResolvedCFGPath: " + ResolvedCFGPath);
                  std::ifstream ResolvedCFGFile(ResolvedCFGPath);
                  if (ResolvedCFGFile.is_open()) {
                    // Fix the Map from the URL to our already downloaded map name
                    ResolvedCFGFile.close();
                    ResolvedCFGExists = true;
                  } else {
                    // Proceed with the download
                    Print("Downloading " + MapSiteUri + "...");
                    auto response = cpr::Get(cpr::Url{MapSiteUri});
                    if (response.status_code != 200) {
                      QueueChatCommand("Map not found in EpicWar. Status code " + to_string(response.status_code), User, Whisper, m_IRC);
                      return;
                    }
                    
                    size_t DownloadUriStartIndex = response.text.find("<a href=\"/maps/download/");
                    if (DownloadUriStartIndex == string::npos) {
                      QueueChatCommand("Map not found in EpicWar.", User, Whisper, m_IRC);
                      return;
                    }
                    size_t DownloadUriEndIndex = response.text.find("\"", DownloadUriStartIndex + 24);
                    if (DownloadUriEndIndex == string::npos) {
                      QueueChatCommand("Map not found in EpicWar.", User, Whisper, m_IRC);
                      return;
                    }
                    MapDownloadUri = "https://epicwar.com" + response.text.substr(DownloadUriStartIndex + 9, (DownloadUriEndIndex) - (DownloadUriStartIndex + 9));
                    size_t LastSlashIndex = MapDownloadUri.rfind("/");
                    if (LastSlashIndex == string::npos) {
                      QueueChatCommand("Map not found in EpicWar.", User, Whisper, m_IRC);
                      return;
                    }
                    string DownloadFileName = DecodeURIComponent(MapDownloadUri.substr(LastSlashIndex + 1, MapDownloadUri.length()));
                    if (DownloadFileName.empty() || DownloadFileName[0] == '.' || DownloadFileName.length() > 80 ||
                      DownloadFileName.find('/') != string::npos || DownloadFileName.find('\\') != string::npos ||
                      DownloadFileName.find('\0') != string::npos) {
                      QueueChatCommand("EpicWar map has an invalid name.", User, Whisper, m_IRC);
                      return;
                    }
                    string DownloadFileExt = DownloadFileName.substr(DownloadFileName.length() - 4, DownloadFileName.length());
                    if (DownloadFileExt != ".w3m" && DownloadFileExt != ".w3x") {
                      QueueChatCommand("EpicWar map has an invalid extension.", User, Whisper, m_IRC);
                      return;
                    }
                    Print("Downloading " + MapDownloadUri + "...");
                    string MapSuffix;
                    bool FoundAvailableSuffix = false;
                    for (int i = 0; i < 10; i++) {
                      if (i != 0) {
                        MapSuffix = "~" + to_string(i);
                      }
                      if (FileExists(m_Aura->m_MapPath + DownloadFileName.substr(0, DownloadFileName.length() - 4) + MapSuffix + DownloadFileExt)) {
                        // Map already exists.
                        // I'd rather directly open the file with wx flags to avoid racing conditions,
                        // but there is no standard C++ way to do this, and cstdio isn't very helpful.
                        continue;
                      }
                      if (m_Aura->m_BusyMaps.find(DownloadFileName.substr(0, DownloadFileName.length() - 4) + MapSuffix + DownloadFileExt) != m_Aura->m_BusyMaps.end()) {
                        // Map already hosted.
                        continue;
                      }
                      FoundAvailableSuffix = true;
                      break;
                    }
                    if (!FoundAvailableSuffix) {
                      QueueChatCommand("Download failed. Duplicate map name.", User, Whisper, m_IRC);
                      return;
                    }
                    DownloadFileName = DownloadFileName.substr(0, DownloadFileName.length() - 4) + MapSuffix + DownloadFileExt;
                    string DestinationPath = m_Aura->m_MapPath + DownloadFileName;
                    std::ofstream MapFile(DestinationPath, std::ios_base::out | std::ios_base::binary);
                    if (!MapFile.is_open()) {
                      QueueChatCommand("Download failed. Unable to start write stream.", User, Whisper, m_IRC);
                      MapFile.close();
                      return;
                    }
                    cpr::Response MapResponse = cpr::Download(MapFile, cpr::Url{MapDownloadUri}, cpr::Header{{"user-agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:109.0) Gecko/20100101 Firefox/115.0"}});
                    MapFile.close();
                    if (MapResponse.status_code != 200) {
                      QueueChatCommand("Failed to download map. Status code " + to_string(MapResponse.status_code), User, Whisper, m_IRC);
                      return;
                    }
                    QueueChatCommand("Downloaded " + MapSiteUri + " as " + DownloadFileName, User, Whisper, m_IRC);
                    Map = DownloadFileName;
                  }
                } else {
                  QueueChatCommand("Download from the specified domain not supported", User, Whisper, m_IRC);
                  return;
                }
              }
            }

            CConfig MapCFG;
            if (ResolvedCFGExists) {
              QueueChatCommand("Loading config file [" + ResolvedCFGPath + "]", User, Whisper, m_IRC);
              MapCFG.Read(ResolvedCFGPath);
            } else {
              // Fuzzy-search
              const vector<string> LocalMatches = m_Aura->MapFilesMatch(Map);
              if (LocalMatches.empty()) {
                if (IsHostCommand) {
                  QueueChatCommand(Map + " not found. Try " + std::string(1, static_cast<char>(m_Aura->m_CommandTrigger)) + "host epicwarlink?", User, Whisper, m_IRC);
                  return;
                }
                QueueChatCommand("Maps: " + GetEpicWarSuggestions(Map, 5), User, Whisper, m_IRC);
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
              const string File = LocalMatches.at(0);
              if (m_Aura->m_ResolveMapToConfig && m_Aura->m_CachedMaps.find(File) != m_Aura->m_CachedMaps.end()) {
                ResolvedCFGName = m_Aura->m_CachedMaps[File];
                ResolvedCFGPath = m_Aura->m_MapCFGPath + ResolvedCFGName;
                QueueChatCommand("Loading config file [" + ResolvedCFGPath + "]", User, Whisper, m_IRC);
                MapCFG.Read(ResolvedCFGPath);
                ResolvedCFGExists = true; // TODO: Read should return success
              } else {
                QueueChatCommand("Loading map file [" + File + "]", User, Whisper, m_IRC);

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
                MapCFG.Set("downloaded_by", User);

                if (File.find("DotA") != string::npos)
                  MapCFG.Set("map_type", "dota");

                if (!ResolvedCFGPath.empty()) {
                  std::vector<uint8_t> OutputBytes = MapCFG.Export();
                  FileWrite(ResolvedCFGPath, OutputBytes.data(), OutputBytes.size());
                  m_Aura->m_CachedMaps[File] = ResolvedCFGName;
                }
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
            } else {
              if (!ResolvedCFGExists && !ResolvedCFGPath.empty() && MapCFG.GetInt("cfg_partial", 0) == 0) {
                // Download and parse successful
                std::vector<uint8_t> OutputBytes = MapCFG.Export();
                FileWrite(ResolvedCFGPath, OutputBytes.data(), OutputBytes.size());
              }
              if (IsHostCommand) {
                m_Aura->CreateGame(
                  m_Aura->m_Map, GAME_PUBLIC, gameName,
                  User, CommandHash == HashCode("hostlan") ? "" : m_Server,
                  User, this, Whisper
                );
                m_Aura->m_Map = nullptr;
              }
            }
          }

          return;
        }

        //
        // !PUB (host public game)
        //

        case HashCode("pub"):
        {
          if (!m_Aura->m_EnabledPublic && !GetIsAdmin(User) && !GetIsRootAdmin(User)) {
            QueueChatCommand("Game hosting temporarily disabled.", User, Whisper, m_IRC);
            return;
          }
          if (!m_Aura->m_Map) {
            QueueChatCommand("A map must be loaded with " + std::string(1, static_cast<char>(m_Aura->m_CommandTrigger)) + "map first.", User, Whisper, m_IRC);
            return;
          }
          if (!Payload.empty())
            m_Aura->CreateGame(m_Aura->m_Map, GAME_PUBLIC, Payload, User, m_Server, User, this, Whisper);

          return;
        }

        //
        // !PRIV (host private game)
        //

        case HashCode("priv"):
        {
          if (!m_Aura->m_EnabledPublic && !GetIsAdmin(User) && !GetIsRootAdmin(User)) {
            QueueChatCommand("Game hosting temporarily disabled.", User, Whisper, m_IRC);
            return;
          }
          if (!m_Aura->m_Map) {
            QueueChatCommand("A map must be loaded with " + std::string(1, static_cast<char>(m_Aura->m_CommandTrigger)) + "map first.", User, Whisper, m_IRC);
            return;
          }
          if (!Payload.empty())
            m_Aura->CreateGame(m_Aura->m_Map, GAME_PRIVATE, Payload, User, m_Server, User, this, Whisper);

          return;
        }

        //
        // !GETPLAYERS
        // !GP
        //

        case HashCode("gp"):
        case HashCode("getplayers"):
        {
          if (Payload.empty())
            return;

          const int32_t GameNumber = stoi(Payload) - 1;

          if (-1 < GameNumber && GameNumber < static_cast<int32_t>(m_Aura->m_Games.size()))
            QueueChatCommand("Players in game [" + m_Aura->m_Games[GameNumber]->GetGameName() + "] are: " + m_Aura->m_Games[GameNumber]->GetPlayers(), User, Whisper, m_IRC);
          else if (GameNumber == -1 && m_Aura->m_CurrentLobby)
            QueueChatCommand("Players in lobby [" + m_Aura->m_CurrentLobby->GetGameName() + "] are: " + m_Aura->m_CurrentLobby->GetPlayers(), User, Whisper, m_IRC);
          else
            QueueChatCommand("Game number " + Payload + " doesn't exist", User, Whisper, m_IRC);

          return;
        }

        //
        // !GETOBSERVERS
        // !GO
        //

        case HashCode("go"):
        case HashCode("getobservers"):
        {
          if (Payload.empty())
            return;

          const int32_t GameNumber = stoi(Payload) - 1;

          if (-1 < GameNumber && GameNumber < static_cast<int32_t>(m_Aura->m_Games.size()))
            QueueChatCommand("Observers in game [" + m_Aura->m_Games[GameNumber]->GetGameName() + "] are: " + m_Aura->m_Games[GameNumber]->GetObservers(), User, Whisper, m_IRC);
          else if (GameNumber == -1 && m_Aura->m_CurrentLobby)
            QueueChatCommand("Observers in lobby [" + m_Aura->m_CurrentLobby->GetGameName() + "] are: " + m_Aura->m_CurrentLobby->GetObservers(), User, Whisper, m_IRC);
          else
            QueueChatCommand("Game number " + Payload + " doesn't exist", User, Whisper, m_IRC);

          return;
        }
      }
    }

    Print("[BNET: " + m_ServerAlias + "] [" + User + "] sent unhandled command [" + Message + "]");
  }
  else if (Event == CBNETProtocol::EID_CHANNEL)
  {
    // keep track of current channel so we can rejoin it after hosting a game

    Print("[BNET: " + m_ServerAlias + "] joined channel [" + Message + "]");

    m_CurrentChannel = Message;
  }
  else if (Event == CBNETProtocol::EID_INFO)
  {
    Print("[INFO: " + m_ServerAlias + "] " + Message);

    // extract the first word which we hope is the username
    // this is not necessarily true though since info messages also include channel MOTD's and such

    if (m_Aura->m_CurrentLobby)
    {
      string            UserName;
      string::size_type Split = Message.find(' ');

      if (Split != string::npos)
        UserName = Message.substr(0, Split);
      else
        UserName = Message;

      if (m_Aura->m_CurrentLobby->GetPlayerFromName(UserName, true))
      {
        // handle spoof checking for current game
        // this case covers whois results which are used when hosting a public game (we send out a "/whois [player]" for each player)
        // at all times you can still /w the bot with "spoofcheck" to manually spoof check

        if (Message.find("Throne in game") != string::npos || Message.find("currently in  game") != string::npos || Message.find("currently in private game") != string::npos)
        {
          // check both the current game name and the last game name against the /whois response
          // this is because when the game is rehosted, players who joined recently will be in the previous game according to battle.net
          // note: if the game is rehosted more than once it is possible (but unlikely) for a false positive because only two game names are checked

          if (Message.find(m_Aura->m_CurrentLobby->GetGameName()) != string::npos || Message.find(m_Aura->m_CurrentLobby->GetLastGameName()) != string::npos)
            m_Aura->m_CurrentLobby->AddToRealmVerified(m_Server, UserName, true);
          else
            m_Aura->m_CurrentLobby->ReportSpoofed(m_Server, UserName);

          return;
        }
      }
    }
  }
  else if (Event == CBNETProtocol::EID_ERROR)
  {
    Print("[ERROR: " + m_ServerAlias + "] " + Message);
  }
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
  if (chatCommand.empty())
    return;

  if (m_LoggedIn)
  {
    if (m_OutPackets.size() <= 10)
    {
      Print("[QUEUED: " + m_ServerAlias + "] " + chatCommand);

      if (m_PvPGN)
        m_OutPackets.push(m_Protocol->SEND_SID_CHATCOMMAND(chatCommand.substr(0, 200)));
      else if (chatCommand.size() > 255)
        m_OutPackets.push(m_Protocol->SEND_SID_CHATCOMMAND(chatCommand.substr(0, 255)));
      else
        m_OutPackets.push(m_Protocol->SEND_SID_CHATCOMMAND(chatCommand));
    }
    else
      Print("[BNET: " + m_ServerAlias + "] too many (" + to_string(m_OutPackets.size()) + ") packets queued, discarding");
  }
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

void CBNET::QueueGameCreate(uint8_t state, const string& gameName, CMap* map, uint32_t hostCounter)
{
  if (m_LoggedIn && map)
  {
    if (!m_CurrentChannel.empty())
      m_FirstChannel = m_CurrentChannel;

    m_InChat = false;

    QueueGameRefresh(state, gameName, map, hostCounter);
  }
}

void CBNET::QueueGameRefresh(uint8_t state, const string& gameName, CMap* map, uint32_t hostCounter)
{
  if (m_LoggedIn && map)
  {
    // construct a fixed host counter which will be used to identify players from this realm
    // the fixed host counter's 4 most significant bits will contain a 4 bit ID (0-15)
    // the rest of the fixed host counter will contain the 28 least significant bits of the actual host counter
    // since we're destroying 4 bits of information here the actual host counter should not be greater than 2^28 which is a reasonable assumption
    // when a player joins a game we can obtain the ID from the received host counter
    // note: LAN broadcasts use an ID of 0, battle.net refreshes use an ID of 1-10, the rest are unused

    uint32_t MapGameType = map->GetMapGameType();
    MapGameType |= MAPGAMETYPE_UNKNOWN0;

    if (state == GAME_PRIVATE)
      MapGameType |= MAPGAMETYPE_PRIVATEGAME;

    m_OutPackets.push(m_Protocol->SEND_SID_STARTADVEX3(
      state, CreateByteArray(MapGameType, false), map->GetMapGameFlags(),
      // use an invalid map width/height to indicate reconnectable games
      m_Aura->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : map->GetMapWidth(),
      m_Aura->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : map->GetMapHeight(),
      gameName, m_UserName,
      0, map->GetMapPath(), map->GetMapCRC(), map->GetMapSHA1(),
      (hostCounter | (m_HostCounterID << 28))
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
  Print("[BNET: " + m_ServerAlias + "] unqueued game refresh packets");
}

bool CBNET::GetIsAdmin(string name)
{
  transform(begin(name), end(name), begin(name), ::tolower);

  if (m_Aura->m_DB->AdminCheck(m_Server, name))
    return true;

  return false;
}

bool CBNET::GetIsRootAdmin(string name)
{
  transform(begin(name), end(name), begin(name), ::tolower);

  if (m_Aura->m_DB->RootAdminCheck(m_Server, name))
    return true;

  return false;
}

CDBBan* CBNET::IsBannedName(string name)
{
  transform(begin(name), end(name), begin(name), ::tolower);

  if (CDBBan* Ban = m_Aura->m_DB->BanCheck(m_Server, name))
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

std::string CBNET::EncodeURIComponent(const std::string &s) {
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

std::string CBNET::DecodeURIComponent(const std::string& encoded) {
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

std::vector<std::pair<std::string, int>> CBNET::ExtractEpicWarMaps(const std::string &s, const int maxCount) {
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

std::string CBNET::GetEpicWarSuggestions(string pattern, int maxCount)
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
