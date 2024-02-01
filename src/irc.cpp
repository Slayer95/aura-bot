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

#include "config_irc.h"
#include "irc.h"
#include "aura.h"
#include "socket.h"
#include "util.h"
#include "bnetprotocol.h"
#include "bnet.h"

#include <utility>
#include <algorithm>

class CIRCConfig;

using namespace std;

//////////////
//// CIRC ////
//////////////

CIRC::CIRC(CAura* nAura, CIRCConfig* nConfig)
  : m_Aura(nAura),
    m_Socket(new CTCPClient()),
    m_Config(nConfig),
    m_NickName(nConfig->m_NickName),
    m_ResolvedAddress(string()),
    m_ResolvedHostName(string()),
    m_LastConnectionAttemptTime(0),
    m_LastPacketTime(GetTime()),
    m_LastAntiIdleTime(GetTime()),
    m_Exiting(false),
    m_WaitingToConnect(true)
{
}

CIRC::~CIRC()
{
  delete m_Config;
  delete m_Socket;
}

uint32_t CIRC::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  // irc socket

  if (!m_Socket->HasError() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 0;
  }

  return 1;
}

bool CIRC::Update(void* fd, void* send_fd)
{
  const int64_t Time = GetTime();

  if (m_Socket->HasError())
  {
    // the socket has an error

    Print("[IRC: " + m_Config->m_HostName + "] disconnected due to socket error,  waiting 60 seconds to reconnect");
    m_Socket->Reset();
    m_WaitingToConnect          = true;
    m_LastConnectionAttemptTime = Time;
    return m_Exiting;
  }

  if (m_Socket->GetConnected())
  {
    // the socket is connected and everything appears to be working properly

    if (Time - m_LastPacketTime > 210)
    {
      Print("[IRC: " + m_Config->m_HostName + "] ping timeout,  reconnecting");
      m_Socket->Reset();
      m_WaitingToConnect = true;
      return m_Exiting;
    }

    if (Time - m_LastAntiIdleTime > 60)
    {
      SendIRC("TIME");
      m_LastAntiIdleTime = Time;
    }

    m_Socket->DoRecv(static_cast<fd_set*>(fd));
    ExtractPackets();
    m_Socket->DoSend(static_cast<fd_set*>(send_fd));
    return m_Exiting;
  }

  if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && !m_WaitingToConnect)
  {
    // the socket was disconnected

    Print("[IRC: " + m_Config->m_HostName + "] disconnected, waiting 60 seconds to reconnect");
    m_Socket->Reset();
    m_WaitingToConnect          = true;
    m_LastConnectionAttemptTime = Time;
    return m_Exiting;
  }

  if (m_Socket->GetConnecting())
  {
    // we are currently attempting to connect to irc

    if (m_Socket->CheckConnect())
    {
      // the connection attempt completed

      m_NickName = m_Config->m_NickName;

      if (m_Config->m_HostName.find("quakenet.org") == string::npos && !m_Config->m_Password.empty())
        SendIRC("PASS " + m_Config->m_Password);

      SendIRC("NICK " + m_Config->m_NickName);
      SendIRC("USER " + m_Config->m_UserName + " " + m_Config->m_NickName + " " + m_Config->m_UserName + " :aura-bot");

      m_Socket->DoSend(static_cast<fd_set*>(send_fd));

      Print("[IRC: " + m_Config->m_HostName + "] connected");

      m_LastPacketTime = Time;

      return m_Exiting;
    }
    else if (Time - m_LastConnectionAttemptTime > 15)
    {
      // the connection attempt timed out (15 seconds)

      Print("[IRC: " + m_Config->m_HostName + "] connect timed out, waiting 60 seconds to reconnect");
      m_Socket->Reset();
      m_LastConnectionAttemptTime = Time;
      m_WaitingToConnect          = true;
      return m_Exiting;
    }
  }

  if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && (Time - m_LastConnectionAttemptTime > 60))
  {
    // attempt to connect to irc

    Print("[IRC: " + m_Config->m_HostName + "] connecting to server [" + m_Config->m_HostName + "] on port " + to_string(m_Config->m_Port));

    if (m_ResolvedHostName == m_Config->m_HostName && !m_ResolvedAddress.empty()) {
      // DNS resolution is blocking, so use cache if posible
      m_Socket->Connect(string(), m_ResolvedAddress, m_Config->m_Port);
    } else {
      m_Socket->Connect(string(), m_Config->m_HostName, m_Config->m_Port);
      if (!m_Socket->HasError()) {
        m_ResolvedHostName = m_Config->m_HostName;
        m_ResolvedAddress  = m_Socket->GetIPString();
      }
    }

    m_WaitingToConnect          = false;
    m_LastConnectionAttemptTime = Time;
  }

  return m_Exiting;
}

