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

#ifndef AURA_ASYNC_OBSERVER_H_
#define AURA_ASYNC_OBSERVER_H_

#include "includes.h"
#include "connection.h"
#include "game.h"
#include "map.h"
#include "realm.h"
#include "protocol/game_protocol.h"

//
// CAsyncObserver
//

class CAsyncObserver final : public CConnection
{
public:
  CGame*                                                        m_Game;
  MapTransfer                                                   m_MapTransfer;
  const CRealm*                                                 m_FromRealm;
  std::shared_ptr<GameHistory>                                  m_GameHistory;
  bool                                                          m_MapReady;
  bool                                                          m_StateSynchronized;
  bool                                                          m_TimeSynchronized;
  size_t                                                        m_Offset;
  uint8_t                                                       m_Goal;
  uint8_t                                                       m_UID;
  uint8_t                                                       m_SID;
  uint8_t                                                       m_Color;
  int64_t                                                       m_FrameRate;
  int64_t                                                       m_Latency;
  size_t                                                        m_SyncCounter;                  // the number of keepalive packets received from this player
  size_t                                                        m_ActionFrameCounter;
  std::queue<uint32_t>                                          m_CheckSums;                    // the last few checksums the player has sent (for detecting desyncs)

  /*
  std::vector<uint32_t>                                         m_RTTValues;                    // store the last few (10) pings received so we can take an average
  OptionalTimedUint32                                           m_MeasuredRTT;
  */

  bool                                                          m_NotifiedCannotDownload;

  bool                                                          m_StartedLoading;
  int64_t                                                       m_StartedLoadingTicks;
  bool                                                          m_FinishedLoading;
  int64_t                                                       m_FinishedLoadingTicks;

  bool                                                          m_SentGameLoadedReport;
  bool                                                          m_PlaybackEnded;
  int64_t                                                       m_LastFrameTicks;
  int64_t                                                       m_LastPingTime;

  int64_t                                                       m_LastProgressReportTime;

  std::string                                                   m_Name;
  std::string                                                   m_LeftReason;

  CAsyncObserver(CConnection* nConnection, CGame* nGame, const CRealm* nFromRealm, uint8_t nUID, const std::string& nName);
  ~CAsyncObserver();

  // processing functions

  void SetTimeout(const int64_t nTicksDelta);
  void SetTimeoutAtLatest(const int64_t nTicks);

  bool CloseConnection(bool recoverable = false);
  void Init();
  [[nodiscard]] uint8_t Update(fd_set* fd, fd_set* send_fd, int64_t timeout);

  [[nodiscard]] inline MapTransfer&             GetMapTransfer() { return m_MapTransfer; }
  [[nodiscard]] inline const CRealm*            GetRealm() { return m_FromRealm; }

  [[nodiscard]] inline bool                     GetNotifiedCannotDownload() { return m_NotifiedCannotDownload; }
  inline void                                   SetNotifiedCannotDownload() { m_NotifiedCannotDownload = true; }

  [[nodiscard]] inline const std::string&       GetName() { return m_Name; }
  [[nodiscard]] inline CGame*                   GetGame() { return m_Game; }

  [[nodiscard]] inline bool                     HasLeftReason() { return !m_LeftReason.empty(); }
  [[nodiscard]] inline std::string              GetLeftReason() { return m_LeftReason; }
  inline void                                   SetLeftReason(const std::string& reason) { m_LeftReason = reason; }
  inline void                                   SetLeftReasonGeneric(const std::string& reason) { if (m_LeftReason.empty()) m_LeftReason = reason; }

  [[nodiscard]] inline uint8_t                  GetSID() const { return m_SID; }
  [[nodiscard]] inline uint8_t                  GetUID() const { return m_UID; }
  
  int64_t                                       GetNextTimedActionByTicks() const;

  bool PushGameFrames(bool isFlush = false);
  inline void FlushGameFrames() { PushGameFrames(true); }
  void CheckGameOver();
  void OnGameReset(const CGame* nGame);
  void UpdateClientGameState(const uint32_t checkSum);
  void CheckClientGameState();
  void UpdateDownloadProgression(const uint8_t downloadProgression);
  [[nodiscard]] uint8_t NextSendMap();
  void EventDesync();
  void EventMapReady();
  void StartLoading();
  void EventGameLoaded();
  void EventChatMessage(const CIncomingChatMessage* chatMessage);
  void EventLeft(const uint32_t clientReason);
  void EventProtocolError();

  // other functions

  void Send(const std::vector<uint8_t>& data) final;
  void SendOtherPlayersInfo();
  void SendChat(const std::string& message);
  void SendGameLoadedReport();
  void SendProgressReport();
  std::string GetLogPrefix() const;
};

#endif // AURA_ASYNC_OBSERVER_H_
