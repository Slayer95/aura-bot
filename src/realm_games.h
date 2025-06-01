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

#ifndef AURA_REALM_GAMES_H_
#define AURA_REALM_GAMES_H_

#include "includes.h"

struct GameSearchQuery
{
  Version                                             m_GameVersion;
  std::shared_ptr<CMap>                               m_Map;
  std::string                                         m_GameName;
  std::string                                         m_HostName;

  GameSearchQuery();
  GameSearchQuery(const Version& gameVersion, const std::string& gameName, const std::string& hostName, std::shared_ptr<CMap> map);
  ~GameSearchQuery();

  [[nodiscard]] bool GetIsMatch(const GameHost& gameHost) const;
  [[nodiscard]] bool EventMatch(const GameHost& gameHost);
};

#endif // AURA_REALM_GAMES_H_
