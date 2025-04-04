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

#ifndef AURA_GAME_STRUCTS_H_
#define AURA_GAME_STRUCTS_H_

#include "includes.h"
#include "list.h"
#include "protocol/game_protocol.h"

struct CGameLogRecord
{
  int64_t                        m_Ticks;
  std::string                    m_Text;

  inline int64_t                 GetTicks() const { return m_Ticks; }
  inline const std::string&      GetText() const { return m_Text; }
  std::string                    ToString() const;

  CGameLogRecord(int64_t gameTicks, std::string text);
  ~CGameLogRecord();
};

struct CQueuedActionsFrame
{
  // action to be performed after this frame is sent: ON_SEND_ACTIONS_PAUSE, ON_SEND_ACTIONS_RESUME
  uint8_t callback;

  // UID of last user that sent a pause action
  uint8_t pauseUID;

  // total size of the active ActionQueue  
  uint16_t bufferSize;

  // ActionQueue we append new incoming actions to
  ActionQueue* activeQueue;

  // queue of queues of size N
  // first (N-1) queues are sent with SEND_W3GS_INCOMING_ACTION2
  // last queue is sent with SEND_W3GS_INCOMING_ACTION, together with the expected delay til next action (latency)
  std::vector<ActionQueue> actions;

  // when a player leaves, the SEND_W3GS_PLAYERLEAVE_OTHERS is delayed until we are sure all their pending actions have been sent
  // so, if they leave during the game, we must append it to the last CQueuedActionsFrame
  // but if they leave while loading, we may append it to the first CQueuedActionsFrame
  UserList leavers;

  CQueuedActionsFrame();
  ~CQueuedActionsFrame();

  void AddAction(CIncomingAction&& action);
  std::vector<uint8_t> GetBytes(const uint16_t sendInterval) const;
  void MergeFrame(CQueuedActionsFrame& frame);
  bool GetHasActionsBy(const uint8_t fromUID) const;
  bool GetIsEmpty() const;
  size_t GetActionCount() const;
  void Reset();
};

struct GameFrame
{
  uint8_t                                                m_Type;
  std::vector<uint8_t>                                   m_Bytes;  

  GameFrame(const uint8_t nType)
   : m_Type(nType),
     m_Bytes(std::vector<uint8_t>())
  {};

  GameFrame(const uint8_t nType, std::vector<uint8_t>& nBytes)
   : m_Type(nType),
     m_Bytes(std::move(nBytes))
  {}

  ~GameFrame() = default;

  inline uint8_t                     GetType() const { return m_Type; }
  inline const std::vector<uint8_t>& GetBytes() const { return m_Bytes; }
  inline std::string GetTypeName() {
    switch (m_Type) {
      case GAME_FRAME_TYPE_ACTIONS: return "actions";
      case GAME_FRAME_TYPE_PAUSED: return "paused";
      case GAME_FRAME_TYPE_LEAVER: return "leaver";
      case GAME_FRAME_TYPE_CHAT: return "chat";
      case GAME_FRAME_TYPE_LATENCY: return "latency";
      case GAME_FRAME_TYPE_GPROXY: return "gproxy";
      default: return "unknown";
    }
  };
};

struct GameHistory
{
  bool                                                   m_Desynchronized;
  bool                                                   m_SoftDesynchronized;            // desynchronizes observers
  uint8_t                                                m_GProxyEmptyActions;
  uint16_t                                               m_DefaultLatency;
  std::optional<int64_t>                                 m_StartedTicks;
  size_t                                                 m_NumActionFrames;
  std::vector<uint32_t>                                  m_CheckSums;
  std::vector<uint8_t>                                   m_LobbyBuffer;
  std::vector<uint8_t>                                   m_PlayersBuffer;
  std::vector<uint8_t>                                   m_SlotsBuffer;
  std::vector<uint8_t>                                   m_LoadingRealBuffer;             // real W3GS_GAMELOADED messages for real players. In standard load, this buffer is filled in real-time. When load-in-game is enabled, this buffer is prefilled.
  std::vector<uint8_t>                                   m_LoadingVirtualBuffer;          // fake W3GS_GAMELOADED messages for fake players, but also for disconnected real players - for consistent game load, m_LoadingVirtualBuffer is sent after m_LoadingRealBuffer
  std::vector<GameFrame>                                 m_PlayingBuffer;

  GameHistory()
   : m_Desynchronized(false),
     m_SoftDesynchronized(false),
     m_GProxyEmptyActions(0),
     m_DefaultLatency(0),
     m_NumActionFrames(0)
  {}

  ~GameHistory() = default;

  void AddCheckSum(const uint32_t checkSum) { m_CheckSums.push_back(checkSum); }
  inline uint32_t GetCheckSum(const size_t index) { return m_CheckSums[index]; }
  inline size_t GetNumCheckSums() { return m_CheckSums.size(); }
  inline void SetDesynchronized(const bool nDesynchronized = true) { m_Desynchronized = nDesynchronized; }
  inline bool GetDesynchronized() { return m_Desynchronized; }
  inline void SetSoftDesynchronized(const bool nSoftDesynchronized = true) { m_SoftDesynchronized = nSoftDesynchronized; }
  inline bool GetSoftDesynchronized() { return m_SoftDesynchronized; }
  inline void SetDefaultLatency(const uint16_t nLatency) { m_DefaultLatency = nLatency; }
  inline uint16_t GetDefaultLatency() { return m_DefaultLatency; }
  inline void SetGProxyEmptyActions(const uint8_t nCount) { m_GProxyEmptyActions = nCount; }
  inline uint32_t GetGProxyEmptyActions() { return m_GProxyEmptyActions; }
  inline void AddActionFrameCounter() { ++m_NumActionFrames; }
  inline size_t GetNumActionFrames() { return m_NumActionFrames; }
  inline void SetStartedTicks(const int64_t nStartedTicks) { m_StartedTicks = nStartedTicks; }
  inline bool GetIsStarted() { return m_StartedTicks.has_value();}
  inline int64_t GetStartedTicks() { return m_StartedTicks.value(); }
};

#endif // AURA_GAME_STRUCTS_H_
