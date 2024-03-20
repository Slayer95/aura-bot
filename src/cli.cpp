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
#ifndef DISABLE_MINIUPNP
  app.add_flag("--auto-port-forward,--no-auto-port-forward{false}", m_EnableUPnP, "Enable automatic port-forwarding, using Universal Plug-and-Play.");
#else
  app.add_flag("--auto-port-forward,--no-auto-port-forward{false}", m_EnableUPnP, "Enable automatic port-forwarding, using Universal Plug-and-Play. (This distribution of Aura does not support this feature.)");
#endif
  app.add_flag("--lan,--no-lan{false}", m_LAN, "Show hosted games on Local Area Network.");
  app.add_flag("--bnet,--no-bnet{false}", m_BNET, "Switch to enable or disable every defined realm.");
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
  app.add_option("-s,--search-type", m_SearchType, "Restricts file searches when hosting from the CLI. Values: map, config, local, any")->check(CLI::IsMember({"map", "config", "local", "any"}))->default_val("any");
  app.add_option("--lan-mode", m_LANMode, "Customizes the behavior of the game discovery service. Values: strict, lax, free.")->check(CLI::IsMember({"strict", "lax", "free"}));

  // Game hosting
  app.add_option("--owner", m_Owner, "Customizes the game owner when hosting from the CLI.");
  app.add_option("--observers", m_Observers, "Customizes observers when hosting from the CLI. Values: no, referees, defeat, full")->check(CLI::IsMember({"no", "referees", "defeat", "full"}));
  app.add_option("--visibility", m_Visibility, "Customizes visibility when hosting from the CLI. Values: default, hide, explored, visible")->check(CLI::IsMember({"default", "hide", "explored", "visible"}));
  app.add_option("--random-races", m_RandomRaces, "Toggles random races when hosting from the CLI.");
  app.add_option("--random-heroes", m_RandomHeroes, "Toggles random heroes when hosting from the CLI.");
  app.add_option("--mirror", m_MirrorSource, "Mirrors a game, listing it in the connected realms. Syntax: IP:PORT#ID.");
  app.add_option("--exclude", m_ExcludedRealms, "Hides the game in the listed realm(s). Repeatable.");
  app.add_option("--timeout", m_GameTimeout, "Sets the time limit for the game lobby.");
  app.add_flag(  "--check-joinable", m_GameCheckJoinable, "Reports whether the game is joinable over the Internet.");

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

  if (m_ExtractJASS.has_value())
    nAura->m_Config->m_ExtractJASS = m_ExtractJASS.value();

  if (m_ExitOnStandby.has_value()) {
    nAura->m_Config->m_ExitOnStandby = m_ExitOnStandby.value();
  }
  if (m_BNET.has_value()) {
    nAura->m_Config->m_EnableBNET = m_BNET.value();
  }
  if (m_LAN.has_value()) {
    nAura->m_GameDefaultConfig->m_UDPEnabled = m_LAN.value();
  }
  if (m_UseMapCFGCache.has_value()) {
    nAura->m_Config->m_EnableCFGCache = m_UseMapCFGCache.value();
  }

  if (m_LANMode.has_value()) {
    const bool isMainServerEnabled = m_LANMode.value() != "free";
    nAura->m_Net->m_Config->m_UDPMainServerEnabled = isMainServerEnabled;
    if (!isMainServerEnabled) {
      nAura->m_Net->m_Config->m_UDPBroadcastStrictMode = m_LANMode.value() == "strict";
    }
  }
  if (m_EnableUPnP.has_value()) {
    nAura->m_Net->m_Config->m_EnableUPnP = m_EnableUPnP.value();
  }
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
    CCommandContext* ctx = new CCommandContext(nAura, &cout, '!');
    CGameSetup* gameSetup = new CGameSetup(nAura, ctx, m_SearchTarget.value(), searchType, true, m_UseStandardPaths, true);
    if (gameSetup && gameSetup->LoadMap()) {
      if (gameSetup->ApplyMapModifiers(&options)) {
        for (const auto& id : m_ExcludedRealms) {
          CRealm* excludedRealm = nAura->GetRealmByInputId(id);
          if (excludedRealm) {
            nAura->m_GameSetup->AddIgnoredRealm(excludedRealm);
          } else {
            Print("[AURA] Unrecognized realm [" + id + "] ignored by --exclude");
          }
        }
        if (m_MirrorSource.has_value()) {
          if (!gameSetup->SetMirrorSource(m_MirrorSource.value())) {
            Print("[AURA] Invalid mirror source [" + m_MirrorSource.value() + "]. Ensure it has the form IP:PORT#ID");
            nAura->UnholdContext(ctx);
            delete gameSetup;
            return false;
          }
        }
        gameSetup->SetName(m_GameName.value_or("Join and play"));
        if (m_GameTimeout.has_value()) gameSetup->SetLobbyTimeout(m_GameTimeout.value());
        if (m_GameCheckJoinable.has_value()) gameSetup->SetIsCheckJoinable(m_GameCheckJoinable.value());
        gameSetup->SetVerbose(m_Verbose);
        gameSetup->SetActive();
        vector<string> hostAction{"host"};
        nAura->m_PendingActions.push(hostAction);
      } else {
        ctx->ErrorReply("Invalid map options. Map has fixed player settings.");
        delete gameSetup;
        nAura->UnholdContext(ctx);
        return false;
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
