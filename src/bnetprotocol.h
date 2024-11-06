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

#ifndef AURA_BNETPROTOCOL_H_
#define AURA_BNETPROTOCOL_H_

//
// CBNETProtocol
//

#define BNET_HEADER_CONSTANT 255

#include <array>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "constants.h"
#include "config.h"

class CConfig;
class CIncomingGameHost;
class CIncomingChatEvent;

namespace BNETProtocol
{
  enum class Magic : uint8_t
  {
    ZERO                   = 0,   // 0x0
    STOPADV                = 2,   // 0x2
    GETADVLISTEX           = 9,   // 0x9
    ENTERCHAT              = 10,  // 0xA
    JOINCHANNEL            = 12,  // 0xC
    CHATMESSAGE            = 14,  // 0xE - PvPGN: CLIENT_MESSAGE
    CHATEVENT              = 15,  // 0xF
    CHECKAD                = 21,  // 0x15
    PUBLICHOST             = 27,  // 0x1B
    STARTADVEX3            = 28,  // 0x1C
    DISPLAYAD              = 33,  // 0x21
    NOTIFYJOIN             = 34,  // 0x22
    PING                   = 37,  // 0x25
    LOGONRESPONSE          = 41,  // 0x29
    AUTH_ACCOUNTSIGNUP     = 42,  // 0x2A
    AUTH_ACCOUNTSIGNUP2    = 61,  // 0x3D
    NETGAMEPORT            = 69,  // 0x45
    AUTH_INFO              = 80,  // 0x50
    AUTH_CHECK             = 81,  // 0x51
    AUTH_ACCOUNTLOGON      = 83,  // 0x53
    AUTH_ACCOUNTLOGONPROOF = 84,  // 0x54
    WARDEN                 = 94,  // 0x5E
    FRIENDLIST             = 101, // 0x65
    FRIENDSUPDATE          = 102, // 0x66
    CLANMEMBERLIST         = 125, // 0x7D
    CLANMEMBERSTATUSCHANGE = 127, // 0x7F
    GETGAMEINFO            = 131, // 0x83
    HOSTGAME               = 132  // 0x84
  };

  enum class KeyResult : uint32_t
  {
    GOOD             = 0,
    BAD              = 1,
    OLD_GAME_VERSION = 256,
    INVALID_VERSION  = 257,
    ROC_KEY_IN_USE   = 513,
    TFT_KEY_IN_USE   = 529
  };

  enum class IncomingChatEvent : uint32_t
  {
    SHOWUSER            = 1,  // received when you join a channel (includes users in the channel and their information)
    JOIN                = 2,  // received when someone joins the channel you're currently in
    LEAVE               = 3,  // received when someone leaves the channel you're currently in
    WHISPER             = 4,  // received a whisper message
    TALK                = 5,  // received when someone talks in the channel you're currently in
    BROADCAST           = 6,  // server broadcast
    CHANNEL             = 7,  // received when you join a channel (includes the channel's name, flags)
    USERFLAGS           = 9,  // user flags updates
    WHISPERSENT         = 10, // sent a whisper message
    CHANNELFULL         = 13, // channel is full
    CHANNELDOESNOTEXIST = 14, // channel does not exist
    CHANNELRESTRICTED   = 15, // channel is restricted
    INFO                = 18, // broadcast/information message
    NOTICE              = 19, // notice/error message
    EMOTE               = 23, // emote
  };
};

class CBNETProtocol
{
private:
  std::array<uint8_t, 4>   m_ClientToken;         // set in constructor
  std::array<uint8_t, 4>   m_LogonType;           // set in RECEIVE_SID_AUTH_INFO
  std::array<uint8_t, 4>   m_ServerToken;         // set in RECEIVE_SID_AUTH_INFO
  std::array<uint8_t, 8>   m_MPQFileTime;         // set in RECEIVE_SID_AUTH_INFO
  std::vector<uint8_t>     m_IX86VerFileName;     // set in RECEIVE_SID_AUTH_INFO
  std::vector<uint8_t>     m_ValueStringFormula;  // set in RECEIVE_SID_AUTH_INFO
  std::vector<uint8_t>     m_KeyStateDescription; // set in RECEIVE_SID_AUTH_CHECK
  std::array<uint8_t, 32>  m_Salt;             // set in RECEIVE_SID_AUTH_ACCOUNTLOGON
  std::array<uint8_t, 32>  m_ServerPublicKey;  // set in RECEIVE_SID_AUTH_ACCOUNTLOGON
  std::vector<uint8_t>     m_UniqueName;          // set in RECEIVE_SID_ENTERCHAT

public:
  CBNETProtocol();
  ~CBNETProtocol();

