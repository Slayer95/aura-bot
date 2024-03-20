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

#include <thread>
#include <string>
#include <utility>
#ifndef DISABLE_CPR
#include <cpr/cpr.h>
#endif
#ifndef DISABLE_MINIUPNP
#include <miniupnpc.h>
#include <upnpcommands.h>
#endif

#include "aura.h"
#include "net.h"
#include "gameprotocol.h"

using namespace std;

//
// CGameTestConnection
//

CGameTestConnection::CGameTestConnection(CAura* nAura, CRealm* nRealm, sockaddr_storage nTargetHost, const uint8_t nType, const string nName)
  : m_TargetHost(nTargetHost),
    m_Aura(nAura),
    m_RealmInternalId(nRealm ? nRealm->GetInternalID() : 0),
    m_Socket(new CTCPClient(static_cast<uint8_t>(nTargetHost.ss_family), nName)),
    m_Type(nType),
    m_Name(nName),
    m_Timeout(0),
    m_LastConnectionFailure(0),
    m_SentJoinRequest(false)
{
}

CGameTestConnection::~CGameTestConnection()
{
  delete m_Socket;
}

uint32_t CGameTestConnection::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  if (!m_Socket->HasError() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 1;
  }

  return 0;
}

bool CGameTestConnection::GetIsRealmOnline()
{
  if (m_RealmInternalId < 0x10) return true;
  string realmId = m_Aura->m_RealmsIdentifiers[m_RealmInternalId];
  CRealm* realm = m_Aura->GetRealmByInputId(realmId);
  if (realm == nullptr) return false;
  return realm->GetLoggedIn();
}

uint16_t CGameTestConnection::GetPort()
{
  if (m_TargetHost.ss_family == AF_INET6) {
    sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(&m_TargetHost);
    return ntohs(addr6->sin6_port);
  } else if (m_TargetHost.ss_family == AF_INET) {
    sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(&m_TargetHost);
    return ntohs(addr4->sin_port);
  } else {
    return 0;
  }
}

bool CGameTestConnection::QueryGameInfo()
{
  if (m_Socket->HasError() || !m_Socket->GetConnected()) {
    return false;
  }

  if (!m_Aura->m_CurrentLobby || (!m_Aura->m_CurrentLobby->GetIsLobby() && !m_Aura->m_CurrentLobby->GetIsMirror())) {
    return false;
  }

  const static string Name = "AuraBot";
  const vector<uint8_t> joinRequest = m_Aura->m_GameProtocol->SEND_W3GS_REQJOIN(
    m_Aura->m_CurrentLobby->GetIsMirror() ? m_Aura->m_CurrentLobby->GetHostCounter() : (m_Aura->m_CurrentLobby->GetHostCounter() | (0x01 << 24)),
    m_Aura->m_CurrentLobby->GetEntryKey(),
    Name
  );
  m_Socket->PutBytes(joinRequest);
  m_SentJoinRequest = true;
  return true;
}

bool CGameTestConnection::Update(void* fd, void* send_fd)
{
  static optional<sockaddr_storage> emptyBindAddress;

  if (m_Passed.has_value()) {
    return false;
  }

  const int64_t Ticks = GetTicks();
  if (m_Socket->HasError()) {
    if (!m_CanConnect.has_value()) m_CanConnect = false;
    m_LastConnectionFailure = Ticks;
    m_Socket->Reset();
  } else if (m_Socket->GetConnected() && Ticks < m_Timeout) {
    bool gotJoinedMessage = false;
    if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
      string* RecvBuffer = m_Socket->GetBytes();
      std::vector<uint8_t> Bytes = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
      gotJoinedMessage = Bytes.size() >= 2 && Bytes[0] == W3GS_HEADER_CONSTANT && Bytes[1] == CGameProtocol::W3GS_SLOTINFOJOIN;
      m_Passed = true;
    }
    if (!m_SentJoinRequest) {
      if (QueryGameInfo()) {
        m_Socket->DoSend(static_cast<fd_set*>(send_fd));
      }
    } else if (gotJoinedMessage) {
      m_Socket->Reset();
      m_Socket->Disconnect();
    }
  } else if (m_Socket->GetConnecting() && m_Socket->CheckConnect()) {
    m_CanConnect = true;
  } else if (m_Timeout <= Ticks && (m_Socket->GetConnecting() || m_CanConnect.has_value())) {
    if (!m_CanConnect.has_value()) m_CanConnect = false;
    m_Passed = false;
    m_LastConnectionFailure = Ticks;
    m_Socket->Reset();
    m_Socket->Disconnect();
  } else if (!m_Socket->GetConnecting() && !m_CanConnect.has_value() && (Ticks - m_LastConnectionFailure > 900)) {
    m_Socket->Connect(emptyBindAddress, m_TargetHost);
    m_Timeout = Ticks + GAME_TEST_TIMEOUT;
  }

  return !m_Passed.has_value();
}

//
// CIPAddressAPIConnection
//

CIPAddressAPIConnection::CIPAddressAPIConnection(CAura* nAura, sockaddr_storage nTargetHost, const string nEndPoint, const string nHostName)
  : m_TargetHost(nTargetHost),
    m_Aura(nAura),
    m_Socket(new CTCPClient(static_cast<uint8_t>(nTargetHost.ss_family), nTargetHost.ss_family == AF_INET6 ? "IPv6 Address" : "IPv4 Address")),
    m_EndPoint(nEndPoint),
    m_HostName(nHostName),
    m_Timeout(0),
    m_LastConnectionFailure(0),
    m_SentQuery(false)
{
}

CIPAddressAPIConnection::~CIPAddressAPIConnection()
{
  delete m_Socket;
}

uint32_t CIPAddressAPIConnection::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  if (!m_Socket->HasError() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 1;
  }

  return 0;
}

bool CIPAddressAPIConnection::QueryIPAddress()
{
  if (m_Socket->HasError() || !m_Socket->GetConnected()) {
    return false;
  }

  const vector<uint8_t> method = {0x47, 0x45, 0x54, 0x20}; // GET
  const vector<uint8_t> httpVersion = {0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0xd, 0xa}; // HTTP/1.1\n\r
  const vector<uint8_t> hostHeader = {0x48, 0x4f, 0x53, 0x54, 0x3a, 0x20}; // HOST: 
  const vector<uint8_t> end = {0xd, 0xa, 0xd, 0xa}; // \n\r\n\r

  vector<uint8_t> query;
  AppendByteArrayFast(query, method);
  AppendByteArray(query, m_EndPoint, false);
  AppendByteArrayFast(query, httpVersion);
  AppendByteArrayFast(query, hostHeader);
  AppendByteArray(query, m_HostName, false);
  AppendByteArrayFast(query, end);
  m_Socket->PutBytes(query);
  m_SentQuery = true;
  return true;
}

