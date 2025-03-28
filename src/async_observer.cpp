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

#include <utility>

#include "async_observer.h"

#include "aura.h"
#include "config/config_bot.h"
#include "game.h"
#include "game_user.h"
#include "map.h"
#include "net.h"
#include "realm.h"
#include "socket.h"
#include "protocol/game_protocol.h"
#include "protocol/gps_protocol.h"
#include "protocol/vlan_protocol.h"

using namespace std;

//
// CAsyncObserver
//

CAsyncObserver::CAsyncObserver(CConnection* nConnection, CGame* nGame, const CRealm* nFromRealm, uint8_t nUID, const string& nName)
  : CConnection(*nConnection),
    m_Game(nGame),
    m_GameHistory(nGame->GetGameHistory()),
    m_FromRealm(nFromRealm),
    m_MapReady(false),
    m_Desynchronized(false),
    m_Offset(0),
    m_Goal(ASYNC_OBSERVER_GOAL_OBSERVER),
    m_UID(nUID),
    m_SID(nGame->GetSIDFromUID(nUID)),
    m_Color(nGame->GetColorFromUID(nUID)),
    m_FrameRate(1),
    m_Latency(nGame->GetGameHistory()->GetDefaultLatency()),
    m_SyncCounter(0),
    m_ActionFrameCounter(0),
    m_NotifiedCannotDownload(false),
    m_StartedLoading(false),
    m_StartedLoadingTicks(0),
    m_FinishedLoading(false),
    m_FinishedLoadingTicks(0),
    m_PlaybackEnded(false),
    m_LastPingTime(APP_MIN_TICKS),
    m_Name(nName)
{
  m_Socket->SetLogErrors(true);
}

CAsyncObserver::~CAsyncObserver()
{
  m_GameHistory.reset();

  if (HasLeftReason()) {
    Print(GetLogPrefix() + "destroyed - " + GetLeftReason());
  } else {
    Print(GetLogPrefix() + "destroyed");
  }
}

void CAsyncObserver::SetTimeout(const int64_t delta)
{
  m_TimeoutTicks = GetTicks() + delta;
}

void CAsyncObserver::SetTimeoutAtLatest(const int64_t atLatestTicks)
{
  if (!m_TimeoutTicks.has_value() || atLatestTicks < m_TimeoutTicks.value()) {
    m_TimeoutTicks = atLatestTicks;
  }
}

bool CAsyncObserver::CloseConnection(bool /*recoverable*/)
{
  if (!m_Socket->GetConnected()) return false;
  m_Socket->Close();
  return true;
}

void CAsyncObserver::Init()
{
}