  inline const std::array<uint8_t, 4>&    GetClientToken() const { return m_ClientToken; }
  inline const std::array<uint8_t, 4>&    GetLogonType() const { return m_LogonType; }
  inline const std::array<uint8_t, 4>&    GetServerToken() const { return m_ServerToken; }
  inline const std::array<uint8_t, 8>&    GetMPQFileTime() const { return m_MPQFileTime; }
  inline const std::vector<uint8_t>       GetIX86VerFileName() const { return m_IX86VerFileName; }
  inline std::string                      GetIX86VerFileNameString() const { return std::string(begin(m_IX86VerFileName), end(m_IX86VerFileName)); }
  inline const std::vector<uint8_t>&      GetValueStringFormula() const { return m_ValueStringFormula; }
  inline std::string                      GetValueStringFormulaString() const { return std::string(begin(m_ValueStringFormula), end(m_ValueStringFormula)); }
  inline std::string                      GetKeyStateDescription() const { return std::string(begin(m_KeyStateDescription), end(m_KeyStateDescription)); }
  inline const std::array<uint8_t, 32>&   GetSalt() const { return m_Salt; }
  inline const std::array<uint8_t, 32>&   GetServerPublicKey() const { return m_ServerPublicKey; }
  inline const std::vector<uint8_t>&      GetUniqueName() const { return m_UniqueName; }

  inline size_t                           GetMessageSize(const std::vector<uint8_t> message) const { return message.size(); }
  inline size_t                           GetWhisperSize(const std::vector<uint8_t> message, const std::vector<uint8_t> name) const { return message.size() + name.size(); }
      
  // receive functions

  bool RECEIVE_SID_ZERO(const std::vector<uint8_t>& data);
  CIncomingGameHost* RECEIVE_SID_GETADVLISTEX(const std::vector<uint8_t>& data);
  bool RECEIVE_SID_ENTERCHAT(const std::vector<uint8_t>& data);
  CIncomingChatEvent* RECEIVE_SID_CHATEVENT(const std::vector<uint8_t>& data);
  bool RECEIVE_SID_CHECKAD(const std::vector<uint8_t>& data);
  bool RECEIVE_SID_STARTADVEX3(const std::vector<uint8_t>& data);
  std::array<uint8_t, 4> RECEIVE_SID_PING(const std::vector<uint8_t>& data);
  bool RECEIVE_SID_AUTH_INFO(const std::vector<uint8_t>& data);
  uint32_t RECEIVE_SID_AUTH_CHECK(const std::vector<uint8_t>& data);
  bool RECEIVE_SID_AUTH_ACCOUNTLOGON(const std::vector<uint8_t>& data);
  bool RECEIVE_SID_AUTH_ACCOUNTLOGONPROOF(const std::vector<uint8_t>& data);
  bool RECEIVE_SID_AUTH_ACCOUNTSIGNUP(const std::vector<uint8_t>& data);
  std::vector<std::string> RECEIVE_SID_FRIENDLIST(const std::vector<uint8_t>& data);
  std::vector<std::string> RECEIVE_SID_CLANMEMBERLIST(const std::vector<uint8_t>& data);
  CConfig* RECEIVE_HOSTED_GAME_CONFIG(const std::vector<uint8_t>& data);

  // send functions

