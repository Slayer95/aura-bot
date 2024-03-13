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
 : m_EarlyAction(0),
   m_UseStandardPaths(false),
   m_SearchType("any"),
   m_ExecAuth("verified"),
   m_ExecScope("none")
{
}

CCLI::~CCLI() = default;

uint8_t CCLI::Parse(const int argc, char** argv)
{
  CLI::App app{"Warcraft III host bot"};
  argv = app.ensure_utf8(argv);

  bool examples = false;
  bool about = false;

  app.option_defaults()->ignore_case();

  app.add_option("target", m_SearchTarget, "Map, config or URI from which to host a game from the CLI.");
  app.add_option("name", m_GameName, "Name assigned to a game hosted from the CLI.");

  app.add_flag("--stdpaths", m_UseStandardPaths, "Makes relative paths resolve from CWD when input through the CLI. Commutative.");
  app.add_flag("--lan,--no-lan{false}", m_LAN, "Show hosted games on Local Area Network.");
  app.add_flag("--bnet,--no-bnet{false}", m_BNET, "Switch to enable or disable every defined realm.");
  app.add_flag("--exit,--no-exit{false}", m_ExitOnStandby, "Terminates the process when idle.");
  app.add_flag("--cache,--no-cache{false}", m_UseMapCFGCache, "Caches loaded map files into the map configs folder.");
  app.add_flag("--about,--version", about, "Display software information.");
  app.add_flag("--example,--examples", examples, "Display examples.");
  // TODO: --help ??

  app.add_option("--config", m_CFGPath, "Customizes the main aura config file. Affected by --stdpaths");
  app.add_option("--w3version", m_War3Version, "Customizes the game version.");
  app.add_option("--w3path", m_War3Path, "Customizes the game path.");
  app.add_option("--mapdir", m_MapPath, "Customizes the maps path.");
  app.add_option("--cfgdir", m_MapCFGPath, "Customizes the map configs path.");
  app.add_option("-s,--search-type", m_SearchType, "Restricts file searches when hosting from the CLI. Values: map, config, local, any")->check(CLI::IsMember({"map", "config", "local", "any"}))->default_val("any");
  app.add_option("--lan-mode", m_LANMode, "Customizes the behavior of the game discovery service. Values: strict, lax, free.")->check(CLI::IsMember({"strict", "lax", "free"}));

  app.add_option("--owner", m_Owner, "Customizes the game owner when hosting from the CLI.");
  app.add_option("--observers", m_Observers, "Customizes observers when hosting from the CLI. Values: no, referees, defeat, full")->check(CLI::IsMember({"no", "referees", "defeat", "full"}));
  app.add_option("--visibility", m_Visibility, "Customizes visibility when hosting from the CLI. Values: default, hide, explored, visible")->check(CLI::IsMember({"default", "hide", "explored", "visible"}));
  app.add_option("--random-races", m_RandomRaces, "Toggles random races when hosting from the CLI.");
  app.add_option("--random-heroes", m_RandomHeroes, "Toggles random heroes when hosting from the CLI.");
  app.add_option("--mirror", m_MirrorSource, "Mirrors a game, listing it in the connected realms. Syntax: IP:PORT#ID:KEY.");
  app.add_option("--exclude", m_ExcludedRealms, "Hides the game in the listed realm(s). Repeatable.");

  app.add_option("--exec", m_ExecCommands, "Runs a command from the CLI. Repeatable.");
  app.add_option("--exec-as", m_ExecAs, "Customizes the user identity when running commands from the CLI.");
  app.add_option("--exec-auth", m_ExecAuth, "Customizes the user permissions when running commands from the CLI.")->check(CLI::IsMember({"spoofed", "verified", "admin", "rootadmin", "sudo"}))->default_val("verified");
  app.add_option("--exec-scope", m_ExecScope, "Customizes the channel when running commands from the CLI. Values: none, lobby, server, game#IDX")->default_val("none");
  app.add_option("--exec-broadcast", m_ExecBroadcast, "Enables broadcasting the command execution to all users in the channel");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    Print("[AURA] invalid CLI usage: " + string(e.what()));
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

  if (m_UseStandardPaths) {
    if (m_SearchTarget.has_value()) m_SearchTarget = filesystem::absolute(m_SearchTarget.value()).lexically_normal().string();
    if (m_CFGPath.has_value()) m_CFGPath = filesystem::absolute(m_CFGPath.value());
    if (m_War3Path.has_value()) m_War3Path = filesystem::absolute(m_War3Path.value());
    if (m_MapPath.has_value()) m_MapPath = filesystem::absolute(m_MapPath.value());
    if (m_MapCFGPath.has_value()) m_MapCFGPath = filesystem::absolute(m_MapCFGPath.value());
  } else {
    if (m_CFGPath.has_value() && !m_CFGPath.value().is_absolute()) m_CFGPath = GetExeDirectory() / m_CFGPath.value();
    if (m_War3Path.has_value() && !m_War3Path.value().is_absolute()) m_War3Path = GetExeDirectory() / m_War3Path.value();
    if (m_MapPath.has_value() && !m_MapPath.value().is_absolute()) m_MapPath = GetExeDirectory() / m_MapPath.value();
    if (m_MapCFGPath.has_value() && !m_MapCFGPath.value().is_absolute()) m_MapCFGPath = GetExeDirectory() / m_MapCFGPath.value();
  }
  if (m_CFGPath.has_value()) NormalizeDirectory(m_CFGPath.value());
  if (m_War3Path.has_value()) NormalizeDirectory(m_War3Path.value());
  if (m_MapPath.has_value()) NormalizeDirectory(m_MapPath.value());
  if (m_MapCFGPath.has_value()) NormalizeDirectory(m_MapCFGPath.value());

  if (m_SearchTarget.has_value()) {
    if (!m_ExitOnStandby.has_value()) m_ExitOnStandby = true;
    if (!m_UseMapCFGCache.has_value()) m_UseMapCFGCache = !m_UseStandardPaths;
  }

  return CLI_OK;
}

