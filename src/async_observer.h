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

//
// CAsyncObserver
//

class CAsyncObserver final : public CConnection
{
public:
  CGame*                                                        m_Game;
  std::shared_ptr<GameHistory>                                  m_GameHistory;
  bool                                                          m_MapReady;
  bool                                                          m_Desynchronized;
  uint32_t                                                      m_Offset;
  uint8_t                                                       m_Goal;
  uint8_t                                                       m_UID;
  uint8_t                                                       m_SID;
  uint8_t                                                       m_FrameRate;
  size_t                                                        m_SyncCounter;                  // the number of keepalive packets received from this player
  std::queue<uint32_t>                                          m_CheckSums;                    // the last few checksums the player has sent (for detecting desyncs)

  /*
  std::vector<uint32_t>                                         m_RTTValues;                    // store the last few (10) pings received so we can take an average
  OptionalTimedUint32                                           m_MeasuredRTT;
  */

  bool                                                          m_DownloadStarted;
  bool                                                          m_DownloadFinished;
  int64_t                                                       m_FinishedDownloadingTime;

  bool                                                          m_FinishedLoading;
  int64_t                                                       m_FinishedLoadingTicks;

  std::optional<int64_t>                                        m_LastFrameTicks;

  std::string                                                   m_Name;

  CAsyncObserver(CConnection* nConnection, CGame* nGame, uint8_t nUID, const std::string& nName);
  ~CAsyncObserver();

  // processing functions

  void SetTimeout(const int64_t nTicks);
  bool CloseConnection();
  void Init();
  [[nodiscard]] uint8_t Update(fd_set* fd, fd_set* send_fd, int64_t timeout);

  [[nodiscard]] inline const std::string&       GetName() { return m_Name; }
  [[nodiscard]] inline uint8_t                  GetSID() const { return m_SID; }
  [[nodiscard]] inline uint8_t                  GetUID() const { return m_UID; }

  void SendUpdates(fd_set* send_fd);
  void OnUnrefGame(CGame* nGame);
  void UpdateClientGameState(const uint32_t checkSum);
  void CheckClientGameState();
  void OnDesync();
  void EventGameLoaded();
  void EventLeft(const uint32_t clientReason);
  void EventProtocolError();

  // other functions

  void Send(const std::vector<uint8_t>& data) final;
  std::string GetLogPrefix() const;
};

#endif // AURA_ASYNC_OBSERVER_H_