uint8_t CAsyncObserver::Update(fd_set* fd, fd_set* send_fd, int64_t timeout)
{
  if (!m_Socket || m_Socket->HasError()) {
    return ASYNC_OBSERVER_DESTROY;
  }

  if (m_DeleteMe) {
    m_Socket->ClearRecvBuffer(); // in case there are pending bytes from a previous recv
    m_Socket->Discard(fd);
    return ASYNC_OBSERVER_DESTROY;
  }

  const int64_t Time = GetTime(), Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
    SetLeftReasonGeneric("observer timeout");
    return ASYNC_OBSERVER_DESTROY;
  }

  uint8_t result = ASYNC_OBSERVER_OK;
  bool Abort = false;
  if (m_Type == INCON_TYPE_KICKED_PLAYER) {
    m_Socket->Discard(fd);
  } else if (m_Socket->DoRecv(fd)) {
    // extract as many packets as possible from the socket's receive buffer and process them
    string*              RecvBuffer         = m_Socket->GetBytes();
    std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
    uint32_t             LengthProcessed    = 0;

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while (Bytes.size() >= 4) {
      // bytes 2 and 3 contain the length of the packet
      const uint16_t Length = ByteArrayToUInt16(Bytes, false, 2);
      if (Length < 4) {
        EventProtocolError();
        Abort = true;
        break;
      }
      if (Bytes.size() < Length) break;
      const std::vector<uint8_t> Data = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      switch (Bytes[0]) {
        case GameProtocol::Magic::W3GS_HEADER: {
          switch (Bytes[1]) {
            case GameProtocol::Magic::LEAVEGAME: {
              if (ValidateLength(Data) && Data.size() >= 8) {
                const uint32_t reason = ByteArrayToUInt32(Data, false, 4);
                EventLeft(reason);
                //m_Socket->SetLogErrors(false);
              } else {
                EventProtocolError();
              }
              Abort = true;
              break;
            }

            case GameProtocol::Magic::GAMELOADED_SELF: {
              if (GameProtocol::RECEIVE_W3GS_GAMELOADED_SELF(Data)) {
                if (m_StartedLoading && !m_FinishedLoading) {
                  m_FinishedLoading      = true;
                  m_FinishedLoadingTicks = GetTicks();
                  m_LastFrameTicks = m_FinishedLoadingTicks;
                  EventGameLoaded();
                }
              }

              break;
            }

            case GameProtocol::Magic::OUTGOING_ACTION: {
              // Ignore all actions performed by observers,
              // and let's see how this turns out.
              Print(GetLogPrefix() + "got action <" + ByteArrayToDecString(Data) + ">");
              break;
            }

            case GameProtocol::Magic::OUTGOING_KEEPALIVE: {
              UpdateClientGameState(GameProtocol::RECEIVE_W3GS_OUTGOING_KEEPALIVE(Data));

              if (!m_Socket->GetConnected()) {
                Abort = true;
              }
              break;
            }

            case GameProtocol::Magic::CHAT_TO_HOST: {
              CIncomingChatMessage* ChatPlayer = GameProtocol::RECEIVE_W3GS_CHAT_TO_HOST(Data);

              if (ChatPlayer) {
                EventChatMessage(ChatPlayer);
                delete ChatPlayer;
              }
              break;
            }

            case GameProtocol::Magic::MAPSIZE: {
              if (m_MapReady || !m_Game || m_Game->GetIsGameOver()) {
                // Protection against rogue clients
                break;
              }

              CIncomingMapFileSize* MapSize = GameProtocol::RECEIVE_W3GS_MAPSIZE(Data);

              if (MapSize) {
                m_Game->EventObserverMapSize(this, MapSize);
              }

              delete MapSize;

              if (!m_Socket->GetConnected()) {
                Abort = true;
              }

              break;
            }

            case GameProtocol::Magic::DROPREQ:
            case GameProtocol::Magic::PONG_TO_HOST: {
              // ignore these
              break;
            }
          }
          break;
        }

        case GPSProtocol::Magic::GPS_HEADER: {
          // GProxy unsupported for observers
          if (/*m_Game && m_Game->GetIsProxyReconnectable() && */Bytes[1] == GPSProtocol::Magic::INIT) {
            Print(GetLogPrefix() + "client started GProxy handshake ");
          }
          break;
        }

        default: {
          Abort = true;
        }
      }

      LengthProcessed += Length;

      if (Abort) {
        // Process no more packets
        break;
      }

      Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
    }

    if (Abort && result != ASYNC_OBSERVER_PROMOTED) {
      result = ASYNC_OBSERVER_DESTROY;
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  } else if (Ticks >= m_Socket->GetLastRecv() + timeout) {
    SetLeftReasonGeneric("connection timed out");
    return ASYNC_OBSERVER_DESTROY;
  }

  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError() || m_Socket->HasFin()) {
    SetLeftReasonGeneric("observer decomissioned");
    return ASYNC_OBSERVER_DESTROY;
  }

  if (m_FinishedLoading && !m_PlaybackEnded) {
    const size_t beforeCounter = m_ActionFrameCounter;
    if (PushGameFrames()) {
      const size_t delta = SubtractClampZero(m_ActionFrameCounter, beforeCounter);
      if (beforeCounter <= 50 || delta > 1) Print(GetLogPrefix() + "pushed " + to_string(delta) + " action frames");
    }
    CheckGameOver();
  }

  if (m_LastPingTime + 5 <= Time) {
    Send(GameProtocol::SEND_W3GS_PING_FROM_HOST());
    m_LastPingTime = Time;
  }

  m_Socket->DoSend(send_fd);
  return result;
}

void CAsyncObserver::CheckGameOver()
{
  if (m_Game && !m_Game->GetIsGameOver()) return;
  if (m_GameHistory->m_PlayingBuffer.size() <= m_Offset) {
    m_PlaybackEnded = true;
    Print(GetLogPrefix() + "playback ended");
    if (!m_TimeoutTicks.has_value()) {
      SetTimeoutAtLatest(GetTicks() + 3000);
    }
  }
}

int64_t CAsyncObserver::GetNextTimedActionByTicks() const
{
  if (m_GameHistory->GetNumActionFrames() <= m_ActionFrameCounter) {
    return APP_MAX_TICKS;
  }
  return m_LastFrameTicks + m_Latency / m_FrameRate;
}