bool CIPAddressAPIConnection::Update(void* fd, void* send_fd)
{
  static optional<sockaddr_storage> emptyBindAddress;

  if (m_Result.has_value()) {
    return false;
  }

  const int64_t Ticks = GetTicks();
  if (m_Socket->HasError()) {
    if (!m_CanConnect.has_value()) m_CanConnect = false;
    m_LastConnectionFailure = Ticks;
    m_Socket->Reset();
  } else if (m_Socket->GetConnected() && Ticks < m_Timeout) {
    bool gotAddress = false;
    if (m_Socket->DoRecv(static_cast<fd_set*>(fd))) {
      string* RecvBuffer = m_Socket->GetBytes();
      std::vector<uint8_t> Bytes = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
      uint16_t size = static_cast<uint16_t>(Bytes.size());
      const bool is200 = size >= 15 && Bytes[9] == 0x32 && Bytes[10] == 0x30 && Bytes[11] == 0x30;
      if (is200) {
        uint16_t endIndex = size;
        if (Bytes[endIndex - 1] == 0xa) endIndex = Bytes[endIndex - 2] == 0xd ? endIndex - 2 : endIndex - 1; // Ignore EOF
        uint16_t responseStart = endIndex;
        uint16_t index = endIndex;
        while (index--) {
          if (Bytes[index] == 0xa) {
            responseStart = index + 1;
            break;
          }
        }
        if (endIndex > responseStart) {
          string responseBody = string(Bytes.begin() + responseStart, Bytes.begin() + endIndex);
          optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(responseBody, m_TargetHost.ss_family == AF_INET6 ? ACCEPT_IPV6 : ACCEPT_IPV4);
          if (maybeAddress.has_value()) {
            m_Result = move(maybeAddress);
            gotAddress = true;
          }
        }
      }
    }
    if (!m_SentQuery) {
      if (QueryIPAddress()) {
        m_Socket->DoSend(static_cast<fd_set*>(send_fd));
      }
    } else if (gotAddress) {
      m_Socket->Reset();
      m_Socket->Disconnect();
    }
  } else if (m_Socket->GetConnecting() && m_Socket->CheckConnect()) {
    m_CanConnect = true;
  } else if (m_Timeout <= Ticks && (m_Socket->GetConnecting() || m_CanConnect.has_value())) {
    if (!m_CanConnect.has_value()) m_CanConnect = false;
    m_LastConnectionFailure = Ticks;
    m_Socket->Reset();
    m_Socket->Disconnect();
  } else if (!m_Socket->GetConnecting() && !m_CanConnect.has_value() && (Ticks - m_LastConnectionFailure > 900)) {
    m_Socket->Connect(emptyBindAddress, m_TargetHost);
    m_Timeout = Ticks + IP_ADDRESS_API_TIMEOUT;
  }

  return !m_Result.has_value();
}

//
// CNet
//

CNet::CNet(CAura* nAura)
  : m_Aura(nAura),
    m_Config(nullptr),
    m_SupportUDPOverIPv6(false),
    m_UDPMainServerEnabled(false),
    m_UDPMainServer(nullptr),
    m_UDPDeafSocket(nullptr),
    m_UDPIPv6Server(nullptr),
    m_UDP4TargetPort(6112), // Constant
    m_UDP6TargetPort(5678), // Only unicast. <net.game_discovery.udp.ipv6.target_port>
    m_UDP4BroadcastTarget(new sockaddr_storage()),
    m_UDP6BroadcastTarget(new sockaddr_storage()),

    m_IPv4CacheV(make_pair(string(), nullptr)),
    m_IPv4CacheT(NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID),
    m_IPv6CacheV(make_pair(string(), nullptr)),
    m_IPv6CacheT(NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID),

    m_HealthCheckVerbose(false),
    m_HealthCheckInProgress(false),
    m_HealthCheckContext(nullptr),

    m_IPAddressFetchInProgress(false),

    m_LastHostPort(0)
{
}

void CNet::InitPersistentConfig()
{
  // Implements non-reloadable config entries.
  m_UDPMainServerEnabled = m_Config->m_UDPMainServerEnabled;
  m_SupportTCPOverIPv6 = m_Config->m_SupportTCPOverIPv6;
  m_SupportUDPOverIPv6 = m_Config->m_SupportUDPOverIPv6;
  m_UDPFallbackPort = m_Config->m_UDPFallbackPort;
  m_UDPIPv6Port = m_Config->m_UDPIPv6Port;
}

bool CNet::Init()
{
  InitPersistentConfig();

  // Main server is the only one that can send W3GS_REFRESHGAME packets. 
  // If disabled, UDP communication is one-way, and can only be through W3GS_GAMEINFO.

  if (m_UDPMainServerEnabled) {
    m_UDPMainServer = new CUDPServer(AF_INET);
    if (!m_UDPMainServer->Listen(m_Config->m_BindAddress4, 6112, false)) {
      Print("====================================================================");
      Print("[UDP] <net.udp_server.enabled = yes> requires active instances of Warcraft to be closed...");
      Print("[UDP] Please release port 6112, or set <net.udp_server.enabled = no>");
      Print("====================================================================");
      Print("[UDP] Waiting...");
      this_thread::sleep_for(chrono::milliseconds(5000));
      if (!m_UDPMainServer->Listen(m_Config->m_BindAddress4, 6112, true)) {
        if (m_Config->m_UDPBroadcastEnabled) {
          Print("[UDP] Failed to start UDP/IPv4 service on port 6112. Cannot start broadcast service.");
        } else {
          Print("[UDP] Failed to start UDP/IPv4 service on port 6112");
        }
        Print("====================================================================");
        return false;
      }
    }
#ifndef DISABLE_MINIUPNP
    if (m_Config->m_EnableUPnP) {
      RequestUPnP("UDP", 6112, 6112, LOG_LEVEL_DEBUG);
    }
#endif
  }

  if (!m_UDPMainServerEnabled) {
    m_UDPDeafSocket = new CUDPServer(AF_INET);
    if (!m_UDPDeafSocket->Listen(m_Config->m_BindAddress4, m_UDPFallbackPort, true)) {
      Print("====================================================================");
      Print("[UDP] Failed to bind to fallback port " + to_string(m_UDPFallbackPort) + ".");
      Print("[UDP] For a random available port, set <net.udp_fallback.outbound_port = 0>");
      if (m_Config->m_UDPBroadcastEnabled) {
        Print("[UDP] Failed to start UDP/IPv4 service. Cannot start broadcast service.");
      } else {
        Print("[UDP] Failed to start UDP/IPv4 service");
      }
      Print("====================================================================");
      return false;
    }
  }

  if (m_SupportUDPOverIPv6) {
    m_UDPIPv6Server = new CUDPServer(AF_INET6);
    if (!m_UDPIPv6Server->Listen(m_Config->m_BindAddress6, m_UDPIPv6Port, true)) {
      Print("====================================================================");
      Print("[UDP] Failed to bind to port " + to_string(m_UDPIPv6Port) + ".");
      Print("[UDP] For a random available port, set <net.udp_ipv6.port = 0>");
      Print("[UDP] Failed to start UDP/IPv6 service");
      Print("====================================================================");
      return false;
    }
  } else {
    for (const auto& clientIp : m_Aura->m_GameDefaultConfig->m_ExtraDiscoveryAddresses) {
      optional<sockaddr_storage> maybeAddress = ParseAddress(clientIp, ACCEPT_IPV6);
      if (maybeAddress.has_value()) {
        Print("[CONFIG] Address " + clientIp + " at <net.game_discovery.udp.extra_clients.ip_addresses> cannot receive game discovery messages, because IPv6 support hasn't been enabled");
        Print("[CONFIG] Set <net.ipv6.tcp.enabled = yes>, and <net.udp_ipv6.enabled = yes> if you want to enable it.");
      }
    }
  }

  m_UDP6TargetPort = m_Config->m_UDP6TargetPort;
  if (m_Config->m_UDPBroadcastEnabled) PropagateBroadcastEnabled(true);
  if (m_Config->m_UDPDoNotRouteEnabled) PropagateDoNotRouteEnabled(true);
  SetBroadcastTarget(m_Config->m_UDPBroadcastTarget);

#ifdef DISABLE_CPR
  QueryIPAddress();
#endif

  return true;
}

