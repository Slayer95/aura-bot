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

#ifndef AURA_CONFIG_REALM_H_
#define AURA_CONFIG_REALM_H_

#include "config.h"
#include "config_net.h"
#include "socket.h"

#include <vector>
#include <string>
#include <optional>
#include <map>
#include <unordered_set>

struct sockaddr_storage;

//
// CRealmConfig
//

class CRealmConfig
{
public:
  std::string m_HostName;                        // server address to connect to
  std::optional<sockaddr_storage> m_BindAddress; // the local address from which we connect
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
  bool m_IsVPN;

  bool m_EnableCustomAddress;                    // enable to make peers from pvpgn servers connect to m_PublicHostAddress
  sockaddr_storage m_PublicHostAddress;          // the address to broadcast in pvpgn servers

  bool m_EnableCustomPort;                       // enable to make peers from pvpgn servers connect to m_PublicHostPort
  uint16_t m_PublicHostPort;                     // the port to broadcast in pvpgn servers

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
  bool m_FloodImmune;                            // whether we are allowed to send unlimited commands to the server

  // Automatically-assigned values
  uint8_t m_ServerIndex;                         // unique server ID to identify players' realms through host counters
  std::string m_CFGKeyPrefix;                    // 


  CRealmConfig(CConfig* CFG, CNetConfig* nNetConfig);
  CRealmConfig(CConfig* CFG, CRealmConfig* nRootConfig, uint8_t nServerIndex);
  ~CRealmConfig();
};

#endif
