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

#ifndef AURA_CONFIG_BOT_H_
#define AURA_CONFIG_BOT_H_

#include "config.h"

#include <vector>
#include <set>
#include <string>
#include <map>

//
// CBotConfig
//

class CBotConfig
{
public:

  bool                                    m_Enabled;                     // set to false to prevent new games from being created
  std::optional<uint8_t>                  m_War3Version;                 // warcraft 3 version
  std::optional<std::filesystem::path>    m_Warcraft3Path;               // Warcraft 3 path
  std::filesystem::path                   m_MapCFGPath;                  // map cfg path
  std::filesystem::path                   m_MapPath;                     // map path

  std::filesystem::path                   m_GreetingPath;                // the path of the greeting the bot sends to all players joining a game
  std::vector<std::string>                m_Greeting;                    // read from m_GreetingPath

  bool                                    m_RTTPings;                    // use LC style pings (divide actual pings by two

  uint32_t                                m_MinHostCounter;              // defines a subspace for game identifiers
  uint32_t                                m_MaxGames;                    // maximum number of games in progress
  bool                                    m_EnableDeleteOversizedMaps;   // may delete maps in m_MapPath exceeding m_MaxSavedMapSize
  uint32_t                                m_MaxSavedMapSize;             // maximum byte size of maps kept persistently in the m_MapPath folder

  bool                                    m_StrictSearch;                // accept only exact paths (no fuzzy searches) for maps, etc.
  bool                                    m_MapSearchShowSuggestions;
  bool                                    m_EnableCFGCache;              // save read map CFGs to disk
  uint8_t                                 m_CFGCacheRevalidateAlgorithm; // always, never, modified

  bool                                    m_ExitOnStandby;
  std::optional<bool>                     m_EnableBNET;                  // master switch to enable/disable ALL bnet configs on startup
  
  explicit CBotConfig(CConfig& CFG);
  ~CBotConfig();
};

#endif
