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

#ifndef AURA_GAMEPLAYER_H_
#define AURA_GAMEPLAYER_H_

#include "socket.h"

#include <queue>
#include <string>
#include <optional>

class CStreamIOSocket;
class CGameProtocol;
class CGame;
class CIncomingJoinRequest;
class CRealm;
class CAura;

#define PREPLAYER_CONNECTION_OK 0u
#define PREPLAYER_CONNECTION_DESTROY 1u
#define PREPLAYER_CONNECTION_PROMOTED 2u

#define CONSISTENT_PINGS_COUNT 3u
#define MAXIMUM_PINGS_COUNT 6u
#define MAX_PING_WEIGHT 4u

#define SMART_COMMAND_NONE 0u
#define SMART_COMMAND_GO 1u

#define INCOMING_CONNECTION_TYPE_NONE 0u
#define INCOMING_CONNECTION_TYPE_UDP_TUNNEL 1u
#define INCOMING_CONNECTION_TYPE_PROMOTED_PLAYER 2u
#define INCOMING_CONNECTION_TYPE_KICKED_PLAYER 3u
#define INCOMING_CONNECTION_TYPE_VLAN 4u

//
// CGameConnection
//

class CGameConnection
{
public:
  CAura*                  m_Aura;
  CGameProtocol*          m_Protocol;
  uint16_t                m_Port;
  uint8_t                 m_Type;
  std::optional<int64_t>  m_Timeout;

protected:
  // note: we permit m_Socket to be NULL in this class to allow for the virtual host player which doesn't really exist
  // it also allows us to convert CGameConnections to CGamePlayers without the CGameConnection's destructor closing the socket

  CStreamIOSocket*          m_Socket;
  CIncomingJoinRequest* m_IncomingJoinPlayer;
  bool                 m_DeleteMe;

public:
  CGameConnection(CGameProtocol* nProtocol, CAura* nAura, uint16_t nPort, CStreamIOSocket* nSocket);
  ~CGameConnection();

  inline CStreamIOSocket*           GetSocket() const { return m_Socket; }
  inline bool                       GetUsingIPv6() const { return m_Socket->GetIsInnerIPv6(); }
  inline std::array<uint8_t, 4>     GetIPv4() const { return m_Socket->GetIPv4(); }
  inline std::string                GetIPString() const { return m_Socket->GetIPString(); }
  inline std::string                GetIPStringStrict() const { return m_Socket->GetIPStringStrict(); }
  inline sockaddr_storage*          GetRemoteAddress() const { return &(m_Socket->m_RemoteHost); }
  inline bool                       GetIsUDPTunnel() const { return m_Type == INCOMING_CONNECTION_TYPE_UDP_TUNNEL; }
  inline bool                       GetIsVLAN() const { return m_Type == INCOMING_CONNECTION_TYPE_VLAN; }
  inline uint8_t                    GetType() const { return m_Type; }
  inline uint16_t                   GetPort() const { return m_Port; }
  inline bool                       GetDeleteMe() const { return m_DeleteMe; }
  inline CIncomingJoinRequest*      GetJoinPlayer() const { return m_IncomingJoinPlayer; }

  inline void SetSocket(CStreamIOSocket* nSocket) { m_Socket = nSocket; }
  inline void SetType(const uint8_t nType) { m_Type = nType; }
  inline void SetDeleteMe(bool nDeleteMe) { m_DeleteMe = nDeleteMe; }

  // processing functions

  void SetTimeout(const int64_t nTime);
  void ResetConnection();
  uint8_t Update(void* fd, void* send_fd);

  // other functions

  void Send(const std::vector<uint8_t>& data) const;
};

//
// CGameUser
//

