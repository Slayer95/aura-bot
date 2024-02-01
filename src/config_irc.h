#ifndef AURA_CONFIG_IRC_H_
#define AURA_CONFIG_IRC_H_

#include "config.h"

#include <vector>
#include <string>
#include <map>
#include <unordered_set>

//
// CIRCConfig
//

class CIRCConfig
{
public:
  std::string                         m_HostName;
  std::string                         m_NickName;
  std::string                         m_UserName;
  std::string                         m_Password;
  std::vector<std::string>            m_Channels;
  std::set<std::string>               m_RootAdmins;
  uint16_t                            m_Port;
  char                                m_CommandTrigger;

  explicit CIRCConfig(CConfig* CFG);
  ~CIRCConfig();
};

#endif
