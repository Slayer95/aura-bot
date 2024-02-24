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
    m_UDPServerEnabled(false),
    m_UDPServer(nullptr),
    m_UDPSocket(nullptr),
    m_UDP4TargetPort(6112),
    m_UDP6TargetPort(5678), // <net.game_discovery.udp.ipv6.target_port>

    m_HealthCheckInProgress(false),

    m_GameRangerLocalPort(0),
    m_GameRangerLocalAddress("255.255.255.255"),
    m_GameRangerRemotePort(0),
    m_GameRangerRemoteAddress({0xFF, 0xFF, 0xFF, 0xFF})
{
}

bool CNet::Init(CConfig* CFG)
{
  // m_UDPServerEnabled may be overriden by the cli.
  bool useDoNotRoute = CFG->GetBool("net.game_discovery.udp.do_not_route", false);
  sockaddr_storage broadcastTarget = CFG->GetAddress("net.game_discovery.udp.broadcast.target", "255.255.255.255");
  if (CFG->GetError() || broadcastTarget.ss_family != AF_INET) {
    Print("[CONFIG] Error: Invalid value for <net.game_discovery.udp.broadcast.target>. Provide an IPv4 subnet or 255.255.255.255");
    return false;
  }
  sockaddr_storage bindAddress = CFG->GetAddress("net.bind_address", "0.0.0.0");
  if (CFG->GetError() || bindAddress.ss_family != AF_INET) {
    Print("[CONFIG] Error: Invalid value for <net.game_discovery.udp.broadcast.target>. Provide an IP address or 0.0.0.0");
    return false;
  }
  if (m_UDPServerEnabled) {
    m_UDPServer = new CUDPServer(AF_INET);
    m_UDPServer->SetBroadcastTarget(broadcastTarget);
    m_UDPServer->SetDontRoute(useDoNotRoute);
    if (!m_UDPServer->Listen(bindAddress, 6112, false)) {
      Print("[UDPSERVER] waiting for active instances of Warcraft to close... Please release port 6112.");
      this_thread::sleep_for(chrono::milliseconds(5000));
      if (!m_UDPServer->Listen(bindAddress, 6112, true)) {
        return false;
      }
    }
  }

  uint16_t socketPort = CFG->GetUint16("net.udp_fallback.outbound_port", 6113);
  m_UDPSocket = new CUDPServer(AF_INET);
  m_UDPSocket->SetBroadcastTarget(broadcastTarget);
  m_UDPSocket->SetDontRoute(useDoNotRoute);
  if (!m_UDPSocket->Listen(bindAddress, socketPort, true) && !m_UDPServerEnabled) {
    Print("[UDPFALLBACK] failed to bind to port " + to_string(socketPort) + ".");
    Print("[UDPFALLBACK] for a random available port, set net.udp_fallback.outbound_port = 0");
    return false;
  }
  return true;
}

void CNet::SendBroadcast(const uint16_t port, const vector<uint8_t>& packet)
{
  if (!m_UDPServerEnabled || !m_UDPServer->Broadcast(port, packet)) {
    m_UDPSocket->Broadcast(port, packet);
  }
}

void CNet::Send(const string& address, const vector<uint8_t>& packet)
{
  const bool isIPv6 = ExtractIPv4(address).empty();
  if (m_UDPServerEnabled) {
    m_UDPServer->SendTo(address, isIPv6 ? m_UDP6TargetPort : m_UDP4TargetPort, packet);
  } else {
    m_UDPSocket->SendTo(address, isIPv6 ? m_UDP6TargetPort : m_UDP4TargetPort, packet);
  }
}

void CNet::Send(const string& address, const uint16_t port, const vector<uint8_t>& packet)
{
  if (m_UDPServerEnabled) {
    m_UDPServer->SendTo(address, port, packet);
  } else {
    m_UDPSocket->SendTo(address, port, packet);
  }
}

void CNet::SendGameDiscovery(const vector<uint8_t>& packet, const set<string>& clientIps)
{
  if (m_Aura->m_Config->m_UDPBroadcastEnabled) {
    m_Aura->m_Net->SendBroadcast(m_UDP4TargetPort, packet);
  }

  for (auto& clientIp : clientIps) {
    Send(clientIp, packet);
  }
}

vector<uint8_t> CNet::GetPublicIP()
{
  static pair<string, vector<uint8_t>> memoized = make_pair(string(), vector<uint8_t>());
  vector<uint8_t> emptyIP;

  switch (m_Aura->m_Config->m_PublicIPAlgorithm) {
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL: {
      return ExtractIPv4(m_Aura->m_Config->m_PublicIPValue);
    }
    case NET_PUBLIC_IP_ADDRESS_ALGORITHM_API: {
      if (memoized.first == m_Aura->m_Config->m_PublicIPValue) {
        return memoized.second;
      }
      auto response = cpr::Get(cpr::Url{m_Aura->m_Config->m_PublicIPValue});
      if (response.status_code != 200) {
        return emptyIP;
      }

      vector<uint8_t> result = ExtractIPv4(response.text);
      if (result.size() != 4) {
        return emptyIP;
      }

      memoized = make_pair(m_Aura->m_Config->m_PublicIPValue, result);
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

vector<uint16_t> CNet::GetPotentialGamePorts()
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

optional<sockaddr_storage> CNet::ParseAddress(const string& address)
{
  std::optional<sockaddr_storage> result;
  // Try IPv4
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

  // Try IPv6
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

CNet::~CNet()
{
  delete m_UDPServer;
  delete m_UDPSocket;
}
