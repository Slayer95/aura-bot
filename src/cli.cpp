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

#include "cli.h"
#include "config_bot.h"
#include "config_game.h"
#include "config_net.h"
#include "net.h"
#include "fileutil.h"
#include "osutil.h"
#include "gameprotocol.h"
#include "gamesetup.h"
#include "realm.h"
#include "CLI11.hpp"

using namespace std;

//
// CCLI
//

CCLI::CCLI()
 : m_UseStandardPaths(false),
   m_EarlyAction(0),
   m_Verbose(false),
   m_ExecAuth("verified"),
   m_ExecScope("none")
{
}

CCLI::~CCLI() = default;

uint8_t CCLI::Parse(const int argc, char** argv)
{
  CLI::App app{AURA_APP_NAME};
  argv = app.ensure_utf8(argv);

  bool examples = false;
  bool about = false;

  app.option_defaults()->ignore_case();

  app.add_option("MAPFILE", m_SearchTarget, "Map, config or URI from which to host a game from the CLI. File names resolve from maps directory, unless --stdpaths.");
  app.add_option("GAMENAME", m_GameName, "Name assigned to a game hosted from the CLI.");

  app.add_flag("--stdpaths", m_UseStandardPaths, "Makes file names always resolve from CWD when input through the CLI. Commutative.");
#ifdef _WIN32
  app.add_flag("--init-system,--no-init-system{false}", m_InitSystem, "Adds Aura to the PATH environment variable, as well as to Windows Explorer context menu.");
#else
  app.add_flag("--init-system,--no-init-system{false}", m_InitSystem, "Adds Aura to the PATH environment variable.");
#endif
#ifndef DISABLE_MINIUPNP
  app.add_flag("--auto-port-forward,--no-auto-port-forward{false}", m_EnableUPnP, "Enable automatic port-forwarding, using Universal Plug-and-Play.");
#else
  app.add_flag("--auto-port-forward,--no-auto-port-forward{false}", m_EnableUPnP, "Enable automatic port-forwarding, using Universal Plug-and-Play. (This distribution of Aura does not support this feature.)");
#endif
  app.add_flag("--lan,--no-lan{false}", m_LAN, "Show hosted games on Local Area Network.");
  app.add_flag("--bnet,--no-bnet{false}", m_BNET, "Switch to enable or disable every defined realm.");
  app.add_flag("--irc,--no-irc{false}", m_IRC, "Switch to enable or disable IRC integration.");
  app.add_flag("--discord,--no-discord{false}", m_Discord, "Switch to enable or disable Discord integration.");
  app.add_flag("--exit,--no-exit{false}", m_ExitOnStandby, "Terminates the process when idle.");
  app.add_flag("--cache,--no-cache{false}", m_UseMapCFGCache, "Caches loaded map files into the map configs folder.");
  app.add_flag("--about,--version", about, "Display software information.");
  app.add_flag("--example,--examples", examples, "Display CLI hosting examples.");
  app.add_flag("--verbose", m_Verbose, "Outputs detailed information when running CLI actions.");
  app.add_flag("--extract-jass,--no-extract-jass{false}", m_ExtractJASS, "Automatically extract files from the game install directory.");

#ifdef _WIN32
  app.add_option("--homedir", m_HomePath, "Customizes Aura home dir (%AURA_HOME%).");
#else
  app.add_option("--homedir", m_HomePath, "Customizes Aura home dir ($AURA_HOME).");
#endif
  app.add_option("--config", m_CFGPath, "Customizes the main Aura config file. File names resolve from home dir, unless --stdpaths.");
  app.add_option("--w3version", m_War3Version, "Customizes the game version.");
  app.add_option("--w3dir", m_War3Path, "Customizes the game install directory.");
  app.add_option("--mapdir", m_MapPath, "Customizes the maps directory.");
  app.add_option("--cfgdir", m_MapCFGPath, "Customizes the map configs directory.");
  app.add_option("--cachedir", m_MapCachePath, "Customizes the map cache directory.");
  app.add_option("--jassdir", m_JASSPath, "Customizes the directory where extracted JASS files are stored.");
  app.add_option("--savedir", m_GameSavePath, "Customizes the game save directory.");
  app.add_option("-s,--search-type", m_SearchType, "Restricts file searches when hosting from the CLI. Values: map, config, local, any")->check(CLI::IsMember({"map", "config", "local", "any"}))->default_val("any");
  app.add_option("--bind-address", m_BindAddress, "Restricts connections to the game server, only allowing the input IPv4 address.")->check(CLI::ValidIPV4);
  app.add_option("--lan-mode", m_LANMode, "Customizes the behavior of the game discovery service. Values: strict, lax, free.")->check(CLI::IsMember({"strict", "lax", "free"}));
  app.add_option("--log-level", m_LogLevel, "Customizes how detailed Aura's output should be. Values: info, debug, trace.")->check(CLI::IsMember({"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", "trace", "trace2"}));

  // Game hosting
  app.add_option("--owner", m_Owner, "Customizes the game owner when hosting from the CLI.");
  app.add_option("--observers", m_Observers, "Customizes observers when hosting from the CLI. Values: no, referees, defeat, full")->check(CLI::IsMember({"no", "referees", "defeat", "full"}));
  app.add_option("--visibility", m_Visibility, "Customizes visibility when hosting from the CLI. Values: default, hide, explored, visible")->check(CLI::IsMember({"default", "hide", "explored", "visible"}));
  app.add_option("--list-visibility", m_GameDisplayMode, "Customizes whether the game is displayed in any realms. Values: public, private, none")->check(CLI::IsMember({"public", "private", "none"}));
  app.add_option("--random-races", m_RandomRaces, "Toggles random races when hosting from the CLI.");
  app.add_option("--random-heroes", m_RandomHeroes, "Toggles random heroes when hosting from the CLI.");
  app.add_option("--alias", m_GameMapAlias, "Registers an alias for the map used when hosting from the CLI.");
  app.add_option("--mirror", m_MirrorSource, "Mirrors a game, listing it in the connected realms. Syntax: IP:PORT#ID.");
  app.add_option("--exclude", m_ExcludedRealms, "Hides the game in the listed realm(s). Repeatable.");
  app.add_option("--lobby-timeout", m_GameLobbyTimeout, "Sets the time limit for the game lobby (seconds.)");
  app.add_option("--auto-start-players", m_GameAutoStartPlayers, "Sets an amount of occupied slots for automatically starting the game.");
  app.add_option("--auto-start-min-time", m_GameAutoStartMinSeconds, "Sets a minimum time that should pass before automatically starting the game (seconds.)");
  app.add_option("--auto-start-max-time", m_GameAutoStartMinSeconds, "Sets a timeout that will forcibly start the game when over (seconds.)");
  app.add_option("--download-timeout", m_GameMapDownloadTimeout, "Sets the time limit for the map download (seconds.)");
  app.add_option("--load", m_GameSavedPath, "Sets the saved game .w3z file path for the game lobby.");
  app.add_option("--reserve", m_GameReservations, "Adds a player to the reserved list of the game lobby.");
  app.add_flag(  "--check-joinable,--no-check-joinable{false}", m_GameCheckJoinable, "Reports whether the game is joinable over the Internet.");
  app.add_flag(  "--check-reservation,--no-check-reservation{false}", m_GameCheckReservation, "Enforces only players in the reserved list be able to join the game.");
  app.add_flag(  "--ffa", m_GameFreeForAll, "Sets free-for-all game mode - every player is automatically assigned to a different team.");
  app.add_flag(  "--check-version,--no-check-version{false}", m_CheckMapVersion, "Whether Aura checks whether the map properly states it's compatible with current game version.");
  app.add_flag(  "--replaceable,--no-replaceable{false}", m_GameLobbyReplaceable, "Whether users can use the !host command to replace the lobby.");
  app.add_flag(  "--auto-rehost,--no-auto-rehost{false}", m_GameLobbyAutoRehosted, "Registers the provided game setup, and rehosts it whenever there is no active lobby.");

  // Command execution
  app.add_option("--exec", m_ExecCommands, "Runs a command from the CLI. Repeatable.");
  app.add_option("--exec-as", m_ExecAs, "Customizes the user identity when running commands from the CLI.");
  app.add_option("--exec-auth", m_ExecAuth, "Customizes the user permissions when running commands from the CLI.")->check(CLI::IsMember({"spoofed", "verified", "admin", "rootadmin", "sudo"}))->default_val("verified");
  app.add_option("--exec-scope", m_ExecScope, "Customizes the channel when running commands from the CLI. Values: none, lobby, server, game#IDX")->default_val("none");
  app.add_flag(  "--exec-broadcast", m_ExecBroadcast, "Enables broadcasting the command execution to all users in the channel");

  // Port-forwarding
#ifndef DISABLE_MINIUPNP
  app.add_option("--port-forward-tcp", m_PortForwardTCP, "Enable port-forwarding on the given TCP ports. Repeatable.");
  app.add_option("--port-forward-udp", m_PortForwardUDP, "Enable port-forwarding on the given UDP ports. Repeatable.");
#else
  app.add_option("--port-forward-tcp", m_PortForwardTCP, "Enable port-forwarding on the given TCP ports. Repeatable. (This distribution of Aura does not support this feature.)");
  app.add_option("--port-forward-udp", m_PortForwardUDP, "Enable port-forwarding on the given UDP ports. Repeatable. (This distribution of Aura does not support this feature.)");
#endif

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    if (0 == app.exit(e)) {
      return CLI_EARLY_RETURN;
    }
    return CLI_ERROR;
  } catch (...) {
    Print("[AURA] CLI unhandled exception");
    return CLI_ERROR;
  }

  if (!m_ExecCommands.empty() && !m_ExecAs.has_value()) {
    Print("[AURA] Option --exec-as is required");
    return CLI_ERROR;
  }

  if (m_ExecScope != "none" && m_ExecScope != "lobby" && m_ExecScope != "server") {
    if (m_ExecScope.substr(0, 5) != "game#") {
      Print("[AURA] Option --exec-scope accepts values: none, lobby, server, game#IDX");
      return CLI_ERROR;
    }
    string gameNumber = m_ExecScope.substr(5);
    try {
      int value = stoul(gameNumber);
      if (value < 0) {
        Print("[AURA] Option --exec-scope accepts values: none, lobby, server, game#IDX");
        return CLI_ERROR;
      }
    } catch (...) {
      Print("[AURA] Option --exec-scope accepts values: none, lobby, server, game#IDX");
      return CLI_ERROR;
    }
  }

  if (about || examples) {
    if (about) {
      m_EarlyAction = CLI_ACTION_ABOUT;
    } else if (examples) {
      m_EarlyAction = CLI_ACTION_EXAMPLES;
    }
    return CLI_EARLY_RETURN;
  }

  // Make sure directories have a trailing slash.
  // But m_CFGPath is just a file.
  if (m_HomePath.has_value()) NormalizeDirectory(m_HomePath.value());
  if (m_War3Path.has_value()) NormalizeDirectory(m_War3Path.value());
  if (m_MapPath.has_value()) NormalizeDirectory(m_MapPath.value());
  if (m_MapCFGPath.has_value()) NormalizeDirectory(m_MapCFGPath.value());
  if (m_MapCachePath.has_value()) NormalizeDirectory(m_MapCachePath.value());
  if (m_JASSPath.has_value()) NormalizeDirectory(m_JASSPath.value());
  if (m_GameSavePath.has_value()) NormalizeDirectory(m_GameSavePath.value());

  if (m_SearchTarget.has_value()) {
    if (!m_ExitOnStandby.has_value()) m_ExitOnStandby = true;
    if (!m_UseMapCFGCache.has_value()) m_UseMapCFGCache = !m_UseStandardPaths;
  }

  return CLI_OK;
}

