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

#include <cstring>

#include "aura.h"
#include "util.h"
#include "socket.h"
#include "includes.h"

using namespace std;

#ifndef WIN32
int32_t GetLastError()
{
  return errno;
}
#endif

//
// CSocket
//

CSocket::CSocket(const uint8_t nFamily)
  : m_Socket(INVALID_SOCKET),
    m_Family(nFamily),
    m_Port(0),
    m_HasError(false),
    m_HasFin(false),
    m_Error(0)
{
}

CSocket::CSocket(const uint8_t nFamily, SOCKET nSocket)
  : m_Socket(nSocket),
    m_Family(nFamily),
    m_Port(0),
    m_HasError(false),
    m_HasFin(false),
    m_Error(0)
{
}

CSocket::CSocket(const uint8_t nFamily, string nName)
  : m_Socket(INVALID_SOCKET),
    m_Family(nFamily),
    m_Port(0),
    m_HasError(false),
    m_HasFin(false),
    m_Name(nName),
    m_Error(0)
{
}

CSocket::~CSocket()
{
  if (m_Socket != INVALID_SOCKET)
    closesocket(m_Socket);
}

string CSocket::GetName() const
{
  return m_Name;
}

string CSocket::GetErrorString() const
{
  if (!m_HasError)
    return "NO ERROR";

  switch (m_Error)
  {
    case EWOULDBLOCK:
      return "EWOULDBLOCK";
    case EINPROGRESS:
      return "EINPROGRESS";
    case EALREADY:
      return "EALREADY";
    case ENOTSOCK:
      return "ENOTSOCK";
    case EDESTADDRREQ:
      return "EDESTADDRREQ";
    case EMSGSIZE:
      return "EMSGSIZE";
    case EPROTOTYPE:
      return "EPROTOTYPE";
    case ENOPROTOOPT:
      return "ENOPROTOOPT";
    case EPROTONOSUPPORT:
      return "EPROTONOSUPPORT";
    case ESOCKTNOSUPPORT:
      return "ESOCKTNOSUPPORT";
    case EOPNOTSUPP:
      return "EOPNOTSUPP";
    case EPFNOSUPPORT:
      return "EPFNOSUPPORT";
    case EAFNOSUPPORT:
      return "EAFNOSUPPORT";
    case EADDRINUSE:
      return "EADDRINUSE";
    case EADDRNOTAVAIL:
      return "EADDRNOTAVAIL";
    case ENETDOWN:
      return "ENETDOWN";
    case ENETUNREACH:
      return "ENETUNREACH";
    case ENETRESET:
      return "ENETRESET";
    case ECONNABORTED:
      return "ECONNABORTED";
    case ENOBUFS:
      return "ENOBUFS";
    case EISCONN:
      return "EISCONN";
    case ENOTCONN:
      return "ENOTCONN";
    case ESHUTDOWN:
      return "ESHUTDOWN";
    case ETOOMANYREFS:
      return "ETOOMANYREFS";
    case ETIMEDOUT:
      return "ETIMEDOUT";
    case ECONNREFUSED:
      return "ECONNREFUSED";
    case ELOOP:
      return "ELOOP";
    case ENAMETOOLONG:
      return "ENAMETOOLONG";
    case EHOSTDOWN:
      return "EHOSTDOWN";
    case EHOSTUNREACH:
      return "EHOSTUNREACH";
    case ENOTEMPTY:
      return "ENOTEMPTY";
    case EUSERS:
      return "EUSERS";
    case EDQUOT:
      return "EDQUOT";
    case ESTALE:
      return "ESTALE";
    case EREMOTE:
      return "EREMOTE";
    case ECONNRESET:
      return "Connection reset by peer";
  }

  return "UNKNOWN ERROR (" + to_string(m_Error) + ")";
}

