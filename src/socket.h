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

#ifndef AURA_SOCKET_H_
#define AURA_SOCKET_H_

#include <optional>

#include "util.h"

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#include <errno.h>

#undef EBADF /* override definition in errno.h */
#define EBADF WSAEBADF
#undef EINTR /* override definition in errno.h */
#define EINTR WSAEINTR
#undef EINVAL /* override definition in errno.h */
#define EINVAL WSAEINVAL
#undef EWOULDBLOCK /* override definition in errno.h */
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef EINPROGRESS /* override definition in errno.h */
#define EINPROGRESS WSAEINPROGRESS
#undef EALREADY /* override definition in errno.h */
#define EALREADY WSAEALREADY
#undef ENOTSOCK /* override definition in errno.h */
#define ENOTSOCK WSAENOTSOCK
#undef EDESTADDRREQ /* override definition in errno.h */
#define EDESTADDRREQ WSAEDESTADDRREQ
#undef EMSGSIZE /* override definition in errno.h */
#define EMSGSIZE WSAEMSGSIZE
#undef EPROTOTYPE /* override definition in errno.h */
#define EPROTOTYPE WSAEPROTOTYPE
#undef ENOPROTOOPT /* override definition in errno.h */
#define ENOPROTOOPT WSAENOPROTOOPT
#undef EPROTONOSUPPORT /* override definition in errno.h */
#define EPROTONOSUPPORT WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT
#undef EOPNOTSUPP /* override definition in errno.h */
#define EOPNOTSUPP WSAEOPNOTSUPP
#define EPFNOSUPPORT WSAEPFNOSUPPORT
#undef EAFNOSUPPORT /* override definition in errno.h */
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#undef EADDRINUSE /* override definition in errno.h */
#define EADDRINUSE WSAEADDRINUSE
#undef EADDRNOTAVAIL /* override definition in errno.h */
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#undef ENETDOWN /* override definition in errno.h */
#define ENETDOWN WSAENETDOWN
#undef ENETUNREACH /* override definition in errno.h */
#define ENETUNREACH WSAENETUNREACH
#undef ENETRESET /* override definition in errno.h */
#define ENETRESET WSAENETRESET
#undef ECONNABORTED /* override definition in errno.h */
#define ECONNABORTED WSAECONNABORTED
#undef ECONNRESET /* override definition in errno.h */
#define ECONNRESET WSAECONNRESET
#undef ENOBUFS /* override definition in errno.h */
#define ENOBUFS WSAENOBUFS
#undef EISCONN /* override definition in errno.h */
#define EISCONN WSAEISCONN
#undef ENOTCONN /* override definition in errno.h */
#define ENOTCONN WSAENOTCONN
#define ESHUTDOWN WSAESHUTDOWN
#define ETOOMANYREFS WSAETOOMANYREFS
#undef ETIMEDOUT /* override definition in errno.h */
#define ETIMEDOUT WSAETIMEDOUT
#undef ECONNREFUSED /* override definition in errno.h */
#define ECONNREFUSED WSAECONNREFUSED
#undef ELOOP /* override definition in errno.h */
#define ELOOP WSAELOOP
#ifndef ENAMETOOLONG /* possible previous definition in errno.h */
#define ENAMETOOLONG WSAENAMETOOLONG
#endif
#define EHOSTDOWN WSAEHOSTDOWN
#undef EHOSTUNREACH /* override definition in errno.h */
#define EHOSTUNREACH WSAEHOSTUNREACH
#ifndef ENOTEMPTY /* possible previous definition in errno.h */
#define ENOTEMPTY WSAENOTEMPTY
#endif
#define EPROCLIM WSAEPROCLIM
#define EUSERS WSAEUSERS
#define EDQUOT WSAEDQUOT
#define ESTALE WSAESTALE
#define EREMOTE WSAEREMOTE
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef int32_t SOCKET;

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#define closesocket close

//extern int32_t GetLastError();
#endif

#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifdef WIN32
#define SHUT_RDWR 2
#endif

#ifdef WIN32
#define ADDRESS_LENGTH_TYPE int
#else
#define ADDRESS_LENGTH_TYPE socklen_t
#endif