uint32_t CNet::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  uint32_t NumFDs = 0;

  for (auto& connection : m_HealthCheckClients)
    NumFDs += connection->SetFD(fd, send_fd, nfds);

  for (auto& connection : m_IPAddressFetchClients)
    NumFDs += connection->SetFD(fd, send_fd, nfds);

  if (m_UDPMainServerEnabled) {
    m_UDPMainServer->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    ++NumFDs;
  } else if (m_UDPDeafSocket) {
    m_UDPDeafSocket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    ++NumFDs;
  }

  return NumFDs;
}

bool CNet::Update(void* fd, void* send_fd)
{
  if (m_HealthCheckInProgress) {
    bool anyPending = false;
    for (auto& testConnection : m_HealthCheckClients) {
      if (testConnection->Update(fd, send_fd)) {
        anyPending = true;
      }
    }
    if (!anyPending) {
      ReportHealthCheck();
    }
  }
  if (m_IPAddressFetchInProgress) {
    bool anyPending = false;
    for (auto& apiConnection : m_IPAddressFetchClients) {
      if (apiConnection->Update(fd, send_fd)) {
        anyPending = true;
      }
    }
    if (!anyPending) {
      HandleIPAddressFetchDone();
    }
  }

  if (m_UDPMainServerEnabled) {
    UDPPkt* pkt = m_UDPMainServer->Accept(static_cast<fd_set*>(fd));
    if (pkt != nullptr) {
      HandleUDP(pkt);
      delete pkt->sender;
      delete pkt;
    }
  } else if (m_UDPDeafSocket) {
    m_UDPDeafSocket->Discard(static_cast<fd_set*>(fd));
  }

  return false;
}

void CNet::SetBroadcastTarget(sockaddr_storage& subnet)
{
  if (subnet.ss_family != AF_INET) {
    Print("Must use IPv4 address for broadcast target");
    return;
  }
  SetAddressPort(&subnet, m_UDP4TargetPort);
  memcpy(m_UDP4BroadcastTarget, &subnet, sizeof(subnet));

  if (reinterpret_cast<sockaddr_in*>(&subnet)->sin_addr.s_addr != htonl(INADDR_BROADCAST))
    Print("[UDPSOCKET] using broadcast target [" + AddressToString(subnet) + "]");

  sockaddr_storage addr6 = IPv4ToIPv6(&subnet);
  memcpy(m_UDP6BroadcastTarget, &addr6, sizeof(addr6));
}

bool CNet::SendBroadcast(const vector<uint8_t>& packet)
{
  if (!m_Config->m_UDPBroadcastEnabled)
    return false;

  if (m_UDPMainServerEnabled) {
    if (m_UDPMainServer->Broadcast(m_UDP4BroadcastTarget, m_UDP6BroadcastTarget, packet)) {
      return true;
    }
  } else {
    if (m_UDPDeafSocket->Broadcast(m_UDP4BroadcastTarget, m_UDP6BroadcastTarget, packet))
      return true;
  }

  return false;
}

void CNet::Send(const sockaddr_storage* address, const vector<uint8_t>& packet)
{
  if (address->ss_family == AF_INET6 && !m_SupportUDPOverIPv6) {
    Print("[CONFIG] Game discovery message to " + AddressToStringStrict(*address) + " cannot be sent, because IPv6 support hasn't been enabled");
    Print("[CONFIG] Set <net.udp_ipv6.enabled = yes> if you want to enable it.");
    return;
  }

  if (address->ss_family == AF_INET6) {
    m_UDPIPv6Server->SendTo(address, packet);
  } else {
    if (m_UDPMainServerEnabled) {
      m_UDPMainServer->SendTo(address, packet);
    } else {
      m_UDPDeafSocket->SendTo(address, packet);
    }
  }
}

void CNet::Send(const string& addressLiteral, const vector<uint8_t>& packet)
{
  optional<sockaddr_storage> maybeAddress = ParseAddress(addressLiteral);
  if (!maybeAddress.has_value())
    return;

  sockaddr_storage* address = &(maybeAddress.value());
  SetAddressPort(address, address->ss_family == AF_INET6 ? m_UDP6TargetPort : m_UDP4TargetPort);
  Send(address, packet);
}

void CNet::Send(const string& addressLiteral, const uint16_t port, const vector<uint8_t>& packet)
{
  optional<sockaddr_storage> maybeAddress = ParseAddress(addressLiteral);
  if (!maybeAddress.has_value())
    return;

  sockaddr_storage* address = &(maybeAddress.value());
  SetAddressPort(address, port);
  Send(address, packet);
}

void CNet::SendArbitraryUnicast(const string& addressLiteral, const uint16_t port, const vector<uint8_t>& packet)
{
  optional<sockaddr_storage> maybeAddress = ParseAddress(addressLiteral);
  if (!maybeAddress.has_value())
    return;

  sockaddr_storage* address = &(maybeAddress.value());
  SetAddressPort(address, port);

  if (m_Config->m_UDPBroadcastEnabled)
    PropagateBroadcastEnabled(false);

  Send(address, packet);

  if (m_Config->m_UDPBroadcastEnabled)
    PropagateBroadcastEnabled(true);
}

