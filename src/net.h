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

#include <map>
#include <optional>
#include <utility>

#define HEALTH_STANDBY 0
#define HEALTH_PROGRESS 1
#define HEALTH_OKAY 2
#define HEALTH_ERROR 3

//
// CNet
//

class CUDPServer;
class CUDPSocket;
class CTCPSocket;
class CPotentialPlayer;
class CConfig;

class CTestConnection
{
public:
  CTestConnection(const std::vector<uint8_t> nAddress, const uint16_t nPort, const std::string nName);
  ~CTestConnection();

  uint32_t  SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool      Update(void* fd, void* send_fd);

  std::vector<uint8_t>        m_Address;
  uint16_t                    m_Port;
  std::string                 m_Name;
  CTCPClient*                 m_Socket;
  std::optional<bool>         m_Passed;
  int64_t                     m_Timeout;
};

class CNet
{
public:
  CNet();
  ~CNet();

  bool                                                        m_UDPServerEnabled;           // whether the bot should listen to UDP traffic in port 6112)
  CUDPServer*                                                 m_UDPServer;                  // a UDP server for incoming traffic, and sending broadcasts, etc. (used with !sendlan)
  CUDPSocket*                                                 m_UDPSocket;                  // a UDP socket for proxying UDP traffic, or broadcasting game info without blocking port 6112

  std::map<uint16_t, CTCPServer*>                             m_GameServers;
  std::map<uint16_t, std::vector<CPotentialPlayer*>>          m_IncomingConnections;        // (connections that haven't sent a W3GS_REQJOIN packet yet)
  std::vector<CTestConnection*>                               m_HealthCheckClients;
  bool                                                        m_HealthCheckInProgress;
  
  uint16_t                                                    m_GameRangerLocalPort;
  std::string                                                 m_GameRangerLocalAddress;
  uint16_t                                                    m_GameRangerRemotePort;
  std::vector<uint8_t>                                        m_GameRangerRemoteAddress;

  bool Init(const CConfig* CFG);
  void SendBroadcast(uint16_t port, const std::vector<uint8_t>& message);

  std::vector<uint8_t> GetPublicIP();
  void StartHealthCheck(const std::vector<std::pair<std::string, std::vector<uint8_t>>> testServers);
  void ResetHealthCheck();
};

#endif // AURA_NET_H_