void CCLI::RunEarlyOptions()
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

void CCLI::OverrideConfig(CAura* nAura)
{
  if (m_War3Version.has_value())
    nAura->m_Config->m_War3Version = m_War3Version.value();
  if (m_War3Path.has_value())
    nAura->m_Config->m_Warcraft3Path = m_War3Path.value();
  if (m_MapPath.has_value())
    nAura->m_Config->m_MapPath = m_MapPath.value();
  if (m_MapCFGPath.has_value())
    nAura->m_Config->m_MapCFGPath = m_MapCFGPath.value();


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
}

void CCLI::QueueActions(CAura* nAura)
{
  if (m_SearchTarget.has_value()) {
    string gameName = m_GameName.value_or("Join and play");

    CGameExtraOptions options;
    if (m_Observers.has_value()) options.ParseMapObservers(m_Observers.value());
    if (m_Visibility.has_value()) options.ParseMapVisibility(m_Visibility.value());
    if (m_RandomRaces.has_value()) options.m_RandomRaces = m_RandomRaces.value();
    if (m_RandomHeroes.has_value()) options.m_RandomHeroes = m_RandomHeroes.value();

    uint8_t searchType;
    if (m_SearchType == "map") {
      searchType = SEARCH_TYPE_ONLY_MAP;
    } else if (m_SearchType == "config") {
      searchType = SEARCH_TYPE_ONLY_CONFIG;
    } else if (m_SearchType == "local") {
      searchType = SEARCH_TYPE_ONLY_FILE;
    } else {
      searchType = SEARCH_TYPE_ANY;
    }
    CCommandContext* ctx = new CCommandContext(nAura, &cout, '!');
    CGameSetup* gameSetup = new CGameSetup(nAura, ctx, m_SearchTarget.value(), searchType, m_UseStandardPaths, true);
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
            Print("[AURA] Invalid mirror source [" + m_MirrorSource.value() + "]. Ensure it has the form IP:PORT#ID:KEY");
            delete gameSetup;
            delete ctx;
            return;
          }
        }
        gameSetup->SetName(gameName);
        gameSetup->SetActive();
        vector<string> hostAction = {"host"};
        nAura->m_PendingActions.push_back(hostAction);
      } else {
        ctx->ErrorReply("Invalid map options. Map has fixed player settings.");
        delete gameSetup;
        delete ctx;
      }
    } else {
      ctx->ErrorReply("Invalid game hosting parameters. Please ensure the provided file is valid. See also: CLI.md");
      delete gameSetup;
      delete ctx;
    }
  }

  while (!m_ExecCommands.empty()) {
    string execCommand, execAs, execScope, execAuth;
    vector<string> action;
    action.push_back("exec");
    action.push_back(m_ExecCommands[0]);
    action.push_back(m_ExecAs.value());
    action.push_back(m_ExecScope);
    action.push_back(m_ExecAuth);
    action.push_back(m_ExecBroadcast ? "1" : "0");
    nAura->m_PendingActions.push_back(action);
    m_ExecCommands.erase(m_ExecCommands.begin());
  }
}
