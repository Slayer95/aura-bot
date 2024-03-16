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
#include <cpr/cpr.h>
#ifndef DISABLE_MINIUPNP
#include <miniupnpc.h>
#include <upnpcommands.h>
#endif

#include "aura.h"
#include "net.h"
#include "gameprotocol.h"

using namespace std;

//
// CTestConnection
//

CTestConnection::CTestConnection(CAura* nAura, sockaddr_storage nTargetHost, const uint8_t nType, const string nName)
  : m_Aura(nAura),
    m_TargetHost(nTargetHost),
    m_Type(nType),
    m_Name(nName),
    m_Timeout(0),
    m_LastConnectionFailure(0),
    m_SentJoinRequest(false),
    m_Socket(new CTCPClient(static_cast<uint8_t>(nTargetHost.ss_family), nName))
{
}

CTestConnection::~CTestConnection()
{
  delete m_Socket;
}

uint32_t CTestConnection::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  if (!m_Socket->HasError() && m_Socket->GetConnected())
  {
    m_Socket->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    return 1;
  }

  return 0;
}

uint16_t CTestConnection::GetPort()
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

bool CTestConnection::QueryGameInfo()
{
  if (m_Socket->HasError() || !m_Socket->GetConnected())
    return false;

  if (!m_Aura->m_CurrentLobby || !m_Aura->m_CurrentLobby->GetIsLobby() && !m_Aura->m_CurrentLobby->GetIsMirror())
    return false;

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

bool CTestConnection::Update(void* fd, void* send_fd)
{
  static optional<sockaddr_storage> emptyBindAddress;

  if (m_Passed.has_value()) {
    return !m_Passed.has_value();
  }

  const int64_t Ticks = GetTicks();
  if (m_Socket->HasError()) {
    if (!m_CanConnect.has_value()) m_CanConnect = false;
    m_Passed = false;
    m_LastConnectionFailure = Ticks;
    m_Socket->Reset();
  } else if (m_Socket->GetConnected() && Ticks < m_Timeout) {
    m_Socket->DoRecv(static_cast<fd_set*>(fd));
    string* RecvBuffer = m_Socket->GetBytes();
    std::vector<uint8_t> Bytes = CreateByteArray((uint8_t*)RecvBuffer->c_str(), RecvBuffer->size());
    const bool IsJoinedMessage = Bytes.size() >= 2 && Bytes[0] == W3GS_HEADER_CONSTANT && Bytes[1] == CGameProtocol::W3GS_SLOTINFOJOIN;
    RecvBuffer->clear();
    if (!m_SentJoinRequest) {
      if (QueryGameInfo()) {
        m_Socket->DoSend(static_cast<fd_set*>(send_fd));
      }
    } else if (IsJoinedMessage) {
      m_Passed = true;
      m_Socket->Disconnect();
    }
  } else if (m_Socket->GetConnecting() && m_Socket->CheckConnect()) {
    m_CanConnect = true;
  } else if (m_Timeout <= Ticks && (m_Socket->GetConnecting() || m_CanConnect.has_value())) {
    if (!m_CanConnect.has_value()) m_CanConnect = false;
    m_Passed = false;
    m_LastConnectionFailure = Ticks;
    m_Socket->Reset();
  } else if (!m_Socket->GetConnecting() && !m_CanConnect.has_value() && (Ticks - m_LastConnectionFailure > 900)) {
    m_Socket->Connect(emptyBindAddress, m_TargetHost);
    m_Timeout = Ticks + 3000;
  }

  return !m_Passed.has_value();
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

    m_HealthCheckInProgress(false),
    m_HealthCheckContext(nullptr),
    m_IPv4CacheV(make_pair(string(), nullptr)),
    m_IPv4CacheT(NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID),
    m_IPv6CacheV(make_pair(string(), nullptr)),
    m_IPv6CacheT(NET_PUBLIC_IP_ADDRESS_ALGORITHM_INVALID),

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
      Print("[DISCOVERY] <net.udp_server.enabled = yes> requires active instances of Warcraft to be closed...");
      Print("[DISCOVERY] Please release port 6112, or set <net.udp_server.enabled = no>");
      Print("====================================================================");
      Print("[DISCOVERY] Waiting...");
      this_thread::sleep_for(chrono::milliseconds(5000));
      if (!m_UDPMainServer->Listen(m_Config->m_BindAddress4, 6112, true)) {
        if (m_Config->m_UDPBroadcastEnabled) {
          Print("[DISCOVERY] Failed to start UDP/IPv4 service on port 6112. Cannot start broadcast service.");
        } else {
          Print("[DISCOVERY] Failed to start UDP/IPv4 service on port 6112");
        }
        Print("====================================================================");
        return false;
      }
    }
  }

  if (!m_UDPMainServerEnabled) {
    m_UDPDeafSocket = new CUDPServer(AF_INET);
    if (!m_UDPDeafSocket->Listen(m_Config->m_BindAddress4, m_UDPFallbackPort, true)) {
      Print("====================================================================");
      Print("[DISCOVERY] Failed to bind to fallback port " + to_string(m_UDPFallbackPort) + ".");
      Print("[DISCOVERY] For a random available port, set <net.udp_fallback.outbound_port = 0>");
      if (m_Config->m_UDPBroadcastEnabled) {
        Print("[DISCOVERY] Failed to start UDP/IPv4 service. Cannot start broadcast service.");
      } else {
        Print("[DISCOVERY] Failed to start UDP/IPv4 service");
      }
      Print("====================================================================");
      return false;
    }
  }

  if (m_SupportUDPOverIPv6) {
    m_UDPIPv6Server = new CUDPServer(AF_INET6);
    if (!m_UDPIPv6Server->Listen(m_Config->m_BindAddress6, m_UDPIPv6Port, true)) {
      Print("====================================================================");
      Print("[DISCOVERY] Failed to bind to port " + to_string(m_UDPIPv6Port) + ".");
      Print("[DISCOVERY] For a random available port, set <net.udp_ipv6.port = 0>");
      Print("[DISCOVERY] Failed to start UDP/IPv6 service");
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
  return true;
}

uint32_t CNet::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  uint32_t NumFDs = 0;

  for (auto& connection : m_HealthCheckClients)
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
      auto response = cpr::Get(cpr::Url{m_Config->m_PublicIPv6Value}, cpr::Timeout{3000});
      if (response.status_code != 200) {
        return nullptr;
      }

      optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(response.text, ACCEPT_IPV6);
      if (!maybeAddress.has_value()) return nullptr;
      sockaddr_storage* cachedAddress = new sockaddr_storage();
      memcpy(cachedAddress, &(maybeAddress.value()), sizeof(sockaddr_storage));
      m_IPv6CacheV = make_pair(m_Config->m_PublicIPv6Value, cachedAddress);
      m_IPv6CacheT = NET_PUBLIC_IP_ADDRESS_ALGORITHM_API;
      return m_IPv6CacheV.second;
    }
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE:
    default:
      return nullptr;
  }
}