void CNet::SendGameDiscovery(const vector<uint8_t>& packet, const set<string>& clientIps)
{
  if (!SendBroadcast(packet))
    return;

  if (!clientIps.empty()) {
    if (m_Config->m_UDPBroadcastEnabled)
      PropagateBroadcastEnabled(false);

    for (auto& clientIp : clientIps)
      Send(clientIp, packet);

    if (m_Config->m_UDPBroadcastEnabled)
      PropagateBroadcastEnabled(true);
  }

  if (m_Config->m_EnableTCPWrapUDP) for (auto& pair : m_IncomingConnections) {
    for (auto& connection : pair.second) {
      if (connection->GetDeleteMe()) continue;
      if (connection->m_IsUDPTunnel) {
        connection->Send(packet);
      }
    }
  }
}

bool CNet::IsIgnoredDatagramSource(string sourceIp)
{
  string element(sourceIp);
  return m_Config->m_UDPBlockedIPs.find(element) != m_Config->m_UDPBlockedIPs.end();
}

void CNet::HandleUDP(UDPPkt* pkt)
{
  std::vector<uint8_t> Bytes              = CreateByteArray((uint8_t*)pkt->buf, pkt->length);
  // pkt->buf->length at least MIN_UDP_PACKET_SIZE

  if (pkt->sender->ss_family != AF_INET && pkt->sender->ss_family != AF_INET6) {
    return;
  }

  uint16_t remotePort = GetAddressPort(pkt->sender);
  string ipAddress = AddressToString(*(pkt->sender));

  if (IsIgnoredDatagramSource(ipAddress)) {
    return;
  }

  if (m_Config->m_UDPForwardTraffic) {
    vector<uint8_t> relayPacket = {W3FW_HEADER_CONSTANT, 0, 0, 0};
    AppendByteArray(relayPacket, ipAddress, true);
    size_t portOffset = relayPacket.size();
    relayPacket.resize(portOffset + 6 + pkt->length);
    relayPacket[portOffset] = static_cast<uint8_t>(remotePort >> 8); // Network-byte-order (Big-endian)
    relayPacket[portOffset + 1] = static_cast<uint8_t>(remotePort);
    memset(relayPacket.data() + portOffset + 2, 0, 4); // Game version unknown at this layer.
    memcpy(relayPacket.data() + portOffset + 6, &(pkt->buf), pkt->length);
    AssignLength(relayPacket);
    Send(&(m_Config->m_UDPForwardAddress), relayPacket);
  }

  if (static_cast<unsigned char>(pkt->buf[0]) != W3GS_HEADER_CONSTANT) {
    return;
  }

  if (!(pkt->length >= 16 && static_cast<unsigned char>(pkt->buf[1]) == CGameProtocol::W3GS_SEARCHGAME)) {
    return;
  }

  if (pkt->buf[8] == m_Aura->m_GameVersion || pkt->buf[8] == 0) {
    if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetUDPEnabled() && !m_Aura->m_CurrentLobby->GetCountDownStarted()) {
      //Print("IP " + ipAddress + " searching from port " + to_string(remotePort) + "...");
      m_Aura->m_CurrentLobby->ReplySearch(pkt->sender, pkt->socket);

      // When we get GAME_SEARCH from a remote port other than 6112, we still announce to port 6112.
      if (remotePort != m_UDP4TargetPort && GetInnerIPVersion(pkt->sender) == AF_INET) {
        m_Aura->m_CurrentLobby->AnnounceToAddress(ipAddress);
      }
    }
  }
}

sockaddr_storage* CNet::GetPublicIPv4()
{
  switch (m_Config->m_PublicIPv4Algorithm) {
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL: {
      if (m_IPv4CacheV.first == m_Config->m_PublicIPv4Value && m_IPv4CacheT == NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL) {
        return m_IPv4CacheV.second;
      }
      if (m_IPv4CacheV.second != nullptr) {
        delete m_IPv4CacheV.second;
        m_IPv4CacheV = make_pair(string(), nullptr);
      }

      optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(m_Config->m_PublicIPv4Value, ACCEPT_IPV4);
      if (!maybeAddress.has_value()) return nullptr; // should never happen
      sockaddr_storage* cachedAddress = new sockaddr_storage();
      memcpy(cachedAddress, &(maybeAddress.value()), sizeof(sockaddr_storage));
      m_IPv4CacheV = make_pair(m_Config->m_PublicIPv4Value, cachedAddress);
      m_IPv4CacheT = NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL;
      return m_IPv4CacheV.second;
    }
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_API: {
      if (m_IPv4CacheV.first == m_Config->m_PublicIPv4Value && m_IPv4CacheT == NET_PUBLIC_IP_ADDRESS_ALGORITHM_API) {
        return m_IPv4CacheV.second;
      }
      if (m_IPv4CacheV.second != nullptr) {
        delete m_IPv4CacheV.second;
        m_IPv4CacheV = make_pair(string(), nullptr);
      }
#ifndef DISABLE_CPR
      auto response = cpr::Get(cpr::Url{m_Config->m_PublicIPv4Value}, cpr::Timeout{3000});
      if (response.status_code != 200) {
        return nullptr;
      }

      optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(response.text, ACCEPT_IPV4);
      if (!maybeAddress.has_value()) return nullptr;
      sockaddr_storage* cachedAddress = new sockaddr_storage();
      memcpy(cachedAddress, &(maybeAddress.value()), sizeof(sockaddr_storage));
      m_IPv4CacheV = make_pair(m_Config->m_PublicIPv4Value, cachedAddress);
      m_IPv4CacheT = NET_PUBLIC_IP_ADDRESS_ALGORITHM_API;
#endif
      return m_IPv4CacheV.second;
    }
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE:
    default:
      return nullptr;
  }
}

sockaddr_storage* CNet::GetPublicIPv6()
{
  switch (m_Config->m_PublicIPv6Algorithm) {
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL: {
      if (m_IPv6CacheV.first == m_Config->m_PublicIPv6Value && m_IPv6CacheT == NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL) {
        return m_IPv6CacheV.second;
      }
      if (m_IPv6CacheV.second != nullptr) {
        delete m_IPv6CacheV.second;
        m_IPv6CacheV = make_pair(string(), nullptr);
      }

      optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(m_Config->m_PublicIPv6Value, ACCEPT_IPV6);
      if (!maybeAddress.has_value()) return nullptr; // should never happen
      sockaddr_storage* cachedAddress = new sockaddr_storage();
      memcpy(cachedAddress, &(maybeAddress.value()), sizeof(sockaddr_storage));
      m_IPv6CacheV = make_pair(m_Config->m_PublicIPv6Value, cachedAddress);
      m_IPv6CacheT = NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL;
      return m_IPv6CacheV.second;
    }
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_API: {
      if (m_IPv6CacheV.first == m_Config->m_PublicIPv6Value && m_IPv6CacheT == NET_PUBLIC_IP_ADDRESS_ALGORITHM_API) {
        return m_IPv6CacheV.second;
      }
      if (m_IPv6CacheV.second != nullptr) {
        delete m_IPv6CacheV.second;
        m_IPv6CacheV = make_pair(string(), nullptr);
      }
#ifndef DISABLE_CPR
      auto response = cpr::Get(cpr::Url{m_Config->m_PublicIPv6Value}, cpr::Timeout{3000});
      if (response.status_code != 200) {
        return nullptr;
      }

      optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(response.text, ACCEPT_IPV6);
      if (!maybeAddress.has_value()) {
        return nullptr;
      }
      sockaddr_storage* cachedAddress = new sockaddr_storage();
      memcpy(cachedAddress, &(maybeAddress.value()), sizeof(sockaddr_storage));
      m_IPv6CacheV = make_pair(m_Config->m_PublicIPv6Value, cachedAddress);
      m_IPv6CacheT = NET_PUBLIC_IP_ADDRESS_ALGORITHM_API;
#endif
      return m_IPv6CacheV.second;
    }
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE:
    default:
      return nullptr;
  }
}

