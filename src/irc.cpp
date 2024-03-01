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

#include "config_irc.h"
#include "irc.h"
#include "command.h"
#include "aura.h"
#include "socket.h"
#include "util.h"
#include "bnetprotocol.h"
#include "realm.h"

#include <utility>
#include <algorithm>

class CIRCConfig;

using namespace std;

//////////////
//// CIRC ////
//////////////

CIRC::CIRC(CAura* nAura, CIRCConfig* nConfig)
  : m_Aura(nAura),
    m_Socket(new CTCPClient(AF_INET, "IRC")),
    m_Config(nConfig),
    m_NickName(nConfig->m_NickName),
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

  if (m_Aura->m_SudoIRC == this) {
    m_Aura->m_SudoIRC = nullptr;
  }
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

  if (!m_Config->m_Enabled) {
    if (m_Socket && m_Socket->GetConnected()) {
      m_Socket->Reset();
      m_WaitingToConnect = false;
    }
    return m_Exiting;
  }

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
      Print("[IRC: " + m_Config->m_HostName + "] ping timeout, reconnecting...");
      m_Socket->Reset();
      m_WaitingToConnect = true;
      return m_Exiting;
    }

    if (Time - m_LastAntiIdleTime > 60)
    {
      Send("TIME");
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
        Send("PASS " + m_Config->m_Password);

      Send("NICK " + m_Config->m_NickName);
      Send("USER " + m_Config->m_UserName + " " + m_Config->m_NickName + " " + m_Config->m_UserName + " :aura-bot");

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

  if (!m_Socket->GetConnecting() && !m_Socket->GetConnected() && (Time - m_LastConnectionAttemptTime > 60)) {
    // attempt to connect to irc

    Print("[IRC: " + m_Config->m_HostName + "] connecting to server [" + m_Config->m_HostName + "] on port " + to_string(m_Config->m_Port));
    optional<sockaddr_storage> emptyBindAddress;
    optional<sockaddr_storage> resolvedAddress = m_Aura->m_Net->ResolveHostName(m_Config->m_HostName);
    if (resolvedAddress.has_value()) {
      m_Socket->Connect(emptyBindAddress, resolvedAddress.value(), m_Config->m_Port);
    } else {
      m_Socket->m_HasError = true;
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
      Send("PONG :" + Packets_Packet.substr(6));
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

      if (Message.empty() || Channel.empty() || Message[0] != m_Config->m_CommandTrigger)
        continue;

      char CommandToken = m_Config->m_CommandTrigger;

      bool IsRootAdmin = m_Config->m_RootAdmins.find(Hostname) != m_Config->m_RootAdmins.end();

      size_t NSIndex = Message.find(" ", 1);
      if (NSIndex == string::npos)
        continue;

      string CmdNameSpace = Message.substr(1, NSIndex - 1);
      string Command = Message.substr(NSIndex + 1);
      string Payload;
      size_t PayloadStart = Command.find(" ");
      if (PayloadStart != string::npos) {
         Payload = Command.substr(PayloadStart + 1);
         Command = Command.substr(0, PayloadStart);
      }

      transform(begin(CmdNameSpace), end(CmdNameSpace), begin(CmdNameSpace), ::tolower);
      transform(begin(Command), end(Command), begin(Command), ::tolower);

      if (CmdNameSpace == "irc") {
        if (!IsRootAdmin) continue;
        //
        // !NICK
        //

        if (Command == "nick") {
          Send("NICK :" + Payload);
          m_NickName     = Payload;
        }        
        continue;
      }

      if (CmdNameSpace == "aura") {
        bool IsWhisper = Channel[0] != '#';
        CCommandContext* ctx = new CCommandContext(m_Aura, this, Channel, Nickname, IsWhisper, &std::cout, CommandToken);
        Print("[IRC] Running command !" + Command + " with payload [" + Payload + "]");
        ctx->UpdatePermissions();
        ctx->SetIRCAdmin(IsRootAdmin);
        ctx->Run(Command, Payload);
        delete ctx;
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
        Send("JOIN " + Tokens[2]);
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
        SendUser("AUTH " + m_Config->m_UserName + " " + m_Config->m_Password, "Q@CServe.quakenet.org");
        Send("MODE " + m_Config->m_NickName + " +x");
      }

      // join channels

      for (auto& channel : m_Config->m_Channels)
        Send("JOIN " + channel);

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
      Send("NICK " + m_NickName);
      continue;
    }
  }

  // clear the whole buffer
  m_Socket->ClearRecvBuffer();
}

void CIRC::Send(const string& message)
{
  // max message length is 512 bytes including the trailing CRLF

  if (m_Socket->GetConnected())
    m_Socket->PutBytes(message + LF);
}

void CIRC::SendUser(const string& message, const string& target)
{
  // max message length is 512 bytes including the trailing CRLF

  if (!m_Socket->GetConnected())
    return;

  m_Socket->PutBytes("PRIVMSG " + target + " :" + (message.size() > 450 ? message.substr(0, 450) : message) + LF);
}

void CIRC::SendChannel(const string& message, const string& target)
{
  // Sending messages to channels or to user works exactly the same, except that channels start with #.
  return SendUser(message, target);
}

void CIRC::SendAllChannels(const string& message)
{
  for (auto& channel : m_Config->m_Channels)
    m_Socket->PutBytes("PRIVMSG " + channel + " :" + (message.size() > 450 ? message.substr(0, 450) : message) + LF);
}
