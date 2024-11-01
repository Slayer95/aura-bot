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
    m_IncomingJoinPlayer(nullptr),
    m_DeleteMe(false)
{
}

CGameSeeker::~CGameSeeker()
{
  delete m_Socket;
  m_Socket = nullptr;
  delete m_IncomingJoinPlayer;
}

void CGameSeeker::SetTimeout(const int64_t delta)
{
  m_TimeoutTicks = GetTicks() + delta;
}

void CGameSeeker::CloseConnection()
{
  m_Socket->Close();
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

        if (Abort) {
          // Process no more packets
          break;
        }

        LengthProcessed += Length;
        Bytes = std::vector<uint8_t>(begin(Bytes) + Length, end(Bytes));
      }
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

  if (Abort) {
    m_DeleteMe = true;
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