void CCLI::RunEarlyOptions() const
{
  switch (m_EarlyAction) {
    case CLI_ACTION_ABOUT: {
      Print("Aura " + string(AURA_VERSION));
      Print("Aura is a permissive-licensed open source project.");
      Print("Say hi at <" + string(AURA_ISSUES_URL) + ">");
      break;
    }
    case CLI_ACTION_EXAMPLES: {
      Print("Usage: aura [MAP NAME] [GAME NAME]");
      Print(R"(Example: aura wormwar "worm wars")");
      Print(R"(Example: aura "lost temple" "2v2")");
      Print("See additional options at CLI.md");
      break;
    }
  }
}

void CCLI::OverrideConfig(CAura* nAura) const
{
  if (m_War3Version.has_value())
    nAura->m_Config->m_War3Version = m_War3Version.value();
  if (m_War3Path.has_value())
    nAura->m_Config->m_Warcraft3Path = m_War3Path.value();
  if (m_MapPath.has_value())
    nAura->m_Config->m_MapPath = m_MapPath.value();
  if (m_MapCFGPath.has_value())
    nAura->m_Config->m_MapCFGPath = m_MapCFGPath.value();
  if (m_MapCachePath.has_value())
    nAura->m_Config->m_MapCachePath = m_MapCachePath.value();
  if (m_JASSPath.has_value())
    nAura->m_Config->m_JASSPath = m_JASSPath.value();
  if (m_GameSavePath.has_value())
    nAura->m_Config->m_GameSavePath = m_GameSavePath.value();

  if (m_ExtractJASS.has_value())
    nAura->m_Config->m_ExtractJASS = m_ExtractJASS.value();

  if (m_ExitOnStandby.has_value()) {
    nAura->m_Config->m_ExitOnStandby = m_ExitOnStandby.value();
  }
  if (m_BNET.has_value()) {
    nAura->m_Config->m_EnableBNET = m_BNET.value();
  }
  if (m_IRC.has_value()) {
    nAura->m_IRC->m_Config->m_Enabled = m_IRC.value();
  }
  if (m_Discord.has_value()) {
    nAura->m_Discord->m_Config->m_Enabled = m_Discord.value();
  }
  if (m_LAN.has_value()) {
    nAura->m_GameDefaultConfig->m_UDPEnabled = m_LAN.value();
  }
  if (m_UseMapCFGCache.has_value()) {
    nAura->m_Config->m_EnableCFGCache = m_UseMapCFGCache.value();
  }
  if (m_LogLevel.has_value()) {
    vector<string> logLevels = {"emergency", "alert", "critical", "error", "warning", "notice", "info", "debug", "trace", "trace2"};
    uint8_t maxIndex = static_cast<uint8_t>(logLevels.size());
    for (uint8_t i = 0; i < maxIndex; ++i) {
      if (m_LogLevel.value() == logLevels[i]) {
        nAura->m_Config->m_LogLevel = 1 + i;
        break;
      }
    }
  }

  if (m_LANMode.has_value()) {
    const bool isMainServerEnabled = m_LANMode.value() != "free";
    nAura->m_Net->m_Config->m_UDPMainServerEnabled = isMainServerEnabled;
    if (!isMainServerEnabled) {
      nAura->m_Net->m_Config->m_UDPBroadcastStrictMode = m_LANMode.value() == "strict";
    }
  }

  if (m_BindAddress.has_value()) {
    optional<sockaddr_storage> address = CNet::ParseAddress(m_BindAddress.value(), ACCEPT_IPV4);
    if (address.has_value()) {
      nAura->m_Net->m_Config->m_BindAddress4 = address.value();
    }
  }

#ifndef DISABLE_MINIUPNP
  if (m_EnableUPnP.has_value()) {
    nAura->m_Net->m_Config->m_EnableUPnP = m_EnableUPnP.value();
  }
#endif
}