bool CAsyncObserver::PushGameFrames()
{
  int64_t Ticks = GetTicks();
  // Note: Actually, each frame may have its own custom latency.
  int64_t gameDurationWanted = m_FrameRate * (Ticks - m_LastFrameTicks);
  if (gameDurationWanted < m_Latency) {
    // Fast path for the common case (there will never be a GAME_FRAME_TYPE_LATENCY hanging)
    return false;
  }
  if (m_ActionFrameCounter >= m_GameHistory->GetNumActionFrames()) {
    return false;
  }

  bool success = false;
  auto it = begin(m_GameHistory->m_PlayingBuffer) + m_Offset;
  auto itEnd = end(m_GameHistory->m_PlayingBuffer);
  while (it != itEnd && (m_Latency <= gameDurationWanted || it->GetType() == GAME_FRAME_TYPE_LATENCY)) {
    switch (it->GetType()) {
      case GAME_FRAME_TYPE_GPROXY:
        // if stored, GAME_FRAME_TYPE_GPROXY always precedes GAME_FRAME_TYPE_ACTIONS
        Send(GameProtocol::SEND_W3GS_EMPTY_ACTIONS(m_GameHistory->GetGProxyEmptyActions()));
        break;
      case GAME_FRAME_TYPE_LATENCY:
        // it stored, GAME_FRAME_TYPE_LATENCY always goes after GAME_FRAME_TYPE_ACTIONS
        m_Latency = ByteArrayToUInt16(it->GetBytes(), false, 0);
        break;
      case GAME_FRAME_TYPE_ACTIONS:  
        gameDurationWanted -= m_Latency;
        // falls through
      case GAME_FRAME_TYPE_PAUSED:
        success = true;
        m_LastFrameTicks = Ticks;
        ++m_ActionFrameCounter;
        Send(it->GetBytes());
        break;
      default:
        // GAME_FRAME_TYPE_LEAVER, GAME_FRAME_TYPE_CHAT
        Send(it->GetBytes());
    }
    ++it;
    ++m_Offset;
  }

  return success;
}

void CAsyncObserver::OnGameReset(const CGame* nGame)
{
  if (m_Game == nGame) {
    m_Game = nullptr;
  }
}

void CAsyncObserver::UpdateClientGameState(const uint32_t checkSum)
{
  if (m_Desynchronized) return;

  if (m_Game && m_Game->GetSyncCounter() <= m_SyncCounter) {
    string text = GetLogPrefix() + "incorrectly ahead of sync";
    Print(text);
    m_Aura->LogPersistent(text);
    return;
  }
  if (m_GameHistory->GetDesynchronized() && m_SyncCounter >= m_GameHistory->GetNumCheckSums()) {
    return;
  }

  m_CheckSums.push(checkSum);
  ++m_SyncCounter;
  CheckClientGameState();
}

void CAsyncObserver::CheckClientGameState()
{
  if (m_Desynchronized) return;

  size_t nextCheckSumIndex = m_SyncCounter - m_CheckSums.size();
  while (!m_CheckSums.empty() && nextCheckSumIndex < m_GameHistory->GetNumCheckSums()) {
    uint32_t nextCheckSum = m_CheckSums.front();
    if (nextCheckSum != m_GameHistory->GetCheckSum(nextCheckSumIndex)) {
      m_Desynchronized = true; // how? idfk
      EventDesync();
      break;
    }
    ++nextCheckSumIndex;
    m_CheckSums.pop();
  }
}

void CAsyncObserver::UpdateDownloadProgression(const uint8_t downloadProgression)
{
  if (!m_Game) return;
  vector<uint8_t> slotInfo = m_Game->GetSlotInfo();
  constexpr static uint16_t fixedOffset = (
    2 /* W3GS type headers */ +
    2 /* W3GS packet byte size */ +
    2 /* EncodeSlotInfo() byte size */ +
    1 /* number of slots */ +
    1 /* download status offset in CGameSlot::GetProtocolArray() */
  );
  uint16_t progressionIndex = 9 * m_SID + fixedOffset;
  slotInfo[progressionIndex] = downloadProgression;
  Send(slotInfo);
}

uint8_t CAsyncObserver::NextSendMap()
{
  return m_Game->NextSendMap(this, GetUID(), GetMapTransfer()); 
}

