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

/*

   Copyright [2010] [Josko Nikolic]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT

 */

#ifndef AURA_DBGAMEPLAYER_H_
#define AURA_DBGAMEPLAYER_H_

#include "includes.h"
#include "game_user.h"

//
// CDBGamePlayer
//

class CDBGamePlayer
{
private:
  std::string               m_Name;
  std::string               m_Server;
  std::string               m_IP;
  uint64_t                  m_LoadingTime;
  uint64_t                  m_LeftTime;
  uint8_t                   m_UID;
  uint8_t                   m_SID;
  uint8_t                   m_Color;
  uint8_t                   m_GameResult;

public:
  CDBGamePlayer(const GameUser::CGameUser* user, const IndexedGameSlot& slot);
  ~CDBGamePlayer();

  [[nodiscard]] inline std::string GetName() const { return m_Name; }
  [[nodiscard]] inline std::string GetServer() const { return m_Server; }
  [[nodiscard]] inline std::string GetIP() const { return m_IP; }
  [[nodiscard]] inline uint64_t    GetLoadingTime() const { return m_LoadingTime; }
  [[nodiscard]] inline uint64_t    GetLeftTime() const { return m_LeftTime; }
  [[nodiscard]] inline uint8_t     GetUID() const { return m_UID; }
  [[nodiscard]] inline uint8_t     GetSID() const { return m_SID; }
  [[nodiscard]] inline uint8_t     GetColor() const { return m_Color; }
  [[nodiscard]] inline uint8_t     GetGameResult() const { return m_GameResult; }

  inline void SetLoadingTime(uint64_t nLoadingTime) { m_LoadingTime = nLoadingTime; }
  inline void SetLeftTime(uint64_t nLeftTime) { m_LeftTime = nLeftTime; }
  inline void SetGameResult(uint8_t nGameResult) { m_GameResult = nGameResult; }
};

#endif // AURA_DBGAMEPLAYER_H_