bool CCLI::QueueActions(CAura* nAura) const
{
  for (const auto& port : m_PortForwardTCP) {
    vector<string> action{
      "port-forward", "TCP", to_string(port), to_string(port)
    };
    nAura->m_PendingActions.push(action);
  }

  for (const auto& port : m_PortForwardUDP) {
    vector<string> action{
      "port-forward", "UDP", to_string(port), to_string(port)
    };
    nAura->m_PendingActions.push(action);
  }

  if (m_SearchTarget.has_value()) {
    CGameExtraOptions options;
    if (m_Observers.has_value()) options.ParseMapObservers(m_Observers.value());
    if (m_Visibility.has_value()) options.ParseMapVisibility(m_Visibility.value());
    if (m_RandomRaces.has_value()) options.m_RandomRaces = m_RandomRaces.value();
    if (m_RandomHeroes.has_value()) options.m_RandomHeroes = m_RandomHeroes.value();

    uint8_t searchType;
    if (m_SearchType.has_value()) {
      if (m_SearchType.value() == "map") {
        searchType = SEARCH_TYPE_ONLY_MAP;
      } else if (m_SearchType.value() == "config") {
        searchType = SEARCH_TYPE_ONLY_CONFIG;
      } else if (m_SearchType.value() == "local") {
        searchType = SEARCH_TYPE_ONLY_FILE;
      } else {
        searchType = SEARCH_TYPE_ANY;
      }
    } else if (m_UseStandardPaths) {
      searchType = SEARCH_TYPE_ONLY_FILE;
    } else {
      searchType = SEARCH_TYPE_ANY;
    }
    uint8_t displayMode;
    if (m_GameDisplayMode.has_value()) {
      if (m_GameDisplayMode.value() == "public") {
        displayMode = GAME_PUBLIC;
      } else if (m_GameDisplayMode.value() == "private") {
        displayMode = GAME_PRIVATE;
      } else {
        displayMode = GAME_NONE;
      }
    } else {
      displayMode = GAME_PUBLIC;
    }
    CCommandContext* ctx = new CCommandContext(nAura, false, &cout);
    optional<string> userName = GetUserMultiPlayerName();
    if (userName.has_value()) {
      ctx->SetIdentity(userName.value());
    }
    CGameSetup* gameSetup = new CGameSetup(nAura, ctx, m_SearchTarget.value(), searchType, true, m_UseStandardPaths, true, !m_CheckMapVersion.value_or(true));
    if (gameSetup) {
      if (m_GameSavedPath.has_value()) gameSetup->SetGameSavedFile(m_GameSavedPath.value());
      if (m_GameMapDownloadTimeout.has_value()) gameSetup->SetDownloadTimeout(m_GameMapDownloadTimeout.value());
      if (gameSetup->LoadMapSync()) {
        if (gameSetup->ApplyMapModifiers(&options)) {
          if (!gameSetup->m_SaveFile.empty()) {
            bool loadFailure = false;
            if (!gameSetup->RestoreFromSaveFile()) {
              Print("[AURA] Invalid save file [" + PathToString(gameSetup->m_SaveFile) + "]");
              loadFailure = true;
            } else if (m_GameCheckReservation.has_value() && !m_GameCheckReservation.value()) {
              Print("[AURA] Resuming a loaded game must always check reservations.");
              loadFailure = true;
            } else if (m_GameLobbyAutoRehosted.value_or(false)) {
              // Do not allow automatically rehosting loads of the same savefile,
              // Because that would mean keeping the CSaveGame around.
              // Also, what's this? The Battle for Wesnoth?
              Print("[AURA] A loaded game cannot be auto rehosted.");
              loadFailure = true;
            }
            if (loadFailure) {
              delete gameSetup;
              nAura->UnholdContext(ctx);
              return false;
            }
          }
          for (const auto& id : m_ExcludedRealms) {
            CRealm* excludedRealm = nAura->GetRealmByInputId(id);
            if (excludedRealm) {
              nAura->m_GameSetup->AddIgnoredRealm(excludedRealm);
            } else {
              Print("[AURA] Unrecognized realm [" + id + "] ignored by --exclude");
            }
          }
          if (m_GameMapAlias.has_value()) {
            string normalizedAlias = GetNormalizedAlias(m_GameMapAlias.value());
            string mapFileName = gameSetup->GetMap()->GetServerFileName();
            if (nAura->m_DB->AliasAdd(normalizedAlias, mapFileName)) {
              Print("[AURA] Alias <<" + m_GameMapAlias.value() + ">> added for [" + mapFileName + "]");
            } else {
              Print("Failed to add alias.");
            }
          }
          if (m_MirrorSource.has_value()) {
            if (!gameSetup->SetMirrorSource(m_MirrorSource.value())) {
              Print("[AURA] Invalid mirror source [" + m_MirrorSource.value() + "]. Ensure it has the form IP:PORT#ID");
              delete gameSetup;
              nAura->UnholdContext(ctx);
              return false;
            }
          }
          if (m_GameName.has_value()) {
            gameSetup->SetBaseName(m_GameName.value());
          } else {
            if (userName.has_value()) {
              gameSetup->SetBaseName(userName.value() + "'s game");
            } else {
              gameSetup->SetBaseName("Join and play");
            }
          }
          if (userName.has_value()) {
            gameSetup->SetCreator(userName.value());
          }
          if (m_GameLobbyTimeout.has_value()) gameSetup->SetLobbyTimeout(m_GameLobbyTimeout.value());
          if (m_GameCheckJoinable.has_value()) gameSetup->SetIsCheckJoinable(m_GameCheckJoinable.value());
          if (m_GameCheckReservation.has_value()) gameSetup->SetCheckReservation(m_GameCheckReservation.value());
          if (m_GameLobbyReplaceable.has_value()) gameSetup->SetLobbyReplaceable(m_GameLobbyReplaceable.value());
          if (m_GameLobbyAutoRehosted.has_value()) gameSetup->SetLobbyAutoRehosted(m_GameLobbyAutoRehosted.value());
          if (m_GameAutoStartPlayers.has_value()) gameSetup->SetAutoStartPlayers(m_GameAutoStartPlayers.value());
          if (m_GameAutoStartMinSeconds.has_value()) gameSetup->SetAutoStartMinSeconds(m_GameAutoStartMinSeconds.value());
          if (m_GameAutoStartMaxSeconds.has_value()) gameSetup->SetAutoStartMaxSeconds(m_GameAutoStartMaxSeconds.value());
          if (m_GameFreeForAll.value_or(false)) gameSetup->SetCustomLayout(CUSTOM_LAYOUT_FFA);
          gameSetup->SetReservations(m_GameReservations);
          gameSetup->SetVerbose(m_Verbose);
          gameSetup->SetDisplayMode(displayMode);
          gameSetup->SetActive();
          vector<string> hostAction{"host"};
          nAura->m_PendingActions.push(hostAction);
        } else {
          ctx->ErrorReply("Invalid map options. Map has fixed player settings.");
          delete gameSetup;
          nAura->UnholdContext(ctx);
          return false;
        }
      }
    } else {
      if (searchType == SEARCH_TYPE_ANY) {
        ctx->ErrorReply("Input does not refer to a valid map, config, or URL.");
      } else if (searchType == SEARCH_TYPE_ONLY_FILE) {
        ctx->ErrorReply("Input does not refer to a valid file");
      } else if (searchType == SEARCH_TYPE_ONLY_MAP) {
        ctx->ErrorReply("Input does not refer to a valid map (.w3x, .w3m)");
      } else if (searchType == SEARCH_TYPE_ONLY_CONFIG) {
        ctx->ErrorReply("Input does not refer to a valid map config file (.cfg)");
      }
      delete gameSetup;
      nAura->UnholdContext(ctx);
      return false;
    }
  }

  for (const auto& execEntry : m_ExecCommands) {
    vector<string> action{
        "exec", execEntry, m_ExecAs.value(), m_ExecScope, m_ExecAuth, m_ExecBroadcast ? "1" : "0"
    };
    nAura->m_PendingActions.push(action);
  }

  return true;
}
