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
#include "gamesetup.h"

using namespace std;

//
// CCLI
//

CCLI::CCLI(CAura* nAura)
 : m_Aura(nAura)
{
}

CCLI::~CCLI() = default;

bool CCLI::Parse(const int argc, const char* argv[])
{
  argh::parser cmdl({
    // TODO(IceSandslash): Switch to another CLI parser library that properly supports flags
    // Flags
    //"--version", "--help", "--stdpaths",
    //"--lan", "--bnet", "--exit", "--cache",
    //"--no-lan", "--no-bnet", "--no-exit", "--no-cache",

    // Parameters
    "--w3version", "--w3path", "--mapdir", "--cfgdir", "--filetype",
    "--lan-mode",
    "--exclude", "--mirror",
    "--observers", "--visibility", "--random-races", "--random-heroes", "--owner",
    "--exec-as", "--exec-auth", "exec-scope", "--exec"
  });

  cmdl.parse(argc, argv, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);

  if (cmdl("--version")) {
    Print("--version");
    return false;
  }
  if (cmdl("help")) {
    Print("Usage:");
    Print("aura DotA.w3x \"come and play\"");
    return false;
  }

  optional<bool> bnet = m_Aura->m_Config->m_EnableBNET;
  bool noexit = !m_Aura->m_Config->m_ExitOnStandby;
  bool nolan = !m_Aura->m_GameDefaultConfig->m_UDPEnabled;
  bool nocache = !m_Aura->m_Config->m_EnableCFGCache;
  bool stdpaths = false;
  
  uint32_t War3Version = m_Aura->m_Config->m_War3Version.has_value() ? m_Aura->m_Config->m_War3Version.value() : 0;
  string War3Path = m_Aura->m_Config->m_Warcraft3Path.has_value() ? m_Aura->m_Config->m_Warcraft3Path.value().string() : string();
  string MapPath = m_Aura->m_Config->m_MapPath.string();
  string MapCFGPath = m_Aura->m_Config->m_MapCFGPath.string();
  string MapFileType = "map";
  string LANMode = m_Aura->m_Net->m_UDPMainServerEnabled ? "strict": "free";

  cmdl("w3version", War3Version) >> War3Version;
  cmdl("w3path", War3Path) >> War3Path;
  cmdl("mapdir", MapPath) >> MapPath;
  cmdl("cfgdir", MapCFGPath) >> MapCFGPath;
  cmdl("filetype", MapFileType) >> MapFileType;
  cmdl("lan-mode", LANMode) >> LANMode;

  cmdl("no-exit", noexit) >> noexit;
  //cmdl("no-bnet", nobnet) >> nobnet;
  cmdl("no-lan", nolan) >> nolan;
  cmdl("no-cache", nocache) >> nocache;
  cmdl("stdpaths") >> stdpaths;

  string rawFirstArgument = cmdl[1];
  if (!rawFirstArgument.empty()) {
    string gameName = cmdl[2].empty() ? "Join and play" : cmdl[2];
    string mirrorTarget, excludedRealm;
    string observerMode, visionMode, randomRaces, randomHeroes, owner;
    cmdl("mirror") >> mirrorTarget;
    cmdl("exclude") >> excludedRealm;      
    cmdl("observers") >> observerMode;
    cmdl("visibility") >> visionMode;
    cmdl("random-races") >> randomRaces;
    cmdl("random-heroes") >> randomHeroes;
    cmdl("owner") >> owner;

    string targetMapPath = rawFirstArgument;
    if (stdpaths) {
      filesystem::path mapPath = targetMapPath;
      targetMapPath = filesystem::absolute(mapPath).lexically_normal().string();
    }

    CGameExtraOptions options;
    options.ParseMapObservers(observerMode);
    options.ParseMapVisibility(visionMode);
    options.ParseMapRandomRaces(randomRaces);
    options.ParseMapRandomHeroes(randomHeroes);

    uint8_t searchType = SEARCH_TYPE_ANY;
    if (MapFileType == "map") {
      searchType = SEARCH_TYPE_ONLY_MAP;
    } else if (MapFileType == "config") {
      searchType = SEARCH_TYPE_ONLY_CONFIG;
    } else if (MapFileType == "local") {
      searchType = SEARCH_TYPE_ONLY_FILE;
    }
    CCommandContext* ctx = new CCommandContext(m_Aura, &cout, '!');
    CGameSetup* gameSetup = new CGameSetup(m_Aura, ctx, targetMapPath, searchType, stdpaths ? SETUP_USE_STANDARD_PATHS : SETUP_PROTECT_ARBITRARY_TRAVERSAL, true);
    if (gameSetup && gameSetup->LoadMap()) {
      if (gameSetup->ApplyMapModifiers(&options)) {
        // CRealm instances do not exist yet. Just use the user input verbatim.
        if (!excludedRealm.empty()) m_Aura->m_GameSetup->AddIgnoredRealm(excludedRealm);
        if (!mirrorTarget.empty()) {
          gameSetup->SetMirrorSource(mirrorTarget);
        }
        gameSetup->SetName(gameName);
        gameSetup->SetActive();
        vector<string> hostAction = {"host"};
        m_Aura->m_PendingActions.push_back(hostAction);
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
    noexit = false;
    //nocache = cmdl("stdpaths");
  }

  m_Aura->m_Config->m_War3Version = War3Version;
  m_Aura->m_Config->m_Warcraft3Path = War3Path;
  m_Aura->m_Config->m_MapPath = MapPath;
  m_Aura->m_Config->m_MapCFGPath = MapCFGPath;

  m_Aura->m_Config->m_ExitOnStandby = !noexit;
  //m_Aura->m_Config->m_EnableBNET = !nobnet;
  m_Aura->m_GameDefaultConfig->m_UDPEnabled = !nolan;
  m_Aura->m_Config->m_EnableCFGCache = !nocache;

  if (LANMode == "strict" || LANMode == "lax") {
    m_Aura->m_Net->m_UDPMainServerEnabled = true;
  } else if (LANMode == "free") {
    m_Aura->m_Net->m_UDPMainServerEnabled = false;
  } else {
    Print("Bad UDPMode: " + LANMode);
  }

  string execCommand, execAs, execScope, execAuth;
  cmdl("exec", execCommand) >> execCommand;
  cmdl("exec-as", execAs) >> execAs;
  cmdl("exec-scope", execScope) >> execScope;
  cmdl("exec-auth", execAuth) >> execAuth;

  if (!execCommand.empty() && !execAs.empty() && !execAuth.empty()) {
    vector<string> action;
    action.push_back("exec");
    action.push_back(execCommand);
    action.push_back(execAs);
    action.push_back(execScope);
    action.push_back(execAuth);
    m_Aura->m_PendingActions.push_back(action);
  }

  return true;
}
