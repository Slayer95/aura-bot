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
#include "../command.h"
#include "../util.h"
#include "../protocol/bnet_protocol.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CRealmConfig
//

CRealmConfig::CRealmConfig(CConfig& CFG, CNetConfig* NetConfig)
  : // Not meant to be inherited
    m_ServerIndex(0), // m_ServerIndex is one-based
    m_CFGKeyPrefix("global_realm."),
    m_CommandCFG(nullptr),

    // Inheritable
    m_Enabled(true),
    m_LocaleShort({83, 69, 115, 101}), // esES, reversed
    m_Locale(PVPGN_LOCALE_ES_ES),
    m_AutoRegister(false),
    m_UserNameCaseSensitive(false),
    m_PassWordCaseSensitive(false),

    m_Admins({}),
    m_MaxUploadSize(NetConfig->m_MaxUploadSize), // The setting in AuraCFG applies to LAN always.
    m_WatchableDisplayMode(REALM_OBSERVER_DISPLAY_NONE),
    m_FloodImmune(false),

    m_QueryGameLists(false)
{
  m_CountryShort           = CFG.GetString(m_CFGKeyPrefix + "country_short", "PER");
  m_Country                = CFG.GetString(m_CFGKeyPrefix + "country", "Peru");
  m_Win32Locale            = CFG.GetString(m_CFGKeyPrefix + "win32.lcid", "system");

  string localeShort = CFG.GetString(m_CFGKeyPrefix + "locale_short", 4, 4, "esES");
  copy_n(localeShort.rbegin(), 4, m_LocaleShort.begin());

  {
    bool localeError = CFG.GetErrorLast();
    if (!localeError) {
      if (
        m_LocaleShort[0] < 0x41 || m_LocaleShort[1] < 0x41 || m_LocaleShort[2] < 0x61 || m_LocaleShort[3] < 0x61 ||
        m_LocaleShort[0] > 0x5a || m_LocaleShort[1] > 0x5a || m_LocaleShort[2] > 0x7a || m_LocaleShort[3] > 0x7a
      )
      {
        localeError = true;
      }
    }
    if (localeError) {
      Print("[CONFIG] Error - invalid value provided for <" + m_CFGKeyPrefix + "locale_short> - must provide a valid pair of ISO 639-1, ISO 3166 alpha-2 identifiers");
      CFG.SetFailed();
    }
  }

  vector<string> shortLocales = {"enUS", "csCZ", "deDE", "esES", "frFR", "itIT", "jaJA", "koKR", "plPL", "ruRU", "zhCN", "zhTW"};
  m_Locale = CFG.GetStringIndex(m_CFGKeyPrefix + "locale_short", shortLocales, PVPGN_LOCALE_ES_ES);

  if (m_Win32Locale == "system") {
    m_Win32LocaleID = 10250;
  } else {
    try {
      m_Win32LocaleID  = stoul(m_Win32Locale);
    } catch (...) {
      m_Win32Locale = "system";
      m_Win32LocaleID = 10250;
    }
  }

  m_Win32LanguageID        = CFG.GetUint32(m_CFGKeyPrefix + "win32.langid", m_Win32LocaleID);

  m_PrivateCmdToken        = CFG.GetString(m_CFGKeyPrefix + "commands.trigger", "!");
  if (!m_PrivateCmdToken.empty() && m_PrivateCmdToken[0] == '/') {
    Print("[CONFIG] Error - invalid value provided for <" + m_CFGKeyPrefix + "commands.trigger> - slash (/) is reserved by Battle.net");
    CFG.SetFailed();
  }
  m_BroadcastCmdToken      = CFG.GetString(m_CFGKeyPrefix + "commands.broadcast.trigger");
  if (!m_BroadcastCmdToken.empty() && m_BroadcastCmdToken[0] == '/') {
    Print("[CONFIG] Error - invalid value provided for <" + m_CFGKeyPrefix + "commands.broadcast.trigger> - slash (/) is reserved by Battle.net");
    CFG.SetFailed();
  }
  m_EnableBroadcast        = CFG.GetBool(m_CFGKeyPrefix + "commands.broadcast.enabled", false);

  m_AnnounceHostToChat     = CFG.GetBool(m_CFGKeyPrefix + "announce_chat", true);
  m_IsMain                 = CFG.GetBool(m_CFGKeyPrefix + "main", false);
  m_IsReHoster             = CFG.GetBool(m_CFGKeyPrefix + "rehoster", false);
  m_IsMirror               = CFG.GetBool(m_CFGKeyPrefix + "mirror", false);
  m_IsVPN                  = CFG.GetBool(m_CFGKeyPrefix + "vpn", false);

  m_IsHostOften            = !CFG.GetBool(m_CFGKeyPrefix + "game_host.throttle", true);
  m_IsHostMulti            = !CFG.GetBool(m_CFGKeyPrefix + "game_host.unique", true);

  m_EnableCustomAddress    = CFG.GetBool(m_CFGKeyPrefix + "custom_ip_address.enabled", false);
  m_PublicHostAddress      = CFG.GetAddressIPv4(m_CFGKeyPrefix + "custom_ip_address.value", "0.0.0.0");
  if (m_EnableCustomAddress)
    CFG.FailIfErrorLast();

  m_EnableCustomPort       = CFG.GetBool(m_CFGKeyPrefix + "custom_port.enabled", false);
  m_PublicHostPort         = CFG.GetUint16(m_CFGKeyPrefix + "custom_port.value", 6112);
  if (m_EnableCustomPort)
    CFG.FailIfErrorLast();

  m_HostName               = CFG.GetString(m_CFGKeyPrefix + "host_name");
  m_ServerPort             = CFG.GetUint16(m_CFGKeyPrefix + "server_port", 6112);

  m_AutoRegister           = CFG.GetBool(m_CFGKeyPrefix + "auto_register", m_AutoRegister);
  m_UserNameCaseSensitive  = CFG.GetBool(m_CFGKeyPrefix + "username.case_sensitive", false);
  m_PassWordCaseSensitive  = CFG.GetBool(m_CFGKeyPrefix + "password.case_sensitive", false);

  m_UserName               = CFG.GetString(m_CFGKeyPrefix + "username", m_UserName);
  m_PassWord               = CFG.GetString(m_CFGKeyPrefix + "password", m_PassWord);

  m_AuthUseCustomVersionData   = CFG.GetBool(m_CFGKeyPrefix + "auth_custom", false);
  m_AuthIgnoreVersionError = CFG.GetBool(m_CFGKeyPrefix + "auth_ignore_version_error", false);
  m_AuthPasswordHashType   = CFG.GetStringIndex(m_CFGKeyPrefix + "auth_password_hash_type", {"pvpgn", "battle.net"}, REALM_AUTH_PVPGN);

  m_GameVersion        = CFG.GetMaybeVersion(m_CFGKeyPrefix + "game_version");
  m_AuthExeVersion         = CFG.GetMaybeUint8Vector(m_CFGKeyPrefix + "auth_exe_version", 4);
  if (m_AuthUseCustomVersionData) CFG.FailIfErrorLast();
  m_AuthExeVersionHash     = CFG.GetMaybeUint8Vector(m_CFGKeyPrefix + "auth_exe_version_hash", 4);
  if (m_AuthUseCustomVersionData) CFG.FailIfErrorLast();
  m_AuthExeInfo            = CFG.GetString(m_CFGKeyPrefix + "auth_exe_info");

  m_FirstChannel           = CFG.GetString(m_CFGKeyPrefix + "first_channel", "The Void");
  m_SudoUsers              = CFG.GetSetInsensitive(m_CFGKeyPrefix + "sudo_users", ',', true, m_SudoUsers);
  m_Admins                 = CFG.GetSetInsensitive(m_CFGKeyPrefix + "admins", ',', true, m_Admins);
  m_LobbyPrefix            = CFG.GetString(m_CFGKeyPrefix + "game_list.lobby_prefix", m_LobbyPrefix);
  m_LobbySuffix            = CFG.GetString(m_CFGKeyPrefix + "game_list.lobby_suffix", m_LobbySuffix);
  m_WatchablePrefix        = CFG.GetString(m_CFGKeyPrefix + "game_list.watchable_prefix", m_WatchablePrefix);
  m_WatchableSuffix        = CFG.GetString(m_CFGKeyPrefix + "game_list.watchable_suffix", m_WatchableSuffix);
  m_MaxUploadSize          = CFG.GetInt(m_CFGKeyPrefix + "map_transfers.max_size", m_MaxUploadSize);
  m_WatchableDisplayMode   = CFG.GetStringIndex(m_CFGKeyPrefix + "watchable_games.display_mode", {"none", "deprioritize", "always"}, m_WatchableDisplayMode);

  m_ConsoleLogChat         = CFG.GetBool(m_CFGKeyPrefix + "logs.console.chat", true);
  m_FloodQuotaLines        = CFG.GetUint8(m_CFGKeyPrefix + "flood.lines", 5) - 1;
  m_FloodQuotaTime         = CFG.GetUint8(m_CFGKeyPrefix + "flood.time", 5);
  m_VirtualLineLength      = CFG.GetUint16(m_CFGKeyPrefix + "flood.wrap", 40);
  m_MaxLineLength          = CFG.GetUint16(m_CFGKeyPrefix + "flood.max_size", 200);
  m_FloodImmune            = CFG.GetBool(m_CFGKeyPrefix + "flood.immune", m_FloodImmune);

  if (0 == m_FloodQuotaLines) {
    m_FloodQuotaLines = 1;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.lines>.");
  }
  if (100 < m_FloodQuotaLines) {
    m_FloodQuotaLines = 100;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.lines>.");
  }
  if (60 < m_FloodQuotaTime) {
    m_FloodQuotaTime = 60;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.time>.");
  }
  if (0 == m_VirtualLineLength || 256 < m_VirtualLineLength) {
    m_VirtualLineLength = 256;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.wrap>.");
  }
  if (m_MaxLineLength < 6 || 256 < m_MaxLineLength) {
    m_MaxLineLength = 256;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.max_size>.");
  }
  if (static_cast<uint32_t>(m_MaxLineLength) > static_cast<uint32_t>(m_VirtualLineLength) * static_cast<uint32_t>(m_FloodQuotaLines)) {
    m_MaxLineLength = static_cast<uint32_t>(m_VirtualLineLength) * static_cast<uint32_t>(m_FloodQuotaLines);
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.max_size>.");
  }

  m_WhisperErrorReply      = CFG.GetString(m_CFGKeyPrefix + "protocol.whisper.error_reply", string());
  m_QueryGameLists         = CFG.GetBool(m_CFGKeyPrefix + "queries.games_list.enabled", m_QueryGameLists);

  m_Enabled                = CFG.GetBool(m_CFGKeyPrefix + "enabled", true);
  m_BindAddress            = CFG.GetMaybeAddress(m_CFGKeyPrefix + "bind_address");

  vector<string> commandPermissions = {"disabled", "sudo", "sudo_unsafe", "rootadmin", "admin", "verified_owner", "owner", "verified", "auto", "potential_owner", "unverified"};

  m_InheritOnlyCommandCommonBasePermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.common.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO);
  m_InheritOnlyCommandHostingBasePermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.hosting.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO);
  m_InheritOnlyCommandModeratorBasePermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.moderator.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO);
  m_InheritOnlyCommandAdminBasePermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.admin.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO);
  m_InheritOnlyCommandBotOwnerBasePermissions = CFG.GetStringIndex(m_CFGKeyPrefix + "commands.bot_owner.permissions", commandPermissions, COMMAND_PERMISSIONS_AUTO);

  m_UnverifiedRejectCommands      = CFG.GetBool(m_CFGKeyPrefix + "unverified_users.reject_commands", false);
  m_UnverifiedCannotStartGame     = CFG.GetBool(m_CFGKeyPrefix + "unverified_users.reject_start", false);
  m_UnverifiedAutoKickedFromLobby = CFG.GetBool(m_CFGKeyPrefix + "unverified_users.auto_kick", false);
  m_AlwaysSpoofCheckPlayers       = CFG.GetBool(m_CFGKeyPrefix + "unverified_users.always_verify", false);
}

