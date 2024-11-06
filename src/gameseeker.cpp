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

#include "config_bot.h"
#include "gameseeker.h"
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
// CGameSeeker
//

CGameSeeker::CGameSeeker(CGameProtocol* nProtocol, CAura* nAura, uint16_t nPort, uint8_t nType, CStreamIOSocket* nSocket)
  : m_Aura(nAura),
    m_Protocol(nProtocol),
    m_Port(nPort),
    m_Type(nType),
    m_Socket(nSocket),
    m_DeleteMe(false)
{
}

CGameSeeker::~CGameSeeker()
{
  delete m_Socket;
  m_Socket = nullptr;
}

void CGameSeeker::SetTimeout(const int64_t delta)
{
  m_TimeoutTicks = GetTicks() + delta;
}

void CGameSeeker::CloseConnection()
{
  m_Socket->Close();
}

void CGameSeeker::Init()
{
  switch (m_Type) {
    case GAMESEEKER_TYPE_UDP_TUNNEL: {
      vector<uint8_t> packet = {GPS_HEADER_CONSTANT, GPSProtocol::Magic::GPS_UDPACK, 4, 0};
      m_Socket->PutBytes(packet);
      break;
    }
    case GAMESEEKER_TYPE_VLAN: {
      // do nothing - client should send VLAN_SEARCHGAME
      break;
    }
  }
}

uint8_t CGameSeeker::Update(void* fd, void* send_fd, int64_t timeout)
{
  if (m_DeleteMe || !m_Socket || m_Socket->HasError()) {
    return GAMESEEKER_DESTROY;
  }

  const int64_t Ticks = GetTicks();

  if (m_TimeoutTicks.has_value() && m_TimeoutTicks.value() < Ticks) {
    return GAMESEEKER_DESTROY;
  }

  uint8_t result = GAMESEEKER_OK;
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
      const uint16_t Length = ByteArrayToUInt16(Bytes, false, 2);
      if (Length < 4) {
        Abort = true;
        break;
      }
      if (Bytes.size() < Length) break;
      const std::vector<uint8_t> Data = std::vector<uint8_t>(begin(Bytes), begin(Bytes) + Length);

      switch (Bytes[0]) {
        case W3GS_HEADER_CONSTANT:
          if (m_Type != GAMESEEKER_TYPE_UDP_TUNNEL) {
            Abort = true;
            break;
          }
          if (Bytes[1] == CGameProtocol::W3GS_REQJOIN) {
            CIncomingJoinRequest* joinRequest = m_Protocol->RECEIVE_W3GS_REQJOIN(Data);
            if (!joinRequest) {
              Abort = true;
              break;
            }
            CGame* targetLobby = m_Aura->GetLobbyByHostCounter(joinRequest->GetHostCounter());
            if (!targetLobby || targetLobby->GetIsMirror() || targetLobby->GetLobbyLoading() || targetLobby->GetExiting()) {
              delete joinRequest;
              break;
            }
            joinRequest->UpdateCensored(targetLobby->m_Config->m_UnsafeNameHandler, targetLobby->m_Config->m_PipeConsideredHarmful);
            if (targetLobby->EventRequestJoin(this, joinRequest)) {
              result = GAMESEEKER_PROMOTED;
              m_Socket = nullptr;
            }
            delete joinRequest;
          } else if (CGameProtocol::W3GS_SEARCHGAME <= Bytes[1] && Bytes[1] <= CGameProtocol::W3GS_DECREATEGAME) {
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

        case VLAN_HEADER_CONSTANT: {
          if (m_Type != GAMESEEKER_TYPE_VLAN) {
            Abort = true;
            break;
          }
          // TODO: VLAN
          break;
        }

         default:
          Abort = true;
      }

      LengthProcessed += Length;

      if (Abort) {
        // Process no more packets
        break;
      }

      Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
    }

    if (Abort && result != GAMESEEKER_PROMOTED) {
      result = GAMESEEKER_DESTROY;
      RecvBuffer->clear();
    } else if (LengthProcessed > 0) {
      *RecvBuffer = RecvBuffer->substr(LengthProcessed);
    }
  } else if (Ticks - m_Socket->GetLastRecv() >= timeout) {
    return GAMESEEKER_DESTROY;
  }

  // At this point, m_Socket may have been transferred to CGameUser
  if (m_DeleteMe || !m_Socket->GetConnected() || m_Socket->HasError() || m_Socket->HasFin()) {
    return GAMESEEKER_DESTROY;
  }

  m_Socket->DoSend(static_cast<fd_set*>(send_fd));

  return result;
}

void CGameSeeker::Send(const std::vector<uint8_t>& data) const
{
  if (m_Socket && !m_Socket->HasError()) {
    m_Socket->PutBytes(data);
  }
}
