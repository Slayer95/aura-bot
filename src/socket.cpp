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

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/
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
    m_Error(0)
{
}

CSocket::CSocket(const uint8_t nFamily, SOCKET nSocket)
  : m_Socket(nSocket),
    m_Family(nFamily),
    m_Port(0),
    m_HasError(false),
    m_Error(0)
{
}

CSocket::CSocket(const uint8_t nFamily, string nName)
  : m_Socket(INVALID_SOCKET),
    m_Family(nFamily),
    m_Port(0),
    m_HasError(false),
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

//
// CStreamIOSocket
//

CStreamIOSocket::CStreamIOSocket(uint8_t nFamily, string nName)
  : CSocket(nFamily, nName),
    m_LastRecv(GetTime()),
    m_Connected(false),
    m_RemoteHost(new sockaddr_storage()),
    m_Server(nullptr),
    m_Counter(0)
{
  memset(m_RemoteHost, 0, sizeof(sockaddr_storage));

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

CStreamIOSocket::CStreamIOSocket(SOCKET nSocket, sockaddr_in& nSIN, CTCPServer* nServer, const uint32_t nCounter)
  : CSocket(AF_INET, nSocket),
    m_LastRecv(GetTime()),
    m_Connected(true),
    m_RemoteHost(new sockaddr_storage()),
    m_Server(nServer),
    m_Counter(nCounter)
{
  memcpy(m_RemoteHost, &nSIN, sizeof(sockaddr_in));
  if (sizeof(m_RemoteHost) > sizeof(sockaddr_in)) {
    memset(reinterpret_cast<char*>(m_RemoteHost) + sizeof(sockaddr_in), 0, sizeof(m_RemoteHost) - sizeof(sockaddr_in));
  }

  // make socket non blocking
#ifdef WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif
}

CStreamIOSocket::CStreamIOSocket(SOCKET nSocket, sockaddr_in6& nSIN, CTCPServer* nServer, const uint32_t nCounter)
  : CSocket(AF_INET6, nSocket),
    m_LastRecv(GetTime()),
    m_Connected(true),
    m_RemoteHost(new sockaddr_storage()),
    m_Server(nServer),
    m_Counter(nCounter)
{
  memcpy(m_RemoteHost, &nSIN, sizeof(sockaddr_in6));
  if (sizeof(m_RemoteHost) > sizeof(sockaddr_in6)) {
    memset(reinterpret_cast<char*>(m_RemoteHost) + sizeof(sockaddr_in6), 0, sizeof(m_RemoteHost) - sizeof(sockaddr_in6));
  }

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
  delete m_RemoteHost;
}

void CStreamIOSocket::Reset()
{
  CSocket::Reset();

  Allocate(m_Family, SOCK_STREAM);

  m_Connected = false;
  m_RecvBuffer.clear();
  m_SendBuffer.clear();
  m_LastRecv = GetTime();
  m_RemoteHost = nullptr;

  memset(m_RemoteHost, 0, sizeof(sockaddr_storage));

// make socket non blocking

#ifdef WIN32
  int32_t iMode = 1;
  ioctlsocket(m_Socket, FIONBIO, (u_long FAR*)&iMode);
#else
  fcntl(m_Socket, F_SETFL, fcntl(m_Socket, F_GETFL) | O_NONBLOCK);
#endif
}

void CStreamIOSocket::DoRecv(fd_set* fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError || !m_Connected)
    return;

  if (FD_ISSET(m_Socket, fd))
  {
    // data is waiting, receive it

    char    buffer[1024];
    int32_t c = recv(m_Socket, buffer, 1024, 0);

    if (c > 0)
    {
      // success! add the received data to the buffer

      m_RecvBuffer += string(buffer, c);
      m_LastRecv = GetTime();
    }
    else if (c == SOCKET_ERROR && GetLastError() != EWOULDBLOCK)
    {
      // receive error

      m_HasError = true;
      m_Error    = GetLastError();
      Print("[TCPSOCKET] (" + GetName() +") error (recv) - " + GetErrorString());
      return;
    }
    else if (c == 0)
    {
      // the other end closed the connection

      Print("[TCPSOCKET] (" + GetName() +") closed by remote host");
      m_Connected = false;
    }
  }
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

  memcpy(m_RemoteHost, &remoteHost, sizeof(sockaddr_in));
  if (port != 0) {
    sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(m_RemoteHost);
    addr4->sin_port = htons(port);
  }

  // connect
  if (connect(m_Socket, reinterpret_cast<struct sockaddr*>(m_RemoteHost), sizeof(sockaddr_in)) == SOCKET_ERROR)
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
    if (m_Family == AF_INET6) {
      return "TCPServer-IPv6@" + to_string(m_Port);
    } else {
      return "TCPServer@" + to_string(m_Port);
    }
  }
  return name;
}