void CAsyncObserver::EventDesync()
{
  while (!m_CheckSums.empty()) {
    m_CheckSums.pop();
  }
  string text = GetLogPrefix() + "desynchronized on " + ToOrdinalName(m_SyncCounter) + " checksum - sent " + to_string(m_Offset) + " total frames (" + to_string(m_ActionFrameCounter) + " actions)";
  Print(text);
  m_Aura->LogPersistent(text);

  if (!CloseConnection()) {
    return;
  }
  SetLeftReasonGeneric("desynchronized");
  SetDeleteMe(true);
}

void CAsyncObserver::EventMapReady()
{
  m_MapReady = true;

  StartLoading();
}

void CAsyncObserver::StartLoading()
{
  if (m_StartedLoading) return;
  Print(GetLogPrefix() + "started loading");
  Send(GameProtocol::SEND_W3GS_COUNTDOWN_START());
  Send(GameProtocol::SEND_W3GS_COUNTDOWN_END());
  m_StartedLoading = true;
}

void CAsyncObserver::EventGameLoaded()
{
  Print(GetLogPrefix() + "finished loading");
  Send(m_GameHistory->m_LoadingRealBuffer);
  Send(m_GameHistory->m_LoadingVirtualBuffer);

  int64_t ss, mm, hh;
  ss = (GetTicks() - m_GameHistory->GetStartedTicks()) / 1000;

  mm = ss / 60;
  ss = ss % 60;
  hh = mm / 60;
  mm = mm % 60;

  if (m_Game && !m_Game->GetIsGameOver()) {
    SendChat("Running spectator mode (delay is " + ToDurationString(hh, mm, ss) + ")");
  } else {
    SendChat("Watching replay");
    SendChat("Game was played " + ToFormattedTimeStamp(hh, mm, ss) + " ago");
  }
}

void CAsyncObserver::EventChatMessage(const CIncomingChatMessage* incomingChatMessage)
{
  string message = incomingChatMessage->GetMessage();
  Print(GetLogPrefix() + ": " + message);
  if (message == "!ff") {
    if (m_FrameRate >= 64) {
      SendChat("Playback rate is limited to 64x");
    } else {
      m_FrameRate *= 2;
      SendChat("Playback rate set to " + to_string(m_FrameRate) + "x");
    }
  } else if (message == "!sync") {
    m_FrameRate = 1;
    SendChat("Playback rate set to " + to_string(m_FrameRate) + "x");
  } else {
    SendChat("You are in spectator mode. Chat is RESTRICTED.");
  }
}

void CAsyncObserver::EventLeft(const uint32_t clientReason)
{
  if (!CloseConnection()) {
    return;
  }
  if (m_StartedLoading) {
    Print(GetLogPrefix() + "left the game (" + GameProtocol::LeftCodeToString(clientReason) + ")");
  } else {
    Print(GetLogPrefix() + "left the lobby");
  }
  SetLeftReasonGeneric("left voluntarily");
  SetDeleteMe(true);
}

void CAsyncObserver::EventProtocolError()
{
  if (!CloseConnection()) {
    return;
  }
  SetLeftReasonGeneric("disconnected due to protocol error");
  SetDeleteMe(true);
}

void CAsyncObserver::Send(const std::vector<uint8_t>& data)
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}

void CAsyncObserver::SendOtherPlayersInfo()
{
  Send(m_GameHistory->m_PlayersBuffer);
}

void CAsyncObserver::SendChat(const string& message)
{
  if (m_StartedLoading && !m_FinishedLoading) {
    return;
  }
  if (!m_StartedLoading) {
    if (message.size() > 254)
      Send(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(m_UID, CreateByteArray(m_UID), 16, std::vector<uint8_t>(), message.substr(0, 254)));
    else
      Send(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(m_UID, CreateByteArray(m_UID), 16, std::vector<uint8_t>(), message));
  } else {
    uint16_t receiverByte = static_cast<uint16_t>(3u + m_Color);
    uint8_t extraFlags[] = {(uint8_t)receiverByte, 0, 0, 0};
    if (message.size() > 127)
      Send(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(m_UID, CreateByteArray(m_UID), 32, CreateByteArray(extraFlags, 4), message.substr(0, 127)));
    else
      Send(GameProtocol::SEND_W3GS_CHAT_FROM_HOST(m_UID, CreateByteArray(m_UID), 32, CreateByteArray(extraFlags, 4), message));
  }
}

string CAsyncObserver::GetLogPrefix() const
{
  if (m_Game) return m_Game->GetLogPrefix() + "[OBSERVER] [" + m_Name + "] ";
  return "[OBSERVER] [" + m_Name + "] ";
}