void CSocket::SetFD(fd_set* fd, fd_set* send_fd, int* nfds)
{
  if (m_Socket == INVALID_SOCKET)
    return;

  FD_SET(m_Socket, fd);
  FD_SET(m_Socket, send_fd);

#ifndef WIN32
  if (m_Socket > *nfds)
    *nfds = m_Socket;
#endif
}

void CSocket::Allocate(const uint8_t family, int type)
{
  m_Socket = socket(family, type, 0);

  if (m_Socket == INVALID_SOCKET)
  {
    m_HasError = true;
    m_Error    = GetLastError();
    Print("[SOCKET] error (socket) - " + GetErrorString());
    return;
  }
}

void CSocket::Reset()
{
  if (m_Socket != INVALID_SOCKET)
    closesocket(m_Socket);

  m_Socket = INVALID_SOCKET;
  m_HasError = false;
  m_Error    = 0;
}

void CSocket::SendReply(const sockaddr_storage* address, const vector<uint8_t>& message)
{
  UNREFERENCED_PARAMETER(address);
  UNREFERENCED_PARAMETER(message);
}

//
// CStreamIOSocket
//

CStreamIOSocket::CStreamIOSocket(uint8_t nFamily, string nName)
  : CSocket(nFamily, nName),
    m_LastRecv(GetTime()),
    m_Connected(false),
    m_Server(nullptr),
    m_Counter(0)
{
  memset(&m_RemoteHost, 0, sizeof(sockaddr_storage));

  Allocate(nFamily, SOCK_STREAM);

  // make socket non blocking
#ifdef WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif

  // disable Nagle's algorithm

  int32_t OptVal = 1;
  setsockopt(m_Socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
}

CStreamIOSocket::CStreamIOSocket(SOCKET nSocket, sockaddr_storage& nAddress, CTCPServer* nServer, const uint16_t nCounter)
  : CSocket(static_cast<uint8_t>(nAddress.ss_family), nSocket),
    m_LastRecv(GetTime()),
    m_Connected(true),
    m_RemoteHost(move(nAddress)),
    m_Server(nServer),
    m_Counter(nCounter)
{
  // make socket non blocking
#ifdef WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif
}

string CStreamIOSocket::GetName() const
{
  string name = CSocket::GetName();
  if (name.empty() && m_Server != nullptr) {
    return m_Server->GetName() + "-C" + to_string(m_Counter);
  }
  return name;
}

CStreamIOSocket::~CStreamIOSocket()
{
  if (m_Socket != INVALID_SOCKET)
    closesocket(m_Socket);

  m_Server = nullptr;
}

void CStreamIOSocket::Reset()
{
  CSocket::Reset();

  Allocate(m_Family, SOCK_STREAM);

  m_Connected = false;
  m_RecvBuffer.clear();
  m_SendBuffer.clear();
  m_LastRecv = GetTime();

  memset(&m_RemoteHost, 0, sizeof(sockaddr_storage));

// make socket non blocking

#ifdef WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif
}

bool CStreamIOSocket::DoRecv(fd_set* fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError || !m_Connected)
    return false;

  if (!FD_ISSET(m_Socket, fd))
    return false;

  // data is waiting, receive it
  char    buffer[1024];
  int32_t c = recv(m_Socket, buffer, 1024, 0);

  if (c > 0) {
    // success! add the received data to the buffer
    m_RecvBuffer += string(buffer, c);
    m_LastRecv = GetTime();
    return true;
  }

  if (c == SOCKET_ERROR && GetLastError() != EWOULDBLOCK) {
    // receive error
    m_HasError = true;
    m_Error    = GetLastError();
    Print("[TCPSOCKET] (" + GetName() +") error (recv) - " + GetErrorString());
  } else if (c == 0) {
    // the other end closed the connection
    if (!m_HasFin) {
      Print("[TCPSOCKET] (" + GetName() +") terminated the connection");
    }
    m_HasFin = true;
  }
  return false;
}

