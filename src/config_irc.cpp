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
  m_HostName            = CFG->GetString("irc_hostname", string());
  m_Port                = CFG->GetInt("irc_port", 6667);
  m_NickName            = CFG->GetString("irc_nickname", string());
  m_UserName            = CFG->GetString("irc_username", string());
  m_Password            = CFG->GetString("irc_password", string());
  m_Enabled             = CFG->GetBool("irc_enabled", false);
  m_EnablePublicCreate  = CFG->GetBool("irc_allowhostnonadmins", false);

  string CommandTrigger = CFG->GetString("irc_commandtrigger", "!");
  if (CommandTrigger.length() == 1) {
    m_CommandTrigger = CommandTrigger[0];
  }

  if (m_UserName.empty())
    m_UserName = m_NickName;
  if (m_NickName.empty())
    m_NickName = m_UserName;

  m_Channels = CFG->GetList("irc_channels", ',', m_Channels);
  m_RootAdmins = CFG->GetSet("irc_rootadmins", ',', m_RootAdmins);

  for (uint8_t i = 0; i < m_Channels.size(); ++i) {
    if (m_Channels[i].length() > 0 && m_Channels[i][0] != '#') {
      m_Channels[i] = "#" + m_Channels[i];
    }
  }
}

CIRCConfig::~CIRCConfig() = default;