class CGameUser
{
public:
  CGameProtocol* m_Protocol;
  CGame*         m_Game;

protected:
  CStreamIOSocket* m_Socket; // note: we permit m_Socket to be NULL in this class to allow for the virtual host player which doesn't really exist

private:
  std::array<uint8_t, 4>           m_IPv4Internal;                 // the player's internal IP address as reported by the player when connecting
  std::vector<uint32_t>            m_RTTValues;                        // store the last few (10) pings received so we can take an average
  std::queue<uint32_t>             m_CheckSums;                    // the last few checksums the player has sent (for detecting desyncs)
  std::queue<std::vector<uint8_t>> m_GProxyBuffer;                 // buffer with data used with GProxy++
  std::string                      m_LeftReason;                   // the reason the player left the game
  uint32_t                         m_RealmInternalId;
  std::string                      m_RealmHostName;                // the realm the player joined on (probable, can be spoofed)
  std::string                      m_Name;                         // the player's name
  size_t                           m_TotalPacketsSent;             // the total number of packets sent to the player
  uint32_t                         m_TotalPacketsReceived;         // the total number of packets received from the player
  uint32_t                         m_LeftCode;                     // the code to be sent in W3GS_PLAYERLEAVE_OTHERS for why this player left the game
  bool                             m_QuitGame;
  uint32_t                         m_PongCounter;
  uint32_t                         m_SyncCounterOffset;              // missed keepalive packets we are gonna ignore
  uint32_t                         m_SyncCounter;                  // the number of keepalive packets received from this player
  int64_t                          m_JoinTime;                     // GetTime when the player joined the game (used to delay sending the /whois a few seconds to allow for some lag)
  uint32_t                         m_LastMapPartSent;              // the last mappart sent to the player (for sending more than one part at a time)
  uint32_t                         m_LastMapPartAcked;             // the last mappart acknowledged by the player
  int64_t                          m_StartedDownloadingTicks;      // GetTicks when the player started downloading the map
  int64_t                          m_FinishedDownloadingTime;      // GetTime when the player finished downloading the map
  int64_t                          m_FinishedLoadingTicks;         // GetTicks when the player finished loading the game
  int64_t                          m_StartedLaggingTicks;          // GetTicks when the player started laggin
  int64_t                          m_LastGProxyWaitNoticeSentTime; // GetTime when the last disconnection notice has been sent when using GProxy++
  uint32_t                         m_GProxyReconnectKey;           // the GProxy++ reconnect key
  int64_t                          m_KickByTime;
  int64_t                          m_LastGProxyAckTime;            // GetTime when we last acknowledged GProxy++ packet
  uint8_t                          m_UID;                          // the player's UID
  bool                             m_Verified;                     // if the player has spoof checked or not
  bool                             m_Owner;                        // if the player has spoof checked or not
  bool                             m_Reserved;                     // if the player is reserved (VIP) or not
  std::optional<int64_t>           m_SudoMode;                     // if the player has enabled sudo mode, its expiration time
  bool                             m_Observer;                     // if the player is an observer
  bool                             m_PowerObserver;                // if the player is a referee - referees can be demoted to full observers
  bool                             m_WhoisShouldBeSent;            // if a battle.net /whois should be sent for this player or not
  bool                             m_WhoisSent;                    // if we've sent a battle.net /whois for this player yet (for spoof checking)
  bool                             m_MapReady;                     // if we're allowed to download the map or not (used with permission based map downloads)
  bool                             m_MapKicked;                    // if we're kicking this player because he has no map and we won't send it to him
  bool                             m_PingKicked;                   // if we're kicking this player because his ping is excessively high
  bool                             m_SpoofKicked;
  std::optional<bool>              m_UserReady;
  bool                             m_Ready;
  int64_t                          m_ReadyReminderLastTime;
  bool                             m_HasHighPing;                  // if last time we checked, the player had high ping
  bool                             m_DownloadAllowed;              // if we're allowed to download the map or not (used with permission based map downloads)
  bool                             m_DownloadStarted;              // if we've started downloading the map or not
  bool                             m_DownloadFinished;             // if we've finished downloading the map or not
  bool                             m_FinishedLoading;              // if the player has finished loading or not
  bool                             m_Lagging;                      // if the player is lagging or not (on the lag screen)
  std::optional<bool>              m_DropVote;                     // if the player voted to drop the laggers or not (on the lag screen)
  std::optional<bool>              m_KickVote;                     // if the player voted to kick a player or not
  bool                             m_Muted;                        // if the player is muted or not
  bool                             m_LeftMessageSent;              // if the playerleave message has been sent or not
  bool                             m_StatusMessageSent;            // if the message regarding player connection mode has been sent or not
  bool                             m_UsedAnyCommands;              // if the playerleave message has been sent or not
  bool                             m_SentAutoCommandsHelp;         // if the playerleave message has been sent or not
  uint8_t                          m_SmartCommand;
  int64_t                          m_CheckStatusByTime;

