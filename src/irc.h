/*

   Copyright [2010] [Josko Nikolic]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

 */

#ifndef AURA_IRC_H_
#define AURA_IRC_H_

#include "config_irc.h"
#include <vector>
#include <string>
#include <cstdint>

#define LF ('\x0A')

class CAura;
class CTCPClient;
class CIRCConfig;

class CIRC
{
public:
  CAura*                   m_Aura;
  CTCPClient*              m_Socket;
  CIRCConfig*              m_Config;
  std::string              m_NickName;
  int64_t                  m_LastConnectionAttemptTime;
  int64_t                  m_LastPacketTime;
  int64_t                  m_LastAntiIdleTime;
  bool                     m_Exiting;
  bool                     m_WaitingToConnect;

  CIRC(CAura* nAura, CIRCConfig* nConfig);
  ~CIRC();
  CIRC(CIRC&) = delete;

  uint32_t SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool Update(void* fd, void* send_fd);
  void ExtractPackets();
  void Send(const std::string& message);
  void SendUser(const std::string& message, const std::string& target);
  void SendChannel(const std::string& message, const std::string& target);
  void SendAllChannels(const std::string& message);
};

#endif // AURA_IRC_H_