bool CTCPServer::Listen(sockaddr_storage& address, uint16_t port, bool retry)
{
  if (m_Socket == INVALID_SOCKET) {
    Print("Socket invalid");
    return false;
  }
  if (m_HasError && !retry) {
    Print("Has error: " + to_string(m_Error));
    return false;
  }

  if (m_HasError) {
    if (!retry) return false;
    m_HasError = false;
    m_Error = 0;
  }

  struct sockaddr_in addr4;
  addr4.sin_family = AF_INET;
  sockaddr_in* remoteAddress = reinterpret_cast<sockaddr_in*>(&address);
  addr4.sin_addr = remoteAddress->sin_addr;
  addr4.sin_port = htons(port);

  if (::bind(m_Socket, reinterpret_cast<struct sockaddr*>(&addr4), sizeof(addr4)) == SOCKET_ERROR) {
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
    socklen_t addr4_len = sizeof(addr4);
    if (getsockname(m_Socket, (struct sockaddr*)&addr4, &addr4_len) == -1) {
      m_HasError = true;
      m_Error = GetLastError();
      Print("[TCPSERVER] error (getsockname) - " + GetErrorString());
      return false;
    }
    m_Port = ntohs(addr4.sin_port);
  } else {
    m_Port = port;
  }

  Print("[TCPSERVER] listening on port " + to_string(m_Port));
  return true;
}

CStreamIOSocket* CTCPServer::Accept(fd_set* fd)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return nullptr;

  if (FD_ISSET(m_Socket, fd))
  {
    // a connection is waiting, accept it

    struct sockaddr_in Addr;
    int32_t            AddrLen = sizeof(Addr);
    SOCKET             NewSocket;
    memset(&Addr, 0, AddrLen);

#ifdef WIN32
    if ((NewSocket = accept(m_Socket, (struct sockaddr*)&Addr, &AddrLen)) != INVALID_SOCKET)
#else
    if ((NewSocket = accept(m_Socket, reinterpret_cast<struct sockaddr*>(&Addr), reinterpret_cast<socklen_t*>(&AddrLen))) != INVALID_SOCKET)
#endif
    {
      // success! return the new socket

      ++m_AcceptCounter;
      return new CStreamIOSocket(NewSocket, Addr, this, m_AcceptCounter);
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

  // enable broadcast support

  int32_t OptVal = 1;
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));

  // set default broadcast target

  m_BroadcastTarget.s_addr = INADDR_BROADCAST;
}

CUDPSocket::~CUDPSocket()
{
}

string CUDPServer::GetName() const
{
  string name = CSocket::GetName();
  if (name.empty()) {
    if (m_Family == AF_INET6) {
      return "UDPServer-IPv6@" + to_string(m_Port);
    } else {
      return "UDPServer@" + to_string(m_Port);
    }
  }
  return name;
}

bool CUDPSocket::SendTo(struct sockaddr_in sin, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return false;

  const string MessageString = string(begin(message), end(message));

  if (sendto(m_Socket, MessageString.c_str(), MessageString.size(), 0, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) == -1)
    return false;

  return true;
}

bool CUDPSocket::SendTo(const string& address, uint16_t port, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return false;

  // get IP address

  struct hostent* HostInfo;
  uint32_t        HostAddress;
  HostInfo = gethostbyname(address.c_str());

  if (!HostInfo)
  {
    m_HasError = true;
    // m_Error = h_error;
    Print("[UDPSOCKET] error (gethostbyname)");
    return false;
  }

  memcpy(&HostAddress, HostInfo->h_addr, HostInfo->h_length);
  struct sockaddr_in sin;
  sin.sin_family      = AF_INET;
  sin.sin_addr.s_addr = HostAddress;
  sin.sin_port        = htons(port);

  return SendTo(sin, message);
}

bool CUDPSocket::Broadcast(uint16_t port, const vector<uint8_t>& message)
{
  if (m_Socket == INVALID_SOCKET || m_HasError)
    return false;

  struct sockaddr_in sin;
  sin.sin_family      = AF_INET;
  sin.sin_addr.s_addr = m_BroadcastTarget.s_addr;
  sin.sin_port        = htons(port);

  const string MessageString = string(begin(message), end(message));

  int result = sendto(m_Socket, MessageString.c_str(), MessageString.size(), 0, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin));
  if (result == -1) {
    int error = WSAGetLastError();
    Print("[UDPSOCKET] failed to broadcast packet (port " + to_string(port) + ", size " + to_string(MessageString.size()) + " bytes) with error: " + to_string(error));
    return false;
  }

  return true;
}

void CUDPSocket::SetBroadcastTarget(sockaddr_storage& subnet)
{
  sockaddr_in* ipv4BroadcastTarget = reinterpret_cast<sockaddr_in*>(&subnet);
  m_BroadcastTarget.s_addr = ipv4BroadcastTarget->sin_addr.s_addr;

  if (m_BroadcastTarget.s_addr != INADDR_BROADCAST) {
    char ipString[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &m_BroadcastTarget, ipString, INET_ADDRSTRLEN);
    Print("[UDPSOCKET] using broadcast target [" + string(ipString) + "]");
  }
}