CRealmConfig::CRealmConfig(CConfig& CFG, CRealmConfig* nRootConfig, uint8_t nServerIndex)
  : m_ServerIndex(nServerIndex),
    m_CFGKeyPrefix("realm_" + to_string(nServerIndex) + "."),

    m_InheritOnlyCommandCommonBasePermissions(nRootConfig->m_InheritOnlyCommandCommonBasePermissions),
    m_InheritOnlyCommandHostingBasePermissions(nRootConfig->m_InheritOnlyCommandHostingBasePermissions),
    m_InheritOnlyCommandModeratorBasePermissions(nRootConfig->m_InheritOnlyCommandModeratorBasePermissions),
    m_InheritOnlyCommandAdminBasePermissions(nRootConfig->m_InheritOnlyCommandAdminBasePermissions),
    m_InheritOnlyCommandBotOwnerBasePermissions(nRootConfig->m_InheritOnlyCommandBotOwnerBasePermissions),

    m_UnverifiedRejectCommands(nRootConfig->m_UnverifiedRejectCommands),
    m_UnverifiedCannotStartGame(nRootConfig->m_UnverifiedCannotStartGame),
    m_UnverifiedAutoKickedFromLobby(nRootConfig->m_UnverifiedAutoKickedFromLobby),
    m_AlwaysSpoofCheckPlayers(nRootConfig->m_AlwaysSpoofCheckPlayers),

    m_CommandCFG(nullptr),

    m_Enabled(nRootConfig->m_Enabled),
    m_BindAddress(nRootConfig->m_BindAddress),

    m_CountryShort(nRootConfig->m_CountryShort),
    m_Country(nRootConfig->m_Country),
    m_Win32Locale(nRootConfig->m_Win32Locale),
    m_Win32LocaleID(nRootConfig->m_Win32LocaleID),
    m_Win32LanguageID(nRootConfig->m_Win32LanguageID),
    m_LocaleShort(nRootConfig->m_LocaleShort),
    m_Locale(nRootConfig->m_Locale),

    m_PrivateCmdToken(nRootConfig->m_PrivateCmdToken),
    m_BroadcastCmdToken(nRootConfig->m_BroadcastCmdToken),
    m_AnnounceHostToChat(nRootConfig->m_AnnounceHostToChat),
    m_IsMain(nRootConfig->m_IsMain),
    m_IsReHoster(nRootConfig->m_IsReHoster),
    m_IsMirror(nRootConfig->m_IsMirror),
    m_IsVPN(nRootConfig->m_IsVPN),

    m_IsHostOften(nRootConfig->m_IsHostOften),
    m_IsHostMulti(nRootConfig->m_IsHostMulti),

    m_EnableCustomAddress(nRootConfig->m_EnableCustomAddress),
    m_PublicHostAddress(nRootConfig->m_PublicHostAddress),

    m_EnableCustomPort(nRootConfig->m_EnableCustomPort),
    m_PublicHostPort(nRootConfig->m_PublicHostPort),

    m_HostName(nRootConfig->m_HostName),
    m_ServerPort(nRootConfig->m_ServerPort),

    m_AutoRegister(nRootConfig->m_AutoRegister),
    m_UserNameCaseSensitive(nRootConfig->m_UserNameCaseSensitive),
    m_PassWordCaseSensitive(nRootConfig->m_PassWordCaseSensitive),
    m_UserName(nRootConfig->m_UserName),
    m_PassWord(nRootConfig->m_PassWord),

    m_AuthUseCustomVersionData(nRootConfig->m_AuthUseCustomVersionData),
    m_AuthIgnoreVersionError(nRootConfig->m_AuthIgnoreVersionError),
    m_AuthPasswordHashType(nRootConfig->m_AuthPasswordHashType),

    m_GameVersion(nRootConfig->m_GameVersion),
    m_AuthExeVersion(nRootConfig->m_AuthExeVersion),
    m_AuthExeVersionHash(nRootConfig->m_AuthExeVersionHash),
    m_AuthExeInfo(nRootConfig->m_AuthExeInfo),

    m_FirstChannel(nRootConfig->m_FirstChannel),
    m_SudoUsers(nRootConfig->m_SudoUsers),
    m_Admins(nRootConfig->m_Admins),
    m_LobbyPrefix(nRootConfig->m_LobbyPrefix),
    m_LobbySuffix(nRootConfig->m_LobbySuffix),
    m_WatchablePrefix(nRootConfig->m_WatchablePrefix),
    m_WatchableSuffix(nRootConfig->m_WatchableSuffix),
    m_MaxUploadSize(nRootConfig->m_MaxUploadSize),
    m_WatchableDisplayMode(nRootConfig->m_WatchableDisplayMode),

    m_ConsoleLogChat(nRootConfig->m_ConsoleLogChat),
    m_FloodQuotaLines(nRootConfig->m_FloodQuotaLines),
    m_FloodQuotaTime(nRootConfig->m_FloodQuotaTime),
    m_VirtualLineLength(nRootConfig->m_VirtualLineLength),
    m_MaxLineLength(nRootConfig->m_MaxLineLength),
    m_FloodImmune(nRootConfig->m_FloodImmune),

    m_WhisperErrorReply(nRootConfig->m_WhisperErrorReply),
    m_QueryGameLists(nRootConfig->m_QueryGameLists)
{
  m_HostName               = ToLowerCase(CFG.GetString(m_CFGKeyPrefix + "host_name", m_HostName));
  m_ServerPort             = CFG.GetUint16(m_CFGKeyPrefix + "server_port", m_ServerPort);
  m_UniqueName             = CFG.GetString(m_CFGKeyPrefix + "unique_name", m_HostName);
  m_CanonicalName          = CFG.GetString(m_CFGKeyPrefix + "canonical_name", m_UniqueName); // may be shared by several servers
  m_InputID                = CFG.GetString(m_CFGKeyPrefix + "input_id", m_UniqueName); // expected unique
  transform(begin(m_InputID), end(m_InputID), begin(m_InputID), [](char c) { return static_cast<char>(std::tolower(c)); });
  m_DataBaseID             = CFG.GetString(m_CFGKeyPrefix + "db_id", m_HostName); // may be shared by several servers
  m_CDKeyROC               = CFG.GetString(m_CFGKeyPrefix + "cd_key.roc", 26, 26, "FFFFFFFFFFFFFFFFFFFFFFFFFF");
  m_CDKeyTFT               = CFG.GetString(m_CFGKeyPrefix + "cd_key.tft", 26, 26, "FFFFFFFFFFFFFFFFFFFFFFFFFF");

  // remove dashes and spaces from CD keys and convert to uppercase
  m_CDKeyROC.erase(remove(begin(m_CDKeyROC), end(m_CDKeyROC), '-'), end(m_CDKeyROC));
  m_CDKeyTFT.erase(remove(begin(m_CDKeyTFT), end(m_CDKeyTFT), '-'), end(m_CDKeyTFT));
  m_CDKeyROC.erase(remove(begin(m_CDKeyROC), end(m_CDKeyROC), ' '), end(m_CDKeyROC));
  m_CDKeyTFT.erase(remove(begin(m_CDKeyTFT), end(m_CDKeyTFT), ' '), end(m_CDKeyTFT));
  transform(begin(m_CDKeyROC), end(m_CDKeyROC), begin(m_CDKeyROC), [](char c) { return static_cast<char>(std::toupper(c)); });
  transform(begin(m_CDKeyTFT), end(m_CDKeyTFT), begin(m_CDKeyTFT), [](char c) { return static_cast<char>(std::toupper(c)); });

  m_CountryShort           = CFG.GetString(m_CFGKeyPrefix + "country_short", m_CountryShort);
  m_Country                = CFG.GetString(m_CFGKeyPrefix + "country", m_Country);
  m_Win32Locale            = CFG.GetString(m_CFGKeyPrefix + "win32.lcid", m_Win32Locale);

  // These are the main supported locales, but we still accept other values
  if (CFG.Exists(m_CFGKeyPrefix + "locale_short")) {
    string localeShort = CFG.GetString(m_CFGKeyPrefix + "locale_short", 4, 4, "esES");
    bool localeError = CFG.GetErrorLast();
    if (!localeError) {
      copy_n(localeShort.rbegin(), 4, m_LocaleShort.begin());
      if (
        m_LocaleShort[0] < 0x41 || m_LocaleShort[1] < 0x41 || m_LocaleShort[2] < 0x61 || m_LocaleShort[3] < 0x61 ||
        m_LocaleShort[0] > 0x5a || m_LocaleShort[1] > 0x5a || m_LocaleShort[2] > 0x7a || m_LocaleShort[3] > 0x7a
      )
      {
        localeError = true;
      }
    }
    if (localeError) {
      Print("[CONFIG] Error - invalid value provided for <" + m_CFGKeyPrefix + "locale_short> - must provide a valid pair of ISO 639-1, ISO 3166 alpha-2 identifiers");
      CFG.SetFailed();
    }
  }

  vector<string> shortLocales = {"enUS", "csCZ", "deDE", "esES", "frFR", "itIT", "jaJA", "koKR", "plPL", "ruRU", "zhCN", "zhTW"};
  m_Locale = CFG.GetStringIndex(m_CFGKeyPrefix + "locale_short", shortLocales, m_Locale);

  if (m_Win32Locale == "system") {
    m_Win32LocaleID = 10250;
  } else {
    try {
      m_Win32LocaleID  = stoul(m_Win32Locale);
    } catch (...) {
      m_Win32Locale = nRootConfig->m_Win32Locale;
    }
  }

  m_Win32LanguageID        = CFG.GetUint32(m_CFGKeyPrefix + "win32.langid", m_Win32LanguageID);

  m_PrivateCmdToken        = CFG.GetString(m_CFGKeyPrefix + "commands.trigger", m_PrivateCmdToken);
  if (!m_PrivateCmdToken.empty() && m_PrivateCmdToken[0] == '/') {
    CFG.SetFailed();
  }
  m_BroadcastCmdToken      = CFG.GetString(m_CFGKeyPrefix + "commands.broadcast.trigger", m_BroadcastCmdToken);
  if (!m_BroadcastCmdToken.empty() && m_BroadcastCmdToken[0] == '/') {
    CFG.SetFailed();
  }
  m_EnableBroadcast        = CFG.GetBool(m_CFGKeyPrefix + "commands.broadcast.enabled", m_EnableBroadcast);

  if (!m_EnableBroadcast)
    m_BroadcastCmdToken.clear();

  m_AnnounceHostToChat     = CFG.GetBool(m_CFGKeyPrefix + "announce_chat", m_AnnounceHostToChat);
  m_IsMain                 = CFG.GetBool(m_CFGKeyPrefix + "main", m_IsMain);
  m_IsReHoster             = CFG.GetBool(m_CFGKeyPrefix + "rehoster", m_IsReHoster);
  m_IsMirror               = CFG.GetBool(m_CFGKeyPrefix + "mirror", m_IsMirror);
  m_IsVPN                  = CFG.GetBool(m_CFGKeyPrefix + "vpn", m_IsVPN);

  m_IsHostOften            = !CFG.GetBool(m_CFGKeyPrefix + "game_host.throttle", !m_IsHostOften);
  m_IsHostMulti            = !CFG.GetBool(m_CFGKeyPrefix + "game_host.unique", !m_IsHostMulti);

  m_EnableCustomAddress    = CFG.GetBool(m_CFGKeyPrefix + "custom_ip_address.enabled", m_EnableCustomAddress);
  optional<sockaddr_storage> maybeAddress = CFG.GetMaybeAddressIPv4(m_CFGKeyPrefix + "custom_ip_address.value");
  if (m_EnableCustomAddress)
    CFG.FailIfErrorLast();
  if (maybeAddress.has_value()) {
    m_PublicHostAddress    = maybeAddress.value();
  }

  m_EnableCustomPort       = CFG.GetBool(m_CFGKeyPrefix + "custom_port.enabled", m_EnableCustomPort);
  m_PublicHostPort         = CFG.GetUint16(m_CFGKeyPrefix + "custom_port.value", m_PublicHostPort);
  if (m_EnableCustomPort)
    CFG.FailIfErrorLast();

  m_AutoRegister           = CFG.GetBool(m_CFGKeyPrefix + "auto_register", m_AutoRegister);
  m_UserNameCaseSensitive  = CFG.GetBool(m_CFGKeyPrefix + "username.case_sensitive", m_UserNameCaseSensitive);
  m_PassWordCaseSensitive  = CFG.GetBool(m_CFGKeyPrefix + "password.case_sensitive", m_PassWordCaseSensitive);

  m_UserName               = CFG.GetString(m_CFGKeyPrefix + "username", m_UserName);
  m_PassWord               = CFG.GetString(m_CFGKeyPrefix + "password", m_PassWord);
  if (!m_UserNameCaseSensitive) m_UserName = ToLowerCase(m_UserName);
  if (!m_PassWordCaseSensitive) m_PassWord = ToLowerCase(m_PassWord);

  m_AuthUseCustomVersionData   = CFG.GetBool(m_CFGKeyPrefix + "auth_custom", m_AuthUseCustomVersionData);
  m_AuthIgnoreVersionError = CFG.GetBool(m_CFGKeyPrefix + "auth_ignore_version_error", m_AuthIgnoreVersionError);
  m_AuthPasswordHashType   = CFG.GetStringIndex(m_CFGKeyPrefix + "auth_password_hash_type", {"pvpgn", "battle.net"}, m_AuthPasswordHashType);

  optional<Version> authWar3Version = CFG.GetMaybeVersion(m_CFGKeyPrefix + "game_version");
  if (authWar3Version.has_value()) m_GameVersion = authWar3Version.value();

  // These are optional, since they can be figured out with bncsutil.
  optional<vector<uint8_t>> authExeVersion = CFG.GetMaybeUint8Vector(m_CFGKeyPrefix + "auth_exe_version", 4);
  if (m_AuthUseCustomVersionData) CFG.FailIfErrorLast();
  optional<vector<uint8_t>> authExeVersionHash = CFG.GetMaybeUint8Vector(m_CFGKeyPrefix + "auth_exe_version_hash", 4);
  if (m_AuthUseCustomVersionData) CFG.FailIfErrorLast();
  string authExeInfo = CFG.GetString(m_CFGKeyPrefix + "auth_exe_info");

  if (m_AuthUseCustomVersionData) {
    if (authExeVersion.has_value()) m_AuthExeVersion = authExeVersion.value();
    if (authExeVersionHash.has_value()) m_AuthExeVersionHash = authExeVersionHash.value();
    if (!authExeInfo.empty()) m_AuthExeInfo = authExeInfo;
  } else {
    m_AuthExeVersion.reset();
    m_AuthExeVersionHash.reset();
    m_AuthExeInfo.clear();
  }

  if (m_AuthExeVersion.has_value() && m_GameVersion.has_value()) {
    if (m_GameVersion->first != m_AuthExeVersion.value()[3] || m_GameVersion->second != m_AuthExeVersion.value()[2]) {
      Print("[CONFIG] Error - mismatch between <" + m_CFGKeyPrefix + "game_version> and <" + m_CFGKeyPrefix + "auth_exe_version>");
      CFG.SetFailed();
    }
  }

  m_FirstChannel           = CFG.GetString(m_CFGKeyPrefix + "first_channel", m_FirstChannel);
  m_SudoUsers              = CFG.GetSetInsensitive(m_CFGKeyPrefix + "sudo_users", ',', true, m_SudoUsers);
  m_Admins                 = CFG.GetSetInsensitive(m_CFGKeyPrefix + "admins", ',', true, m_Admins);
  m_LobbyPrefix            = CFG.GetString(m_CFGKeyPrefix + "game_list.lobby_prefix", 0, 16, m_LobbyPrefix);
  m_LobbySuffix            = CFG.GetString(m_CFGKeyPrefix + "game_list.lobby_suffix", 0, 16, m_LobbySuffix);
  m_WatchablePrefix        = CFG.GetString(m_CFGKeyPrefix + "game_list.watchable_prefix", 0, 16, m_WatchablePrefix);
  m_WatchableSuffix        = CFG.GetString(m_CFGKeyPrefix + "game_list.watchable_suffix", 0, 16, m_WatchableSuffix);
  m_MaxUploadSize          = CFG.GetInt(m_CFGKeyPrefix + "map_transfers.max_size", m_MaxUploadSize);
  m_WatchableDisplayMode   = CFG.GetStringIndex(m_CFGKeyPrefix + "watchable_games.display_mode", {"none", "deprioritize", "always"}, m_WatchableDisplayMode);

  m_ConsoleLogChat         = CFG.GetBool(m_CFGKeyPrefix + "logs.console.chat", m_ConsoleLogChat);
  m_FloodQuotaLines        = CFG.GetUint8(m_CFGKeyPrefix + "flood.lines", m_FloodQuotaLines + 1) - 1;
  m_FloodQuotaTime         = CFG.GetUint8(m_CFGKeyPrefix + "flood.time", m_FloodQuotaTime);
  m_VirtualLineLength      = CFG.GetUint16(m_CFGKeyPrefix + "flood.wrap", m_VirtualLineLength);
  m_MaxLineLength          = CFG.GetUint16(m_CFGKeyPrefix + "flood.max_size", m_MaxLineLength);
  m_FloodImmune            = CFG.GetBool(m_CFGKeyPrefix + "flood.immune", m_FloodImmune);

  if (0 == m_FloodQuotaLines) {
    m_FloodQuotaLines = 1;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.lines>.");
  }
  if (100 < m_FloodQuotaLines) {
    m_FloodQuotaLines = 100;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.lines>.");
  }
  if (60 < m_FloodQuotaTime) {
    m_FloodQuotaTime = 60;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.time>.");
  }
  if (0 == m_VirtualLineLength || 256 < m_VirtualLineLength) {
    m_VirtualLineLength = 256;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.wrap>.");
  }
  if (m_MaxLineLength < 6 || 256 < m_MaxLineLength) {
    m_MaxLineLength = 256;
    Print("[CONFIG] Error - Invalid value provided for <" + m_CFGKeyPrefix + "flood.max_size>.");
  }
  if (static_cast<uint32_t>(m_MaxLineLength) > static_cast<uint32_t>(m_VirtualLineLength) * static_cast<uint32_t>(m_FloodQuotaLines)) {
    m_MaxLineLength = static_cast<uint32_t>(m_VirtualLineLength) * static_cast<uint32_t>(m_FloodQuotaLines);
    // PvPGN defaults make no sense: 40x5=200 seem logical, but in fact the 5th line is not allowed.
    Print("[CONFIG] using <" + m_CFGKeyPrefix + "flood.max_size = " + to_string(m_MaxLineLength) + ">");
  }

  m_WhisperErrorReply      = CFG.GetString(m_CFGKeyPrefix + "protocol.whisper.error_reply", m_WhisperErrorReply);

  if (m_WhisperErrorReply.empty()) {
    switch (m_Locale) {
      case PVPGN_LOCALE_DE_DE:
        m_WhisperErrorReply = "Dieser Nutzer ist nicht eingeloggt.";
        break;
      case PVPGN_LOCALE_ES_ES:
        m_WhisperErrorReply = "Ese usuario no está conectado.";
        break;
      case PVPGN_LOCALE_EN_US:
      default:
        m_WhisperErrorReply = "That user is not logged on.";
        break;
    }
  }

  m_QueryGameLists         = CFG.GetBool(m_CFGKeyPrefix + "queries.games_list.enabled", m_QueryGameLists);

  m_UnverifiedRejectCommands      = CFG.GetBool(m_CFGKeyPrefix + "unverified_users.reject_commands", m_UnverifiedRejectCommands);
  m_UnverifiedCannotStartGame     = CFG.GetBool(m_CFGKeyPrefix + "unverified_users.reject_start", m_UnverifiedCannotStartGame);
  m_UnverifiedAutoKickedFromLobby = CFG.GetBool(m_CFGKeyPrefix + "unverified_users.auto_kick", m_UnverifiedAutoKickedFromLobby);
  m_AlwaysSpoofCheckPlayers       = CFG.GetBool(m_CFGKeyPrefix + "unverified_users.always_verify", m_AlwaysSpoofCheckPlayers);

  vector<string> commandPermissions = {"disabled", "sudo", "sudo_unsafe", "rootadmin", "admin", "verified_owner", "owner", "verified", "auto", "potential_owner", "unverified"};

  m_CommandCFG             = new CCommandConfig(
    CFG, m_CFGKeyPrefix, false, m_UnverifiedRejectCommands,
    CFG.GetStringIndex(m_CFGKeyPrefix + "commands.common.permissions", commandPermissions, m_InheritOnlyCommandCommonBasePermissions),
    CFG.GetStringIndex(m_CFGKeyPrefix + "commands.hosting.permissions", commandPermissions, m_InheritOnlyCommandHostingBasePermissions),
    CFG.GetStringIndex(m_CFGKeyPrefix + "commands.moderator.permissions", commandPermissions, m_InheritOnlyCommandModeratorBasePermissions),
    CFG.GetStringIndex(m_CFGKeyPrefix + "commands.admin.permissions", commandPermissions, m_InheritOnlyCommandAdminBasePermissions),
    CFG.GetStringIndex(m_CFGKeyPrefix + "commands.bot_owner.permissions", commandPermissions, m_InheritOnlyCommandBotOwnerBasePermissions)
  );

  m_Enabled                       = CFG.GetBool(m_CFGKeyPrefix + "enabled", m_Enabled);

  optional<sockaddr_storage> customBindAddress = CFG.GetMaybeAddress(m_CFGKeyPrefix + "bind_address");
  if (customBindAddress.has_value())
    m_BindAddress            = customBindAddress.value();
}

void CRealmConfig::Reset()
{
  m_CommandCFG = nullptr;
}

CRealmConfig::~CRealmConfig()
{
  delete m_CommandCFG;
}
