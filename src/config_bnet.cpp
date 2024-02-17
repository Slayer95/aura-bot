#include "config_bnet.h"
#include "config_bot.h"
#include "util.h"

#include <utility>
#include <algorithm>

using namespace std;

//
// CBNETConfig
//

CBNETConfig::CBNETConfig(CConfig* CFG, CBotConfig* AuraCFG)
  : m_CountryShort("PE"),
    m_Country("Peru"),
    m_Locale("system"),
    m_LocaleID(10250),

    m_CommandTrigger('!'),
    m_EnablePublicCreate(true),
    m_AnnounceHostToChat(true),
    m_IsMirror(false),

    m_EnableTunnel(false),
    m_PublicHostPort(6112),
    m_PublicHostAddress(string()),

    m_UserName(string()),
    m_PassWord(string()),

    m_AuthWar3Version(27),
    m_AuthExeVersion({173, 1, 27, 1}),
    m_AuthExeVersionHash({72, 160, 171, 170}),
    m_AuthPasswordHashType("pvpgn"),

    m_FirstChannel("The Void"),
    m_RootAdmins({}),
    m_GamePrefix(string()),
    m_MaxUploadSize(AuraCFG->m_MaxUploadSize), // The setting in AuraCFG applies to LAN always.
    m_FloodImmune(false),

    m_BindAddress(string()),
    m_Enabled(true),

    m_ServerIndex(0), // m_ServerIndex is one-based
    m_CFGKeyPrefix("global_bnet_")
{
  m_CountryShort           = CFG->GetString(m_CFGKeyPrefix + "countryshort", m_CountryShort);
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

  string CommandTrigger = CFG->GetString(m_CFGKeyPrefix + "commandtrigger", "");
  if (CommandTrigger.length() == 1) {
    m_CommandTrigger       = CommandTrigger[0];
  }
  m_EnablePublicCreate     = CFG->GetBool(m_CFGKeyPrefix + "allowhostnonadmins", m_EnablePublicCreate);
  m_AnnounceHostToChat     = CFG->GetBool(m_CFGKeyPrefix + "announcechat", m_AnnounceHostToChat);
  m_IsMirror               = CFG->GetBool(m_CFGKeyPrefix + "mirror", m_IsMirror);

  m_EnableTunnel           = CFG->GetBool(m_CFGKeyPrefix + "enabletunnel", m_EnableTunnel);
  m_PublicHostPort         = CFG->GetInt(m_CFGKeyPrefix + "publicport", m_PublicHostPort);
  m_PublicHostAddress      = CFG->GetString(m_CFGKeyPrefix + "publicip", m_PublicHostAddress);

  m_UserName               = CFG->GetString(m_CFGKeyPrefix + "username", m_UserName);
  m_PassWord               = CFG->GetString(m_CFGKeyPrefix + "password", m_PassWord);

  m_AuthWar3Version        = CFG->GetInt(m_CFGKeyPrefix + "auth_war3version", m_AuthWar3Version);
  m_AuthExeVersion         = CFG->GetUint8Vector(m_CFGKeyPrefix + "auth_exeversion", 4, m_AuthExeVersion);
  m_AuthExeVersionHash     = CFG->GetUint8Vector(m_CFGKeyPrefix + "auth_exeversionhash", 4, m_AuthExeVersionHash);
  m_AuthPasswordHashType   = CFG->GetString(m_CFGKeyPrefix + "auth_passwordhashtype", m_AuthPasswordHashType);

  m_FirstChannel           = CFG->GetString(m_CFGKeyPrefix + "firstchannel", m_FirstChannel);
  m_SudoUsers              = CFG->GetSet(m_CFGKeyPrefix + "sudousers", ',', m_SudoUsers);
  m_RootAdmins             = CFG->GetSet(m_CFGKeyPrefix + "rootadmins", ',', m_RootAdmins);
  m_GamePrefix             = CFG->GetString(m_CFGKeyPrefix + "gameprefix", m_GamePrefix);
  m_MaxUploadSize          = CFG->GetInt(m_CFGKeyPrefix + "maxuploadsize", m_MaxUploadSize);
  m_FloodImmune            = CFG->GetBool(m_CFGKeyPrefix + "floodimmune", m_FloodImmune);

  m_BindAddress            = CFG->GetString(m_CFGKeyPrefix + "bindaddress", string());
  m_Enabled                = CFG->GetBool(m_CFGKeyPrefix + "enabled", true);
}

