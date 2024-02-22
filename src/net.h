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
#include "config.h"
#include "aura.h"

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

#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_NONE 0
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_MANUAL 1
#define NET_PUBLIC_IP_ADDRESS_ALGORITHM_API 2

//
// CNet
//

class CAura;
class CUDPServer;
class CUDPSocket;
class CTCPSocket;
class CPotentialPlayer;
class CConfig;

class CTestConnection
{
public:
  CTestConnection(CAura* nAura, const std::vector<uint8_t> nAddress, const uint16_t nPort, const uint8_t nType, const std::string nName);
  ~CTestConnection();

  uint32_t  SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool      Update(void* fd, void* send_fd);
  bool      QueryGameInfo();

  CAura*                      m_Aura;
  std::vector<uint8_t>        m_Address;
  uint16_t                    m_Port;
  uint8_t                     m_Type;
  std::string                 m_Name;
  CTCPClient*                 m_Socket;
  std::optional<bool>         m_Passed;
  std::optional<bool>         m_CanConnect;
  int64_t                     m_Timeout;
  bool                        m_SentJoinRequest;
};

class CNet
{
public:
  CNet(CAura* nAura);
  ~CNet();

  CAura*                                                      m_Aura;
  bool                                                        m_UDPServerEnabled;           // whether the bot should listen to UDP traffic in port 6112)
  CUDPServer*                                                 m_UDPServer;                  // a UDP server for i/o: incoming traffic, and sending broadcasts, etc. (used with !sendlan), proxying UDP game traffic, game lists, etc
  CUDPServer*                                                 m_UDPSocket;                  // a UDP server for outgoing traffic, meant not to block port 6112

  std::map<uint16_t, CTCPServer*>                             m_GameServers;
  std::map<uint16_t, std::vector<CPotentialPlayer*>>          m_IncomingConnections;        // (connections that haven't sent a W3GS_REQJOIN packet yet)
  std::vector<CTestConnection*>                               m_HealthCheckClients;
  bool                                                        m_HealthCheckInProgress;
  
  uint16_t                                                    m_GameRangerLocalPort;
  std::string                                                 m_GameRangerLocalAddress;
  uint16_t                                                    m_GameRangerRemotePort;
  std::vector<uint8_t>                                        m_GameRangerRemoteAddress;

  bool Init(const CConfig* CFG);
  void SendBroadcast(const uint16_t port, const std::vector<uint8_t>& packet);
  void Send(const std::string& address, const uint16_t port, const std::vector<uint8_t>& packet);
  void SendGameDiscovery(const std::vector<uint8_t>& packet, const std::set<std::string>& clientIps);

  std::vector<uint8_t> GetPublicIP();
  std::vector<uint16_t> GetPotentialGamePorts();
  uint8_t EnableUPnP(const uint16_t externalPort, const uint16_t internalPort);
  void StartHealthCheck(const std::vector<std::tuple<std::string, uint8_t, std::vector<uint8_t>, uint16_t>> testServers);
  void ResetHealthCheck();

  static std::optional<sockaddr_in> ResolveHost(const std::string& hostName);
};

#endif // AURA_NET_H_
