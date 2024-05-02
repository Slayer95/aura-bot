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

#ifndef AURA_CLI_H_
#define AURA_CLI_H_

#include <map>
#include <string>
#include <vector>
#include <set>
#include <optional>
#include <filesystem>

#include "aura.h"

#define CLI_OK 0
#define CLI_ERROR 1
#define CLI_EARLY_RETURN 2

#define CLI_ACTION_ABOUT 1
#define CLI_ACTION_EXAMPLES 2

//
// CCLI
//

class CAura;

class CCLI
{
public:
  std::optional<std::filesystem::path>  m_CFGPath;
  std::optional<std::filesystem::path>  m_HomePath;
  bool                                  m_UseStandardPaths;

private:
  uint8_t                               m_EarlyAction;

  bool                                  m_Verbose;
  std::optional<bool>                   m_LAN;
  std::optional<bool>                   m_BNET;
  std::optional<bool>                   m_IRC;
  std::optional<bool>                   m_Discord;
  std::optional<bool>                   m_ExitOnStandby;
  std::optional<bool>                   m_UseMapCFGCache;
  std::optional<std::string>            m_BindAddress;
  std::optional<std::string>            m_LANMode;
  std::optional<std::string>            m_LogLevel;
  std::optional<bool>                   m_InitSystem;

  std::optional<uint8_t>                m_War3Version;
  std::optional<std::filesystem::path>  m_War3Path;
  std::optional<std::filesystem::path>  m_MapPath;
  std::optional<std::filesystem::path>  m_MapCFGPath;
  std::optional<std::filesystem::path>  m_MapCachePath;
  std::optional<std::filesystem::path>  m_JASSPath;
  std::optional<std::filesystem::path>  m_GameSavePath;
  std::optional<bool>                   m_ExtractJASS;

  // Host flags
  std::optional<std::string>            m_SearchTarget;
  std::optional<std::string>            m_GameName;
  std::optional<bool>                   m_RandomRaces;
  std::optional<bool>                   m_RandomHeroes;
  std::optional<std::string>            m_SearchType;
  std::optional<std::string>            m_Observers;
  std::optional<std::string>            m_Visibility;
  std::optional<std::string>            m_Owner;
  std::vector<std::string>              m_ExcludedRealms;
  std::optional<std::string>            m_MirrorSource;
  std::optional<uint32_t>               m_GameLobbyTimeout;
  std::optional<uint8_t>                m_GameAutoStartPlayers;
  std::optional<int64_t>                m_GameAutoStartMinSeconds;
  std::optional<int64_t>                m_GameAutoStartMaxSeconds;
  std::optional<uint32_t>               m_GameMapDownloadTimeout;
  std::optional<bool>                   m_GameCheckJoinable;
  std::optional<bool>                   m_GameLobbyReplaceable;
  std::optional<bool>                   m_GameLobbyAutoRehosted;
  std::optional<bool>                   m_GameCheckReservation;
  std::optional<bool>                   m_GameFreeForAll;
  std::vector<std::string>              m_GameReservations;
  std::optional<bool>                   m_CheckMapVersion;
  std::optional<std::filesystem::path>  m_GameSavedPath;
  std::optional<std::string>            m_GameMapAlias;
  std::optional<std::string>            m_GameDisplayMode;

  // UPnP
  std::optional<bool>                   m_EnableUPnP;
  std::vector<uint16_t>                 m_PortForwardTCP;
  std::vector<uint16_t>                 m_PortForwardUDP;

  // Command queue
  std::optional<std::string>            m_ExecAs;
  std::string                           m_ExecAuth;
  std::string                           m_ExecScope;
  std::vector<std::string>              m_ExecCommands;
  bool                                  m_ExecBroadcast;

public:
  CCLI();
  ~CCLI();

  uint8_t Parse(const int argc, char** argv);
  void RunEarlyOptions() const;
  void OverrideConfig(CAura* nAura) const;
  bool QueueActions(CAura* nAura) const;
  inline std::optional<bool> GetInitSystem() const { return m_InitSystem; };
};

#endif // AURA_CLI_H_