CBNETConfig::CBNETConfig(CConfig* CFG, CBNETConfig* nRootConfig, uint8_t nServerIndex)
  : m_CountryShort(nRootConfig->m_CountryShort),
    m_Country(nRootConfig->m_Country),
    m_Locale(nRootConfig->m_Locale),
    m_LocaleID(nRootConfig->m_LocaleID),

    m_CommandTrigger(nRootConfig->m_CommandTrigger),
    m_EnablePublicCreate(nRootConfig->m_EnablePublicCreate),
    m_AnnounceHostToChat(nRootConfig->m_AnnounceHostToChat),
    m_IsMirror(nRootConfig->m_IsMirror),

    m_EnableTunnel(nRootConfig->m_EnableTunnel),
    m_PublicHostPort(nRootConfig->m_PublicHostPort),
    m_PublicHostAddress(nRootConfig->m_PublicHostAddress),

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

    m_BindAddress(string()),
    m_Enabled(nRootConfig->m_Enabled),

    m_ServerIndex(nServerIndex),
    m_CFGKeyPrefix("bnet_" + to_string(nServerIndex) + "_")
{
  m_HostName               = CFG->GetString(m_CFGKeyPrefix + "hostname", string());
  m_UniqueName             = CFG->GetString(m_CFGKeyPrefix + "uniquename", m_HostName);
  m_CanonicalName          = CFG->GetString(m_CFGKeyPrefix + "canonicalname", m_UniqueName); // may be shared by several servers
  m_InputID                = CFG->GetString(m_CFGKeyPrefix + "inputid", m_UniqueName); // expected unique
  transform(begin(m_InputID), end(m_InputID), begin(m_InputID), ::tolower);
  m_DataBaseID             = CFG->GetString(m_CFGKeyPrefix + "dbid", m_HostName); // may be shared by several servers
  m_CDKeyROC               = CFG->GetString(m_CFGKeyPrefix + "cdkeyroc", "FFFFFFFFFFFFFFFFFFFFFFFFFF");
  m_CDKeyTFT               = CFG->GetString(m_CFGKeyPrefix + "cdkeytft", "FFFFFFFFFFFFFFFFFFFFFFFFFF");

  // remove dashes and spaces from CD keys and convert to uppercase
  m_CDKeyROC.erase(remove(begin(m_CDKeyROC), end(m_CDKeyROC), '-'), end(m_CDKeyROC));
  m_CDKeyTFT.erase(remove(begin(m_CDKeyTFT), end(m_CDKeyTFT), '-'), end(m_CDKeyTFT));
  m_CDKeyROC.erase(remove(begin(m_CDKeyROC), end(m_CDKeyROC), ' '), end(m_CDKeyROC));
  m_CDKeyTFT.erase(remove(begin(m_CDKeyTFT), end(m_CDKeyTFT), ' '), end(m_CDKeyTFT));
  transform(begin(m_CDKeyROC), end(m_CDKeyROC), begin(m_CDKeyROC), ::toupper);
  transform(begin(m_CDKeyTFT), end(m_CDKeyTFT), begin(m_CDKeyTFT), ::toupper);

  m_CountryShort           = CFG->GetString(m_CFGKeyPrefix + "countryshort", m_CountryShort);
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

  string CommandTrigger    = CFG->GetString(m_CFGKeyPrefix + "commandtrigger", "");
  if (CommandTrigger.length() == 1) {
    m_CommandTrigger       = CommandTrigger[0];
  }
  m_EnablePublicCreate     = CFG->GetBool(m_CFGKeyPrefix + "allowhostnonadmins", m_EnablePublicCreate);
  m_AnnounceHostToChat     = CFG->GetBool(m_CFGKeyPrefix + "announcechat", m_AnnounceHostToChat);
  m_IsMirror               = CFG->GetBool(m_CFGKeyPrefix + "mirror", m_IsMirror);

  m_EnableTunnel           = CFG->GetBool(m_CFGKeyPrefix + "enabletunnel", m_EnableTunnel);
  m_PublicHostPort         = CFG->GetInt(m_CFGKeyPrefix + "publicport", m_PublicHostPort);
  m_PublicHostAddress      = CFG->GetString(m_CFGKeyPrefix + "publicip", m_PublicHostAddress);

  m_UserName               = CFG->GetString(m_CFGKeyPrefix + "username", m_UserName);
  m_PassWord               = CFG->GetString(m_CFGKeyPrefix + "password", m_PassWord);

  m_AuthWar3Version        = CFG->GetInt(m_CFGKeyPrefix + "auth_war3version", m_AuthWar3Version);
  m_AuthExeVersion         = CFG->GetUint8Vector(m_CFGKeyPrefix + "auth_exeversion", 4, m_AuthExeVersion);
  m_AuthExeVersionHash     = CFG->GetUint8Vector(m_CFGKeyPrefix + "auth_exeversionhash", 4, m_AuthExeVersionHash);
  m_AuthPasswordHashType   = CFG->GetString(m_CFGKeyPrefix + "auth_passwordhashtype", m_AuthPasswordHashType);

  m_FirstChannel           = CFG->GetString(m_CFGKeyPrefix + "firstchannel", m_FirstChannel);
  m_SudoUsers              = CFG->GetSet(m_CFGKeyPrefix + "sudousers", ',', m_SudoUsers);
  m_RootAdmins             = CFG->GetSet(m_CFGKeyPrefix + "rootadmins", ',', m_RootAdmins);
  m_GamePrefix             = CFG->GetString(m_CFGKeyPrefix + "gameprefix", m_GamePrefix);
  m_MaxUploadSize          = CFG->GetInt(m_CFGKeyPrefix + "maxuploadsize", m_MaxUploadSize);
  m_FloodImmune            = CFG->GetBool(m_CFGKeyPrefix + "floodimmune", m_FloodImmune);

  m_BindAddress            = CFG->GetString(m_CFGKeyPrefix + "bindaddress", m_BindAddress);
  m_Enabled                = CFG->GetBool(m_CFGKeyPrefix + "enabled", m_Enabled);
}

CBNETConfig::~CBNETConfig() = default;