#define MIN_UDP_PACKET_SIZE 4
#define INET_ADDRSTRLEN_IPV4 16

class CStreamIOSocket;
class CTCPServer;
class CUDPServer;

struct UDPPkt
{
  sockaddr_storage sender;
  int length;
  char buf[1024];
  CUDPServer* socket;
};

inline bool isLoopbackAddress(const sockaddr_storage& addr) {
  if (addr.ss_family == AF_INET) {
    const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(&addr);
    return (addr4->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
  } else if (addr.ss_family == AF_INET6) {
    const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(&addr);
    if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
      const in_addr* addr4 = reinterpret_cast<const in_addr*>(addr6->sin6_addr.s6_addr + 12);
      return (addr4->s_addr == htonl(INADDR_LOOPBACK));
    } else {
      return (memcmp(&addr6->sin6_addr, &in6addr_loopback, sizeof(in6_addr)) == 0);
    }
  }
  return false;
}

inline void SetAddressPort(sockaddr_storage& address, const uint16_t port)
{
  if (address.ss_family == AF_INET6) {
    reinterpret_cast<struct sockaddr_in6*>(&address)->sin6_port = htons(port);
  } else {
    reinterpret_cast<struct sockaddr_in*>(&address)->sin_port = htons(port);
  }
}

inline uint16_t GetAddressPort(sockaddr_storage& address)
{
  if (address.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<struct sockaddr_in6*>(&address)->sin6_port);
  } else {
    return ntohs(reinterpret_cast<struct sockaddr_in*>(&address)->sin_port);
  }
}

//
// CSocket
//

class CSocket
{
public:
  SOCKET             m_Socket;
  uint8_t            m_Family;
  uint16_t           m_Port;
  bool               m_HasError;
  int                m_Error;
  std::string        m_Name;

  CSocket(const uint8_t nFamily);
  CSocket(const uint8_t nFamily, SOCKET nSocket);
  CSocket(const uint8_t nFamily, std::string nName);
  ~CSocket();

  std::string                 GetErrorString() const;
  std::string                 GetName() const;
  inline uint16_t             GetPort() const { return m_Port; }
  inline std::vector<uint8_t> GetPortLE() const { return CreateByteArray(m_Port, false); }
  inline std::vector<uint8_t> GetPortBE() const { return CreateByteArray(m_Port, true); } // Network-byte-order
  inline int32_t              GetError() const { return m_Error; }
  inline bool                 HasError() const { return m_HasError; }

  inline ADDRESS_LENGTH_TYPE  GetAddressLength() const {
    if (m_Family == AF_INET6)
      return sizeof(sockaddr_in6);
    return sizeof(sockaddr_in);
  }

  void SetFD(fd_set* fd, fd_set* send_fd, int32_t* nfds);
  void Reset();
  void Allocate(const uint8_t family, int type);
};

//
// CStreamIOSocket
//

class CStreamIOSocket : public CSocket
{
public:
  std::string               m_RecvBuffer;
  std::string               m_SendBuffer;
  uint32_t                  m_RemoteSocketCounter;
  int64_t                   m_LastRecv;
  bool                      m_Connected;

  sockaddr_storage          m_RemoteHost;
  CTCPServer*               m_Server;
  uint16_t                  m_Counter;

  CStreamIOSocket(uint8_t nFamily, std::string nName);
  CStreamIOSocket(SOCKET nSocket, sockaddr_storage& remoteAddress, CTCPServer* nServer, const uint16_t nCounter);
  ~CStreamIOSocket();

  inline int64_t                    GetLastRecv() const { return m_LastRecv; }
  std::string                       GetName() const;

