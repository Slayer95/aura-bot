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
  const static string emptyString;
  m_HostName            = CFG->GetString("irc.host_name", emptyString);
  m_Port                = CFG->GetInt("irc.port", 6667);
  m_NickName            = CFG->GetString("irc.nickname", emptyString);
  m_UserName            = CFG->GetString("irc.username", emptyString);
  m_Password            = CFG->GetString("irc.password", emptyString);
  m_Enabled             = CFG->GetBool("irc.enabled", false);
  m_EnablePublicCreate  = CFG->GetBool("irc.allow_host_non_admins", false);

  string CommandTrigger = CFG->GetString("irc.command_trigger", "!");
  if (CommandTrigger.length() == 1) {
    m_CommandTrigger = CommandTrigger[0];
  }

  if (m_UserName.empty())
    m_UserName = m_NickName;
  if (m_NickName.empty())
    m_NickName = m_UserName;

  m_Channels = CFG->GetList("irc.channels", ',', m_Channels);
  m_RootAdmins = CFG->GetSet("irc.root_admins", ',', m_RootAdmins);

  for (uint8_t i = 0; i < m_Channels.size(); ++i) {
    if (m_Channels[i].length() > 0 && m_Channels[i][0] != '#') {
      m_Channels[i] = "#" + m_Channels[i];
    }
  }

  if (m_Enabled && (m_HostName.empty() || m_NickName.empty() || m_Port == 0)) {
    CFG->SetFailed();
  }
}

CIRCConfig::~CIRCConfig() = default;