void CStreamIOSocket::DoSend(fd_set* send_fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError || !m_Connected || m_SendBuffer.empty())
    return;

  if (FD_ISSET(m_Socket, send_fd))
  {
    // socket is ready, send it

    int32_t s = send(m_Socket, m_SendBuffer.c_str(), static_cast<int32_t>(m_SendBuffer.size()), MSG_NOSIGNAL);

    if (s > 0)
    {
      // success! only some of the data may have been sent, remove it from the buffer

      m_SendBuffer = m_SendBuffer.substr(s);
    }
    else if (s == SOCKET_ERROR && GetLastError() != EWOULDBLOCK)
    {
      // send error

      m_HasError = true;
      m_Error    = GetLastError();
      Print("[TCPSOCKET] (" + GetName() +") error (send) - " + GetErrorString());
      return;
    }
  }
}

void CStreamIOSocket::SendReply(const sockaddr_storage* address, const vector<uint8_t>& message)
{
  UNREFERENCED_PARAMETER(address);
  PutBytes(message);
}


void CStreamIOSocket::Disconnect()
{
  if (m_Socket != INVALID_SOCKET)
    shutdown(m_Socket, SHUT_RDWR);

  m_Connected = false;
}

//
// CTCPClient
//

CTCPClient::CTCPClient(uint8_t nFamily, string nName)
  : CStreamIOSocket(nFamily, nName),
    m_Connecting(false)
{
}

CTCPClient::~CTCPClient()
{
}

void CTCPClient::Reset()
{
  CStreamIOSocket::Reset();

  m_Connecting = false;
}

void CTCPClient::Disconnect()
{
  if (m_Socket != INVALID_SOCKET)
    shutdown(m_Socket, SHUT_RDWR);

  m_Connected  = false;
  m_Connecting = false;
}

void CTCPClient::Connect(optional<sockaddr_storage>& localAddress, sockaddr_storage& remoteHost, const uint16_t port)
{
  if (m_Socket == INVALID_SOCKET || m_HasError || m_Connecting || m_Connected)
    return;

  if (localAddress.has_value()) {
    if (localAddress.value().ss_family != AF_INET)
      return;

    struct sockaddr_in LocalSIN;
    memset(&LocalSIN, 0, sizeof(LocalSIN));
    LocalSIN.sin_family = AF_INET;

    sockaddr_in* ipv4 = reinterpret_cast<sockaddr_in*>(&(localAddress.value()));
    LocalSIN.sin_addr = ipv4->sin_addr;
    LocalSIN.sin_port = htons(0);

    if (::bind(m_Socket, reinterpret_cast<struct sockaddr*>(&LocalSIN), sizeof(LocalSIN)) == SOCKET_ERROR) {
      m_HasError = true;
      m_Error    = GetLastError();
      Print("[TCPCLIENT] (" + GetName() +") error (bind) - " + GetErrorString());
      return;
    }
  }

  memcpy(&m_RemoteHost, &remoteHost, sizeof(sockaddr_storage));
  if (port != 0) {
    SetAddressPort(&m_RemoteHost, port);
  }

  // connect
  if (connect(m_Socket, reinterpret_cast<struct sockaddr*>(&m_RemoteHost), sizeof(sockaddr_storage)) == SOCKET_ERROR)
  {
    if (GetLastError() != EINPROGRESS && GetLastError() != EWOULDBLOCK)
    {
      // connect error

      m_HasError = true;
      m_Error    = GetLastError();
      Print("[TCPCLIENT] (" + GetName() +") error (connect) - " + GetErrorString());
      return;
    }
  }

  m_Connecting = true;
}

bool CTCPClient::CheckConnect()
{
  if (m_Socket == INVALID_SOCKET || m_HasError || !m_Connecting)
    return false;

  fd_set fd;
  FD_ZERO(&fd);
  FD_SET(m_Socket, &fd);

  struct timeval tv;
  tv.tv_sec  = 0;
  tv.tv_usec = 0;

// check if the socket is connected

#ifdef WIN32
  if (select(1, nullptr, &fd, nullptr, &tv) == SOCKET_ERROR)
#else
  if (select(m_Socket + 1, nullptr, &fd, nullptr, &tv) == SOCKET_ERROR)
#endif
  {
    m_HasError = true;
    m_Error    = GetLastError();
    return false;
  }

  if (FD_ISSET(m_Socket, &fd))
  {
    m_Connecting = false;
    m_Connected  = true;
    return true;
  }

  return false;
}