  inline std::vector<uint8_t>       GetIPv4() const {
    if (m_RemoteHost.ss_family == AF_INET) {
      std::vector<uint8_t> ipBytes(4);
      const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(&m_RemoteHost);
      memcpy(ipBytes.data(), &addr4->sin_addr.s_addr, 4);
      return ipBytes;
    } else if (m_RemoteHost.ss_family == AF_INET6) {
      const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(&m_RemoteHost);
      if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
        std::vector<uint8_t> ipBytes(4);
        const in_addr* addr4 = reinterpret_cast<const in_addr*>(addr6->sin6_addr.s6_addr + 12);
        memcpy(ipBytes.data(), &addr4->s_addr, 4);
        return ipBytes;
      }
    }
    return {0, 0, 0, 0};
  }

  inline std::string                GetIPString() const {
    char ipString[INET6_ADDRSTRLEN];
    if (m_RemoteHost.ss_family == AF_INET) { // IPv4
      const sockaddr_in* addr4 = reinterpret_cast<const sockaddr_in*>(&m_RemoteHost);
      inet_ntop(AF_INET, &(addr4->sin_addr), ipString, INET_ADDRSTRLEN);
    } else if (m_RemoteHost.ss_family == AF_INET6) { // IPv6
      const sockaddr_in6* addr6 = reinterpret_cast<const sockaddr_in6*>(&m_RemoteHost);
      if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
        const in_addr* addr4 = reinterpret_cast<const in_addr*>(addr6->sin6_addr.s6_addr + 12);
        inet_ntop(AF_INET, addr4, ipString, INET_ADDRSTRLEN);
      } else {
        inet_ntop(AF_INET6, &(addr6->sin6_addr), ipString, INET6_ADDRSTRLEN);
      }
    } else {
      return std::string();
    }
    return std::string(ipString);
  }
  inline bool GetIsLoopback() const { return isLoopbackAddress(m_RemoteHost); }

  inline bool                       GetConnected() const { return m_Connected; }
  void Disconnect();

  inline std::string*               GetBytes() { return &m_RecvBuffer; }
  inline void ClearRecvBuffer() { m_RecvBuffer.clear(); }
  inline void SubstrRecvBuffer(uint32_t i) { m_RecvBuffer = m_RecvBuffer.substr(i); }
  void DoRecv(fd_set* fd);

  inline uint32_t                   PutBytes(const std::string& bytes) {
    m_SendBuffer += bytes;
    return bytes.size();
  }
  inline uint32_t                   PutBytes(const std::vector<uint8_t>& bytes) {
    m_SendBuffer += std::string(begin(bytes), end(bytes));
    return bytes.size();
  }
  inline void                           ClearSendBuffer() { m_SendBuffer.clear(); }
  void DoSend(fd_set* send_fd);

  void Reset();
};

//
// CTCPClient
//

class CTCPClient final : public CStreamIOSocket
{
public:
  bool                      m_Connecting;

  CTCPClient(uint8_t nFamily, std::string nName);
  ~CTCPClient();

  inline bool         GetConnecting() const { return m_Connecting; }
  bool                CheckConnect();
  void                Connect(std::optional<sockaddr_storage>& localAddress, sockaddr_storage& remoteHost, const uint16_t port = 0);

  // Overrides
  void                Reset();
  void                Disconnect();
};

//
// CTCPServer
//

class CTCPServer final : public CSocket
{
public:
  uint16_t                        m_AcceptCounter;

  CTCPServer(uint8_t nFamily);
  ~CTCPServer();

  std::string                     GetName() const;
  bool                            Listen(sockaddr_storage& address, const uint16_t port, bool retry);
  CStreamIOSocket*                Accept(fd_set* fd);
};

//
// CUDPSocket
//

class CUDPSocket : public CSocket
{
public:
  struct in_addr              m_BroadcastTarget;

  CUDPSocket(uint8_t nFamily);
  ~CUDPSocket();

  bool                        SendTo(sockaddr_storage& address, const std::vector<uint8_t>& message);
  bool                        SendTo(const std::string& addressLiteral, uint16_t port, const std::vector<uint8_t>& message);
  bool                        Broadcast(uint16_t port, const std::vector<uint8_t>& message);

  void                        Reset();
  void                        EnableBroadcast(); // IPv4-only
  void                        SetBroadcastTarget(sockaddr_storage& subnet); // IPv4-only
  void                        SetDontRoute(bool dontRoute);
};

class CUDPServer final : public CUDPSocket
{
public:
  CUDPServer(uint8_t nFamily);
  ~CUDPServer();

  std::string                 GetName() const;
  bool                        Listen(sockaddr_storage& address, const uint16_t port, bool retry);
  UDPPkt*                     Accept(fd_set* fd);
  void                        Discard(fd_set* fd);
};

#endif // AURA_SOCKET_H_
