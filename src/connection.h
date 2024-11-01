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

#ifndef AURA_CONNECTION_H_
#define AURA_CONNECTION_H_

#include <queue>
#include <string>
#include <optional>

#include "socket.h"

class CStreamIOSocket;
class CGameProtocol;
class CGame;
class CIncomingJoinRequest;
class CAura;

#define PREPLAYER_CONNECTION_OK 0u
#define PREPLAYER_CONNECTION_DESTROY 1u
#define PREPLAYER_CONNECTION_PROMOTED 2u
#define PREPLAYER_CONNECTION_RECONNECTED 3u

#define INCOMING_CONNECTION_TYPE_NONE 0u
#define INCOMING_CONNECTION_TYPE_UDP_TUNNEL 1u
#define INCOMING_CONNECTION_TYPE_PROMOTED_PLAYER 2u
#define INCOMING_CONNECTION_TYPE_KICKED_PLAYER 3u
#define INCOMING_CONNECTION_TYPE_VLAN 4u

//
// CConnection
//

class CConnection
{
public:
  CAura*                  m_Aura;
  CGameProtocol*          m_Protocol;
  uint16_t                m_Port;
  uint8_t                 m_Type;
  std::optional<int64_t>  m_TimeoutTicks;

protected:
  // note: we permit m_Socket to be NULL in this class to allow for the virtual host player which doesn't really exist
  // it also allows us to convert CGameConnections to CGamePlayers without the CConnection's destructor closing the socket

  CStreamIOSocket*          m_Socket;
  CIncomingJoinRequest*     m_IncomingJoinPlayer;
  bool                      m_DeleteMe;

public:
  CConnection(CGameProtocol* nProtocol, CAura* nAura, uint16_t nPort, CStreamIOSocket* nSocket);
  ~CConnection();

  inline CStreamIOSocket*           GetSocket() const { return m_Socket; }
  inline bool                       GetUsingIPv6() const { return m_Socket->GetIsInnerIPv6(); }
  inline std::array<uint8_t, 4>     GetIPv4() const { return m_Socket->GetIPv4(); }
  inline std::string                GetIPString() const { return m_Socket->GetIPString(); }
  inline std::string                GetIPStringStrict() const { return m_Socket->GetIPStringStrict(); }
  inline sockaddr_storage*          GetRemoteAddress() const { return &(m_Socket->m_RemoteHost); }
  inline bool                       GetIsUDPTunnel() const { return m_Type == INCOMING_CONNECTION_TYPE_UDP_TUNNEL; }
  inline bool                       GetIsVLAN() const { return m_Type == INCOMING_CONNECTION_TYPE_VLAN; }
  inline uint8_t                    GetType() const { return m_Type; }
  inline uint16_t                   GetPort() const { return m_Port; }
  inline bool                       GetDeleteMe() const { return m_DeleteMe; }
  inline CIncomingJoinRequest*      GetJoinPlayer() const { return m_IncomingJoinPlayer; }

  inline void SetSocket(CStreamIOSocket* nSocket) { m_Socket = nSocket; }
  inline void SetType(const uint8_t nType) { m_Type = nType; }
  inline void SetDeleteMe(bool nDeleteMe) { m_DeleteMe = nDeleteMe; }

  // processing functions

  void SetTimeout(const int64_t nTicks);
  void CloseConnection();
  uint8_t Update(void* fd, void* send_fd, int64_t timeout);

  // other functions

  void Send(const std::vector<uint8_t>& data) const;
};

#endif // AURA_CONNECTION_H_