//
// CTCPServer
//

CTCPServer::CTCPServer(uint8_t nFamily)
  : CSocket(nFamily),
    m_AcceptCounter(0)
{
  Allocate(m_Family, SOCK_STREAM);

  // make socket non blocking
#ifdef WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif

  // set the socket to reuse the address in case it hasn't been released yet
  int32_t optval = 1;
#ifdef WIN32
  setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(int32_t));
#else
  setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int32_t));
#endif

  // accept IPv4 additionally to IPv6
  if (m_Family == AF_INET6) {
    int32_t OptVal = 0;
    setsockopt(m_Socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
  }

  // disable Nagle's algorithm
  int32_t OptVal = 1;
  setsockopt(m_Socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
}

CTCPServer::~CTCPServer()
{
}

string CTCPServer::GetName() const
{
  string name = CSocket::GetName();
  if (name.empty()) {
    return "TCPServer@" + to_string(m_Port);
  }
  return name;
}

bool CTCPServer::Listen(sockaddr_storage& address, const uint16_t port, bool retry)
{
  if (m_Socket == INVALID_SOCKET) {
    Print("[TCPServer] Socket invalid");
    return false;
  }

  if (m_HasError && !retry) {
    Print("[TCPServer] Failed to listen TCP at port " + to_string(port) + ". Error " + to_string(m_Error));
    return false;
  }

  if (m_HasError) {
    m_HasError = false;
    m_Error = 0;
  }

  ADDRESS_LENGTH_TYPE addressLength = GetAddressLength();
  SetAddressPort(&address, port);

  if (::bind(m_Socket, reinterpret_cast<struct sockaddr*>(&address), addressLength) == SOCKET_ERROR) {
    m_HasError = true;
    m_Error    = GetLastError();
    Print("[TCPSERVER] error (bind) - " + GetErrorString());
    return false;
  }

  // listen, queue length 8

  if (listen(m_Socket, 8) == SOCKET_ERROR) {
    m_HasError = true;
    m_Error    = GetLastError();
    Print("[TCPSERVER] error (listen) - " + GetErrorString());
    return false;
  }

  if (port == 0) {
    if (getsockname(m_Socket, reinterpret_cast<struct sockaddr*>(&address), &addressLength) == -1) {
      m_HasError = true;
      m_Error = GetLastError();
      Print("[TCPSERVER] error (getsockname) - " + GetErrorString());
      return false;
    }
    m_Port = GetAddressPort(address);
  } else {
    m_Port = port;
  }

  if (m_Family == AF_INET6) {
    Print("[TCPSERVER] IPv6 listening on port " + to_string(m_Port) + " (IPv4 too)");
  } else {
    Print("[TCPSERVER] IPv4 listening on port " + to_string(m_Port));
  }
  return true;
}

CStreamIOSocket* CTCPServer::Accept(fd_set* fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return nullptr;

  if (FD_ISSET(m_Socket, fd)) {
    // a connection is waiting, accept it

    sockaddr_storage         address;
    ADDRESS_LENGTH_TYPE      addressLength = GetAddressLength();
    SOCKET                   NewSocket;
    memset(&address, 0, addressLength);

    if ((NewSocket = accept(m_Socket, reinterpret_cast<struct sockaddr*>(&address), &addressLength)) != INVALID_SOCKET) {
      ++m_AcceptCounter;
      return new CStreamIOSocket(NewSocket, address, this, m_AcceptCounter);
    }
  }

  return nullptr;
}

//
// CUDPSocket
//

CUDPSocket::CUDPSocket(uint8_t nFamily)
  : CSocket(nFamily)
{
  Allocate(m_Family, SOCK_DGRAM);

  if (m_Family == AF_INET6) {
    int32_t OptVal = 0;
    setsockopt(m_Socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
  }
}

CUDPSocket::~CUDPSocket()
{
}

bool CUDPSocket::SendTo(const sockaddr_storage* address, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return false;

  if (m_Family == address->ss_family) {
    const string MessageString = string(begin(message), end(message));
    return -1 != sendto(m_Socket, MessageString.c_str(), MessageString.size(), 0, reinterpret_cast<const struct sockaddr*>(address), sizeof(sockaddr_storage));
  }
  if (m_Family == AF_INET && address->ss_family == AF_INET6) {
    Print("Error - Attempt to send UDP6 message from UDP4 socket: " + ByteArrayToDecString(message));
    return false;
  }
  if (m_Family == AF_INET6 && address->ss_family == AF_INET) {
    sockaddr_storage addr6 = IPv4ToIPv6(address);
    const string MessageString = string(begin(message), end(message));
    return -1 != sendto(m_Socket, MessageString.c_str(), MessageString.size(), 0, reinterpret_cast<const struct sockaddr*>(&addr6), sizeof(addr6));
  }
  return false;
}

bool CUDPSocket::SendTo(const string& addressLiteral, uint16_t port, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError) {
    return false;
  }

  optional<sockaddr_storage> address = CNet::ParseAddress(addressLiteral);
  if (!address.has_value()) {
    m_HasError = true;
    // m_Error = h_error;
    Print("[DISCOVERY] error (gethostbyname)");
    return false;
  }
  
  sockaddr_storage* targetAddress = &(address.value());
  SetAddressPort(targetAddress, port);
  return SendTo(targetAddress, message);
}

bool CUDPSocket::Broadcast(const sockaddr_storage* addr4, const sockaddr_storage* addr6, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError) {
    Print("Broadcast critical error");
    return false;
  }
  if (!isIPv4MappedAddress(addr6)) {
    Print("[DEBUG] Broadcast is only allowed to IPv4 addresses");
    return false;
  }
  const in6_addr* _addr6 = &(reinterpret_cast<const sockaddr_in6*>(addr6)->sin6_addr);
  if (!IN6_IS_ADDR_V4MAPPED(_addr6)) {
    Print("[DEBUG] Wrong IN6_IS_ADDR_V4MAPPED macro usage.");
  }

  const string MessageString = string(begin(message), end(message));
  int result;
  if (m_Family == AF_INET6) {
    // FIXME: Dead code path. This probably doesn't work because IPv6 doesn't support broadcast at all.
    result = sendto(m_Socket, MessageString.c_str(), MessageString.size(), 0, reinterpret_cast<const struct sockaddr*>(addr6), sizeof(sockaddr_in6));
  } else {
    result = sendto(m_Socket, MessageString.c_str(), MessageString.size(), 0, reinterpret_cast<const struct sockaddr*>(addr4), sizeof(sockaddr_in));
  }
  if (result == -1) {
    int error = WSAGetLastError();
    Print("[DISCOVERY] failed to broadcast packet to " + AddressToString(*addr4) + ", size " + to_string(MessageString.size()) + " bytes) with error: " + to_string(error));
    return false;
  }

  return true;
}