#ifndef DISABLE_MINIUPNP
uint8_t CNet::RequestUPnP(const string& protocol, const uint16_t externalPort, const uint16_t internalPort, const uint8_t logLevel)
{
  struct UPNPDev* devlist = NULL;
  struct UPNPDev* device;
  struct UPNPUrls urls;
  struct IGDdatas data;
  char lanaddr[64];

  devlist = upnpDiscover(2000, NULL, NULL, 0, 0, 2, 0);
  uint8_t success = 0;

  string extPort = to_string(externalPort);
  string intPort = to_string(internalPort);

  // Iterate through the discovered devices
  for (device = devlist; device; device = device->pNext) {
    // Get the UPnP URLs and IGD data for this device
    int checkIGD = UPNP_GetValidIGD(device, &urls, &data, lanaddr, sizeof(lanaddr));
    if (checkIGD != 1 && checkIGD != 2) {
      if (logLevel >= LOG_LEVEL_DEBUG) Print("[UPNP] unsupported device found: <" + string(device->descURL) + ">");
      continue;
    }
    if (checkIGD == 2) {
      if (logLevel >= LOG_LEVEL_DEBUG) Print("[UPNP] unconnected gateway found: <" + string(urls.controlURL) + ">");
    } else {
      if (logLevel >= LOG_LEVEL_DEBUG) Print("[UPNP] connected gateway found: <" + string(urls.controlURL) + ">");
    }
    if (logLevel >= LOG_LEVEL_INFO) Print("[UPNP] trying to forward traffic to LAN address " + string(lanaddr) + "...");

    int r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, extPort.c_str(), intPort.c_str(), lanaddr, "Warcraft 3 game hosting", protocol.c_str(), NULL, "86400");

    if (r == UPNPCOMMAND_SUCCESS) {
      success = success | (1 << (checkIGD - 1));
    } else if (logLevel >= LOG_LEVEL_INFO) {
      Print("[UPNP] failed to add port mapping - error " + to_string(r));
    }

    // Free UPnP URLs and IGD data
    FreeUPNPUrls(&urls);
    //FreeIGDdatas(&data);
  }

  // Free the UPnP device list
  freeUPNPDevlist(devlist);

  if (logLevel >= LOG_LEVEL_INFO) {
    if (success == 0) {
      Print("[UPNP] Universal Plug and Play is not supported by the host router.");
    } else if (0 != (success & 1)) {
      Print("[UPNP] forwarding " + protocol + " external port " + extPort + " to internal port " + intPort + " OK.");
    } else {
      Print("[UPNP] warning - multi-layer NAT detected, port-forwarding may fail.");
    }
  }

  return success;
}
#endif

bool CNet::QueryHealthCheck(CCommandContext* ctx, const uint8_t checkMode, CRealm* targetRealm, const uint16_t gamePort)
{
  if (m_HealthCheckInProgress) {
    return false;
  }

  bool isVerbose = 0 != (checkMode & HEALTH_CHECK_VERBOSE);

  if (0 != (checkMode & HEALTH_CHECK_LOOPBACK_IPV4)) {
    sockaddr_storage loopBackAddress;
    memset(&loopBackAddress, 0, sizeof(sockaddr_storage));
    loopBackAddress.ss_family = AF_INET;
    sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(&loopBackAddress);
    addr4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr4->sin_port = htons(gamePort);
    m_HealthCheckClients.push_back(new CGameTestConnection(m_Aura, nullptr, loopBackAddress, CONNECTION_TYPE_LOOPBACK, "[Loopback]"));
  }

  if (0 != (checkMode & HEALTH_CHECK_LOOPBACK_IPV6)) {
    sockaddr_storage loopBackAddress;
    memset(&loopBackAddress, 0, sizeof(sockaddr_storage));
    loopBackAddress.ss_family = AF_INET6;
    sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(&loopBackAddress);
    memcpy(&(addr6->sin6_addr), &in6addr_loopback, sizeof(in6_addr));
    addr6->sin6_port = htons(gamePort);
    m_HealthCheckClients.push_back(new CGameTestConnection(m_Aura, nullptr, loopBackAddress, CONNECTION_TYPE_LOOPBACK, "[Loopback IPv6]"));
  }

  sockaddr_storage* publicIPv4 = GetPublicIPv4();
  sockaddr_storage* publicIPv6 = GetPublicIPv6();
  if (publicIPv4 == nullptr && (0 != (checkMode & HEALTH_CHECK_PUBLIC_IPV4)) && m_Config->m_PublicIPv4Algorithm != NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE) {
    Print("[NET] Public IPv4 address unknown - check <net.ipv4.public_address.algorithm>, <net.ipv4.public_address.value>");
  }
  if (publicIPv6 == nullptr && (0 != (checkMode & HEALTH_CHECK_PUBLIC_IPV6)) && m_Config->m_PublicIPv6Algorithm != NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE) {
    Print("[NET] Public IPv6 address unknown - check <net.ipv6.public_address.algorithm>, <net.ipv6.public_address.value>");
  }

  bool anySendsPublicIp = false;
  for (const auto& realm : m_Aura->m_Realms) {
    if ((0 == (checkMode & HEALTH_CHECK_REALM)) && realm != targetRealm) {
      continue;
    }
    sockaddr_storage* selfIPInThisRealm = realm->GetUsesCustomIPAddress() ? realm->GetPublicHostAddress() : publicIPv4;
    if (publicIPv4 == nullptr) {
      continue;
    }
    uint8_t connectionType = CONNECTION_TYPE_DEFAULT;
    uint16_t port = realm->GetUsesCustomPort() ? realm->GetPublicHostPort() : gamePort;
    string NameSuffix;
    if (realm->GetIsVPN()) {
      NameSuffix = " [VPN]";
      connectionType = connectionType | CONNECTION_TYPE_VPN;
    }
    if (realm->GetUsesCustomIPAddress()) {
      if (NameSuffix.empty()) NameSuffix = " [Tunnel]";
      connectionType = connectionType | CONNECTION_TYPE_CUSTOM_IP_ADDRESS;
    }
    if (realm->GetUsesCustomPort()) {
      if (NameSuffix.empty()) NameSuffix = " [Tunnel]";
      connectionType = connectionType | CONNECTION_TYPE_CUSTOM_PORT;
    }

    sockaddr_storage targetHost;
    memcpy(&targetHost, selfIPInThisRealm, sizeof(sockaddr_storage));
    SetAddressPort(&targetHost, port);
    m_HealthCheckClients.push_back(new CGameTestConnection(m_Aura, realm, targetHost, connectionType, realm->GetUniqueDisplayName()));

    if (reinterpret_cast<sockaddr_in*>(selfIPInThisRealm)->sin_addr.s_addr == reinterpret_cast<sockaddr_in*>(publicIPv4)->sin_addr.s_addr) {
      anySendsPublicIp = true;
    }
  }
  if (!anySendsPublicIp && publicIPv4 != nullptr && (
    (0 != (checkMode & HEALTH_CHECK_PUBLIC_IPV4)))
  ) {
    sockaddr_storage targetHost;
    memcpy(&targetHost, publicIPv4, sizeof(sockaddr_storage));
    SetAddressPort(&targetHost, gamePort);
    m_HealthCheckClients.push_back(new CGameTestConnection(m_Aura, nullptr, targetHost, 0, "[Public IPv4]"));
  }
  if (publicIPv6 != nullptr && !IN6_IS_ADDR_UNSPECIFIED(&(reinterpret_cast<sockaddr_in6*>(publicIPv6)->sin6_addr)) && (
    (0 != (checkMode & HEALTH_CHECK_PUBLIC_IPV6))
    )) {
    sockaddr_storage targetHost;
    memcpy(&targetHost, publicIPv6, sizeof(sockaddr_storage));
    SetAddressPort(&targetHost, gamePort);
    m_HealthCheckClients.push_back(new CGameTestConnection(m_Aura, nullptr, targetHost, CONNECTION_TYPE_IPV6, "[Public IPv6]"));
  }

  if (m_HealthCheckClients.empty()) {
    return false;
  }

  m_Aura->HoldContext(ctx);
  m_HealthCheckVerbose = isVerbose;
  m_HealthCheckContext = ctx;
  m_HealthCheckInProgress = true;
  return true;
}

