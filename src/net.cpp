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
#include <miniupnpc.h>
#include <upnpcommands.h>

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
    m_SentJoinRequest(false),
    m_Socket(new CTCPClient(AF_INET, nName))
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
  sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(&m_TargetHost);
  return ntohs(addr4->sin_port);
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
    m_Socket->Reset();
  } else if (!m_Socket->GetConnecting() && !m_CanConnect.has_value()) {
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

    m_GameRangerLocalPort(0),
    m_GameRangerLocalAddress("255.255.255.255"),
    m_GameRangerRemotePort(0),
    m_GameRangerRemoteAddress({0xFF, 0xFF, 0xFF, 0xFF})
{
}

bool CNet::Init(CConfig* CFG)
{
  // m_UDPMainServerEnabled may be overriden by the cli.
  bool useDoNotRoute = CFG->GetBool("net.game_discovery.udp.do_not_route", false);

  // Main server is the only one that can send W3GS_REFRESHGAME packets. 
  // If disabled, UDP communication is one-way, and can only be through W3GS_GAMEINFO.
  m_UDPMainServerEnabled = CFG->GetBool("net.udp_server.enabled", false);
  m_SupportTCPOverIPv6 = CFG->GetBool("net.ipv6.tcp.enabled", true);
  m_SupportUDPOverIPv6 = CFG->GetBool("net.udp_ipv6.enabled", true);

  if (m_UDPMainServerEnabled) {
    m_UDPMainServer = new CUDPServer(AF_INET);
    m_UDPMainServer->SetDontRoute(useDoNotRoute);
    if (!m_UDPMainServer->Listen(m_Aura->m_Config->m_BindAddress4, 6112, false)) {
      Print("====================================================================");
      Print("[DISCOVERY] <net.udp_server.enabled = yes> requires active instances of Warcraft to be closed...");
      Print("[DISCOVERY] Please release port 6112, or set <net.udp_server.enabled = no>");
      Print("====================================================================");
      Print("[DISCOVERY] Waiting...");
      this_thread::sleep_for(chrono::milliseconds(5000));
      if (!m_UDPMainServer->Listen(m_Aura->m_Config->m_BindAddress4, 6112, true)) {
        if (m_Aura->m_Config->m_UDPBroadcastEnabled) {
          Print("[DISCOVERY] Failed to start UDP/IPv4 service on port 6112. Cannot start broadcast service.");
        } else {
          Print("[DISCOVERY] Failed to start UDP/IPv4 service on port 6112");
        }
        Print("====================================================================");
        return false;
      }
    }
  }

  uint16_t fallbackPort = CFG->GetUint16("net.udp_fallback.outbound_port", 6113);
  if (!m_UDPMainServerEnabled) {
    m_UDPDeafSocket = new CUDPServer(AF_INET);
    m_UDPDeafSocket->SetDontRoute(useDoNotRoute);
    if (!m_UDPDeafSocket->Listen(m_Aura->m_Config->m_BindAddress4, fallbackPort, true)) {
      Print("====================================================================");
      Print("[DISCOVERY] Failed to bind to fallback port " + to_string(fallbackPort) + ".");
      Print("[DISCOVERY] For a random available port, set <net.udp_fallback.outbound_port = 0>");
      if (m_Aura->m_Config->m_UDPBroadcastEnabled) {
        Print("[DISCOVERY] Failed to start UDP/IPv4 service. Cannot start broadcast service.");
      } else {
        Print("[DISCOVERY] Failed to start UDP/IPv4 service");
      }
      Print("====================================================================");
      return false;
    }
  }

  uint16_t ipv6Port = CFG->GetUint16("net.udp_ipv6.port", 6110);
  if (m_SupportUDPOverIPv6) {
    m_UDPIPv6Server = new CUDPServer(AF_INET6);
    m_UDPIPv6Server->SetDontRoute(useDoNotRoute);
    if (!m_UDPIPv6Server->Listen(m_Aura->m_Config->m_BindAddress6, ipv6Port, true)) {
      Print("====================================================================");
      Print("[DISCOVERY] Failed to bind to port " + to_string(ipv6Port) + ".");
      Print("[DISCOVERY] For a random available port, set <net.udp_ipv6.port = 0>");
      Print("[DISCOVERY] Failed to start UDP/IPv6 service");
      Print("====================================================================");
      return false;
    }
  }

  m_UDP6TargetPort = m_Aura->m_Config->m_UDP6TargetPort;
  if (m_Aura->m_Config->m_UDPBroadcastEnabled) PropagateBroadcastEnabled(true);
  SetBroadcastTarget(m_Aura->m_Config->m_BroadcastTarget);
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
      m_Aura->HandleHealthCheck();
    }
  }

  if (m_UDPMainServerEnabled) {
    UDPPkt* pkt = m_UDPMainServer->Accept(static_cast<fd_set*>(fd));
    if (pkt != nullptr) {
      m_Aura->HandleUDP(pkt);
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
  if (!m_Aura->m_Config->m_UDPBroadcastEnabled)
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

  if (m_UDPMainServerEnabled) {
    m_UDPMainServer->SendTo(address, packet);
  } else {
    m_UDPDeafSocket->SendTo(address, packet);
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

  if (m_Aura->m_Config->m_UDPBroadcastEnabled)
    PropagateBroadcastEnabled(false);

  Send(address, packet);

  if (m_Aura->m_Config->m_UDPBroadcastEnabled)
    PropagateBroadcastEnabled(true);
}

void CNet::SendGameDiscovery(const vector<uint8_t>& packet, const set<string>& clientIps)
{
  if (!SendBroadcast(packet))
    return;

  if (!clientIps.empty()) {
    if (m_Aura->m_Config->m_UDPBroadcastEnabled)
      PropagateBroadcastEnabled(false);

    for (auto& clientIp : clientIps)
      Send(clientIp, packet);

    if (m_Aura->m_Config->m_UDPBroadcastEnabled)
      PropagateBroadcastEnabled(true);
  }

  if (m_Aura->m_Config->m_EnableTCPWrapUDP) for (auto& pair : m_IncomingConnections) {
    for (auto& connection : pair.second) {
      if (connection->GetDeleteMe()) continue;
      if (connection->m_IsUDPTunnel) {
        connection->Send(packet);
      }
    }
  }
}

vector<uint8_t> CNet::GetPublicIP()
{
  static pair<string, vector<uint8_t>> memoized = make_pair(string(), vector<uint8_t>());
  vector<uint8_t> emptyIP;

  switch (m_Aura->m_Config->m_PublicIPv4Algorithm) {
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL: {
      return ExtractIPv4(m_Aura->m_Config->m_PublicIPv4Value);
    }
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_API: {
      if (memoized.first == m_Aura->m_Config->m_PublicIPv4Value) {
        return memoized.second;
      }
      auto response = cpr::Get(cpr::Url{m_Aura->m_Config->m_PublicIPv4Value});
      if (response.status_code != 200) {
        return emptyIP;
      }

      vector<uint8_t> result = ExtractIPv4(response.text);
      if (result.size() != 4) {
        return emptyIP;
      }

      memoized = make_pair(m_Aura->m_Config->m_PublicIPv4Value, result);
      return memoized.second;
    }
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE:
    default:
      return emptyIP;
  }
}

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

void CNet::StartHealthCheck(const vector<tuple<string, uint8_t, vector<uint8_t>, uint16_t>> testServers)
{
  for (auto& testServer: testServers) {
    vector<uint8_t> ipBytes = get<2>(testServer);
    sockaddr_storage targetHost;
    sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(&targetHost);
    memset(addr4, 0, sizeof(sockaddr_in));
    addr4->sin_family = AF_INET;
    addr4->sin_port = htons(get<3>(testServer));
    addr4->sin_addr.s_addr = *reinterpret_cast<const uint32_t*>(ipBytes.data());
    m_HealthCheckClients.push_back(new CTestConnection(m_Aura, targetHost, get<1>(testServer), get<0>(testServer)));
  }
  m_HealthCheckInProgress = true;
}

void CNet::ResetHealthCheck()
{
  for (auto& testConnection : m_HealthCheckClients) {
    delete testConnection;
  }
  m_HealthCheckClients.clear();
  m_HealthCheckInProgress = false;
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

vector<uint16_t> CNet::GetPotentialGamePorts() const
{
  vector<uint16_t> result;

  uint16_t port = m_Aura->m_Config->m_MinHostPort;
  if (m_Aura->m_Config->m_UDPEnableCustomPortTCP4 && m_Aura->m_Config->m_UDPCustomPortTCP4 < port) {
    result.push_back(m_Aura->m_Config->m_UDPCustomPortTCP4);
  }
  while (port <= m_Aura->m_Config->m_MaxHostPort) {
    result.push_back(port);
    ++port;
  }
  if (m_Aura->m_Config->m_UDPEnableCustomPortTCP4 && m_Aura->m_Config->m_UDPCustomPortTCP4 > port) {
    result.push_back(m_Aura->m_Config->m_UDPCustomPortTCP4);
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

  auto it = m_DNSCache.find(hostName);
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

  m_DNSCache[hostName] = address;
  result = address;
  return result;
}

void CNet::FlushDNS()
{
  m_DNSCache.clear();
}

void CNet::PropagateBroadcastEnabled(const bool nEnable)
{
  if (m_UDPMainServerEnabled) {
    m_UDPMainServer->SetBroadcastEnabled(nEnable);
  } else {
    m_UDPDeafSocket->SetBroadcastEnabled(nEnable);
  }
}

void CNet::OnConfigReload()
{
  m_UDP6TargetPort = m_Aura->m_Config->m_UDP6TargetPort;
  PropagateBroadcastEnabled(m_Aura->m_Config->m_UDPBroadcastEnabled);
  SetBroadcastTarget(m_Aura->m_Config->m_BroadcastTarget);
}

CNet::~CNet()
{
  delete m_UDPMainServer;
  delete m_UDPDeafSocket;
  delete m_UDP4BroadcastTarget;
  delete m_UDP6BroadcastTarget;
}
