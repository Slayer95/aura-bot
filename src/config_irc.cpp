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

#include "config_irc.h"
#include "util.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CIRCConfig
//

CIRCConfig::CIRCConfig(CConfig* CFG)
  : m_Port(6667),
  m_CommandTrigger('!')
{
  m_HostName            = CFG->GetString("irc_host_name", string());
  m_Port                = CFG->GetInt("irc_port", 6667);
  m_NickName            = CFG->GetString("irc_nickname", string());
  m_UserName            = CFG->GetString("irc_username", string());
  m_Password            = CFG->GetString("irc_password", string());
  m_Enabled             = CFG->GetBool("irc_enabled", false);
  m_EnablePublicCreate  = CFG->GetBool("irc_allow_host_non_admins", false);

  string CommandTrigger = CFG->GetString("irc_command_trigger", "!");
  if (CommandTrigger.length() == 1) {
    m_CommandTrigger = CommandTrigger[0];
  }

  if (m_UserName.empty())
    m_UserName = m_NickName;
  if (m_NickName.empty())
    m_NickName = m_UserName;

  m_Channels = CFG->GetList("irc_channels", ',', m_Channels);
  m_RootAdmins = CFG->GetSet("irc_root_admins", ',', m_RootAdmins);

  for (uint8_t i = 0; i < m_Channels.size(); ++i) {
    if (m_Channels[i].length() > 0 && m_Channels[i][0] != '#') {
      m_Channels[i] = "#" + m_Channels[i];
    }
  }
}

CIRCConfig::~CIRCConfig() = default;