  std::vector<uint8_t> SEND_PROTOCOL_INITIALIZE_SELECTOR();
  std::vector<uint8_t> SEND_SID_ZERO();
  std::vector<uint8_t> SEND_SID_STOPADV();
  std::vector<uint8_t> SEND_SID_GETADVLISTEX();
  std::vector<uint8_t> SEND_SID_ENTERCHAT();
  std::vector<uint8_t> SEND_SID_JOINCHANNEL(const std::string& channel);
  std::vector<uint8_t> SEND_SID_CHAT_PUBLIC(const std::string& message);
  std::vector<uint8_t> SEND_SID_CHAT_WHISPER(const std::string& message, const std::string& user);
  std::vector<uint8_t> SEND_SID_CHAT_PUBLIC(const std::vector<uint8_t>& message);
  std::vector<uint8_t> SEND_SID_CHAT_WHISPER(const std::vector<uint8_t>& message, const std::vector<uint8_t>& user);
  std::vector<uint8_t> SEND_SID_CHECKAD();
  std::vector<uint8_t> SEND_SID_PUBLICHOST(const std::array<uint8_t, 4> address, uint16_t port);
  std::vector<uint8_t> SEND_SID_STARTADVEX3(uint8_t state, const uint32_t mapGameType, const uint32_t mapFlags, const std::array<uint8_t, 2>& mapWidth, const std::array<uint8_t, 2>& mapHeight, const std::string& gameName, const std::string& hostName, uint32_t upTime, const std::string& mapPath, const std::array<uint8_t, 4>& mapCRC, const std::array<uint8_t, 20>& mapSHA1, uint32_t hostCounter, uint8_t maxSupportedSlots);
  std::vector<uint8_t> SEND_SID_NOTIFYJOIN(const std::string& gameName);
  std::vector<uint8_t> SEND_SID_PING(const std::array<uint8_t, 4>& pingValue);
  std::vector<uint8_t> SEND_SID_LOGONRESPONSE(const std::vector<uint8_t>& clientToken, const std::vector<uint8_t>& serverToken, const std::vector<uint8_t>& passwordHash, const std::string& accountName);
  std::vector<uint8_t> SEND_SID_NETGAMEPORT(uint16_t serverPort);
  std::vector<uint8_t> SEND_SID_AUTH_INFO(uint8_t ver, uint32_t localeID, const std::string& CountryShort, const std::string& country);
  std::vector<uint8_t> SEND_SID_AUTH_CHECK(const std::array<uint8_t, 4>& clientToken, const std::array<uint8_t, 4>& exeVersion, const std::array<uint8_t, 4>& exeVersionHash, const std::vector<uint8_t>& keyInfoROC, const std::vector<uint8_t>& keyInfoTFT, const std::string& exeInfo, const std::string& keyOwnerName);
  std::vector<uint8_t> SEND_SID_AUTH_ACCOUNTLOGON(const std::array<uint8_t, 32>& clientPublicKey, const std::string& accountName);
  std::vector<uint8_t> SEND_SID_AUTH_ACCOUNTLOGONPROOF(const std::array<uint8_t, 20>& clientPasswordProof);
  std::vector<uint8_t> SEND_SID_AUTH_ACCOUNTSIGNUP(const std::string& userName, const std::array<uint8_t, 20>& clientPasswordProof);
  std::vector<uint8_t> SEND_SID_FRIENDLIST();
  std::vector<uint8_t> SEND_SID_CLANMEMBERLIST();

  // other functions

private:
  bool ValidateLength(const std::vector<uint8_t>& content);
};

//
// CIncomingGameHost
//

class CIncomingGameHost
{
private:
  std::string                 m_GameName;
  std::array<uint8_t, 4>      m_IP;
  std::array<uint8_t, 4>      m_HostCounter;
  uint16_t                    m_Port;

public:
  CIncomingGameHost(std::array<uint8_t, 4>& nIP, uint16_t nPort, const std::vector<uint8_t>& nGameName, std::array<uint8_t, 4>& nHostCounter);
  ~CIncomingGameHost();

  std::string                           GetIPString() const;
  inline const std::array<uint8_t, 4>&  GetIP() const { return m_IP; }
  inline const uint16_t&                GetPort() const { return m_Port; }
  inline const std::string&             GetGameName() const { return m_GameName; }
  inline const std::array<uint8_t, 4>&  GetHostCounter() const { return m_HostCounter; }
};

//
// CIncomingChatEvent
//

class CIncomingChatEvent
{
private:
  std::string                      m_User;
  std::string                      m_Message;
  BNETProtocol::IncomingChatEvent m_ChatEvent;

public:
  CIncomingChatEvent(BNETProtocol::IncomingChatEvent nChatEvent, std::string nUser, std::string nMessage);
  ~CIncomingChatEvent();

  inline BNETProtocol::IncomingChatEvent GetChatEvent() const { return m_ChatEvent; }
  inline std::string                      GetUser() const { return m_User; }
  inline std::string                      GetMessage() const { return m_Message; }
};

#endif // AURA_BNETPROTOCOL_H_
