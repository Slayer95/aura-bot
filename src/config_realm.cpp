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

#include "config_realm.h"
#include "config_bot.h"
#include "util.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CRealmConfig
//

CRealmConfig::CRealmConfig(CConfig* CFG, CNetConfig* NetConfig)
  : m_CountryShort("PE"),
    m_Country("Peru"),
    m_Locale("system"),
    m_LocaleID(10250),

    m_CommandTrigger('!'),
    m_EnablePublicCreate(true),
    m_AnnounceHostToChat(true),
    m_IsMirror(false),
    m_IsVPN(false),

    m_EnableCustomAddress(false),

    m_EnableCustomPort(false),
    m_PublicHostPort(6112),

    m_UserName(string()),
    m_PassWord(string()),

    m_AuthWar3Version(27),
    m_AuthExeVersion({173, 1, 27, 1}),
    m_AuthExeVersionHash({72, 160, 171, 170}),
    m_AuthPasswordHashType("pvpgn"),

    m_FirstChannel("The Void"),
    m_RootAdmins({}),
    m_GamePrefix(string()),
    m_MaxUploadSize(NetConfig->m_MaxUploadSize), // The setting in AuraCFG applies to LAN always.
    m_FloodImmune(false),

    m_Enabled(true),

    m_ServerIndex(0), // m_ServerIndex is one-based
    m_CFGKeyPrefix("global_realm.")
{
  const static string emptyString;
  m_CountryShort           = CFG->GetString(m_CFGKeyPrefix + "country_short", m_CountryShort);
  m_Country                = CFG->GetString(m_CFGKeyPrefix + "country", m_Country);
  m_Locale                 = CFG->GetString(m_CFGKeyPrefix + "locale", m_Locale);

  if (m_Locale == "system") {
    m_LocaleID = 10250;
  } else {
    try {
      m_LocaleID  = stoul(m_Locale);
    } catch (...) {
      m_Locale = "system";
    }
  }

  string CommandTrigger = CFG->GetString(m_CFGKeyPrefix + "command_trigger", "");
  if (CommandTrigger.length() == 1) {
    m_CommandTrigger       = CommandTrigger[0];
  }
  m_EnablePublicCreate     = CFG->GetBool(m_CFGKeyPrefix + "allow_host_non_admins", m_EnablePublicCreate);
  m_AnnounceHostToChat     = CFG->GetBool(m_CFGKeyPrefix + "announce_chat", m_AnnounceHostToChat);
  m_IsMirror               = CFG->GetBool(m_CFGKeyPrefix + "mirror", m_IsMirror);
  m_IsVPN                  = CFG->GetBool(m_CFGKeyPrefix + "vpn", m_IsVPN);

  m_EnableCustomAddress    = CFG->GetBool(m_CFGKeyPrefix + "custom_ip_address.enabled", m_EnableCustomAddress);
  m_PublicHostAddress      = CFG->GetAddressIPv4(m_CFGKeyPrefix + "custom_ip_address.value", "0.0.0.0");
  if (m_EnableCustomAddress)
    CFG->FailIfErrorLast();

  m_EnableCustomPort       = CFG->GetBool(m_CFGKeyPrefix + "custom_port.enabled", m_EnableCustomPort);
  m_PublicHostPort         = CFG->GetUint16(m_CFGKeyPrefix + "custom_port.value", m_PublicHostPort);
  if (m_EnableCustomPort)
    CFG->FailIfErrorLast();

  m_UserName               = CFG->GetString(m_CFGKeyPrefix + "username", m_UserName);
  m_PassWord               = CFG->GetString(m_CFGKeyPrefix + "password", m_PassWord);

  m_AuthWar3Version        = CFG->GetUint8(m_CFGKeyPrefix + "auth_game_version", m_AuthWar3Version);
  m_AuthExeVersion         = CFG->GetUint8Vector(m_CFGKeyPrefix + "auth_exe_version", 4, m_AuthExeVersion);
  m_AuthExeVersionHash     = CFG->GetUint8Vector(m_CFGKeyPrefix + "auth_exe_version_hash", 4, m_AuthExeVersionHash);
  m_AuthPasswordHashType   = CFG->GetString(m_CFGKeyPrefix + "auth_password_hash_type", m_AuthPasswordHashType);

  m_FirstChannel           = CFG->GetString(m_CFGKeyPrefix + "first_channel", m_FirstChannel);
  m_SudoUsers              = CFG->GetSet(m_CFGKeyPrefix + "sudo_users", ',', m_SudoUsers);
  m_RootAdmins             = CFG->GetSet(m_CFGKeyPrefix + "root_admins", ',', m_RootAdmins);
  m_GamePrefix             = CFG->GetString(m_CFGKeyPrefix + "game_prefix", m_GamePrefix);
  m_MaxUploadSize          = CFG->GetInt(m_CFGKeyPrefix + "map_transfers.max_size", m_MaxUploadSize);
  m_FloodImmune            = CFG->GetBool(m_CFGKeyPrefix + "flood_immune", m_FloodImmune);

  m_BindAddress            = CFG->GetMaybeAddress(m_CFGKeyPrefix + "bind_address");
  m_Enabled                = CFG->GetBool(m_CFGKeyPrefix + "enabled", true);
}

