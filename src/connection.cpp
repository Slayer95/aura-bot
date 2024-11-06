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

#include <utility>

#include "config_bot.h"
#include "connection.h"
#include "gameuser.h"
#include "aura.h"
#include "realm.h"
#include "map.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "vlanprotocol.h"
#include "game.h"
#include "socket.h"
#include "net.h"

using namespace std;

//
// CConnection
//

CConnection::CConnection(CAura* nAura, uint16_t nPort, CStreamIOSocket* nSocket)
  : m_Aura(nAura),
    m_Port(nPort),
    m_Type(INCON_TYPE_NONE),
    m_Socket(nSocket),
    m_DeleteMe(false)
{
}

CConnection::CConnection(const CConnection& nFromCopy)
  : m_Aura(nFromCopy.m_Aura),
    m_Port(nFromCopy.m_Port),
    m_Type(nFromCopy.m_Type),
    m_Socket(nFromCopy.m_Socket),
    m_DeleteMe(nFromCopy.m_DeleteMe)
{
}

CConnection::~CConnection()
{
  delete m_Socket;
  m_Socket = nullptr;
}

void CConnection::SetTimeout(const int64_t delta)
{
  m_TimeoutTicks = GetTicks() + delta;
}

void CConnection::CloseConnection()
{
  m_Socket->Close();
}

uint8_t CConnection::Update(void* fd, void* send_fd, int64_t timeout)
{
  if (m_DeleteMe || !m_Socket || m_Socket->HasError()) {
    return INCON_UPDATE_DESTROY;
  }

  const int64_t Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
    return INCON_UPDATE_DESTROY;
  }

  uint8_t result = INCON_UPDATE_OK;
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
        Abort = true;
        break;
      }
      if (Bytes.size() < Length) break;
      const std::vector<uint8_t> Data = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      switch (Bytes[0]) {
        case W3GS_HEADER_CONSTANT:
          if (Bytes[1] == static_cast<uint8_t>(GameProtocol::Magic::REQJOIN)) {
            CIncomingJoinRequest* joinRequest = GameProtocol::RECEIVE_W3GS_REQJOIN(Data);
            if (!joinRequest) {
              if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
                Print("[AURA] Got invalid REQJOIN " + ByteArrayToDecString(Bytes));
              }
              Abort = true;
              break;
            }
            if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
              Print("[AURA] Got valid REQJOIN " + ByteArrayToDecString(Bytes));
            }
            CGame* targetLobby = m_Aura->GetLobbyByHostCounter(joinRequest->GetHostCounter());
            if (!targetLobby || targetLobby->GetIsMirror() || targetLobby->GetLobbyLoading() || targetLobby->GetExiting()) {
              break;
            }
            joinRequest->UpdateCensored(targetLobby->m_Config->m_UnsafeNameHandler, targetLobby->m_Config->m_PipeConsideredHarmful);
            if (targetLobby->EventRequestJoin(this, joinRequest)) {
              result = INCON_UPDATE_PROMOTED;
              m_Type = INCON_TYPE_PLAYER;
              m_Socket = nullptr;
            }
            Abort = true;
          } else if (static_cast<uint8_t>(GameProtocol::Magic::SEARCHGAME) <= Bytes[1] && Bytes[1] <= static_cast<uint8_t>(GameProtocol::Magic::DECREATEGAME)) {
            if (Length > 1024) {
              Abort = true;
              break;
            }
            struct UDPPkt pkt;
            pkt.socket = m_Socket;
            pkt.sender = &(m_Socket->m_RemoteHost);
            memcpy(pkt.buf, Bytes.data(), Length);
            pkt.length = Length;
            m_Aura->m_Net->HandleUDP(&pkt);
          } else {
            Abort = true;
            break;
          }
          break;

        case GPS_HEADER_CONSTANT: {
          if (Length >= 13 && Bytes[1] == static_cast<uint8_t>(GPSProtocol::Magic::RECONNECT) && m_Type == INCON_TYPE_NONE && m_Aura->m_Net->m_Config->m_ProxyReconnect > 0) {
            const uint32_t reconnectKey = ByteArrayToUInt32(Bytes, false, 5);
            const uint32_t lastPacket = ByteArrayToUInt32(Bytes, false, 9);
            CGameUser* targetUser = nullptr;
            if (Length >= 17) {
              targetUser = m_Aura->m_Net->GetReconnectTargetUser(ByteArrayToUInt32(Bytes, false, 13), Bytes[4]);
            } else {
              targetUser = m_Aura->m_Net->GetReconnectTargetUserLegacy(Bytes[4], reconnectKey);
            }
            if (!targetUser || targetUser->GetGProxyReconnectKey() != reconnectKey) {
              m_Socket->PutBytes(GPSProtocol::SEND_GPSS_REJECT(targetUser == nullptr ? REJECTGPS_NOTFOUND : REJECTGPS_INVALID));
              if (targetUser) targetUser->EventGProxyReconnectInvalid();
              Abort = true;
            } else {
              // reconnect successful!
              targetUser->EventGProxyReconnect(this, lastPacket);
              result = INCON_UPDATE_RECONNECTED;
              Abort = true;
            }          
          } else if (Length >= 4 && Bytes[1] == static_cast<uint8_t>(GPSProtocol::Magic::UDPSYN) && m_Aura->m_Net->m_Config->m_EnableTCPWrapUDP) {
            // in-house extension
            m_Aura->m_Net->RegisterGameSeeker(this, INCON_TYPE_UDP_TUNNEL);
            result = INCON_UPDATE_PROMOTED;
            Abort = true;
          }
          break;
        }

        case VLAN_HEADER_CONSTANT: {
          if (m_Type != INCON_TYPE_NONE) {
            Abort = true;
            break;
          }
          m_Aura->m_Net->RegisterGameSeeker(this, INCON_TYPE_VLAN);
          result = INCON_UPDATE_PROMOTED_PASSTHROUGH;
          Abort = true;
          break;
        }

         default:
          Abort = true;
      }

      if (result != INCON_UPDATE_PROMOTED_PASSTHROUGH) {
        LengthProcessed += Length;
      }

      if (Abort) {
        // Process no more packets
        break;
      }

      Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
    }

    if (Abort && result != INCON_UPDATE_PROMOTED && result != INCON_UPDATE_PROMOTED_PASSTHROUGH && result != INCON_UPDATE_RECONNECTED) {
      result = INCON_UPDATE_DESTROY;
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  } else if (Ticks - m_Socket->GetLastRecv() >= timeout) {
    return INCON_UPDATE_DESTROY;
  }

  if (Abort) {
    m_DeleteMe = true;
  }

  /*
  if (result == INCON_UPDATE_PROMOTED || result == INCON_UPDATE_PROMOTED_PASSTHROUGH || result == INCON_UPDATE_RECONNECTED) {
    return result;
  }
  */

  // At this point, m_Socket may have been transferred to CGameUser
  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError() || m_Socket->HasFin()) {
    return INCON_UPDATE_DESTROY;
  }

  m_Socket->DoSend(static_cast<fd_set*>(send_fd));

  if (m_Type == INCON_TYPE_KICKED_PLAYER && !m_Socket->GetIsSendPending()) {
    return INCON_UPDATE_DESTROY;
  }

  return INCON_UPDATE_OK;
}

void CConnection::Send(const std::vector<uint8_t>& data)
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}