  bool                             m_GProxy;                       // if the player is using GProxy++
  uint16_t                         m_GProxyPort;                   // port where GProxy will try to reconnect
  bool                             m_GProxyDisconnectNoticeSent;   // if a disconnection notice has been sent or not when using GProxy++

  bool                             m_GProxyExtended;               // if the player is using GProxyDLL
  uint32_t                         m_GProxyVersion;
  bool                             m_Disconnected;
  int64_t                          m_TotalDisconnectTime;
  int64_t                          m_LastDisconnectTime;

  std::string                      m_LastCommand;
  uint8_t                          m_TeamCaptain;

  std::string                      m_PinnedMessage;

  // Actions
  bool                             m_Saved;
  uint8_t                          m_RemainingPauses;

protected:
  bool m_DeleteMe;

public:
  CGameUser(CGame* game, CGameConnection* connection, uint8_t nUID, uint32_t nJoinedRealmInternalId, std::string nJoinedRealm, std::string nName, std::array<uint8_t, 4> nInternalIP, bool nReserved);
  ~CGameUser();

  uint32_t GetOperationalRTT() const;
  uint32_t GetDisplayRTT() const;
  uint32_t GetRTT() const;
  inline CStreamIOSocket*         GetSocket() const { return m_Socket; }
  inline bool                     GetUsingIPv6() const { return m_Socket->GetIsInnerIPv6(); }
  inline std::array<uint8_t, 4>   GetIPv4() const { return m_Socket->GetIPv4(); }
  inline std::string              GetIPString() const { return m_Socket->GetIPString(); }
  inline std::string              GetIPStringStrict() const { return m_Socket->GetIPStringStrict(); }
  inline bool                     GetIsReady() const { return m_Ready; }
  inline bool                     GetDeleteMe() const { return m_DeleteMe; }
  inline uint8_t                  GetUID() const { return m_UID; }
  inline std::string              GetName() const { return m_Name; }
  std::string                     GetLowerName() const;
  std::string                     GetDisplayName() const;
  inline std::array<uint8_t, 4>   GetIPv4Internal() const { return m_IPv4Internal; }
  inline size_t                   GetStoredRTTCount() const { return m_RTTValues.size(); }
  inline uint32_t                 GetPongCounter() const { return m_PongCounter; }
  inline size_t                   GetNumCheckSums() const { return m_CheckSums.size(); }
  inline std::queue<uint32_t>*    GetCheckSums() { return &m_CheckSums; }
  inline bool                     HasCheckSums() const { return !m_CheckSums.empty(); }
  inline std::string              GetLeftReason() const { return m_LeftReason; }
  inline uint32_t                 GetLeftCode() const { return m_LeftCode; }
  inline bool                     GetQuitGame() const { return m_QuitGame; }
  CRealm*                         GetRealm(bool mustVerify) const;
  std::string                     GetRealmDataBaseID(bool mustVerify) const;
  inline uint32_t                 GetRealmInternalID() const { return m_RealmInternalId; }
  inline std::string              GetRealmHostName() const { return m_RealmHostName; }
  inline std::string              GetExtendedName() const {
    if (m_RealmHostName.empty()) {
      return m_Name + "@@@LAN/VPN";
    } else {
      return m_Name + "@" + m_RealmHostName;
    }
  }
  inline bool                  IsRealmVerified() const { return m_Verified; }
  inline uint32_t              GetSyncCounter() const { return m_SyncCounter; }
  inline uint32_t              GetNormalSyncCounter() const { return m_SyncCounter + m_SyncCounterOffset; }
  inline int64_t               GetJoinTime() const { return m_JoinTime; }
  inline uint32_t              GetLastMapPartSent() const { return m_LastMapPartSent; }
  inline uint32_t              GetLastMapPartAcked() const { return m_LastMapPartAcked; }
  inline int64_t               GetStartedDownloadingTicks() const { return m_StartedDownloadingTicks; }
  inline int64_t               GetFinishedDownloadingTime() const { return m_FinishedDownloadingTime; }
  inline int64_t               GetFinishedLoadingTicks() const { return m_FinishedLoadingTicks; }

