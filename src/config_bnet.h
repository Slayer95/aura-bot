#ifndef AURA_CONFIG_BNET_H_
#define AURA_CONFIG_BNET_H_

#include "config.h"
#include "config_bot.h"

#include <vector>
#include <string>
#include <map>
#include <unordered_set>

//
// CBNETConfig
//

class CBNETConfig
{
public:
  std::string m_HostName;                        // server address to connect to
  std::string m_BindAddress;                     // the local address to connect with
  bool        m_Enabled;
  std::string m_InputID;                         // for IRC commands
  std::string m_UniqueName;                      // displayed on the console
  std::string m_CanonicalName;                   // displayed on game rooms
  std::string m_DataBaseID;                      // server ID for database queries
  std::string m_CDKeyROC;
  std::string m_CDKeyTFT;

  // Inheritable
  std::string m_CountryShort;                    // 2-3 letter country code for pvpgn servers
  std::string m_Country;                         // country name
  std::string m_Locale;                          // locale used: numeric or "system"
  uint32_t m_LocaleID;                           // see: http://msdn.microsoft.com/en-us/library/0h88fahh%28VS.85%29.aspx

  char m_CommandTrigger;                         // the character prefix to identify commands
  bool m_EnablePublicCreate;                     // whether non-admins are allowed to create games
  bool m_AnnounceHostToChat;
  bool m_IsMirror;

  bool m_EnableTunnel;                           // enable to make peers from pvpgn servers connect to m_PublicHostAddress:m_PublicHostPort
  uint16_t m_PublicHostPort;                     // the port to broadcast in pvpgn servers
  std::string m_PublicHostAddress;               // the address to broadcast in pvpgn servers

  std::string m_UserName;                        //
  std::string m_PassWord;                        //

  uint8_t m_AuthWar3Version;                     // WC3 minor version
  std::vector<uint8_t> m_AuthExeVersion;         // 4 bytes: WC3 version as {patch, minor, major, 1}
  std::vector<uint8_t> m_AuthExeVersionHash;     // 4 bytes
  std::string m_AuthPasswordHashType;            // pvpgn or battle.net

  std::string m_FirstChannel;                    //
  std::set<std::string> m_SudoUsers;             //
  std::set<std::string> m_RootAdmins;            //
  std::string m_GamePrefix;                      // string prepended to game names
  uint32_t m_MaxUploadSize;                      // in KB
  uint8_t m_ServerIndex;                            // unique server ID to identify players' realms through host counters
  std::string m_CFGKeyPrefix;                    // 


  CBNETConfig(CConfig* CFG, CBotConfig* nBotConfig);
  CBNETConfig(CConfig* CFG, CBNETConfig* nRootConfig, uint8_t nServerIndex);
  ~CBNETConfig();
};

#endif
