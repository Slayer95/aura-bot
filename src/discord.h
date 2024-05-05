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

#ifndef AURA_DISCORD_H_
#define AURA_DISCORD_H_

#include "config_discord.h"
#include <vector>
#include <string>
#include <cstdint>
#include <queue>

#ifndef DISABLE_DPP
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4251)
#endif

#include <dpp/dpp.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#endif

class CAura;
class CDiscordConfig;

class CDiscord
{
public:
  CAura*                   m_Aura;
#ifndef DISABLE_DPP
  dpp::cluster*                     m_Client;
#else
  void*                             m_Client;
#endif
  CDiscordConfig*                   m_Config;
  std::string                       m_NickName;
  int64_t                           m_LastPacketTime;
  int64_t                           m_LastAntiIdleTime;
  bool                              m_Exiting;
  bool                              m_WaitingToConnect;
#ifndef DISABLE_DPP
  std::queue<dpp::slashcommand_t*>  m_CommandQueue;
#endif

  CDiscord(CAura* nAura);
  ~CDiscord();
  CDiscord(CDiscord&) = delete;

  bool Update();

#ifndef DISABLE_DPP
  bool Init();
  void RegisterCommands();
  void SendUser(const std::string& message, const uint64_t target);
  void LeaveServer(const uint64_t target, const std::string& name, const bool isJoining);
#endif

  bool GetIsServerAllowed(const uint64_t target) const;
  bool GetIsUserAllowed(const uint64_t target) const;
  bool GetIsActive() const { return m_Client != nullptr; }
  bool GetIsSudoer(const uint64_t nIdentifier);
};

#endif // AURA_DISCORD_H_