  inline int64_t               GetStartedLaggingTicks() const { return m_StartedLaggingTicks; }
  inline int64_t               GetLastGProxyWaitNoticeSentTime() const { return m_LastGProxyWaitNoticeSentTime; }
  inline uint32_t              GetGProxyReconnectKey() const { return m_GProxyReconnectKey; }
  inline bool                  GetGProxyAny() const { return m_GProxy; }
  inline bool                  GetGProxyLegacy() const { return m_GProxy && !m_GProxyExtended; }
  inline bool                  GetGProxyExtended() const { return m_GProxyExtended; }
  inline bool                  GetGProxyDisconnectNoticeSent() const { return m_GProxyDisconnectNoticeSent; }
  
	inline bool                  GetDisconnected() const { return m_Disconnected; }
	inline int64_t               GetLastDisconnectTime() const { return m_LastDisconnectTime; }
	int64_t                      GetTotalDisconnectTime() const;
  std::string                  GetDelayText(bool displaySync) const;
  std::string                  GetSyncText() const;
  
  inline bool                  GetIsReserved() const { return m_Reserved; }
  bool                         GetIsSudoMode() const;
  bool                         CheckSudoMode();
  void                         SudoModeStart();
  void                         SudoModeEnd();
  inline bool                  GetIsObserver() const { return m_Observer; }
  inline bool                  GetIsPowerObserver() const { return m_PowerObserver; }
  bool                         GetIsNativeReferee() const;
  bool                         GetCanUsePublicChat() const;
  inline bool                  GetWhoisShouldBeSent() const { return m_WhoisShouldBeSent; }
  inline bool                  GetWhoisSent() const { return m_WhoisSent; }
  inline bool                  GetDownloadAllowed() const { return m_DownloadAllowed; }
  inline bool                  GetDownloadStarted() const { return m_DownloadStarted; }
  inline bool                  GetDownloadFinished() const { return m_DownloadFinished; }
  inline bool                  GetFinishedLoading() const { return m_FinishedLoading; }
  inline bool                  GetMapReady() const { return m_MapReady; }
  inline bool                  GetMapKicked() const { return m_MapKicked; }
  inline bool                  GetPingKicked() const { return m_PingKicked; }
  inline bool                  GetSpoofKicked() const { return m_SpoofKicked; }
  inline bool                  GetHasHighPing() const { return m_HasHighPing; }
  inline int64_t               GetKickByTime() const { return m_KickByTime; }
  inline bool                  GetKickQueued() const { return m_KickByTime != 0; }
  inline bool                  GetLagging() const { return m_Lagging; }
  inline std::optional<bool>   GetDropVote() const { return m_DropVote; }
  inline std::optional<bool>   GetKickVote() const { return m_KickVote; }
  inline bool                  GetMuted() const { return m_Muted; }
  inline bool                  GetStatusMessageSent() const { return m_StatusMessageSent; }
  inline bool                  GetLeftMessageSent() const { return m_LeftMessageSent; }
  inline bool                  GetUsedAnyCommands() const { return m_UsedAnyCommands; }
  inline bool                  GetSentAutoCommandsHelp() const { return m_SentAutoCommandsHelp; }
  inline uint8_t               GetSmartCommand() const { return m_SmartCommand; }
  bool                         UpdateReady();
  bool                         GetIsOwner(std::optional<bool> nAssumeVerified) const;
  inline bool                  GetIsDraftCaptain() { return m_TeamCaptain != 0; }
  inline bool                  GetIsDraftCaptainOf(const uint8_t nTeam) { return m_TeamCaptain == nTeam + 1; }
  inline bool                  GetCanPause() { return m_RemainingPauses > 0; }
  inline bool                  GetSaved() { return m_Saved; }
  inline void SetSocket(CStreamIOSocket* nSocket) { m_Socket = nSocket; }
  inline void SetDeleteMe(bool nDeleteMe) { m_DeleteMe = nDeleteMe; }
  inline void SetLeftReason(const std::string& nLeftReason) { m_LeftReason = nLeftReason; }
  inline void SetLeftCode(uint32_t nLeftCode) { m_LeftCode = nLeftCode; }
  inline void SetQuitGame(bool nQuitGame) { m_QuitGame = nQuitGame; }
  inline void SetSyncCounter(uint32_t nSyncCounter) { m_SyncCounter = nSyncCounter; }
  inline void AddSyncCounterOffset(const uint32_t nOffset) { m_SyncCounterOffset += nOffset; }
  inline void ResetSyncCounterOffset() { m_SyncCounterOffset = 0; }
  inline void SetLastMapPartSent(uint32_t nLastMapPartSent) { m_LastMapPartSent = nLastMapPartSent; }
  inline void SetLastMapPartAcked(uint32_t nLastMapPartAcked) { m_LastMapPartAcked = nLastMapPartAcked; }
  inline void SetStartedDownloadingTicks(uint64_t nStartedDownloadingTicks) { m_StartedDownloadingTicks = nStartedDownloadingTicks; }
  inline void SetFinishedDownloadingTime(uint64_t nFinishedDownloadingTime) { m_FinishedDownloadingTime = nFinishedDownloadingTime; }
  inline void SetStartedLaggingTicks(uint64_t nStartedLaggingTicks) { m_StartedLaggingTicks = nStartedLaggingTicks; }
  inline void SetRealmVerified(bool nVerified) { m_Verified = nVerified; }
  inline void SetOwner(bool nOwner) { m_Owner = nOwner; }
  inline void SetReserved(bool nReserved) { m_Reserved = nReserved; }
  inline void SetObserver(bool nObserver) { m_Observer = nObserver; }
  inline void SetPowerObserver(bool nPowerObserver) { m_PowerObserver = nPowerObserver; }
  inline void SetWhoisShouldBeSent(bool nWhoisShouldBeSent) { m_WhoisShouldBeSent = nWhoisShouldBeSent; }
  inline void SetDownloadAllowed(bool nDownloadAllowed) { m_DownloadAllowed = nDownloadAllowed; }
  inline void SetDownloadStarted(bool nDownloadStarted) { m_DownloadStarted = nDownloadStarted; }
  inline void SetDownloadFinished(bool nDownloadFinished) { m_DownloadFinished = nDownloadFinished; }
  inline void SetMapReady(bool nHasMap) { m_MapReady = nHasMap; }
  inline void SetMapKicked(bool nMapKicked) { m_MapKicked = nMapKicked; }
  inline void SetPingKicked(bool nPingKicked) { m_PingKicked = nPingKicked; }
  inline void SetSpoofKicked(bool nSpoofKicked) { m_SpoofKicked = nSpoofKicked; }
  inline void SetHasHighPing(bool nHasHighPing) { m_HasHighPing = nHasHighPing; }
  inline void SetLagging(bool nLagging) { m_Lagging = nLagging; }
  inline void SetDropVote(bool nDropVote) { m_DropVote = nDropVote; }
  inline void SetKickVote(bool nKickVote) { m_KickVote = nKickVote; }
  inline void SetMuted(bool nMuted) { m_Muted = nMuted; }
  inline void SetStatusMessageSent(bool nStatusMessageSent) { m_StatusMessageSent = nStatusMessageSent; }
  inline void SetLeftMessageSent(bool nLeftMessageSent) { m_LeftMessageSent = nLeftMessageSent; }
  inline void SetGProxyDisconnectNoticeSent(bool nGProxyDisconnectNoticeSent) { m_GProxyDisconnectNoticeSent = nGProxyDisconnectNoticeSent; }
  inline void SetLastGProxyWaitNoticeSentTime(uint64_t nLastGProxyWaitNoticeSentTime) { m_LastGProxyWaitNoticeSentTime = nLastGProxyWaitNoticeSentTime; }
  inline void SetKickByTime(int64_t nKickByTime) { m_KickByTime = nKickByTime; }
  inline void ClearKickByTime() { m_KickByTime = 0; }
  inline void KickAtLatest(int64_t nKickByTime) {
    if (m_KickByTime == 0 || nKickByTime < m_KickByTime) {
      m_KickByTime = nKickByTime;
    }
  }
  inline void SetUserReady(bool nReady) { m_UserReady = nReady; }
  inline void ClearUserReady() { m_UserReady = std::nullopt; }

