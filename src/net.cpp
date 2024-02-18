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
#include <cpr/cpr.h>

#include "net.h"

using namespace std;

//
// CTestConnection
//

CTestConnection::CTestConnection(const vector<uint8_t> nAddress, const uint16_t nPort, const string nName)
  : m_Address(nAddress),
    m_Port(nPort),
    m_Name(nName),
    m_Timeout(0),
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

bool CTestConnection::Update(void* fd, void* send_fd)
{
  const static string emptyBindAddress;

  if (m_Passed.has_value()) {
    return !m_Passed.has_value();
  }

  const uint64_t Ticks = GetTicks();
  if (m_Socket->GetConnected() || m_Socket->GetConnecting() && m_Socket->CheckConnect()) {
    Print("[HEALTH] Connected to " + IPv4ToString(m_Address) + ":" + to_string(m_Port) + ".");
    m_Socket->DoRecv(static_cast<fd_set*>(fd));
    m_Socket->GetBytes()->clear();
    m_Passed = true;
    m_Socket->Disconnect();
  } else if (m_Socket->HasError() || m_Socket->GetConnecting() && Ticks > m_Timeout) {
    m_Passed = false;
    m_Socket->Reset();
  } else if (!m_Socket->GetConnecting()) {
    Print("[HEALTH] Connecting to " + IPv4ToString(m_Address) + ":" + to_string(m_Port) + "...");
    m_Socket->Connect(emptyBindAddress, IPv4ToString(m_Address), m_Port);
    m_Timeout = Ticks + 3000;
  }

  return !m_Passed.has_value();
}

//
// CNet
//

CNet::CNet()
  : m_UDPServerEnabled(false),
    m_UDPServer(nullptr),
    m_UDPSocket(new CUDPSocket()),

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
  if (m_UDPServerEnabled) {
    m_UDPServer = new CUDPServer();
    m_UDPServer->SetBroadcastTarget(CFG->GetString("net_udp_broadcast_target", string()));
    m_UDPServer->SetDontRoute(CFG->GetBool("net_udp_do_not_route", false));
    if (!m_UDPServer->Listen(CFG->GetString("net_bind_address", "0.0.0.0"), 6112, false)) {
      Print("[UDPSERVER] waiting for active instances of Warcraft to close... Please release port 6112.");
      this_thread::sleep_for(chrono::milliseconds(5000));
      if (!m_UDPServer->Listen(CFG->GetString("net_bind_address", "0.0.0.0"), 6112, true)) {
        return false;
      }
    }
  }

  m_UDPSocket->SetBroadcastTarget(CFG->GetString("net_udp_broadcast_target", string()));
  m_UDPSocket->SetDontRoute(CFG->GetBool("net_udp_do_not_route", false));
  return true;
}

void CNet::SendBroadcast(uint16_t port, const vector<uint8_t>& message)
{
  if (!m_UDPServerEnabled || !m_UDPServer->Broadcast(port, message)) {
    m_UDPSocket->Broadcast(port, message);
  }
}

vector<uint8_t> CNet::GetPublicIP()
{
  static vector<uint8_t> memoized;
  if (memoized.size() == 4) {
    return memoized;
  }

  vector<uint8_t> emptyIP;
  auto response = cpr::Get(cpr::Url{"https://api.ipify.org"});
  if (response.status_code != 200) {
    return emptyIP;
  }

  vector<uint8_t> result = ExtractIPv4(response.text);
  if (result.size() != 4) {
    return emptyIP;
  }

  memoized = result;
  return memoized;
}

void CNet::StartHealthCheck(const vector<pair<string, vector<uint8_t>>> testServers)
{
  for (uint16_t i = 0; i < testServers.size(); ++i) {
    const string id = testServers[i].first;
    vector<uint8_t> ip(testServers[i].second.begin(), testServers[i].second.begin() + 4);
    uint16_t port = testServers[i].second[4];
    port = (port << 8) + static_cast<uint16_t>(testServers[i].second[5]);
    m_HealthCheckClients.push_back(new CTestConnection(ip, port, id));
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

CNet::~CNet() = default;
