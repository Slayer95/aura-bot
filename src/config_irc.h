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
  bool                                m_Enabled;
  bool                                m_EnablePublicCreate;
  std::vector<std::string>            m_Channels;
  std::set<std::string>               m_RootAdmins;
  uint16_t                            m_Port;
  char                                m_CommandTrigger;

  explicit CIRCConfig(CConfig* CFG);
  ~CIRCConfig();
};

#endif
