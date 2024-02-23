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

CTestConnection::CTestConnection(CAura* nAura, const vector<uint8_t> nAddress, const uint16_t nPort, const uint8_t nType, const string nName)
  : m_Aura(nAura),
    m_Address(nAddress),
    m_Port(nPort),
    m_Type(nType),
    m_Name(nName),
    m_Timeout(0),
    m_SentJoinRequest(false),
    m_Socket(new CTCPClient(nName))
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
  const static string emptyBindAddress;

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
    m_Socket->Connect(emptyBindAddress, IPv4ToString(m_Address), m_Port);
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

bool CNet::Init(const CConfig* CFG)
{
  // m_UDPServerEnabled may be overriden by the cli.
  bool useDoNotRoute = CFG->GetBool("net.game_discovery.udp.do_not_route", false);
  string emptyString;
  string broadcastTarget = CFG->GetString("net.game_discovery.udp.broadcast.target", emptyString);
  string bindAddress = CFG->GetString("net.bind_address", "0.0.0.0");
  if (m_UDPServerEnabled) {
    m_UDPServer = new CUDPServer("UDPv4-" + to_string(6112));
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

  uint16_t socketPort = static_cast<uint16_t>(CFG->GetInt("net.udp_fallback.outbound_port", 6113));
  m_UDPSocket = new CUDPServer("UDPv4-" + to_string(socketPort));
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

optional<sockaddr_in> CNet::ResolveHost(const string& hostName)
{
  optional<sockaddr_in> maybeSin;

  // get IP address

  struct hostent* HostInfo;
  uint32_t        HostAddress;
  HostInfo = gethostbyname(hostName.c_str());

  if (!HostInfo) {
    return maybeSin;
  }

  memcpy(&HostAddress, HostInfo->h_addr, HostInfo->h_length);

  struct sockaddr_in sin;
  sin.sin_family      = AF_INET;
  sin.sin_addr.s_addr = HostAddress;
  sin.sin_port        = htons(80);
  maybeSin = sin;

  return maybeSin;
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
    m_HealthCheckClients.push_back(new CTestConnection(m_Aura, get<2>(testServer), get<3>(testServer), get<1>(testServer), get<0>(testServer)));
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

CNet::~CNet()
{
  delete m_UDPServer;
  delete m_UDPSocket;
}
