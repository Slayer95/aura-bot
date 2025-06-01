/*

  Copyright [2024-2025] [Leonardo Julca]

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

#include "realm_games.h"

#include "protocol/bnet_protocol.h"
#include "includes.h"
#include "map.h"
#include "game_host.h"

using namespace std;

//
// GameSearchQuery
//

GameSearchQuery::GameSearchQuery()
{
}

GameSearchQuery::GameSearchQuery(const Version& gameVersion, const string& gameName, const string& hostName, shared_ptr<CMap> map)
 : m_GameVersion(gameVersion),
   m_Map(map),
   m_GameName(gameName),
   m_HostName(hostName)
{
}

GameSearchQuery::~GameSearchQuery()
{
}

bool GameSearchQuery::GetIsMatch(const GameHost& gameHost) const
{
  if (!m_GameName.empty() && gameHost.GetGameName() != m_GameName) return false;
  if (!m_HostName.empty() && gameHost.GetHostName() != m_HostName) return false;
  if (m_Map) {
    if (m_Map->GetMapScriptsBlizz(m_GameVersion) != gameHost.GetMapScriptsBlizz()) return false;
    if (m_Map->GetClientFileName() != gameHost.GetMapClientFileName()) return false;
  }
  return true;
}

bool GameSearchQuery::EventMatch(const GameHost& gameHost)
{
  if (m_Map) m_Map->SetGameConvertedFlags(gameHost.GetMapFlags());
  return false; // stop searching
}
