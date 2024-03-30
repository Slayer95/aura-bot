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

#ifndef AURA_BNET_H_
#define AURA_BNET_H_

#include "includes.h"
#include "config_realm.h"

#include <queue>
#include <iostream>
#include <fstream>
#include <string>
#include <tuple>

#define PACKET_TYPE_GAME_LIST 10
#define PACKET_TYPE_GAME_REFRESH 8
#define PACKET_TYPE_CHAT_BLOCKING 6
#define PACKET_TYPE_CHAT_JOIN 2
#define PACKET_TYPE_PRIORITY 1
#define PACKET_TYPE_DEFAULT 0

//
// CRealm
//

class CAura;
class CCommandContext;
class CTCPClient;
class CBNCSUtilInterface;
class CBNETProtocol;
class CGameProtocol;
class CGame;
class CIncomingChatEvent;
class CDBBan;
class CIRC;
class CMap;

class CRealm
{
public:
  CAura* m_Aura;

private:
  CRealmConfig*                    m_Config;
  CTCPClient*                      m_Socket;                                               // the connection to battle.net
  CBNETProtocol*                   m_Protocol;                                             // battle.net protocol
  CBNCSUtilInterface*              m_BNCSUtil;                                             // the interface to the bncsutil library (used for logging into battle.net)
  std::queue<std::tuple<int64_t, uint32_t, uint8_t, std::vector<uint8_t>>> m_OutPackets;   // queue of outgoing packets to be sent (to prevent getting kicked for flooding)
  std::vector<std::string>         m_Friends;                                              // std::vector of friends
  std::vector<std::string>         m_Clan;                                                 // std::vector of clan members
  std::vector<uint8_t>             m_EXEVersion;                                           // custom exe version for PvPGN users
  std::vector<uint8_t>             m_EXEVersionHash;                                       // custom exe version hash for PvPGN users
  std::string                      m_FirstChannel;                                         // the first chat channel to join upon entering chat (note: store the last channel when entering a game)
  std::string                      m_CurrentChannel;            // the current chat channel
  std::string                      m_HostName;                  // 
  uint8_t                          m_ServerIndex;               // 
  uint32_t                         m_InternalServerID;          // internal server ID, maps 1:1 to CRealmConfig::m_InputID
  uint8_t                          m_PublicServerID;            // for building host counters, which allows matching game join requests to a realm (or none)
  std::string                      m_LastTell;                  // content of the most recent tell or invite sent by the bot in this realm
  std::string                      m_LastTellTo;                // who sent the most recent tell or invite to in this realm
  CCommandContext*                 m_LastTellCtx;               // who told us to send the most recent tell or invite in this realm
  int64_t                          m_LastDisconnectedTime;      // GetTime when we were last disconnected from battle.net
  int64_t                          m_LastConnectionAttemptTime; // GetTime when we last attempted to connect to battle.net
  int64_t                          m_LastGameListTime;          // GetTime when the last game list request was sent
  int64_t                          m_LastOutPacketTicks;        // GetTicks when the last packet was sent for the m_OutPackets queue
  int64_t                          m_LastAdminRefreshTime;      // GetTime when the admin list was last refreshed from the database
  int64_t                          m_LastBanRefreshTime;        // GetTime when the ban list was last refreshed from the database
  int64_t                          m_ReconnectDelay;            // interval between two consecutive connect attempts
  uint32_t                         m_LastOutPacketSize;         // byte size of the last packet we sent from the m_OutPackets queue
  uint32_t                         m_SessionID;                 // reconnection counter
  bool                             m_Exiting;                   // set to true and this class will be deleted next update
  bool                             m_FirstConnect;              // if we haven't tried to connect to battle.net yet
  bool                             m_ReconnectNextTick;         // ignore reconnect delay
  bool                             m_WaitingToConnect;          // if we're waiting to reconnect to battle.net after being disconnected
  bool                             m_LoggedIn;                  // if we've logged into battle.net or not
  bool                             m_InChat;                    // if we've entered chat or not (but we're not necessarily in a chat channel yet
  bool                             m_HadChatActivity;           // whether we've received chat/whisper events
  std::map<std::string, uint8_t>   m_ReplyQuota;                // how many messages we are allowed to queue because of each user's commands
  std::map<std::string, uint8_t>   m_ReplyPendingCount;
  // ReplyQuota implements exponential backoff
  // Factor: Log_2(1 + m_ReplyQuota.size()) * (m_ChatQueue.size() / m_RecvMessages[user])
  // Whenever m_ChatQueue empties:
  //  m_ReplyQuota.clear();
  //  m_RecvMessages.clear();
  //
  // If in threshold, replace message by:
  //   "OK, but don't spam (you may be ignored.)" <- 40 chars length
  //   "Sorry, don't spam (you may be ignored.)" <- 39 chars length
  //
  // m_ChatQueueHighPriority
  // m_ChatQueueLowPriority

