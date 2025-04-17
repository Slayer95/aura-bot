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
#include "game_structs.h"
#include "map.h"
#include "realm.h"
#include "protocol/game_protocol.h"

//
// CAsyncObserver
//

class CAsyncObserver final : public CConnection
{
public:
  std::weak_ptr<CGame>                                          m_Game;
  MapTransfer                                                   m_MapTransfer;
  std::weak_ptr<CRealm>                                         m_FromRealm;
  std::shared_ptr<GameHistory>                                  m_GameHistory;
  bool                                                          m_MapChecked;                   // if we received any W3GS_MAPSIZE packet from the client
  bool                                                          m_MapReady;                     // if we received a valid W3GS_MAPSIZE packet from the client matching the map size
  bool                                                          m_StateSynchronized;
  bool                                                          m_TimeSynchronized;
  size_t                                                        m_Offset;
  uint8_t                                                       m_Goal;
  uint8_t                                                       m_UID;
  uint8_t                                                       m_SID;
  uint8_t                                                       m_Color;
  bool                                                          m_GameVersionIsExact;
  Version                                                       m_GameVersion;
  uint8_t                                                       m_MissingLog;
  int64_t                                                       m_FrameRate;
  int64_t                                                       m_Latency;
  size_t                                                        m_SyncCounter;                  // the number of keepalive packets received from this player
  size_t                                                        m_ActionFrameCounter;
  std::queue<uint32_t>                                          m_CheckSums;                    // the last few checksums the player has sent (for detecting desyncs)

  /*
  std::vector<uint32_t>                                         m_RTTValues;                    // store the last few (10) pings received so we can take an average
  OptionalTimedUint32                                           m_MeasuredRTT;
  */

  bool                                                          m_StartedLoading;
  int64_t                                                       m_StartedLoadingTicks;
  bool                                                          m_FinishedLoading;
  int64_t                                                       m_FinishedLoadingTicks;
  int64_t                                                       m_GameTicks;

  bool                                                          m_SentGameLoadedReport;
  bool                                                          m_PlaybackEnded;
  int64_t                                                       m_LastFrameTicks;
  int64_t                                                       m_LastPingTicks;

  int64_t                                                       m_LastProgressReportTime;
  uint8_t                                                       m_LastProgressReportLog;


  bool                                                          m_UsedAnyCommands;              // if the playerleave message has been sent or not
  bool                                                          m_SentAutoCommandsHelp;         // if the playerleave message has been sent or not
  uint8_t                                                       m_SmartCommand;
  std::string                                                   m_LastCommand;

  std::string                                                   m_Name;
  std::string                                                   m_LeftReason;

  CAsyncObserver(std::shared_ptr<CGame> nGame, CConnection* nConnection, uint8_t nUID, const bool gameVersionIsExact, const Version& gameVersion, std::shared_ptr<CRealm> nFromRealm, const std::string& nName);
  ~CAsyncObserver();

  // processing functions

  void SetTimeout(const int64_t nTicksDelta);
  void SetTimeoutAtLatest(const int64_t nTicks);

  bool CloseConnection(bool recoverable = false);
  void Init();
  [[nodiscard]] uint8_t Update(fd_set* fd, fd_set* send_fd, int64_t timeout);

  [[nodiscard]] inline MapTransfer&             GetMapTransfer() { return m_MapTransfer; }
  [[nodiscard]] inline const MapTransfer&       InspectMapTransfer() const { return m_MapTransfer; }
  [[nodiscard]] inline std::shared_ptr<CRealm>  GetRealm() { return m_FromRealm.lock(); }

  [[nodiscard]] inline bool                     GetGameVersionIsExact() const { return m_GameVersionIsExact; }
  [[nodiscard]] inline Version                  GetGameVersion() const { return m_GameVersion; }
  [[nodiscard]] inline const std::string&       GetName() const { return m_Name; }
  [[nodiscard]] inline std::shared_ptr<CGame>   GetGame() const { return m_Game.lock(); }

