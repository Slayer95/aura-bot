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

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#include "includes.h"
#include "aura.h"
#include "game.h"
#include "gameprotocol.h"

#include <map>
#include <utility>
#include <vector>

#ifndef W3MMD_H
#define W3MMD_H

#define MMD_VALUE_TYPE_INT 0
#define MMD_VALUE_TYPE_REAL 1
#define MMD_VALUE_TYPE_STRING 2

#define MMD_RESULT_LOSER 0
#define MMD_RESULT_DRAWER 1
#define MMD_RESULT_WINNER 2

class CAura;
class CGame;
class CIncomingAction;

//
// CW3MMD
//

typedef std::pair<uint32_t, std::string> VarP;

class CW3MMD
{
private:
  CGame*                                          m_Game;
  bool                                            m_GameOver;
  bool                                            m_Error;
  uint32_t                                        m_NextValueID;
  uint32_t                                        m_NextCheckID;
  std::map<uint32_t, std::string>                 m_PIDToName;           // pid -> player name (e.g. 0 -> "Varlock") --- note: will not be automatically converted to lower case
  std::map<uint32_t, uint8_t>                     m_Flags;               // pid -> flag (e.g. 0 -> MMD_RESULT_WINNER)
  std::map<uint32_t, bool>                        m_FlagsLeaver;         // pid -> leaver flag (e.g. 0 -> true) --- note: will only be present if true
  std::map<uint32_t, bool>                        m_FlagsPracticing;     // pid -> practice flag (e.g. 0 -> true) --- note: will only be present if true
  std::map<std::string, uint8_t>                  m_DefVarPs;            // varname -> value type (e.g. "kills" -> MMD_VALUE_TYPE_INT)
  std::map<VarP, int32_t>                         m_VarPInts;            // pid,varname -> value (e.g. 0,"kills" -> 5)
  std::map<VarP, double>                          m_VarPReals;           // pid,varname -> value (e.g. 0,"x" -> 0.8)
  std::map<VarP, std::string>                     m_VarPStrings;         // pid,varname -> value (e.g. 0,"hero" -> "heroname")
  std::map<std::string, std::vector<std::string>> m_DefEvents;           // event -> vector of arguments + format

public:
  CW3MMD(CGame* nGame);
  ~CW3MMD();

  inline bool GetIsGameOver() { return m_GameOver; }

  bool HandleTokens(std::vector<std::string> tokens);
  bool ProcessAction(CIncomingAction *Action);
  std::vector<std::string> TokenizeKey(std::string key) const;
  std::vector<std::string> GetWinners() const;
  std::string GetLogPrefix() const;
};

#endif
