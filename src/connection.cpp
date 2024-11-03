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

CConnection::CConnection(CGameProtocol* nProtocol, CAura* nAura, uint16_t nPort, CStreamIOSocket* nSocket)
  : m_Aura(nAura),
    m_Protocol(nProtocol),
    m_Port(nPort),
    m_Type(INCOMING_CONNECTION_TYPE_NONE),
    m_Socket(nSocket),
    m_IncomingJoinPlayer(nullptr),
    m_DeleteMe(false)
{
}

CConnection::~CConnection()
{
  delete m_Socket;
  m_Socket = nullptr;
  delete m_IncomingJoinPlayer;
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
    return PREPLAYER_CONNECTION_DESTROY;
  }

  const int64_t Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  uint8_t result = PREPLAYER_CONNECTION_OK;
  bool Abort = false;
  if (m_Type == INCOMING_CONNECTION_TYPE_KICKED_PLAYER) {
    m_Socket->Discard(static_cast<fd_set*>(fd));
  } else if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
    // extract as many packets as possible from the socket's receive buffer and process them
    string*              RecvBuffer         = m_Socket->GetBytes();
    std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
    uint32_t             LengthProcessed    = 0;

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while (Bytes.size() >= 4) {
      // bytes 2 and 3 contain the length of the packet
      const uint16_t             Length = ByteArrayToUInt16(Bytes, false, 2);
      if (Length < 4) {
        Abort = true;
        break;
      }
      if (Bytes.size() < Length) break;
      const std::vector<uint8_t> Data   = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      if (Bytes[0] == W3GS_HEADER_CONSTANT ||
        (Bytes[0] == GPS_HEADER_CONSTANT && m_Aura->m_Net->m_Config->m_ProxyReconnect > 0) ||
        Bytes[0] == VLAN_HEADER_CONSTANT
        ) {
        if (Length >= 8 && Bytes[0] == W3GS_HEADER_CONSTANT && Bytes[1] == CGameProtocol::W3GS_REQJOIN) {
          if (!m_Aura->m_CurrentLobby || m_Aura->m_CurrentLobby->GetIsMirror() || m_Aura->m_CurrentLobby->GetLobbyLoading() || m_Aura->m_CurrentLobby->GetExiting()) {
            // Game already started
            Abort = true;
            break;
          }
          if (GetIsVLAN()) {
            // VLAN uses separate TCP connections for game discovery than for joining games.
            Abort = true;
            break;
          }
          if (GetIsUDPTunnel()) {
            vector<uint8_t> packet = {GPS_HEADER_CONSTANT, CGPSProtocol::GPS_UDPFIN, 4, 0};
            m_Socket->PutBytes(packet);
          }

          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
            Print("[AURA] Got REQJOIN " + ByteArrayToDecString(Bytes));
          }
          delete m_IncomingJoinPlayer;
          m_IncomingJoinPlayer = m_Protocol->RECEIVE_W3GS_REQJOIN(
            Data,
            m_Aura->m_CurrentLobby->m_Config->m_UnsafeNameHandler,
            m_Aura->m_CurrentLobby->m_Config->m_PipeConsideredHarmful
          );
          if (!m_IncomingJoinPlayer) {
            // Invalid request.
          } else if (m_Aura->m_CurrentLobby->GetHostCounter() != (m_IncomingJoinPlayer->GetHostCounter() & 0x00FFFFFF)) {
            // Trying to join the wrong game.
          } else if (m_Aura->m_CurrentLobby->EventRequestJoin(this, m_IncomingJoinPlayer)) {
            result = PREPLAYER_CONNECTION_PROMOTED;
            m_Socket = nullptr;
          } else {
            // Join failed.
          }
          Abort = true;
          if (result == PREPLAYER_CONNECTION_PROMOTED) {
            m_Type = INCOMING_CONNECTION_TYPE_PROMOTED_PLAYER;
          }
        } else if (GetIsUDPTunnel() && Bytes[0] == W3GS_HEADER_CONSTANT && CGameProtocol::W3GS_SEARCHGAME <= Bytes[1] && Bytes[1] <= CGameProtocol::W3GS_DECREATEGAME) {
          if (Length <= 1024) {
            struct UDPPkt pkt;
            pkt.socket = m_Socket;
            pkt.sender = &(m_Socket->m_RemoteHost);
            memcpy(pkt.buf, Bytes.data(), Length);
            pkt.length = Length;
            m_Aura->m_Net->HandleUDP(&pkt);
          } 
        } else if (Length >= 13 && Bytes[0] == GPS_HEADER_CONSTANT && Bytes[1] == CGPSProtocol::GPS_RECONNECT && m_Type == INCOMING_CONNECTION_TYPE_NONE) {
          const uint32_t reconnectKey = ByteArrayToUInt32(Bytes, false, 5);
          const uint32_t lastPacket = ByteArrayToUInt32(Bytes, false, 9);
          uint32_t gameID  = 0;
          if (Length >= 17) {
            gameID = ByteArrayToUInt32(Bytes, false, 13);
          }
          CGameUser* targetUser = m_Aura->m_Net->GetReconnectTargetUser(gameID, Bytes[4], reconnectKey);
          if (!targetUser || targetUser->GetGProxyReconnectKey() != reconnectKey) {
            m_Socket->PutBytes(m_Aura->m_GPSProtocol->SEND_GPSS_REJECT(targetUser == nullptr ? REJECTGPS_NOTFOUND : REJECTGPS_INVALID));
            if (targetUser) targetUser->EventGProxyReconnectInvalid();
            Abort = true;
          } else {
            // reconnect successful!
            targetUser->EventGProxyReconnect(m_Socket, lastPacket);
            m_Socket = nullptr;
            result = PREPLAYER_CONNECTION_RECONNECTED;
            Abort = true;
          }
        } else {
          bool anyExtensions = m_Aura->m_Net->m_Config->m_EnableTCPWrapUDP || m_Aura->m_Net->m_Config->m_VLANEnabled;
          if (m_Type == INCOMING_CONNECTION_TYPE_NONE && Length == 4 && Bytes[0] == GPS_HEADER_CONSTANT && Bytes[1] == CGPSProtocol::GPS_UDPSYN) {
            if (m_Aura->m_Net->m_Config->m_EnableTCPWrapUDP) {
              vector<uint8_t> packet = {GPS_HEADER_CONSTANT, CGPSProtocol::GPS_UDPACK, 4, 0};
              m_Socket->PutBytes(packet);
              m_Type = INCOMING_CONNECTION_TYPE_UDP_TUNNEL;
            } else if (!anyExtensions) {
              Abort = true;
            }
          } else if (m_Type == INCOMING_CONNECTION_TYPE_NONE && Length == 4 && Bytes[0] == VLAN_HEADER_CONSTANT && Bytes[1] == 0xFF) { // TODO: VLAN
            if (m_Aura->m_Net->m_Config->m_VLANEnabled) {
              m_Type = INCOMING_CONNECTION_TYPE_VLAN;
            } else if (!anyExtensions) {
              Abort = true;
            }
          }
        }

        if (Abort) {
          // Process no more packets
          break;
        }

        LengthProcessed += Length;
        Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
      }
    }

    if (Abort && result != PREPLAYER_CONNECTION_PROMOTED && result != PREPLAYER_CONNECTION_RECONNECTED) {
      result = PREPLAYER_CONNECTION_DESTROY;
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  } else if (Ticks - m_Socket->GetLastRecv() >= timeout) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  if (Abort) {
    m_DeleteMe = true;
  }

  /*
  if (result == PREPLAYER_CONNECTION_PROMOTED || result == PREPLAYER_CONNECTION_RECONNECTED) {
    return result;
  }
  */

  // At this point, m_Socket may have been transferred to CGameUser
  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError() || m_Socket->HasFin()) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  m_Socket->DoSend(static_cast<fd_set*>(send_fd));

  if (m_Type == INCOMING_CONNECTION_TYPE_KICKED_PLAYER && !m_Socket->GetIsSendPending()) {
    return PREPLAYER_CONNECTION_DESTROY;
  }

  return PREPLAYER_CONNECTION_OK;
}

void CConnection::Send(const std::vector<uint8_t>& data) const
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}
