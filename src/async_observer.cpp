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

CAsyncObserver::CAsyncObserver(CConnection* nConnection, CGame* nGame, uint8_t nUID, const string& nName)
  : CConnection(*nConnection),
    m_Game(nGame),
    m_GameHistory(nGame->GetGameHistory()),
    m_MapReady(false),
    m_Desynchronized(false),
    m_Offset(0),
    m_Goal(ASYNC_OBSERVER_GOAL_OBSERVER),
    m_UID(nUID),
    m_SID(nGame->GetSIDFromUID(nUID)),
    m_FrameRate(1),
    m_SyncCounter(0),
    m_DownloadStarted(false),
    m_DownloadFinished(false),
    m_FinishedDownloadingTime(0),
    m_StartedLoading(false),
    m_StartedLoadingTicks(0),
    m_FinishedLoading(false),
    m_FinishedLoadingTicks(0),
    m_PlaybackEnded(false),
    m_LastPingTime(0),
    m_Name(nName)
{
  m_Socket->SetLogErrors(true);
}

CAsyncObserver::~CAsyncObserver()
{
  m_GameHistory.reset();
}

void CAsyncObserver::SetTimeout(const int64_t delta)
{
  m_TimeoutTicks = GetTicks() + delta;
}

bool CAsyncObserver::CloseConnection()
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
    return m_DeleteMe;
  }

  const int64_t Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
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
                m_Socket->SetLogErrors(false);
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
                  EventGameLoaded();
                }
              }

              break;
            }

            case GameProtocol::Magic::OUTGOING_ACTION: {
              // Ignore all actions performed by observers,
              // and let's see how this turns out.
              break;
            }

            case GameProtocol::Magic::OUTGOING_KEEPALIVE: {
              UpdateClientGameState(GameProtocol::RECEIVE_W3GS_OUTGOING_KEEPALIVE(Data));
              break;
            }

            case GameProtocol::Magic::CHAT_TO_HOST: {
              CIncomingChatPlayer* ChatPlayer = GameProtocol::RECEIVE_W3GS_CHAT_TO_HOST(Data);

              if (ChatPlayer) {
                Print(GetLogPrefix() + "[" + this->GetName() + "] (observer): " + ChatPlayer->GetMessage());
                delete ChatPlayer;
              }
              break;
            }

            case GameProtocol::Magic::MAPSIZE: {
              if (m_MapReady) {
                // Protection against rogue clients
                break;
              }

              CIncomingMapSize* MapSize = GameProtocol::RECEIVE_W3GS_MAPSIZE(Data);

              if (MapSize && m_Game) {
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
  } else if (Ticks - m_Socket->GetLastRecv() >= timeout) {
    return ASYNC_OBSERVER_DESTROY;
  }

  // At this point, m_Socket may have been transferred to GameUser::CGameUser
  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError() || m_Socket->HasFin()) {
    if (m_DeleteMe) {
      Print("ASYNC_OBSERVER_DESTROY L246");
    } else {
      Print("ASYNC_OBSERVER_DESTROY L248");
      if (!m_Socket->GetConnected()) Print("ASYNC_OBSERVER_DESTROY L249");
      if (m_Socket->HasError()) Print("ASYNC_OBSERVER_DESTROY L250");
      if (m_Socket->HasFin()) Print("ASYNC_OBSERVER_DESTROY L251");
    }
    return ASYNC_OBSERVER_DESTROY;
  }

  SendUpdates(send_fd);

  if (!m_PlaybackEnded && !m_Game && m_GameHistory->m_PlayingBuffer.size() <= m_Offset) {
    m_PlaybackEnded = true;
    Print("[CAsyncObserver] Sent entire game to [" + GetName() + "]");
    if (!m_TimeoutTicks.has_value()) {
      SetTimeout(3000);
    }
  }

  return result;
}

void CAsyncObserver::SendUpdates(fd_set* send_fd)
{
  int64_t Time = GetTime();
  if (m_FinishedLoading) {
    int64_t prevTicks = m_LastFrameTicks.has_value() ? m_LastFrameTicks.value() : m_FinishedLoadingTicks;
    const size_t totalPendingUpdates = static_cast<size_t>((int64_t)(m_FrameRate) * (GetTicks() - prevTicks) / (int64_t)(m_GameHistory->GetLatency()));
    size_t pendingUpdates = totalPendingUpdates;

    bool anyUpdated = false;
    if (pendingUpdates >= 1) {
      auto it = begin(m_GameHistory->m_PlayingBuffer) + m_Offset;
      auto itEnd = end(m_GameHistory->m_PlayingBuffer);
      while (it != itEnd) {
        anyUpdated = true;
        ++m_Offset;
        if (it->GetType() == GAME_FRAME_TYPE_GPROXY) {
          Send(GameProtocol::SEND_W3GS_EMPTY_ACTIONS(m_GameHistory->GetGProxyEmptyActions()));
        } else {
          Send(it->GetBytes());
        }
        if (it->GetType() == GAME_FRAME_TYPE_ACTIONS && (--pendingUpdates == 0)) {
          break;
        }
        ++it;
      }
    }

    if (anyUpdated) {
      m_LastFrameTicks = GetTicks();
    }
  }

  if (Time - m_LastPingTime >= 5) {
    Send(GameProtocol::SEND_W3GS_PING_FROM_HOST());
    m_LastPingTime = Time;
  }

  m_Socket->DoSend(send_fd);
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
    string text = GetLogPrefix() + "observer [" + m_Name + "] incorrectly ahead of sync";
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
      OnDesync();
    }
    ++nextCheckSumIndex;
    m_CheckSums.pop();
  }
}

void CAsyncObserver::OnDesync()
{
  while (!m_CheckSums.empty()) {
    m_CheckSums.pop();
  }
  string text = GetLogPrefix() + "observer [" + m_Name + "] desynchronized";
  Print(text);
  m_Aura->LogPersistent(text);
}

void CAsyncObserver::EventMapReady()
{
  if (m_StartedLoading) return;
  Send(GameProtocol::SEND_W3GS_COUNTDOWN_START());
  Send(GameProtocol::SEND_W3GS_COUNTDOWN_END());
  m_StartedLoading = true;
}

void CAsyncObserver::EventGameLoaded()
{
  Print("Observer [" + GetName() + "] finished loading");
  Send(m_GameHistory->m_LoadingRealBuffer);
  Send(m_GameHistory->m_LoadingVirtualBuffer);
}

void CAsyncObserver::EventLeft(const uint32_t clientReason)
{
  if (!CloseConnection()) {
    return;
  }
  Print("Observer [" + GetName() + "] left the game (" + GameProtocol::LeftCodeToString(clientReason) + ")");
  SetDeleteMe(true);
}

void CAsyncObserver::EventProtocolError()
{
  if (!CloseConnection()) {
    return;
  }
  Print("Observer [" + GetName() + "] disconnected due to protocol error");
  SetDeleteMe(true);
}

void CAsyncObserver::Send(const std::vector<uint8_t>& data)
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}

string CAsyncObserver::GetLogPrefix() const
{
  if (m_Game) return m_Game->GetLogPrefix();
  return "[OBSERVER] ";
}