void CUDPSocket::SetDontRoute(bool dontRoute)
{
  int32_t OptVal = 0;

  if (dontRoute)
    OptVal = 1;

  // don't route packets; make them ignore routes set by routing table and send them to the interface
  // belonging to the target address directly

  setsockopt(m_Socket, SOL_SOCKET, SO_DONTROUTE, reinterpret_cast<const char*>(&OptVal), sizeof(int32_t));
}

void CUDPSocket::Reset()
{
  CSocket::Reset();
  Allocate(m_Family, SOCK_DGRAM);

  // enable broadcast support
  int32_t OptVal = 1;
#ifdef WIN32
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST, (const char*)&OptVal, sizeof(int32_t));
#else
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (const void*)&OptVal, sizeof(int32_t));
#endif

  // set default broadcast target
  m_BroadcastTarget.s_addr = INADDR_BROADCAST;
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

  // set the socket to reuse the address in case it hasn't been released yet

  int32_t optval = 1;

#ifdef WIN32
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST, (const char*)&optval, sizeof(int32_t));
#else
  setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, (const void*)&optval, sizeof(int32_t));
#endif
}

CUDPServer::~CUDPServer()
{
}

bool CUDPServer::Listen(sockaddr_storage& address, uint16_t port, bool retry)
{
  if (m_Socket == INVALID_SOCKET || m_HasError && !retry) {
    Print("[UDPServer] Failed to listen UDP at port " + to_string(port) + ".");
    return false;
  }
  if (m_HasError) {
    m_HasError = false;
    m_Error = 0;
  }

  struct sockaddr_in addr4;
  addr4.sin_family = AF_INET;
  sockaddr_in* remoteAddress = reinterpret_cast<sockaddr_in*>(&address);
  addr4.sin_addr = remoteAddress->sin_addr;
  addr4.sin_port = htons(port);

  if (::bind(m_Socket, reinterpret_cast<struct sockaddr*>(&addr4), sizeof(addr4)) == SOCKET_ERROR) {
    m_HasError = true;
    m_Error    = GetLastError();
    Print("[UDPSERVER] error (bind) - " + GetErrorString());
    return false;
  }

  if (port == 0) {
    socklen_t addr4_len = sizeof(addr4);
    if (getsockname(m_Socket, (struct sockaddr*)&addr4, &addr4_len) == -1) {
      m_HasError = true;
      m_Error = GetLastError();
      Print("[UDPSERVER] error (getsockname) - " + GetErrorString());
      return false;
    }
    m_Port = ntohs(addr4.sin_port);
  } else {
    m_Port = port;
  }

  Print("[UDPSERVER] Using port " + to_string(m_Port));
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
  int receivedBytes;

  struct UDPPkt pkt;
  pkt.length = 0;
  memset(&(pkt.sender), 0, sizeof(sockaddr_storage));
#ifdef WIN32
  int addressLength = sizeof(sockaddr_storage);
#else
  socklen_t addressLength = sizeof(sockaddr_storage);
#endif

  receivedBytes = recvfrom(m_Socket, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(&(pkt.sender)), &addressLength);
#ifdef WIN32
  if (receivedBytes == SOCKET_ERROR) {
    int error = WSAGetLastError();
#else
  if (receivedBytes < 0) {
    int error = errno;
#endif
    if (pkt.sender.ss_family == AF_INET) {
      struct sockaddr_in* sender_ipv4 = reinterpret_cast<struct sockaddr_in*>(&pkt.sender);
      char ipAddress[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(sender_ipv4->sin_addr), ipAddress, INET_ADDRSTRLEN);
      Print("[UDPServer] Error receiving data from " + string(ipAddress));
    } else if (pkt.sender.ss_family == AF_INET6) {
      struct sockaddr_in6* sender_ipv6 = reinterpret_cast<struct sockaddr_in6*>(&pkt.sender);
      char ipAddress[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &(sender_ipv6->sin6_addr), ipAddress, INET6_ADDRSTRLEN);
      Print("[UDPServer] Error receiving data from " + string(ipAddress));
    }
    return nullptr;
  }
  if (receivedBytes <= MIN_UDP_PACKET_SIZE) {
    return nullptr;
  }

  pkt.length = receivedBytes;
  memcpy(pkt.buf, buffer, receivedBytes);

  // Allocate on the heap and check for allocation failure
  UDPPkt* result = new(std::nothrow) UDPPkt(pkt);
  if (result == nullptr) {
    return nullptr;
  }

  return result;
}

void CUDPServer::Discard(fd_set* fd) {
  if (!FD_ISSET(m_Socket, fd)){
    return;
  }
  
  char buffer[1024];
  recv(m_Socket, buffer, sizeof(buffer), 0);
}
