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

#ifndef AURA_CONFIG_DISCORD_H_
#define AURA_CONFIG_DISCORD_H_

#include "config.h"
#include "config_commands.h"

#include <vector>
#include <string>
#include <map>

#define FILTER_SERVERS_ALLOW_ALL 0
#define FILTER_SERVERS_DENY_ALL 1
#define FILTER_SERVERS_ALLOW_LIST 2
#define FILTER_SERVERS_DENY_LIST 3

//
// CDiscordConfig
//

class CDiscordConfig
{
public:
  std::string                         m_HostName;
  std::string                         m_Token;
  bool                                m_Enabled;
  uint8_t                             m_FilterJoinServersMode;
  std::set<uint64_t>                  m_FilterJoinServersList;
  std::set<uint64_t>                  m_SudoUsers;
  CCommandConfig*                     m_CommandCFG;

  explicit CDiscordConfig(CConfig& CFG);
  ~CDiscordConfig();
};

#endif