CRealmConfig::CRealmConfig(CConfig* CFG, CRealmConfig* nRootConfig, uint8_t nServerIndex)
  : m_CountryShort(nRootConfig->m_CountryShort),
    m_Country(nRootConfig->m_Country),
    m_Locale(nRootConfig->m_Locale),
    m_LocaleID(nRootConfig->m_LocaleID),

    m_CommandTrigger(nRootConfig->m_CommandTrigger),
    m_EnablePublicCreate(nRootConfig->m_EnablePublicCreate),
    m_AnnounceHostToChat(nRootConfig->m_AnnounceHostToChat),
    m_IsMirror(nRootConfig->m_IsMirror),
    m_IsVPN(nRootConfig->m_IsVPN),

    m_EnableCustomAddress(nRootConfig->m_EnableCustomAddress),
    m_PublicHostAddress(nRootConfig->m_PublicHostAddress),

    m_EnableCustomPort(nRootConfig->m_EnableCustomPort),
    m_PublicHostPort(nRootConfig->m_PublicHostPort),

    m_UserName(nRootConfig->m_UserName),
    m_PassWord(nRootConfig->m_PassWord),    

    m_AuthWar3Version(nRootConfig->m_AuthWar3Version),
    m_AuthExeVersion(nRootConfig->m_AuthExeVersion),
    m_AuthExeVersionHash(nRootConfig->m_AuthExeVersionHash),
    m_AuthPasswordHashType(nRootConfig->m_AuthPasswordHashType),

    m_FirstChannel(nRootConfig->m_FirstChannel),
    m_SudoUsers(nRootConfig->m_SudoUsers),
    m_RootAdmins(nRootConfig->m_RootAdmins),
    m_GamePrefix(nRootConfig->m_GamePrefix),
    m_MaxUploadSize(nRootConfig->m_MaxUploadSize),
    m_FloodImmune(nRootConfig->m_FloodImmune),

    m_BindAddress(nRootConfig->m_BindAddress),
    m_Enabled(nRootConfig->m_Enabled),

    m_ServerIndex(nServerIndex),
    m_CFGKeyPrefix("realm_" + to_string(nServerIndex) + ".")
{
  const static string emptyString;
  m_HostName               = CFG->GetString(m_CFGKeyPrefix + "host_name", emptyString);
  m_UniqueName             = CFG->GetString(m_CFGKeyPrefix + "unique_name", m_HostName);
  m_CanonicalName          = CFG->GetString(m_CFGKeyPrefix + "canonical_name", m_UniqueName); // may be shared by several servers
  m_InputID                = CFG->GetString(m_CFGKeyPrefix + "input_id", m_UniqueName); // expected unique
  transform(begin(m_InputID), end(m_InputID), begin(m_InputID), ::tolower);
  m_DataBaseID             = CFG->GetString(m_CFGKeyPrefix + "db_id", m_HostName); // may be shared by several servers
  m_CDKeyROC               = CFG->GetString(m_CFGKeyPrefix + "cd_key.roc", "FFFFFFFFFFFFFFFFFFFFFFFFFF");
  m_CDKeyTFT               = CFG->GetString(m_CFGKeyPrefix + "cd_key.tft", "FFFFFFFFFFFFFFFFFFFFFFFFFF");

  // remove dashes and spaces from CD keys and convert to uppercase
  m_CDKeyROC.erase(remove(begin(m_CDKeyROC), end(m_CDKeyROC), '-'), end(m_CDKeyROC));
  m_CDKeyTFT.erase(remove(begin(m_CDKeyTFT), end(m_CDKeyTFT), '-'), end(m_CDKeyTFT));
  m_CDKeyROC.erase(remove(begin(m_CDKeyROC), end(m_CDKeyROC), ' '), end(m_CDKeyROC));
  m_CDKeyTFT.erase(remove(begin(m_CDKeyTFT), end(m_CDKeyTFT), ' '), end(m_CDKeyTFT));
  transform(begin(m_CDKeyROC), end(m_CDKeyROC), begin(m_CDKeyROC), ::toupper);
  transform(begin(m_CDKeyTFT), end(m_CDKeyTFT), begin(m_CDKeyTFT), ::toupper);

  m_CountryShort           = CFG->GetString(m_CFGKeyPrefix + "country_short", m_CountryShort);
  m_Country                = CFG->GetString(m_CFGKeyPrefix + "country", m_Country);
  m_Locale                 = CFG->GetString(m_CFGKeyPrefix + "locale", m_Locale);

  if (m_Locale == "system") {
    m_LocaleID = 10250;
  } else {
    try {
      m_LocaleID  = stoul(m_Locale);
    } catch (...) {
      m_Locale = nRootConfig->m_Locale;
    }
  }

  string CommandTrigger    = CFG->GetString(m_CFGKeyPrefix + "command_trigger", "");
  if (CommandTrigger.length() == 1) {
    m_CommandTrigger       = CommandTrigger[0];
  }
  m_EnablePublicCreate     = CFG->GetBool(m_CFGKeyPrefix + "allow_host_non_admins", m_EnablePublicCreate);
  m_AnnounceHostToChat     = CFG->GetBool(m_CFGKeyPrefix + "announce_chat", m_AnnounceHostToChat);
  m_IsMirror               = CFG->GetBool(m_CFGKeyPrefix + "mirror", m_IsMirror);
  m_IsVPN                  = CFG->GetBool(m_CFGKeyPrefix + "vpn", m_IsVPN);

  m_EnableCustomAddress    = CFG->GetBool(m_CFGKeyPrefix + "custom_ip_address.enabled", m_EnableCustomAddress);
  optional<sockaddr_storage> maybeAddress = CFG->GetMaybeAddressIPv4(m_CFGKeyPrefix + "custom_ip_address.value");
  if (m_EnableCustomAddress)
    CFG->FailIfErrorLast();
  if (maybeAddress.has_value()) {
    m_PublicHostAddress    = maybeAddress.value();
  }

  m_EnableCustomPort       = CFG->GetBool(m_CFGKeyPrefix + "custom_port.enabled", m_EnableCustomPort);
  m_PublicHostPort         = CFG->GetUint16(m_CFGKeyPrefix + "custom_port.value", m_PublicHostPort);
  if (m_EnableCustomPort)
    CFG->FailIfErrorLast();

  m_UserName               = CFG->GetString(m_CFGKeyPrefix + "username", m_UserName);
  m_PassWord               = CFG->GetString(m_CFGKeyPrefix + "password", m_PassWord);

  m_AuthWar3Version        = CFG->GetUint8(m_CFGKeyPrefix + "auth_game_version", m_AuthWar3Version);
  m_AuthExeVersion         = CFG->GetUint8Vector(m_CFGKeyPrefix + "auth_exe_version", 4, m_AuthExeVersion);
  m_AuthExeVersionHash     = CFG->GetUint8Vector(m_CFGKeyPrefix + "auth_exe_version_hash", 4, m_AuthExeVersionHash);
  m_AuthPasswordHashType   = CFG->GetString(m_CFGKeyPrefix + "auth_password_hash_type", m_AuthPasswordHashType);

  m_FirstChannel           = CFG->GetString(m_CFGKeyPrefix + "first_channel", m_FirstChannel);
  m_SudoUsers              = CFG->GetSet(m_CFGKeyPrefix + "sudo_users", ',', m_SudoUsers);
  m_RootAdmins             = CFG->GetSet(m_CFGKeyPrefix + "root_admins", ',', m_RootAdmins);
  m_GamePrefix             = CFG->GetString(m_CFGKeyPrefix + "game_prefix", m_GamePrefix);
  m_MaxUploadSize          = CFG->GetInt(m_CFGKeyPrefix + "map_transfers.max_size", m_MaxUploadSize);
  m_FloodImmune            = CFG->GetBool(m_CFGKeyPrefix + "flood_immune", m_FloodImmune);

  optional<sockaddr_storage> customBindAddress = CFG->GetMaybeAddress(m_CFGKeyPrefix + "bind_address");
  if (customBindAddress.has_value())
    m_BindAddress            = customBindAddress.value();

  m_Enabled                = CFG->GetBool(m_CFGKeyPrefix + "enabled", m_Enabled);
}

CRealmConfig::~CRealmConfig() = default;