#ifndef DISABLE_MINIUPNP
uint8_t CNet::EnableUPnP(const uint16_t externalPort, const uint16_t internalPort)
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
      if (checkIGD == 0) {
        printf("Found UPnP Device: %s - but no IGD found\n", device->descURL);
      } else if (checkIGD == 3) {
        printf("Found UPnP Device: %s - but not recognized as an IGD\n", device->descURL);
      } else {
        printf("Found UPnP Device: %s - but got an internal error checking its data\n", device->descURL);
      }
      continue;
    }
    if (checkIGD == 2) {
      printf("Found unconnected Internet Gateway Device at %s\n", urls.controlURL);
    } else {
      printf("Found connected Internet Gateway Device at %s\n", urls.controlURL);
    }
    printf("LAN address: %s\n", lanaddr);

    int r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, extPort.c_str(), intPort.c_str(), lanaddr, "Warcraft 3 game hosting", "TCP", NULL, "86400");

    if (r != UPNPCOMMAND_SUCCESS) {
      printf("Failed to add port mapping: %d\n", r);
    } else {
      printf("Port mapping added successfully!\n");
      success = success | (1 << (checkIGD - 1));
    }

    // Free UPnP URLs and IGD data
    FreeUPNPUrls(&urls);
    //FreeIGDdatas(&data);
  }

  // Free the UPnP device list
  freeUPNPDevlist(devlist);
  return success;
}
#endif

