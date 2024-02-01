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

#ifndef AURA_BNET_H_
#define AURA_BNET_H_

#include "includes.h"
#include "config_bnet.h"

#include <queue>
#include <iostream>
#include <fstream>
#include <string>

//
// CBNET
//

class CAura;
class CTCPClient;
class CBNCSUtilInterface;
class CBNETProtocol;
class CGameProtocol;
class CGame;
class CIncomingChatEvent;
class CDBBan;
class CIRC;
class CMap;

class CBNET
{
public:
  CAura* m_Aura;

private:
  CBNETConfig*                     m_Config;
  CTCPClient*                      m_Socket;                    // the connection to battle.net
  CBNETProtocol*                   m_Protocol;                  // battle.net protocol
  CBNCSUtilInterface*              m_BNCSUtil;                  // the interface to the bncsutil library (used for logging into battle.net)
  std::queue<std::vector<uint8_t>> m_OutPackets;                // queue of outgoing packets to be sent (to prevent getting kicked for flooding)
  std::vector<std::string>         m_Friends;                   // std::vector of friends
  std::vector<std::string>         m_Clan;                      // std::vector of clan members
  std::vector<uint8_t>             m_EXEVersion;                // custom exe version for PvPGN users
  std::vector<uint8_t>             m_EXEVersionHash;            // custom exe version hash for PvPGN users
  std::string                      m_ResolvedHostName;          // DNS cache - host name
  std::string                      m_ResolvedAddress;           // DNS cache - resolved IP address
  std::string                      m_FirstChannel;              // the first chat channel to join upon entering chat (note: store the last channel when entering a game)
  std::string                      m_CurrentChannel;            // the current chat channel
  std::string                      m_HostName;                  // 
  uint8_t                          m_ServerIndex;               // 
  uint8_t                          m_ServerID;                  // 
  std::string                      m_IRC;                       // IRC channel we're sending the message to
  int64_t                          m_LastDisconnectedTime;      // GetTime when we were last disconnected from battle.net
  int64_t                          m_LastConnectionAttemptTime; // GetTime when we last attempted to connect to battle.net
  int64_t                          m_LastGameListTime;          // GetTime when the last game list request was sent
  int64_t                          m_LastOutPacketTicks;        // GetTicks when the last packet was sent for the m_OutPackets queue
  int64_t                          m_LastAdminRefreshTime;      // GetTime when the admin list was last refreshed from the database
  int64_t                          m_LastBanRefreshTime;        // GetTime when the ban list was last refreshed from the database
  int64_t                          m_ReconnectDelay;            // interval between two consecutive connect attempts
  uint32_t                         m_LastOutPacketSize;         // byte size of the last packet we sent from the m_OutPackets queue
  bool                             m_Exiting;                   // set to true and this class will be deleted next update
  bool                             m_FirstConnect;              // if we haven't tried to connect to battle.net yet
  bool                             m_ReconnectNextTick;        // ignore reconnect delay
  bool                             m_WaitingToConnect;          // if we're waiting to reconnect to battle.net after being disconnected
  bool                             m_LoggedIn;                  // if we've logged into battle.net or not
  bool                             m_InChat;                    // if we've entered chat or not (but we're not necessarily in a chat channel yet
  bool                             m_HadChatActivity;           // whether we've received chat/whisper events

public:
  CBNET(CAura* nAura, CBNETConfig* nBNETConfig);
  ~CBNET();
  CBNET(CBNET&) = delete;

  inline bool          GetExiting() const { return m_Exiting; }
  inline bool          GetLoggedIn() const { return m_LoggedIn; }
  inline bool          GetInChat() const { return m_InChat; }
  inline uint32_t      GetOutPacketsQueued() const { return m_OutPackets.size(); }

  bool                 GetEnabled() const;
  bool                 GetPvPGN() const;
  std::string          GetServer() const;
  std::string          GetInputID() const;
  std::string          GetUniqueDisplayName() const;
  std::string          GetCanonicalDisplayName() const;
  std::string          GetDataBaseID() const;
  std::string          GetLogPrefix() const;
  uint8_t              GetHostCounterID() const;
  std::string          GetLoginName() const;
  bool                 GetIsMirror() const;
  bool                 GetTunnelEnabled() const;
  uint16_t             GetPublicHostPort() const;
  std::string          GetPublicHostAddress() const;
  uint32_t             GetMaxUploadSize() const;
  std::string          GetCommandToken() const;
  std::string          GetPrefixedGameName(const std::string& gameName) const;
  bool                 GetAnnounceHostToChat() const;

  // processing functions

  uint32_t SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool Update(void* fd, void* send_fd);
  void ProcessChatEvent(const CIncomingChatEvent* chatEvent);

  // functions to send packets to battle.net

  void SendGetFriendsList();
  void SendGetClanList();
  void QueueEnterChat();
  void QueueChatCommand(const std::string& chatCommand);
  void QueueChatCommand(const std::string& chatCommand, const std::string& user, bool whisper, const std::string& irc);
  void QueueGameCreate(uint8_t state, const std::string& gameName, CMap* map, uint32_t hostCounter, uint16_t hostPort);
  void QueueGameMirror(uint8_t state, const std::string& gameName, CMap* map, uint32_t hostCounter, uint16_t hostPort);
  void QueueGameRefresh(uint8_t state, const std::string& gameName, CMap* map, uint32_t hostCounter, bool useServerNamespace);
  void QueueGameUncreate();

  void UnqueueGameRefreshes();
  void ResetConnection(bool hadError);
  inline void SetReconnectNextTick(bool nReconnectNextTick) { m_ReconnectNextTick = nReconnectNextTick; };

  // other functions

  bool GetIsAdmin(std::string name);
  bool GetIsRootAdmin(std::string name);
  CDBBan* IsBannedName(std::string name);
  void HoldFriends(CGame* game);
  void HoldClan(CGame* game);

  void SetConfig(CBNETConfig* CFG) {
    delete m_Config;
    m_Config = CFG;
  };

  private:
};

#endif // AURA_BNET_H_