void CNet::ResetHealthCheck()
{
  if (!m_HealthCheckInProgress)
    return;

  for (auto& testConnection : m_HealthCheckClients) {
    delete testConnection;
  }
  m_HealthCheckClients.clear();
  m_HealthCheckInProgress = false;
  m_Aura->UnholdContext(m_HealthCheckContext);
  m_HealthCheckContext = nullptr;
}

void CNet::ReportHealthCheck()
{
  bool hasDirectAttempts = false;
  bool anyDirectSuccess = false;
  vector<string> ChatReport;
  vector<uint16_t> FailPorts;
  bool isIPv6Reachable = false;
  for (auto& testConnection : m_HealthCheckClients) {
    bool success = false;
    string ResultText;
    if (testConnection->GetIsRealmOnline()) {
      if (testConnection->m_Passed.value_or(false)) {
        success = true;
        ResultText = "OK";
      } else if (testConnection->m_CanConnect.value_or(false)) {
        ResultText = "Error";
      } else {
        ResultText = "Offline";
      }
    } else {
      ResultText = "Realm offline";
    }
    ChatReport.push_back(testConnection->m_Name + " - " + ResultText);
    if (!m_HealthCheckContext->GetWritesToStdout()) {
      Print("[AURA] Game at " + testConnection->m_Name + " - " + ResultText);
    }
    if (0 == (testConnection->m_Type & ~(CONNECTION_TYPE_CUSTOM_PORT))) {
      hasDirectAttempts = true;
      if (success) anyDirectSuccess = true;
    }
    if (0 != (testConnection->m_Type & (CONNECTION_TYPE_IPV6))) {
      if (success) isIPv6Reachable = true;
    }
    if (m_HealthCheckVerbose && !success) {
      uint16_t port = testConnection->GetPort();
      if (std::find(FailPorts.begin(), FailPorts.end(), port) == FailPorts.end()) {
        FailPorts.push_back(port);
      }
    }
  }
  sockaddr_storage* publicIPv4 = GetPublicIPv4();
  if (publicIPv4 != nullptr && hasDirectAttempts) {
    string portForwardInstructions;
    if (m_HealthCheckVerbose && m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetIsLobby()) {
      portForwardInstructions = "About port-forwarding: Setup your router to forward external port(s) {" + JoinVector(FailPorts, false) + "} to internal port(s) {" + JoinVector(GetPotentialGamePorts(), false) + "}";
    }
    if (anyDirectSuccess) {
      Print("[NET] This bot CAN be reached through the IPv4 Internet. Address: " + AddressToString(*publicIPv4));
      if (!m_HealthCheckContext->GetWritesToStdout()) {
        m_HealthCheckContext->SendAll("This bot CAN be reached through the IPv4 Internet.");
      }
    } else {
      Print("[NET] This bot is disconnected from the IPv4 Internet, because its public address is unreachable. Address: " + AddressToString(*publicIPv4));
      Print("[NET] Please setup port-forwarding to allow connections.");
      if (m_HealthCheckVerbose) {
        if (!portForwardInstructions.empty())
          Print("[NET] " + portForwardInstructions);
#ifndef DISABLE_MINIUPNP
        Print("[NET] If your router has Universal Plug and Play, the command [upnp] will automatically setup port-forwarding.");
#endif
        Print("[NET] Note that you may still play online if you got a VPN, or an active tunnel. See NETWORKING.md for details.");
        Print("[NET] But make sure your firewall allows Aura inbound TCP connections.");
        if (!m_HealthCheckContext->GetWritesToStdout()) {
          m_HealthCheckContext->SendAll("============= READ IF YOU ARE RUNNING AURA =====================================");
          m_HealthCheckContext->SendAll("This bot is disconnected from the IPv4 Internet, because its public IPv4 address is unreachable.");
          m_HealthCheckContext->SendAll("Please setup port-forwarding to allow connections.");
          m_HealthCheckContext->SendAll(portForwardInstructions);
#ifndef DISABLE_MINIUPNP
          m_HealthCheckContext->SendAll("If your router has Universal Plug and Play, the command [upnp] will automatically setup port-forwarding.");
#endif
          m_HealthCheckContext->SendAll("Note that you may still play online if you got a VPN, or an active tunnel. See NETWORKING.md for details.");
          m_HealthCheckContext->SendAll("But make sure your firewall allows Aura inbound TCP connections.");
          m_HealthCheckContext->SendAll("=================================================================================");
        }
      }
    }
  }
  sockaddr_storage* publicIPv6 = GetPublicIPv6();
  if (publicIPv6 != nullptr) {
    if (isIPv6Reachable) {
      Print("[NET] This bot CAN be reached through the IPv6 Internet. Address: " + AddressToString(*publicIPv6));
      Print("[NET] See NETWORKING.md for instructions to use IPv6 TCP tunneling.");
    } else {
      Print("[NET] This bot is disconnected from the IPv6 Internet, because its public address is unreachable. Address: " + AddressToString(*publicIPv6));
    }
    if (!m_HealthCheckContext->GetWritesToStdout()) {
      if (isIPv6Reachable) {
        m_HealthCheckContext->SendAll("This bot CAN be reached through the IPv6 Internet.");
        m_HealthCheckContext->SendAll("See NETWORKING.md for instructions to use IPv6 TCP tunneling.");
        m_HealthCheckContext->SendAll("=================================================================================");
      } else {
        Print("[NET] This bot is disconnected from the IPv6 Internet, because its public address is unreachable. Address: " + AddressToString(*publicIPv6) + ".");
        m_HealthCheckContext->SendAll("This bot is disconnected from the IPv6 Internet, because its public address is unreachable.");
        m_HealthCheckContext->SendAll("=================================================================================");
      }
    }
  }
  m_HealthCheckContext->SendAll(JoinVector(ChatReport, " | ", false));
  ResetHealthCheck();
}

