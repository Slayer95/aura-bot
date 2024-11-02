/*

  Copyright [2024] [Leonardo Julca]

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:
7
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

#ifndef AURA_GAMEPROTOCOL_H_
#define AURA_GAMEPROTOCOL_H_

#include "includes.h"
#include "constants.h"
#include "aura.h"

//
// CGameProtocol
//

class CAura;
class CGameUser;
class CIncomingJoinRequest;
class CIncomingAction;
class CIncomingChatPlayer;
class CIncomingMapSize;
class CGameSlot;

class CGameProtocol
{
public:
  CAura* m_Aura;
  std::vector<uint8_t>  m_EmptyAction;

  enum Protocol
  {
    W3GS_PING_FROM_HOST     = 1,  // 0x01
    W3GS_SLOTINFOJOIN       = 4,  // 0x04
    W3GS_REJECTJOIN         = 5,  // 0x05
    W3GS_PLAYERINFO         = 6,  // 0x06
    W3GS_PLAYERLEAVE_OTHERS = 7,  // 0x07
    W3GS_GAMELOADED_OTHERS  = 8,  // 0x08
    W3GS_SLOTINFO           = 9,  // 0x09
    W3GS_COUNTDOWN_START    = 10, // 0x0A
    W3GS_COUNTDOWN_END      = 11, // 0x0B
    W3GS_INCOMING_ACTION    = 12, // 0x0C
    W3GS_CHAT_FROM_HOST     = 15, // 0x0F
    W3GS_START_LAG          = 16, // 0x10
    W3GS_STOP_LAG           = 17, // 0x11
    W3GS_HOST_KICK_PLAYER   = 28, // 0x1C
    W3GS_REQJOIN            = 30, // 0x1E
    W3GS_LEAVEGAME          = 33, // 0x21
    W3GS_GAMELOADED_SELF    = 35, // 0x23
    W3GS_OUTGOING_ACTION    = 38, // 0x26
    W3GS_OUTGOING_KEEPALIVE = 39, // 0x27
    W3GS_CHAT_TO_HOST       = 40, // 0x28
    W3GS_DROPREQ            = 41, // 0x29
    W3GS_SEARCHGAME         = 47, // 0x2F (UDP/LAN)
    W3GS_GAMEINFO           = 48, // 0x30 (UDP/LAN)
    W3GS_CREATEGAME         = 49, // 0x31 (UDP/LAN)
    W3GS_REFRESHGAME        = 50, // 0x32 (UDP/LAN)
    W3GS_DECREATEGAME       = 51, // 0x33 (UDP/LAN)
    W3GS_CHAT_OTHERS        = 52, // 0x34
    W3GS_PING_FROM_OTHERS   = 53, // 0x35
    W3GS_PONG_TO_OTHERS     = 54, // 0x36
    W3GS_MAPCHECK           = 61, // 0x3D
    W3GS_STARTDOWNLOAD      = 63, // 0x3F
    W3GS_MAPSIZE            = 66, // 0x42
    W3GS_MAPPART            = 67, // 0x43
    W3GS_MAPPARTNOTOK       = 69, // 0x45 - just a guess, received this packet after forgetting to send a crc in W3GS_MAPPART (f7 45 0a 00 01 02 01 00 00 00)
    W3GS_PONG_TO_HOST       = 70, // 0x46
    W3GS_INCOMING_ACTION2   = 72  // 0x48 - received this packet when there are too many actions to fit in W3GS_INCOMING_ACTION
  };

  explicit CGameProtocol(CAura* nAura);
  ~CGameProtocol();

  // utility functions

  const std::vector<uint8_t>& GetEmptyAction() const { return m_EmptyAction; }

  // receive functions

  CIncomingJoinRequest* RECEIVE_W3GS_REQJOIN(const std::vector<uint8_t>& data, uint8_t unsafeNameHandler, bool pipeConsideredHarmful);
  uint32_t RECEIVE_W3GS_LEAVEGAME(const std::vector<uint8_t>& data);
  bool RECEIVE_W3GS_GAMELOADED_SELF(const std::vector<uint8_t>& data);
  CIncomingAction* RECEIVE_W3GS_OUTGOING_ACTION(const std::vector<uint8_t>& data, uint8_t UID);
  uint32_t RECEIVE_W3GS_OUTGOING_KEEPALIVE(const std::vector<uint8_t>& data);
  CIncomingChatPlayer* RECEIVE_W3GS_CHAT_TO_HOST(const std::vector<uint8_t>& data);
  CIncomingMapSize* RECEIVE_W3GS_MAPSIZE(const std::vector<uint8_t>& data);
  uint32_t RECEIVE_W3GS_PONG_TO_HOST(const std::vector<uint8_t>& data);

  // send functions

  std::vector<uint8_t> SEND_W3GS_PING_FROM_HOST();
  std::vector<uint8_t> SEND_W3GS_REQJOIN(const uint32_t HostCounter, const uint32_t EntryKey, const std::string& Name);
  std::vector<uint8_t> SEND_W3GS_SLOTINFOJOIN(uint8_t UID, const std::array<uint8_t, 2>& port, const std::array<uint8_t, 4>& externalIP, const std::vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots);
  std::vector<uint8_t> SEND_W3GS_REJECTJOIN(uint32_t reason);
  std::vector<uint8_t> SEND_W3GS_PLAYERINFO(uint8_t UID, const std::string& name, const std::array<uint8_t, 4>& externalIP, const std::array<uint8_t, 4>& internalIP);
  std::vector<uint8_t> SEND_W3GS_PLAYERINFO_EXCLUDE_IP(uint8_t UID, const std::string& name);
  std::vector<uint8_t> SEND_W3GS_PLAYERLEAVE_OTHERS(uint8_t UID, uint32_t leftCode);
  std::vector<uint8_t> SEND_W3GS_GAMELOADED_OTHERS(uint8_t UID);
  std::vector<uint8_t> SEND_W3GS_SLOTINFO(std::vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots);
  std::vector<uint8_t> SEND_W3GS_COUNTDOWN_START();
  std::vector<uint8_t> SEND_W3GS_COUNTDOWN_END();
  std::vector<uint8_t> SEND_W3GS_INCOMING_ACTION(std::queue<CIncomingAction*> actions, uint16_t sendInterval);
  std::vector<uint8_t> SEND_W3GS_INCOMING_ACTION2(std::queue<CIncomingAction*> actions);
  std::vector<uint8_t> SEND_W3GS_CHAT_FROM_HOST(uint8_t fromUID, const std::vector<uint8_t>& toUIDs, uint8_t flag, const std::vector<uint8_t>& flagExtra, const std::string& message);
  std::vector<uint8_t> SEND_W3GS_START_LAG(std::vector<CGameUser*> users);
  std::vector<uint8_t> SEND_W3GS_STOP_LAG(CGameUser* user);
  std::vector<uint8_t> SEND_W3GS_GAMEINFO(const uint8_t war3Version, const uint32_t mapGameType, const uint32_t mapFlags, const std::vector<uint8_t>& mapWidth, const std::vector<uint8_t>& mapHeight, const std::string& gameName, const std::string& hostName, uint32_t upTime, const std::string& mapPath, const std::vector<uint8_t>& mapHash, uint32_t slotsTotal, uint32_t slotsAvailableOff, uint16_t port, uint32_t hostCounter, uint32_t entryKey);
  std::vector<uint8_t> SEND_W3GS_GAMEINFO_TEMPLATE(uint16_t* gameVersionOffset, uint16_t* dynamicInfoOffset, const uint32_t mapGameType, const uint32_t mapFlags, const std::array<uint8_t, 2>& mapWidth, const std::array<uint8_t, 2>& mapHeight, const std::string& gameName, const std::string& hostName, const std::string& mapPath, const std::array<uint8_t, 4>& mapHash, uint32_t slotsTotal, uint32_t hostCounter, uint32_t entryKey);
  std::vector<uint8_t> SEND_W3GS_CREATEGAME(const uint8_t war3Version, const uint32_t hostCounter);
  std::vector<uint8_t> SEND_W3GS_REFRESHGAME(const uint32_t hostCounter, const uint32_t players, const uint32_t playerSlots);
  std::vector<uint8_t> SEND_W3GS_DECREATEGAME(const uint32_t hostCounter);
  std::vector<uint8_t> SEND_W3GS_MAPCHECK(const std::string& mapPath, const std::array<uint8_t, 4>& mapSize, const std::array<uint8_t, 4>& mapCRC32, const std::array<uint8_t, 4>& mapHash);
  std::vector<uint8_t> SEND_W3GS_MAPCHECK(const std::string& mapPath, const std::array<uint8_t, 4>& mapSize, const std::array<uint8_t, 4>& mapCRC32, const std::array<uint8_t, 4>& mapHash, const std::array<uint8_t, 20>& mapSHA1);
  std::vector<uint8_t> SEND_W3GS_STARTDOWNLOAD(uint8_t fromUID);
  std::vector<uint8_t> SEND_W3GS_MAPPART(uint8_t fromUID, uint8_t toUID, uint32_t start, const std::string* mapData);

  // other functions

private:
  bool ValidateLength(const std::vector<uint8_t>& content);
  std::vector<uint8_t> EncodeSlotInfo(const std::vector<CGameSlot>& slots, uint32_t randomSeed, uint8_t layoutStyle, uint8_t playerSlots);
};

//
// CIncomingJoinRequest
//

class CIncomingJoinRequest
{
private:
  bool                      m_Censored;
  std::string               m_Name;
  std::string               m_OriginalName;
  std::array<uint8_t, 4>    m_IPv4Internal;
  uint32_t                  m_HostCounter;
  uint32_t                  m_EntryKey;

public:
  CIncomingJoinRequest(uint32_t nHostCounter, uint32_t nEntryKey, std::string nName, std::array<uint8_t, 4> nIPv4Internal, uint8_t unsafeNameHandler, bool pipeConsideredHarmful);
  ~CIncomingJoinRequest();

  inline bool                   GetIsCensored() const { return m_Censored; }
  inline uint32_t               GetHostCounter() const { return m_HostCounter; }
  inline uint32_t               GetEntryKey() const { return m_EntryKey; }
  inline std::string            GetName() const { return m_Name; }
  inline std::string            GetOriginalName() const { return m_OriginalName; }
  inline std::array<uint8_t, 4> GetIPv4Internal() const { return m_IPv4Internal; }

  static std::string            CensorName(const std::string& originalName, const bool pipeConsideredHarmful);
};

//
// CIncomingAction
//

class CIncomingAction
{
private:
  std::vector<uint8_t> m_CRC;
  std::vector<uint8_t> m_Action;
  uint8_t              m_UID;

public:
  CIncomingAction(uint8_t nUID, std::vector<uint8_t> nCRC, std::vector<uint8_t> nAction);
  ~CIncomingAction();

  inline uint8_t               GetUID() const { return m_UID; }
  inline std::vector<uint8_t>  GetCRC() const { return m_CRC; }
  inline std::vector<uint8_t>* GetAction() { return &m_Action; }
  inline size_t                GetLength() const {
    size_t result = m_Action.size() + 3;
    return result < 3 ? m_Action.size() : result;
  }
};

//
// CIncomingChatPlayer
//

class CIncomingChatPlayer
{
public:
  enum ChatToHostType
  {
    CTH_MESSAGE        = 0, // a chat message
    CTH_MESSAGEEXTRA   = 1, // a chat message with extra flags
    CTH_TEAMCHANGE     = 2, // a team change request
    CTH_COLOURCHANGE   = 3, // a colour change request
    CTH_RACECHANGE     = 4, // a race change request
    CTH_HANDICAPCHANGE = 5  // a handicap change request
  };

private:
  std::string          m_Message;
  ChatToHostType       m_Type;
  uint8_t              m_Byte;
  uint8_t              m_FromUID;
  uint8_t              m_Flag;
  std::vector<uint8_t> m_ToUIDs;
  std::vector<uint8_t> m_ExtraFlags;

public:
  CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, std::string nMessage);
  CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, std::string nMessage, std::vector<uint8_t> nExtraFlags);
  CIncomingChatPlayer(uint8_t nFromUID, std::vector<uint8_t> nToUIDs, uint8_t nFlag, uint8_t nByte);
  ~CIncomingChatPlayer();

  inline ChatToHostType       GetType() const { return m_Type; }
  inline uint8_t              GetFromUID() const { return m_FromUID; }
  inline std::vector<uint8_t> GetToUIDs() const { return m_ToUIDs; }
  inline uint8_t              GetFlag() const { return m_Flag; }
  inline std::string          GetMessage() const { return m_Message; }
  inline uint8_t              GetByte() const { return m_Byte; }
  inline std::vector<uint8_t> GetExtraFlags() const { return m_ExtraFlags; }
};

class CIncomingMapSize
{
private:
  uint32_t m_MapSize;
  uint8_t  m_SizeFlag;

public:
  CIncomingMapSize(uint8_t nSizeFlag, uint32_t nMapSize);
  ~CIncomingMapSize();

  inline uint8_t  GetSizeFlag() const { return m_SizeFlag; }
  inline uint32_t GetMapSize() const { return m_MapSize; }
};

#endif // AURA_GAMEPROTOCOL_H_
