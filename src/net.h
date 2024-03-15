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

#ifndef AURA_NET_H_
#define AURA_NET_H_

#include "socket.h"
#include "gameplayer.h"
#include "config_net.h"
#include "aura.h"
#include "command.h"

#include <map>
#include <set>
#include <optional>
#include <utility>
#include <tuple>

#pragma once

#define HEALTH_STANDBY 0
#define HEALTH_PROGRESS 1
#define HEALTH_OKAY 2
#define HEALTH_ERROR 3

#define CONNECTION_TYPE_DEFAULT 0
#define CONNECTION_TYPE_LOOPBACK 1 << 0 
#define CONNECTION_TYPE_CUSTOM_PORT 1 << 1
#define CONNECTION_TYPE_CUSTOM_IP_ADDRESS 1 << 2
#define CONNECTION_TYPE_VPN 1 << 3
#define CONNECTION_TYPE_IPV6 1 << 4

#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE 0
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL 1
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_API 2
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID 3

#define ACCEPT_IPV4 1 << 0
#define ACCEPT_IPV6 1 << 1
#define ACCEPT_ANY 3

//
// CNet
//

class CAura;
class CCommandContext;
class CUDPServer;
class CUDPSocket;
class CStreamIOSocket;
class CGameConnection;
class CNetConfig;

struct sockaddr_storage;
struct UDPPkt;

class CTestConnection
{
public:
  CTestConnection(CAura* nAura, sockaddr_storage nTargetHost, const uint8_t nType, const std::string nName);
  ~CTestConnection();

  uint32_t  SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool      Update(void* fd, void* send_fd);
  bool      QueryGameInfo();
  uint16_t  GetPort();

  CAura*                      m_Aura;
  sockaddr_storage            m_TargetHost;
  uint8_t                     m_Type;
  std::string                 m_Name;
  CTCPClient*                 m_Socket;
  std::optional<bool>         m_Passed;
  std::optional<bool>         m_CanConnect;
  int64_t                     m_Timeout;
  int64_t                     m_LastConnectionFailure;
  bool                        m_SentJoinRequest;
};

class CNet
{
public:
  CNet(CAura* nAura);
  ~CNet();

  CAura*                                                      m_Aura;
  CNetConfig*                                                 m_Config;

  // == SECTION START ==
  // Implements non-reloadable config entries.
  bool                                                        m_SupportUDPOverIPv6;
  bool                                                        m_SupportTCPOverIPv6;
  bool                                                        m_UDPMainServerEnabled;      // (IPv4) whether the bot should listen to UDP traffic in port 6112)
  uint16_t                                                    m_UDPFallbackPort;
  uint16_t                                                    m_UDPIPv6Port;
  // == SECTION END ==

  CUDPServer*                                                 m_UDPMainServer;             // (IPv4) UDP I/O at port 6112. Supports broadcasts. May also act as reverse-proxy for UDP traffic.
  CUDPServer*                                                 m_UDPDeafSocket;             // (IPv4) UDP outbound traffic. Uses <net.udp_fallback.outbound_port> (should NOT be 6112). Supports broadcasts.
  CUDPServer*                                                 m_UDPIPv6Server;

  uint16_t                                                    m_UDP4TargetPort;
  uint16_t                                                    m_UDP6TargetPort;
  sockaddr_storage*                                           m_UDP4BroadcastTarget;
  sockaddr_storage*                                           m_UDP6BroadcastTarget;

  std::map<uint16_t, CTCPServer*>                             m_GameServers;
  std::map<uint16_t, std::vector<CGameConnection*>>           m_IncomingConnections;        // (connections that haven't sent a W3GS_REQJOIN packet yet)
  std::map<std::string, sockaddr_storage>                     m_DNSCache;
  std::pair<std::string, sockaddr_storage*>                   m_IPv4CacheV;
  uint8_t                                                     m_IPv4CacheT;
  std::pair<std::string, sockaddr_storage*>                   m_IPv6CacheV;
  uint8_t                                                     m_IPv6CacheT;

  std::vector<CTestConnection*>                               m_HealthCheckClients;
  bool                                                        m_HealthCheckInProgress;
  CCommandContext*                                            m_HealthCheckContext;
  uint16_t                                                    m_LastHostPort;               // the port of the last hosted game

  void InitPersistentConfig();
  bool Init();
  uint32_t SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool Update(void* fd, void* send_fd);
  bool SendBroadcast(const std::vector<uint8_t>& packet);
  void Send(const sockaddr_storage* address, const std::vector<uint8_t>& packet);
  void Send(const std::string& addressLiteral, const std::vector<uint8_t>& packet);
  void Send(const std::string& addressLiteral, const uint16_t port, const std::vector<uint8_t>& packet);
  void SendArbitraryUnicast(const std::string& addressLiteral, const uint16_t port, const std::vector<uint8_t>& packet);
  void SendGameDiscovery(const std::vector<uint8_t>& packet, const std::set<std::string>& clientIps);
  void HandleUDP(UDPPkt* pkt);

  sockaddr_storage*               GetPublicIPv4();
  sockaddr_storage*               GetPublicIPv6();
  
  std::vector<uint16_t>           GetPotentialGamePorts() const;
  uint16_t                        GetUDPPort(const uint8_t protocol) const;

  std::optional<sockaddr_storage> ResolveHostName(const std::string& hostName);
  void             FlushDNSCache();
  void             FlushSelfIPCache();

  uint8_t EnableUPnP(const uint16_t externalPort, const uint16_t internalPort);
  bool StartHealthCheck(const std::vector<std::tuple<std::string, uint8_t, sockaddr_storage>> testServers, CCommandContext* nCtx);
  void ResetHealthCheck();
  void ReportHealthCheck();

  uint16_t NextHostPort();

  static std::optional<sockaddr_storage> ParseAddress(const std::string& address, const uint8_t inputMode = ACCEPT_ANY);
  void                                   SetBroadcastTarget(sockaddr_storage& subnet);
  void                                   PropagateBroadcastEnabled(const bool nEnable);
  void                                   PropagateDoNotRouteEnabled(const bool nEnable);
  void                                   OnConfigReload();

  bool                                   IsIgnoredDatagramSource(std::string sourceIp);
};

#endif // AURA_NET_H_