  friend class CCommandContext;

public:
  CRealm(CAura* nAura, CRealmConfig* nRealmConfig);
  ~CRealm();
  CRealm(CRealm&) = delete;

  inline bool          GetExiting() const { return m_Exiting; }
  inline bool          GetLoggedIn() const { return m_LoggedIn; }
  inline bool          GetInChat() const { return m_InChat; }
  inline size_t        GetOutPacketsQueued() const { return m_OutPackets.size(); }

  bool                 GetEnabled() const;
  bool                 GetPvPGN() const;
  std::string          GetServer() const;
  uint16_t             GetServerPort() const;
  std::string          GetInputID() const;
  std::string          GetUniqueDisplayName() const;
  std::string          GetCanonicalDisplayName() const;
  std::string          GetDataBaseID() const;
  std::string          GetLogPrefix() const;
  inline uint8_t       GetHostCounterID() const { return m_PublicServerID; }
  inline uint8_t       GetInternalID() const { return m_InternalServerID; }
  std::string          GetLoginName() const;
  bool                 GetIsMirror() const;
  bool                 GetIsVPN() const;
  bool                 GetUsesCustomIPAddress() const;
  bool                 GetUsesCustomPort() const;
  uint16_t             GetPublicHostPort() const;
  sockaddr_storage*    GetPublicHostAddress() const;
  uint32_t             GetMaxUploadSize() const;
  bool                 GetIsFloodImmune() const;
  std::string          GetCommandToken() const;
  std::string          GetPrefixedGameName(const std::string& gameName) const;
  bool                 GetAnnounceHostToChat() const;

  bool                 GetHasEnhancedAntiSpoof() const;
  bool                 GetUnverifiedCannotStartGame() const;
  bool                 GetUnverifiedAutoKickedFromLobby() const;
  CCommandConfig*      GetCommandConfig() const;

  // processing functions

  uint32_t SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool Update(void* fd, void* send_fd);
  void ProcessChatEvent(const CIncomingChatEvent* chatEvent);

  // functions to send packets to battle.net

  void SendGetFriendsList();
  void SendGetClanList();
  void SendNetworkConfig();
  void JoinFirstChannel();
  void QueuePacket(const std::vector<uint8_t> message, uint8_t mode);
  void QueuePacket(const std::vector<uint8_t> message);
  void QueueEnterChat();
  void SendCommand(const std::string& chatCommand);
  void SendChatChannel(const std::string& chatCommand);
  void SendWhisper(const std::string& message, const std::string& user);
  void SendWhisper(const std::string& message, const std::string& user, CCommandContext* nFromCtx);
  void SendChatOrWhisper(const std::string& chatCommand, const std::string& user, bool whisper);
  void TrySendChat(const std::string& chatCommand, const std::string& user, bool isPrivate, std::ostream* errorLog);
  void QueueGameCreate(uint8_t displayMode, const std::string& gameName, CMap* map, uint32_t hostCounter, uint16_t hostPort);
  void QueueGameMirror(uint8_t displayMode, const std::string& gameName, CMap* map, uint32_t hostCounter, uint16_t hostPort);
  void QueueGameRefresh(uint8_t displayMode, const std::string& gameName, CMap* map, uint32_t hostCounter, bool useServerNamespace);
  void QueueGameUncreate();

  void ResetConnection(bool hadError);
  inline void SetReconnectNextTick(bool nReconnectNextTick) { m_ReconnectNextTick = nReconnectNextTick; };

  // other functions

  bool GetIsModerator(std::string name) const;
  bool GetIsAdmin(std::string name) const;
  bool GetIsSudoer(std::string name) const;
  bool IsBannedName(std::string name) const;
  void HoldFriends(CGame* game);
  void HoldClan(CGame* game);

  inline void SetConfig(CRealmConfig* CFG) {
    delete m_Config;
    m_Config = CFG;
  }

  inline void SetHostCounter(const uint8_t nHostCounter) { m_PublicServerID = nHostCounter; }

  private:
};

#endif // AURA_BNET_H_