  bool GetReadyReminderIsDue() const;
  void SetReadyReminded();

  inline std::string GetLastCommand() const { return m_LastCommand; }
  inline void ClearLastCommand() { m_LastCommand.clear(); }
  inline void SetLastCommand(const std::string nLastCommand) { m_LastCommand = nLastCommand; }
  inline void SetDraftCaptain(const uint8_t nTeamNumber) { m_TeamCaptain = nTeamNumber; }
  inline void SetSaved(const bool nValue) { m_Saved = nValue; }
  inline void SetUsedAnyCommands(const bool nValue) { m_UsedAnyCommands = nValue; }
  inline void SetSentAutoCommandsHelp(const bool nValue) { m_SentAutoCommandsHelp = nValue; }
  inline void SetSmartCommand(const uint8_t nValue) { m_SmartCommand = nValue; }
  inline void ClearSmartCommand() { m_SmartCommand = SMART_COMMAND_NONE; }
  inline void DropRemainingPauses() { --m_RemainingPauses; }
  inline void SetCannotPause() { m_RemainingPauses = 0; }
  inline void ClearStalePings() {
    if (m_RTTValues.empty()) return;
    uint32_t lastPing = m_RTTValues[m_RTTValues.size() - 1];
    m_RTTValues.clear();
    m_RTTValues.push_back(lastPing);
  }
  inline void ClearPings() { m_RTTValues.clear(); }