bool CNet::QueryIPAddress()
{
  if (m_IPAddressFetchInProgress) {
    return false;
  }

  if (m_Config->m_PublicIPv4Algorithm == NET_PUBLIC_IP_ADDRESS_ALGORITHM_API) {
    if (m_IPv4CacheV.first != m_Config->m_PublicIPv4Value || m_IPv4CacheT != NET_PUBLIC_IP_ADDRESS_ALGORITHM_API) {
      if (m_IPv4CacheV.second != nullptr) {
        delete m_IPv4CacheV.second;
        m_IPv4CacheV = make_pair(string(), nullptr);
      }
      optional<tuple<string, string, uint16_t, string>> parsedURL = CNet::ParseURL(m_Config->m_PublicIPv4Value);
      if (parsedURL.has_value() && get<0>(parsedURL.value()) == "http:") {
        string hostName = get<1>(parsedURL.value());
        uint16_t port = get<2>(parsedURL.value());
        string path = get<3>(parsedURL.value());
        optional<sockaddr_storage> resolvedAddress = port == 80 ? ResolveHostName(hostName) : ResolveHostName(hostName, port);
        if (resolvedAddress.has_value()) {
          SetAddressPort(&(resolvedAddress.value()), port);
          CIPAddressAPIConnection* client = new CIPAddressAPIConnection(m_Aura, resolvedAddress.value(), path, hostName);
          m_IPAddressFetchClients.push_back(client);
          m_IPAddressFetchInProgress = true;
        }
      }
    }
  }
  if (m_Config->m_PublicIPv6Algorithm == NET_PUBLIC_IP_ADDRESS_ALGORITHM_API) {
    if (m_IPv6CacheV.first != m_Config->m_PublicIPv6Value || m_IPv6CacheT != NET_PUBLIC_IP_ADDRESS_ALGORITHM_API) {
      if (m_IPv6CacheV.second != nullptr) {
        delete m_IPv6CacheV.second;
        m_IPv6CacheV = make_pair(string(), nullptr);
      }
      // TODO
    }
  }

  return m_IPAddressFetchInProgress;
}

void CNet::ResetIPAddressFetch()
{
  if (!m_IPAddressFetchInProgress)
    return;

  for (auto& apiClient : m_IPAddressFetchClients) {
    delete apiClient;
  }
  m_IPAddressFetchClients.clear();
  m_IPAddressFetchInProgress = false;
}

void CNet::HandleIPAddressFetchDone()
{
  for (auto& apiClient : m_IPAddressFetchClients) {
    if (!apiClient->m_Result.has_value()) continue;
    sockaddr_storage* cachedAddress = new sockaddr_storage();
    memcpy(cachedAddress, &(apiClient->m_Result.value()), sizeof(sockaddr_storage));
    if (apiClient->m_TargetHost.ss_family == AF_INET6) {
      m_IPv6CacheV = make_pair(m_Config->m_PublicIPv6Value, cachedAddress);
      m_IPv6CacheT = m_Config->m_PublicIPv6Algorithm;
    } else {
      m_IPv4CacheV = make_pair(m_Config->m_PublicIPv4Value, cachedAddress);
      m_IPv4CacheT = m_Config->m_PublicIPv4Algorithm;
    }
  }
  ResetIPAddressFetch();

  if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetIsCheckJoinable()) {
    uint8_t checkMode = HEALTH_CHECK_ALL;
    if (!m_SupportTCPOverIPv6) {
      checkMode &= ~HEALTH_CHECK_PUBLIC_IPV6;
      checkMode &= ~HEALTH_CHECK_LOOPBACK_IPV6;
    }
    if (m_Aura->m_CurrentLobby->GetIsVerbose()) {
      checkMode |= HEALTH_CHECK_VERBOSE;
    }
    CCommandContext* ctx = new CCommandContext(m_Aura, &cout, '!');
    QueryHealthCheck(ctx, checkMode, nullptr, m_Aura->m_CurrentLobby->GetHostPortForDiscoveryInfo(AF_INET));
    m_Aura->UnholdContext(ctx);
    m_Aura->m_CurrentLobby->SetIsCheckJoinable(false);
  }
}

uint16_t CNet::GetUDPPort(const uint8_t protocol) const
{
  if (protocol == AF_INET) {
    return m_UDPMainServerEnabled ? m_UDPMainServer->GetPort() : m_UDPDeafSocket->GetPort();
  } else if (protocol == AF_INET6) {
    if (m_SupportUDPOverIPv6) {
      return m_UDPIPv6Server->GetPort();
    }
  }
  return 0;
}

uint16_t CNet::NextHostPort()
{
  ++m_LastHostPort;
  if (m_LastHostPort > m_Config->m_MaxHostPort || m_LastHostPort < m_Config->m_MinHostPort) {
    m_LastHostPort = m_Config->m_MinHostPort;
  }
  return m_LastHostPort;
}

vector<uint16_t> CNet::GetPotentialGamePorts() const
{
  vector<uint16_t> result;

  uint16_t port = m_Config->m_MinHostPort;
  if (m_Config->m_UDPEnableCustomPortTCP4 && m_Config->m_UDPCustomPortTCP4 < port) {
    result.push_back(m_Config->m_UDPCustomPortTCP4);
  }
  while (port <= m_Config->m_MaxHostPort) {
    result.push_back(port);
    ++port;
  }
  if (m_Config->m_UDPEnableCustomPortTCP4 && m_Config->m_UDPCustomPortTCP4 > port) {
    result.push_back(m_Config->m_UDPCustomPortTCP4);
  }
  
  return result;
}

