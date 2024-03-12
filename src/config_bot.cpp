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

#include "config_bot.h"
#include "util.h"
#include "net.h"

#include <utility>
#include <algorithm>
#include <fstream>

using namespace std;

//
// CBotConfig
//

CBotConfig::CBotConfig(CConfig* CFG)
{
  m_Enabled                      = CFG->GetBool("hosting.enabled", true);
  m_War3Version                  = CFG->GetMaybeInt("game.version");
  CFG->FailIfErrorLast();
  m_Warcraft3Path                = CFG->GetMaybePath("game.install_path");
  m_MapCFGPath                   = CFG->GetPath("bot.map_configs_path", filesystem::path());
  m_MapPath                      = CFG->GetPath("bot.maps_path", filesystem::path());

  m_RTTPings                     = CFG->GetBool("metrics.rtt_pings", false);

  m_MinHostCounter               = CFG->GetInt("hosting.namepace.first_game_id", 100);
  m_MaxGames                     = CFG->GetInt("hosting.max_games", 20);
  m_MaxSavedMapSize              = CFG->GetInt("bot.max_persistent_size", 0xFFFFFFFF);

  optional<filesystem::path> maybeGreeting = CFG->GetMaybePath("bot.greeting_path");
  if (maybeGreeting.has_value() && !maybeGreeting.value().empty()) {
    m_Greeting = ReadChatTemplate(maybeGreeting.value());
  }

  m_StrictSearch                  = CFG->GetBool("bot.load_maps.strict_search", false);
  m_MapSearchShowSuggestions     = CFG->GetBool("bot.load_maps.show_suggestions", true);
  m_EnableCFGCache               = CFG->GetBool("bot.load_maps.cache.enabled", true);

  m_ExitOnStandby                = CFG->GetBool("bot.exit_on_standby", false);

  // Master switch mainly intended for CLI. CFG key provided for completeness.
  m_EnableBNET                   = CFG->GetMaybeBool("bot.toggle_every_realm");
}

CBotConfig::~CBotConfig() = default;
