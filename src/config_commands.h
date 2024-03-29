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

#ifndef AURA_CONFIG_COMMANDS_H_
#define AURA_CONFIG_COMMANDS_H_

#include "config.h"

#include <vector>
#include <string>
#include <optional>
#include <map>

#define REALM_AUTH_PVPGN 0
#define REALM_AUTH_BATTLENET 1

#define COMMAND_PERMISSIONS_DISABLED 0
#define COMMAND_PERMISSIONS_SUDO 1
#define COMMAND_PERMISSIONS_SUDO_UNSAFE 2
#define COMMAND_PERMISSIONS_ROOTADMIN 3
#define COMMAND_PERMISSIONS_ADMIN 4
#define COMMAND_PERMISSIONS_VERIFIED_OWNER 5
#define COMMAND_PERMISSIONS_OWNER 6
#define COMMAND_PERMISSIONS_VERIFIED 7
#define COMMAND_PERMISSIONS_AUTO 8
#define COMMAND_PERMISSIONS_POTENTIAL_OWNER 9
#define COMMAND_PERMISSIONS_UNVERIFIED 10

#define COMMANDS_ALLOWED_NONE 0
#define COMMANDS_ALLOWED_UNVERIFIED 1
#define COMMANDS_ALLOWED_VERIFIED 2

//
// CCommandConfig
//

class CCommandConfig
{
public:
  bool m_Enabled;
  bool m_RequireVerified;                          // whether commands are enabled or not, and whether they require verification if sent by a CGamePlayer

  uint8_t m_CommonBasePermissions;
  uint8_t m_HostingBasePermissions;
  uint8_t m_ModeratorBasePermissions;
  uint8_t m_AdminBasePermissions;
  uint8_t m_BotOwnerBasePermissions;

  uint8_t m_HostPermissions;
  uint8_t m_HostRawPermissions;
  uint8_t m_StartPermissions;
  uint8_t m_TellPermissions;
  uint8_t m_WhoisPermissions;

  std::string m_CFGKeyPrefix;                     // 

  CCommandConfig(CConfig& CFG, const std::string& nKeyPrefix, const bool requireVerified, const uint8_t commonPermissions, const uint8_t hostingPermissions, const uint8_t moderatorPermissions, const uint8_t adminPermissions, const uint8_t botOwnerPermissions);
  ~CCommandConfig();
};

#endif