  [[nodiscard]] inline int64_t                  GetFrameRate() const { return m_FrameRate; }
  inline void                                   SetFrameRate(int64_t nFrameRate) { m_FrameRate = nFrameRate; }

  [[nodiscard]] inline bool                     HasLeftReason() { return !m_LeftReason.empty(); }
  [[nodiscard]] inline std::string              GetLeftReason() { return m_LeftReason; }
  inline void                                   SetLeftReason(const std::string& reason) { m_LeftReason = reason; }
  inline void                                   SetLeftReasonGeneric(const std::string& reason) { if (m_LeftReason.empty()) m_LeftReason = reason; }

  [[nodiscard]] inline bool                     GetMapChecked() const { return m_MapChecked; }
  inline void                                   SetMapChecked(bool nChecked) { m_MapChecked = nChecked; }
  [[nodiscard]] inline bool                     GetMapReady() const { return m_MapReady; }
  inline void                                   SetMapReady(bool nHasMap) { m_MapReady = nHasMap; }

  [[nodiscard]] inline uint8_t                  GetSID() const { return m_SID; }
  [[nodiscard]] inline uint8_t                  GetUID() const { return m_UID; }
  
  [[nodiscard]] inline int64_t                  GetGameTicks() const { return m_GameTicks; }
  int64_t                                       GetNextTimedActionByTicks() const;

  [[nodiscard]] inline bool                     GetIsRealmVerified() const { return false; }

  bool PushGameFrames(bool isFlush = false);
  inline void FlushGameFrames() { PushGameFrames(true); }
  void CheckGameOver();
  void EventGameReset(std::shared_ptr<const CGame> nGame);
  void EventRealmDeleted(std::shared_ptr<const CRealm> nRealm);
  void UpdateClientGameState(const uint32_t checkSum);
  void CheckClientGameState();
  void UpdateDownloadProgression(const uint8_t downloadProgression);
  [[nodiscard]] uint8_t NextSendMap();
  void EventDesync();
  void EventMapReady();
  void StartLoading();
  void EventGameLoaded();
  void EventChatOrPlayerSettings(const CIncomingChatMessage* chatPlayer);
  void EventChat(const CIncomingChatMessage* chatPlayer);
  void EventLeft(const uint32_t clientReason);
  void EventProtocolError();

  // other functions

  void Send(const std::vector<uint8_t>& data) final;
  void SendOtherPlayersInfo();
  void SendChat(const std::string& message);
  void SendGameLoadedReport();
  uint8_t GetMissingLog() const;
  void SendProgressReport();
  std::string GetLogPrefix() const;

  [[nodiscard]] inline bool static SortObserversByDownloadProgressAscending(const CAsyncObserver* a, const CAsyncObserver* b) {
    return a->InspectMapTransfer().GetLastSentOffsetEnd() < b->InspectMapTransfer().GetLastSentOffsetEnd();
  }  

  [[nodiscard]] inline std::string GetLastCommand() const { return m_LastCommand; }
  inline void ClearLastCommand() { m_LastCommand.clear(); }
  inline void SetLastCommand(const std::string nLastCommand) { m_LastCommand = nLastCommand; }
  [[nodiscard]] inline bool                  GetUsedAnyCommands() const { return m_UsedAnyCommands; }
  [[nodiscard]] inline bool                  GetSentAutoCommandsHelp() const { return m_SentAutoCommandsHelp; }
  [[nodiscard]] inline uint8_t               GetSmartCommand() const { return m_SmartCommand; }
  inline void SetUsedAnyCommands(const bool nValue) { m_UsedAnyCommands = nValue; }
  inline void SetSentAutoCommandsHelp(const bool nValue) { m_SentAutoCommandsHelp = nValue; }
  inline void SetSmartCommand(const uint8_t nValue) { m_SmartCommand = nValue; }
  inline void ClearSmartCommand() { m_SmartCommand = SMART_COMMAND_NONE; }
};

#endif // AURA_ASYNC_OBSERVER_H_
