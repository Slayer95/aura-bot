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

#include "includes.h"
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
#define CONNECTION_TYPE_LOOPBACK (1 << 0)
#define CONNECTION_TYPE_CUSTOM_PORT (1 << 1)
#define CONNECTION_TYPE_CUSTOM_IP_ADDRESS (1 << 2)
#define CONNECTION_TYPE_VPN (1 << 3)
#define CONNECTION_TYPE_IPV6 (1 << 4)

#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE 0
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL 1
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_API 2
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID 3

#define ACCEPT_IPV4 (1 << 0)
#define ACCEPT_IPV6 (1 << 1)
#define ACCEPT_ANY (ACCEPT_IPV4 | ACCEPT_IPV6)

#define HEALTH_CHECK_PUBLIC_IPV4 (1 << 0)
#define HEALTH_CHECK_PUBLIC_IPV6 (1 << 1)
#define HEALTH_CHECK_LOOPBACK_IPV4 (1 << 2)
#define HEALTH_CHECK_LOOPBACK_IPV6 (1 << 3)
#define HEALTH_CHECK_REALM (1 << 4)
#define HEALTH_CHECK_ALL (HEALTH_CHECK_PUBLIC_IPV4 | HEALTH_CHECK_PUBLIC_IPV6 | HEALTH_CHECK_LOOPBACK_IPV4 | HEALTH_CHECK_LOOPBACK_IPV6 | HEALTH_CHECK_REALM)
#define HEALTH_CHECK_VERBOSE (1 << 5)

#define GAME_TEST_TIMEOUT 3000
#define IP_ADDRESS_API_TIMEOUT 3000

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

class CGameTestConnection
{
public:
  CGameTestConnection(CAura* nAura, CRealm* nRealm, sockaddr_storage nTargetHost, const uint8_t nType, const std::string nName);
  ~CGameTestConnection();

  uint32_t  SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool      Update(void* fd, void* send_fd);
  bool      QueryGameInfo();
  bool      GetIsRealmOnline();
  uint16_t  GetPort();

  sockaddr_storage            m_TargetHost;
  CAura*                      m_Aura;
  uint32_t                    m_RealmInternalId;
  CTCPClient*                 m_Socket;
  uint8_t                     m_Type;
  std::string                 m_Name;
  std::optional<bool>         m_Passed;
  std::optional<bool>         m_CanConnect;
  int64_t                     m_Timeout;
  int64_t                     m_LastConnectionFailure;
  bool                        m_SentJoinRequest;
};

class CIPAddressAPIConnection
{
public:
  CIPAddressAPIConnection(CAura* nAura, const sockaddr_storage& nTargetHost, const std::string& nEndPoint, const std::string& nHostName);
  ~CIPAddressAPIConnection();

  uint32_t  SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool      Update(void* fd, void* send_fd);
  bool QueryIPAddress();

  sockaddr_storage                  m_TargetHost;
  CAura*                            m_Aura;
  CTCPClient*                       m_Socket;
  std::string                       m_EndPoint;
  std::string                       m_HostName;
  std::optional<sockaddr_storage>   m_Result;
  std::optional<bool>               m_CanConnect;
  int64_t                           m_Timeout;
  int64_t                           m_LastConnectionFailure;
  bool                              m_SentQuery;
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
  std::map<std::string, sockaddr_storage*>                    m_IPv4DNSCache;
  std::map<std::string, sockaddr_storage*>                    m_IPv6DNSCache;
  std::pair<std::string, sockaddr_storage*>                   m_IPv4SelfCacheV;
  uint8_t                                                     m_IPv4SelfCacheT;
  std::pair<std::string, sockaddr_storage*>                   m_IPv6SelfCacheV;
  uint8_t                                                     m_IPv6SelfCacheT;

  std::vector<CGameTestConnection*>                           m_HealthCheckClients;
  std::vector<CIPAddressAPIConnection*>                       m_IPAddressFetchClients;
  bool                                                        m_HealthCheckVerbose;
  bool                                                        m_HealthCheckInProgress;
  CCommandContext*                                            m_HealthCheckContext;
  bool                                                        m_IPAddressFetchInProgress;
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

  bool                            ResolveHostName(sockaddr_storage& address, const uint8_t nAcceptFamily, const std::string& hostName, const uint16_t port);
  bool                            ResolveHostNameInner(sockaddr_storage& address, const std::string& hostName, const uint16_t port, const uint8_t nFamily, std::map<std::string, sockaddr_storage*>&);
  void                            FlushDNSCache();
  void                            FlushSelfIPCache();

#ifndef DISABLE_MINIUPNP
  uint8_t RequestUPnP(const std::string& protocol, const uint16_t externalPort, const uint16_t internalPort, const uint8_t logLevel = LOG_LEVEL_INFO);
#endif
  bool QueryHealthCheck(CCommandContext* ctx, const uint8_t checkMode, CRealm* realm, const uint16_t gamePort);
  void ResetHealthCheck();
  void ReportHealthCheck();

  bool QueryIPAddress();
  void ResetIPAddressFetch();
  void HandleIPAddressFetchDone();

  uint16_t NextHostPort();

  static std::optional<std::tuple<std::string, std::string, uint16_t, std::string>> ParseURL(const std::string& address);
  static std::optional<sockaddr_storage> ParseAddress(const std::string& address, const uint8_t inputMode = ACCEPT_ANY);
  void                                   SetBroadcastTarget(sockaddr_storage& subnet);
  void                                   PropagateBroadcastEnabled(const bool nEnable);
  void                                   PropagateDoNotRouteEnabled(const bool nEnable);
  void                                   OnConfigReload();

  bool                                   IsIgnoredDatagramSource(std::string sourceIp);
  bool                                   GetIsFetchingIPAddresses() const { return m_IPAddressFetchInProgress; }
};

#endif // AURA_NET_H_