  inline std::string GetPinnedMessage() { return  m_PinnedMessage; }
  inline bool GetHasPinnedMessage() { return !m_PinnedMessage.empty(); }
  inline void SetPinnedMessage(const std::string nPinnedMessage) { m_PinnedMessage = nPinnedMessage; }
  inline void ClearPinnedMessage() { m_PinnedMessage.clear(); }

  // processing functions

  bool Update(void* fd);

  // other functions

  void Send(const std::vector<uint8_t>& data);
  void EventGProxyReconnect(CStreamIOSocket* NewSocket, const uint32_t LastPacket);
  void ResetConnection();
};

inline std::string PlayersToNameListString(std::vector<const CGameUser*> playerList, bool useRealNames = false) {
  if (playerList.empty()) return std::string();
  std::vector<std::string> playerNames;
  for (const auto& player : playerList) {
    if (useRealNames) {
      playerNames.push_back("[" + player->GetName() + "]");
    } else {
      playerNames.push_back("[" + player->GetDisplayName() + "]");
    }
  }
  return JoinVector(playerNames, ", ", false);
}
inline std::string PlayersToNameListString(std::vector<CGameUser*> playerList, bool useRealNames = false) {
  if (playerList.empty()) return std::string();
  std::vector<std::string> playerNames;
  for (const auto& player : playerList) {
    if (useRealNames) {
      playerNames.push_back("[" + player->GetName() + "]");
    } else {
      playerNames.push_back("[" + player->GetDisplayName() + "]");
    }
  }
  return JoinVector(playerNames, ", ", false);
}

#endif // AURA_GAMEPLAYER_H_