void CIRC::ExtractPackets()
{
  const int64_t Time = GetTime();
  string*       Recv = m_Socket->GetBytes();

  // separate packets using the CRLF delimiter

  vector<string> Packets = Tokenize(*Recv, '\n');

  for (auto& Packets_Packet : Packets)
  {
    // delete the superflous '\r'

    const string::size_type pos = Packets_Packet.find('\r');

    if (pos != string::npos)
      Packets_Packet.erase(pos, 1);

    // track timeouts

    m_LastPacketTime = Time;

    // ping packet
    // in:  PING :2748459196
    // out: PONG :2748459196
    // respond to the packet sent by the server

    if (Packets_Packet.compare(0, 4, "PING") == 0)
    {
      SendIRC("PONG :" + Packets_Packet.substr(6));
      continue;
    }

    // notice packet
    // in: NOTICE AUTH :*** Checking Ident
    // print the message on console

    if (Packets_Packet.compare(0, 6, "NOTICE") == 0)
    {
      Print("[IRC: " + m_Config->m_HostName + "] " + Packets_Packet);
      continue;
    }

    // now we need to further tokenize each packet
    // the delimiter is space
    // we use a std::vector so we can check its number of tokens

    const vector<string> Tokens = Tokenize(Packets_Packet, ' ');

    // privmsg packet
    // in:  :nickname!~username@hostname PRIVMSG #channel :message
    // print the message, check if it's a command then execute if it is

    if (Tokens.size() > 3 && Tokens[1] == "PRIVMSG")
    {
      // don't bother parsing if the message is very short (1 character)
      // since it's surely not a command

      if (Tokens[3].size() < 3)
        continue;

      string Nickname, Hostname;

      // get the nickname

      uint32_t i = 1;

      for (; Tokens[0][i] != '!'; ++i)
        Nickname += Tokens[0][i];

      // skip the username

      for (; Tokens[0][i] != '@'; ++i)
        ;

      // get the hostname

      for (++i; i < Tokens[0].size(); ++i)
        Hostname += Tokens[0][i];

      // get the channel

      string Channel = Tokens[2];

      // get the message

      string Message = Packets_Packet.substr(Tokens[0].size() + Tokens[1].size() + Tokens[2].size() + 4);

      // relay messages to bnet

      if (Message.empty() || Message[0] != m_Config->m_CommandTrigger)
        continue;

      bool IsRoot = m_Config->m_RootAdmins.find(Hostname) != m_Config->m_RootAdmins.end();
      if (!IsRoot) {
        continue;
      }

      size_t StartIndex = 1;
      size_t EndIndex = Message.find(" ", StartIndex);
      if (EndIndex == string::npos)
        continue;

      string FirstWord = Message.substr(StartIndex, EndIndex - StartIndex);
      StartIndex = EndIndex + 1;
      EndIndex = Message.find(" ", StartIndex);
      if (EndIndex == string::npos)
        continue;

      string SecondWord = Message.substr(StartIndex, EndIndex - StartIndex);
      StartIndex = EndIndex + 1;
      EndIndex = Message.length();

      transform(begin(FirstWord), end(FirstWord), begin(FirstWord), ::tolower);
      transform(begin(SecondWord), end(SecondWord), begin(SecondWord), ::tolower);

      string Payload = Message.substr(StartIndex, EndIndex);

      if (FirstWord == "irc") {
        if (!IsRoot) continue;
        //
        // !NICK
        //

        if (SecondWord == "nick") {
          SendIRC("NICK :" + Payload);
          m_NickName     = Payload;
        }        
        continue;
      }

      if (FirstWord == "bnet") {
        string BNETUser;
        if (IsRoot) {
          BNETUser = Nickname; // TODO: Fixme
        }

        CBNET* Realm = nullptr;

        for (auto& bnet : m_Aura->m_BNETs) {
          if (!bnet->GetEnabled())
            continue;
          if (bnet->GetInputID() == SecondWord) {
            Realm = bnet;
            break;
          }
        }

        if (Realm) {
          const CIncomingChatEvent event = CIncomingChatEvent(CBNETProtocol::EID_IRC, BNETUser, Channel + " " + Realm->GetCommandToken() + Payload);
          Realm->ProcessChatEvent(&event);
        }
        continue;
      }

      continue;
    }

    // kick packet
    // in:  :nickname!~username@hostname KICK #channel nickname :reason
    // out: JOIN #channel
    // rejoin the channel if we're the victim

    if (Tokens.size() == 5 && Tokens[1] == "KICK")
    {
      if (Tokens[3] == m_NickName) {
        SendIRC("JOIN " + Tokens[2]);
      }

      continue;
    }

    // message of the day end packet
    // in: :server 376 nickname :End of /MOTD command.
    // out: JOIN #channel
    // join channels and auth and set +x on QuakeNet

    if (Tokens.size() >= 2 && Tokens[1] == "376") {
      // auth if the server is QuakeNet

      if (m_Config->m_HostName.find("quakenet.org") != string::npos && !m_Config->m_Password.empty()) {
        SendMessageIRC("AUTH " + m_Config->m_UserName + " " + m_Config->m_Password, "Q@CServe.quakenet.org");
        SendIRC("MODE " + m_Config->m_NickName + " +x");
      }

      // join channels

      for (auto& channel : m_Config->m_Channels)
        SendIRC("JOIN " + channel);

      continue;
    }

    // nick taken packet
    // in:  :server 433 CurrentNickname WantedNickname :Nickname is already in use.
    // out: NICK NewNickname
    // append an underscore and send the new nickname

    if (Tokens.size() >= 2 && Tokens[1] == "433")
    {
      // nick taken, append _

      m_NickName += '_';
      SendIRC("NICK " + m_NickName);
      continue;
    }
  }

  // clear the whole buffer
  //TODO: delete only full packets we've processed and leave partial ones in buffer

  m_Socket->ClearRecvBuffer();
}

void CIRC::SendIRC(const string& message)
{
  // max message length is 512 bytes including the trailing CRLF

  if (m_Socket->GetConnected())
    m_Socket->PutBytes(message + LF);
}

void CIRC::SendMessageIRC(const string& message, const string& target)
{
  // max message length is 512 bytes including the trailing CRLF

  if (m_Socket->GetConnected())
  {
    if (target.empty())
      for (auto& channel : m_Config->m_Channels)
        m_Socket->PutBytes("PRIVMSG " + channel + " :" + (message.size() > 450 ? message.substr(0, 450) : message) + LF);
    else
      m_Socket->PutBytes("PRIVMSG " + target + " :" + (message.size() > 450 ? message.substr(0, 450) : message) + LF);
  }
}
