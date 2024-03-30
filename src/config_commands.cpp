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

#include "config_commands.h"
#include "command.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CCommandConfig
//

CCommandConfig::CCommandConfig(CConfig& CFG, const string& nKeyPrefix, const bool requireVerified, const uint8_t commonPermissions, const uint8_t hostingPermissions, const uint8_t moderatorPermissions, const uint8_t adminPermissions, const uint8_t botOwnerPermissions)
  : m_CFGKeyPrefix(nKeyPrefix)
{
  vector<string> commandPermissions = {"disabled", "sudo", "sudo_unsafe", "rootadmin", "admin", "verified_owner", "owner", "verified", "auto", "potential_owner", "unverified"};

  m_CommonBasePermissions = commonPermissions;
  m_HostingBasePermissions = hostingPermissions;
  m_ModeratorBasePermissions = moderatorPermissions;
  m_AdminBasePermissions = adminPermissions;
  m_BotOwnerBasePermissions = botOwnerPermissions;

  m_HostPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_host.permissions", commandPermissions, hostingPermissions);
  m_HostRawPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_hostraw.permissions", commandPermissions, hostingPermissions);
  m_StartPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_start.permissions", commandPermissions, hostingPermissions);
  m_SayPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_say.permissions", commandPermissions, moderatorPermissions);
  m_TellPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_tell.permissions", commandPermissions, moderatorPermissions);
  m_WhoisPermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.custom_whois.permissions", commandPermissions, moderatorPermissions);

  m_Enabled = CFG.GetBool(m_CFGKeyPrefix + "commands.enabled", true);
  m_RequireVerified = requireVerified;
}

CCommandConfig::~CCommandConfig() = default;