optional<tuple<string, string, uint16_t, string>> CNet::ParseURL(const string& address)
{
  if (address.empty()) return nullopt;
  const size_t colonIndex = address.find(':');
  if (colonIndex == string::npos || (colonIndex != 4 && colonIndex != 5)) return nullopt;
  string protocol = address.substr(0, colonIndex + 1);
  const uint16_t port = protocol == "https:" ? 443 : 80;
  if (address.length() <= protocol.length() + 2 ||
    address[protocol.length()] != '/' ||
    address[protocol.length() + 1] != '/'
  ) {
    return nullopt;
  }
  size_t pathIndex = address.find('/', protocol.length() + 2);
  if (pathIndex == string::npos) pathIndex = address.size();
  string hostName = address.substr(protocol.length() + 2, pathIndex - (protocol.length() + 2));
  string path = pathIndex >= address.length() ? "/" : address.substr(pathIndex);
  tuple<string, string, uint16_t, string> URL(protocol, hostName, port, path);
  optional<tuple<string, string, uint16_t, string>> result;
  result = URL;
  return result;
}

optional<sockaddr_storage> CNet::ParseAddress(const string& address, const uint8_t inputMode)
{
  std::optional<sockaddr_storage> result;
  if (0 != (inputMode & ACCEPT_IPV4)) {
    sockaddr_in addr4;
    memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    if (inet_pton(AF_INET, address.c_str(), &addr4.sin_addr) == 1) {
      struct sockaddr_storage ipv4;
      memset(&ipv4, 0, sizeof(ipv4));
      ipv4.ss_family = AF_INET;
      memcpy(&ipv4, &addr4, sizeof(addr4));
      result = ipv4;
      return result;
    }
  }

  if (0 != (inputMode & ACCEPT_IPV6)) {
    sockaddr_in6 addr6;
    memset(&addr6, 0, sizeof(addr6));
    addr6.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, address.c_str(), &addr6.sin6_addr) == 1) {
      struct sockaddr_storage ipv6;
      memset(&ipv6, 0, sizeof(ipv6));
      ipv6.ss_family = AF_INET6;
      memcpy(&ipv6, &addr6, sizeof(addr6));
      result = ipv6;
      return result;
    }
  }

  return result;
}

optional<sockaddr_storage> CNet::ResolveHostName(const string& hostName)
{
  optional<sockaddr_storage> result = ParseAddress(hostName);
  if (result.has_value()) {
    return result;
  }

  string cacheKey = hostName + ":";
  auto it = m_DNSCache.find(cacheKey);
  if (it != end(m_DNSCache)) {
    result = it->second;
    return result;
  }

  struct hostent* HostInfo;
  HostInfo = gethostbyname(hostName.c_str());

  if (!HostInfo) {
    Print("[DNS] cannot resolve " + hostName + " - check your Internet connection");
    return result;
  }

  struct sockaddr_storage address;
  struct sockaddr_in *addr4 = (struct sockaddr_in *)&address;
  addr4->sin_family = AF_INET;
  addr4->sin_port = 0;
  memcpy(&(addr4->sin_addr.s_addr), HostInfo->h_addr, HostInfo->h_length);

  m_DNSCache[cacheKey] = address;
  result = address;
  return result;
}

optional<sockaddr_storage> CNet::ResolveHostName(const string& hostName, const uint16_t hostPort)
{
  optional<sockaddr_storage> result = ParseAddress(hostName);
  if (result.has_value()) {
    return result;
  }

  string cacheKey = hostName + ":" + to_string(hostPort);
  auto it = m_DNSCache.find(cacheKey);
  if (it != end(m_DNSCache)) {
    result = it->second;
    return result;
  }

  auto baseMatch = m_DNSCache.find(hostName + ":");
  if (baseMatch != end(m_DNSCache)) {
    struct sockaddr_storage addressCopy;
    memcpy(&addressCopy, &(baseMatch->second), sizeof(sockaddr_storage));
    m_DNSCache[cacheKey] = addressCopy;
    result = addressCopy;
    return result;
  }

  struct hostent* HostInfo;
  HostInfo = gethostbyname(hostName.c_str());

  if (!HostInfo) {
    Print("[DNS] cannot resolve " + hostName + " - check your Internet connection");
    return result;
  }

  struct sockaddr_storage address;
  struct sockaddr_in *addr4 = (struct sockaddr_in *)&address;
  addr4->sin_family = AF_INET;
  addr4->sin_port = 0;
  memcpy(&(addr4->sin_addr.s_addr), HostInfo->h_addr, HostInfo->h_length);

  m_DNSCache[cacheKey] = address;
  result = address;
  return result;
}

void CNet::FlushDNSCache()
{
  m_DNSCache.clear();
}

void CNet::FlushSelfIPCache()
{
  if (m_IPv4CacheV.second != nullptr)
    delete m_IPv4CacheV.second;
  if (m_IPv6CacheV.second != nullptr)
    delete m_IPv6CacheV.second;
  m_IPv4CacheV = make_pair(string(), nullptr);
  m_IPv4CacheT = NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID;
  m_IPv6CacheV = make_pair(string(), nullptr);
  m_IPv6CacheT = NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID;
}

void CNet::PropagateBroadcastEnabled(const bool nEnable)
{
  if (m_UDPMainServerEnabled) {
    m_UDPMainServer->SetBroadcastEnabled(nEnable);
  } else {
    m_UDPDeafSocket->SetBroadcastEnabled(nEnable);
  }
}

void CNet::PropagateDoNotRouteEnabled(const bool nEnable)
{
  if (m_UDPMainServerEnabled) {
    m_UDPMainServer->SetDontRoute(nEnable);
  } else {
    m_UDPDeafSocket->SetDontRoute(nEnable);
  }

  if (m_SupportUDPOverIPv6) {
    m_UDPIPv6Server->SetDontRoute(nEnable);
  }
}

void CNet::OnConfigReload()
{
  m_UDP6TargetPort = m_Config->m_UDP6TargetPort;
  PropagateBroadcastEnabled(m_Config->m_UDPBroadcastEnabled);
  PropagateDoNotRouteEnabled(m_Config->m_UDPDoNotRouteEnabled);
  SetBroadcastTarget(m_Config->m_UDPBroadcastTarget);
}

CNet::~CNet()
{
  delete m_Config;
  delete m_UDPMainServer;
  delete m_UDPDeafSocket;
  delete m_UDPIPv6Server;
  delete m_UDP4BroadcastTarget;
  delete m_UDP6BroadcastTarget;
  FlushSelfIPCache();
  m_Aura->UnholdContext(m_HealthCheckContext);
  m_HealthCheckContext = nullptr;
}
