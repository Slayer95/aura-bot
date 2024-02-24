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

//
// CPotentialPlayer
//

class CPotentialPlayer
{
public:
  CGameProtocol* m_Protocol;
  CAura*         m_Aura;
  uint16_t       m_Port;

protected:
  // note: we permit m_Socket to be NULL in this class to allow for the virtual host player which doesn't really exist
  // it also allows us to convert CPotentialPlayers to CGamePlayers without the CPotentialPlayer's destructor closing the socket

  CStreamIOSocket*          m_Socket;
  CIncomingJoinRequest* m_IncomingJoinPlayer;
  bool                 m_DeleteMe;

public:
  CPotentialPlayer(CGameProtocol* nProtocol, CAura* nAura, uint16_t nPort, CStreamIOSocket* nSocket);
  ~CPotentialPlayer();

  inline CStreamIOSocket*          GetSocket() const { return m_Socket; }
  inline std::vector<uint8_t> GetIPv4() const { return m_Socket->GetIPv4(); }
  inline std::string          GetIPString() const { return m_Socket->GetIPString(); }
  inline bool                 GetDeleteMe() const { return m_DeleteMe; }
  inline CIncomingJoinRequest* GetJoinPlayer() const { return m_IncomingJoinPlayer; }

  inline void SetSocket(CStreamIOSocket* nSocket) { m_Socket = nSocket; }
  inline void SetDeleteMe(bool nDeleteMe) { m_DeleteMe = nDeleteMe; }

  // processing functions

  uint8_t Update(void* fd, void* send_fd);

  // other functions

  void Send(const std::vector<uint8_t>& data) const;
};

//
// CGamePlayer
//

class CGamePlayer
{
public:
  CGameProtocol* m_Protocol;
  CGame*         m_Game;

protected:
  CStreamIOSocket* m_Socket; // note: we permit m_Socket to be NULL in this class to allow for the virtual host player which doesn't really exist

private:
  std::vector<uint8_t>             m_IPv4Internal;                 // the player's internal IP address as reported by the player when connecting
  std::vector<uint32_t>            m_Pings;                        // store the last few (10) pings received so we can take an average
  std::queue<uint32_t>             m_CheckSums;                    // the last few checksums the player has sent (for detecting desyncs)
  std::queue<std::vector<uint8_t>> m_GProxyBuffer;                 // buffer with data used with GProxy++
  std::string                      m_LeftReason;                   // the reason the player left the game
  uint8_t                          m_JoinedRealmID;
  std::string                      m_JoinedRealm;                  // the realm the player joined on (probable, can be spoofed)
  std::string                      m_Name;                         // the player's name
  uint32_t                         m_TotalPacketsSent;             // the total number of packets sent to the player
  uint32_t                         m_TotalPacketsReceived;         // the total number of packets received from the player
  uint32_t                         m_LeftCode;                     // the code to be sent in W3GS_PLAYERLEAVE_OTHERS for why this player left the game
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
  uint8_t                          m_PID;                          // the player's PID
  bool                             m_Verified;                      // if the player has spoof checked or not
  bool                             m_Reserved;                     // if the player is reserved (VIP) or not
  bool                             m_Observer;                     // if the player is an observer
  bool                             m_PowerObserver;                // if the player is a referee - referees can be demoted to full observers
  bool                             m_WhoisShouldBeSent;            // if a battle.net /whois should be sent for this player or not
  bool                             m_WhoisSent;                    // if we've sent a battle.net /whois for this player yet (for spoof checking)
  bool                             m_HasMap;                       // if we're allowed to download the map or not (used with permission based map downloads)
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
  bool                             m_GProxy;                       // if the player is using GProxy++
  uint16_t                         m_GProxyPort;                   // port where GProxy will try to reconnect
  bool                             m_GProxyDisconnectNoticeSent;   // if a disconnection notice has been sent or not when using GProxy++

protected:
  bool m_DeleteMe;

public:
  CGamePlayer(CGame* game, CPotentialPlayer* potential, uint8_t nPID, uint8_t nJoinedRealmID, std::string nJoinedRealm, std::string nName, std::vector<uint8_t> nInternalIP, bool nReserved);
  ~CGamePlayer();