void CUDPSocket::SetBroadcastEnabled(const bool nEnable)
{
  // Broadcast is only defined over IPv4, but a subset of IPv6 maps to IPv6.

  int32_t OptVal = nEnable;
#ifdef WIN32
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST, (const char*)&OptVal, sizeof(int32_t));
#else
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (const void*)&OptVal, sizeof(int32_t));
#endif
}

void CUDPSocket::SetDontRoute(bool dontRoute)
{
  // whether to let packets ignore routes set by routing table
  // if DONTROUTE is enabled, packets are sent directly to the interface belonging to the target address
  int32_t OptVal = dontRoute;
  setsockopt(m_Socket, SOL_SOCKET, SO_DONTROUTE, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
}

void CUDPSocket::Reset()
{
  CSocket::Reset();
  Allocate(m_Family, SOCK_DGRAM);
}

void CUDPSocket::SendReply(const sockaddr_storage* address, const vector<uint8_t>& message)
{
  SendTo(address, message);
}

CUDPServer::CUDPServer(uint8_t nFamily)
  : CUDPSocket(nFamily)
{
  // make socket non blocking
#ifdef WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif
}

CUDPServer::~CUDPServer()
{
}

string CUDPServer::GetName() const
{
  string name = CSocket::GetName();
  if (name.empty()) {
    return "UDPServer@" + to_string(m_Port);
  }
  return name;
}

bool CUDPServer::Listen(sockaddr_storage& address, const uint16_t port, bool retry)
{
  if (m_Socket == INVALID_SOCKET) {
    Print("[UDPServer] Socket invalid");
    return false;
  }

  if (m_HasError && !retry) {
    Print("[UDPServer] Failed to listen UDP at port " + to_string(port) + ". Error " + to_string(m_Error));
    return false;
  }  

  if (m_HasError) {
    m_HasError = false;
    m_Error = 0;
  }

  ADDRESS_LENGTH_TYPE addressLength = GetAddressLength();
  SetAddressPort(&address, port);

  if (::bind(m_Socket, reinterpret_cast<struct sockaddr*>(&address), addressLength) == SOCKET_ERROR) {
    m_HasError = true;
    m_Error    = GetLastError();
    Print("[DISCOVERY] error (bind) - " + GetErrorString());
    return false;
  }

  if (port == 0) {
    if (getsockname(m_Socket, reinterpret_cast<struct sockaddr*>(&address), &addressLength) == -1) {
      m_HasError = true;
      m_Error = GetLastError();
      Print("[DISCOVERY] error (getsockname) - " + GetErrorString());
      return false;
    }
    m_Port = GetAddressPort(address);
  } else {
    m_Port = port;
  }

  if (m_Family == AF_INET6) {
    Print("[DISCOVERY] Listening IPv4/IPv6 UDP traffic on port " + to_string(m_Port));
  } else {
    Print("[DISCOVERY] Listening IPv4-only UDP traffic on port " + to_string(m_Port));
  }
  return true;
}

UDPPkt* CUDPServer::Accept(fd_set* fd) {
  if (m_Socket == INVALID_SOCKET || m_HasError) {
    Print("Accept() failed socket has error.");
    return nullptr;
  }

  if (!FD_ISSET(m_Socket, fd)) {
    return nullptr;
  }

  char buffer[1024];
  sockaddr_storage* address = new sockaddr_storage(); // It's the responsibility of the caller to delete this.
  ADDRESS_LENGTH_TYPE addressLength = sizeof(sockaddr_storage);

  int bytesRead = recvfrom(m_Socket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(address), &addressLength);
#ifdef WIN32
  if (bytesRead == SOCKET_ERROR) {
    //int error = WSAGetLastError();
#else
  if (bytesRead < 0) {
    //int error = errno;
#endif
    //Print("Error code " + to_string(error) + " receiving data from " + AddressToString(*pkt.sender));
    delete address;
    return nullptr;
  }
  if (bytesRead < MIN_UDP_PACKET_SIZE) {
    delete address;
    return nullptr;
  }

  UDPPkt* pkt = new UDPPkt();
  if (pkt == nullptr)
    return nullptr;

  pkt->socket = this;
  pkt->sender = address;
  pkt->length = bytesRead;
  memcpy(pkt->buf, buffer, bytesRead);
  return pkt;
}

void CUDPServer::Discard(fd_set* fd) {
  if (!FD_ISSET(m_Socket, fd)){
    return;
  }
  
  char buffer[1024];
  recv(m_Socket, buffer, sizeof(buffer), 0);
}
