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
    m_MapReady(false),
    m_Synchronized(false),
    m_Offset(0),
    m_Goal(ASYNC_OBSERVER_GOAL_OBSERVER),
    m_UID(nUID),
    m_SID(nGame->GetSIDFromUID(nUID)),
    m_FrameRate(1),
    m_SyncCounter(0),
    m_DownloadStarted(false),
    m_DownloadFinished(false),
    m_FinishedDownloadingTime(0),
    m_FinishedLoading(false),
    m_FinishedLoadingTicks(0),
    m_Name(nName)
{
}

CAsyncObserver::~CAsyncObserver()
{  
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

uint8_t CAsyncObserver::Update(void* fd, void* send_fd, int64_t timeout)
{
  if (m_DeleteMe || !m_Socket || m_Socket->HasError()) {
    return ASYNC_OBSERVER_DESTROY;
  }

  const int64_t Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
    return ASYNC_OBSERVER_DESTROY;
  }

  uint8_t result = ASYNC_OBSERVER_OK;
  bool Abort = false;
  if (m_Type == INCON_TYPE_KICKED_PLAYER) {
    m_Socket->Discard(static_cast<fd_set*>(fd));
  } else if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
    // extract as many packets as possible from the socket's receive buffer and process them
    string*              RecvBuffer         = m_Socket->GetBytes();
    std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
    uint32_t             LengthProcessed    = 0;

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while (Bytes.size() >= 4) {
      // bytes 2 and 3 contain the length of the packet
      const uint16_t Length = ByteArrayToUInt16(Bytes, false, 2);
      if (Length < 4) {
        m_Game->EventObserverDisconnectProtocolError(this);
        Abort = true;
        break;
      }
      if (Bytes.size() < Length) break;
      const std::vector<uint8_t> Data = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      switch (Bytes[0]) {
        case GameProtocol::Magic::W3GS_HEADER:
          switch (Bytes[1]) {
            case GameProtocol::Magic::LEAVEGAME: {
              if (ValidateLength(Data) && Data.size() >= 8) {
                const uint32_t reason = ByteArrayToUInt32(Data, false, 4);
                m_Game->EventObserverLeft(this, reason);
                m_Socket->SetLogErrors(false);
              } else {
                m_Game->EventObserverDisconnectProtocolError(this);
              }
              Abort = true;
              break;
            }

            case GameProtocol::Magic::GAMELOADED_SELF: {
              if (GameProtocol::RECEIVE_W3GS_GAMELOADED_SELF(Data)) {
                if (!m_FinishedLoading) {
                  m_FinishedLoading      = true;
                  m_FinishedLoadingTicks = GetTicks();
                  m_Game->EventObserverLoaded(this);
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
              //m_CheckSums.push(GameProtocol::RECEIVE_W3GS_OUTGOING_KEEPALIVE(Data));
              ++m_SyncCounter;
              m_Game->EventObserverKeepAlive(this);
              break;
            }

            case GameProtocol::Magic::CHAT_TO_HOST: {
              CIncomingChatPlayer* ChatPlayer = GameProtocol::RECEIVE_W3GS_CHAT_TO_HOST(Data);

              if (ChatPlayer) {
                m_Game->LogApp("[" + this->GetName() + "] (observer): " + ChatPlayer->GetMessage(), LOG_C);
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

              if (MapSize) {
                m_Game->EventObserverMapSize(this, MapSize);
              }

              delete MapSize;
              break;
            }

            case GameProtocol::Magic::DROPREQ:
            case GameProtocol::Magic::PONG_TO_HOST: {
              // ignore these
              break;
            }
          }
          break;


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
    return ASYNC_OBSERVER_DESTROY;
  }

  m_Socket->DoSend(static_cast<fd_set*>(send_fd));

  return result;
}

void CAsyncObserver::Send(const std::vector<uint8_t>& data)
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}