  uint32_t GetPing() const;
  inline CStreamIOSocket*           GetSocket() const { return m_Socket; }
  inline std::vector<uint8_t>  GetIPv4() const { return m_Socket->GetIPv4(); }
  inline std::string           GetIPString() const { return m_Socket->GetIPString(); }
  inline bool                  GetDeleteMe() const { return m_DeleteMe; }
  inline uint8_t               GetPID() const { return m_PID; }
  inline std::string           GetName() const { return m_Name; }
  inline std::vector<uint8_t>  GetIPv4Internal() const { return m_IPv4Internal; }
  inline uint32_t              GetNumPings() const { return m_Pings.size(); }
  inline uint32_t              GetNumCheckSums() const { return m_CheckSums.size(); }
  inline std::queue<uint32_t>* GetCheckSums() { return &m_CheckSums; }
  inline std::string           GetLeftReason() const { return m_LeftReason; }
  inline uint32_t              GetLeftCode() const { return m_LeftCode; }
  CRealm*                       GetRealm(bool mustVerify);
  std::string                  GetRealmDataBaseID(bool mustVerify);
  inline uint8_t               GetRealmID() const { return m_JoinedRealmID; }
  inline std::string           GetRealmHostName() const { return m_JoinedRealm; }
  inline bool                  IsRealmVerified() const { return m_Verified; }
  inline uint32_t              GetSyncCounter() const { return m_SyncCounter; }
  inline int64_t               GetJoinTime() const { return m_JoinTime; }
  inline uint32_t              GetLastMapPartSent() const { return m_LastMapPartSent; }
  inline uint32_t              GetLastMapPartAcked() const { return m_LastMapPartAcked; }
  inline int64_t               GetStartedDownloadingTicks() const { return m_StartedDownloadingTicks; }
  inline int64_t               GetFinishedDownloadingTime() const { return m_FinishedDownloadingTime; }
  inline int64_t               GetFinishedLoadingTicks() const { return m_FinishedLoadingTicks; }
  inline int64_t               GetStartedLaggingTicks() const { return m_StartedLaggingTicks; }
  inline int64_t               GetLastGProxyWaitNoticeSentTime() const { return m_LastGProxyWaitNoticeSentTime; }
  inline uint32_t              GetGProxyReconnectKey() const { return m_GProxyReconnectKey; }
  inline bool                  GetGProxy() const { return m_GProxy; }
  inline bool                  GetGProxyDisconnectNoticeSent() const { return m_GProxyDisconnectNoticeSent; }
  inline bool                  GetReserved() const { return m_Reserved; }
  inline bool                  GetObserver() const { return m_Observer; }
  inline bool                  GetPowerObserver() const { return m_PowerObserver; }
  inline bool                  GetWhoisShouldBeSent() const { return m_WhoisShouldBeSent; }
  inline bool                  GetWhoisSent() const { return m_WhoisSent; }
  inline bool                  GetDownloadAllowed() const { return m_DownloadAllowed; }
  inline bool                  GetDownloadStarted() const { return m_DownloadStarted; }
  inline bool                  GetDownloadFinished() const { return m_DownloadFinished; }
  inline bool                  GetFinishedLoading() const { return m_FinishedLoading; }
  inline bool                  GetHasMap() const { return m_HasMap; }
  inline bool                  GetHasHighPing() const { return m_HasHighPing; }
  inline int64_t               GetKickByTime() const { return m_KickByTime; }
  inline bool                  GetKickQueued() const { return m_KickByTime != 0; }
  inline bool                  GetLagging() const { return m_Lagging; }
  inline std::optional<bool>   GetDropVote() const { return m_DropVote; }
  inline std::optional<bool>   GetKickVote() const { return m_KickVote; }
  inline bool                  GetMuted() const { return m_Muted; }
  inline bool                  GetLeftMessageSent() const { return m_LeftMessageSent; }
  inline void SetSocket(CStreamIOSocket* nSocket) { m_Socket = nSocket; }
  inline void SetDeleteMe(bool nDeleteMe) { m_DeleteMe = nDeleteMe; }
  inline void SetLeftReason(const std::string& nLeftReason) { m_LeftReason = nLeftReason; }
  inline void SetLeftCode(uint32_t nLeftCode) { m_LeftCode = nLeftCode; }
  inline void SetSyncCounter(uint32_t nSyncCounter) { m_SyncCounter = nSyncCounter; }
  inline void SetLastMapPartSent(uint32_t nLastMapPartSent) { m_LastMapPartSent = nLastMapPartSent; }
  inline void SetLastMapPartAcked(uint32_t nLastMapPartAcked) { m_LastMapPartAcked = nLastMapPartAcked; }
  inline void SetStartedDownloadingTicks(uint64_t nStartedDownloadingTicks) { m_StartedDownloadingTicks = nStartedDownloadingTicks; }
  inline void SetFinishedDownloadingTime(uint64_t nFinishedDownloadingTime) { m_FinishedDownloadingTime = nFinishedDownloadingTime; }
  inline void SetStartedLaggingTicks(uint64_t nStartedLaggingTicks) { m_StartedLaggingTicks = nStartedLaggingTicks; }
  inline void SetRealmVerified(bool nVerified) { m_Verified = nVerified; }
  inline void SetReserved(bool nReserved) { m_Reserved = nReserved; }
  inline void SetObserver(bool nObserver) { m_Observer = nObserver; }
  inline void SetPowerObserver(bool nPowerObserver) { m_PowerObserver = nPowerObserver; }
  inline void SetWhoisShouldBeSent(bool nWhoisShouldBeSent) { m_WhoisShouldBeSent = nWhoisShouldBeSent; }
  inline void SetDownloadAllowed(bool nDownloadAllowed) { m_DownloadAllowed = nDownloadAllowed; }
  inline void SetDownloadStarted(bool nDownloadStarted) { m_DownloadStarted = nDownloadStarted; }
  inline void SetDownloadFinished(bool nDownloadFinished) { m_DownloadFinished = nDownloadFinished; }
  inline void SetHasMap(bool nHasMap) { m_HasMap = nHasMap; }
  inline void SetHasHighPing(bool nHasHighPing) { m_HasHighPing = nHasHighPing; }
  inline void SetLagging(bool nLagging) { m_Lagging = nLagging; }
  inline void SetDropVote(bool nDropVote) { m_DropVote = nDropVote; }
  inline void SetKickVote(bool nKickVote) { m_KickVote = nKickVote; }
  inline void SetMuted(bool nMuted) { m_Muted = nMuted; }
  inline void SetLeftMessageSent(bool nLeftMessageSent) { m_LeftMessageSent = nLeftMessageSent; }
  inline void SetGProxyDisconnectNoticeSent(bool nGProxyDisconnectNoticeSent) { m_GProxyDisconnectNoticeSent = nGProxyDisconnectNoticeSent; }
  inline void SetLastGProxyWaitNoticeSentTime(uint64_t nLastGProxyWaitNoticeSentTime) { m_LastGProxyWaitNoticeSentTime = nLastGProxyWaitNoticeSentTime; }
  inline void SetKickByTime(int64_t nKickByTime) { m_KickByTime = nKickByTime; }

  // processing functions

  bool Update(void* fd);

  // other functions

  void Send(const std::vector<uint8_t>& data);
  void EventGProxyReconnect(CStreamIOSocket* NewSocket, uint32_t LastPacket);
};

#endif // AURA_GAMEPLAYER_H_