bool CNet::StartHealthCheck(const vector<tuple<string, uint8_t, sockaddr_storage>> testServers, CCommandContext* nCtx)
{
  if (m_HealthCheckInProgress) {
    return false;
  }
  for (auto& testServer: testServers) {
    m_HealthCheckClients.push_back(new CTestConnection(m_Aura, get<2>(testServer), get<1>(testServer), get<0>(testServer)));
  }
  m_Aura->HoldContext(nCtx);
  m_HealthCheckContext = nCtx;
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
    if (testConnection->m_Passed.value_or(false)) {
      success = true;
      ResultText = "OK";
    } else if (testConnection->m_CanConnect.value_or(false)) {
      ResultText = "Cannot join";
    } else {
      ResultText = "Cannot connect";
    }
    ChatReport.push_back(testConnection->m_Name + " - " + ResultText);
    Print("[AURA] Game at " + testConnection->m_Name + " - " + ResultText);
    if (0 == (testConnection->m_Type & ~(CONNECTION_TYPE_CUSTOM_PORT))) {
      hasDirectAttempts = true;
      if (success) anyDirectSuccess = true;
    }
    if (0 != (testConnection->m_Type & (CONNECTION_TYPE_IPV6))) {
      if (success) isIPv6Reachable = true;
    }
    if (!success) {
      uint16_t port = testConnection->GetPort();
      if (std::find(FailPorts.begin(), FailPorts.end(), port) == FailPorts.end())
        FailPorts.push_back(port);
    }
  }
  sockaddr_storage* publicIPv4 = GetPublicIPv4();
  if (publicIPv4 != nullptr && hasDirectAttempts) {
    string portForwardInstructions;
    if (m_Aura->m_CurrentLobby && m_Aura->m_CurrentLobby->GetIsLobby()) {
      portForwardInstructions = "About port-forwarding: Setup your router to forward external port(s) {" + JoinVector(FailPorts, false) + "} to internal port(s) {" + JoinVector(GetPotentialGamePorts(), false) + "}";
    }
    if (anyDirectSuccess) {
      Print("[Network] This bot CAN be reached through the IPv4 Internet. Address: " + AddressToString(*publicIPv4) + ".");
      m_HealthCheckContext->SendAll("This bot CAN be reached through the IPv4 Internet.");
    } else {
      Print("[Network] This bot is disconnected from the IPv4 Internet, because its public address is unreachable. Address: " + AddressToString(*publicIPv4) + ".");
      Print("[Network] Please setup port-forwarding to allow connections.");
      if (!portForwardInstructions.empty())
        Print("[Network] " + portForwardInstructions);
#ifndef DISABLE_MINIUPNP
      Print("[Network] If your router has Universal Plug and Play, the command [upnp] will automatically setup port-forwarding.");
#endif
      Print("[Network] Note that you may still play online if you got a VPN, or an active tunnel. See NETWORKING.md for details.");
      Print("[Network] But make sure your firewall allows Aura inbound TCP connections.");

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
  sockaddr_storage* publicIPv6 = GetPublicIPv6();
  if (publicIPv6 != nullptr) {
    if (isIPv6Reachable) {
      Print("[Network] This bot CAN be reached through the IPv6 Internet. Address: " + AddressToString(*publicIPv6) + ".");
      Print("[Network] See NETWORKING.md for instructions to use IPv6 TCP tunneling.");
      m_HealthCheckContext->SendAll("This bot CAN be reached through the IPv6 Internet.");
      m_HealthCheckContext->SendAll("See NETWORKING.md for instructions to use IPv6 TCP tunneling.");
      m_HealthCheckContext->SendAll("=================================================================================");
    } else {
      Print("[Network] This bot is disconnected from the IPv6 Internet, because its public address is unreachable. Address: " + AddressToString(*publicIPv6) + ".");
      m_HealthCheckContext->SendAll("This bot is disconnected from the IPv6 Internet, because its public address is unreachable.");
      m_HealthCheckContext->SendAll("=================================================================================");
    }
  }
  m_HealthCheckContext->SendAll(JoinVector(ChatReport, " | ", false));
  ResetHealthCheck();
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
    Print("DNS resolution failed for " + hostName);
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
    Print("DNS resolution failed for " + hostName);
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
