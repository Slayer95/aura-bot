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

#include "util.h"
#include "w3mmd.h"

using namespace std;

//
// CW3MMD
//

CW3MMD::CW3MMD(CGame *nGame)
  : m_Game(nGame),
    m_GameOver(false),
    m_Error(false),
    m_NextValueID(0),
    m_NextCheckID(0)
{
  Print("[W3MMD] using Warcraft 3 Map Meta Data stats parser version 1");
}

CW3MMD::~CW3MMD()
{
}

bool CW3MMD::HandleTokens(vector<string> Tokens)
{
  if (Tokens.empty()) {
    return false;
  }
  const string& actionType = Tokens[0];
  if (actionType == "init" && Tokens.size() >= 2) {
    if (Tokens[1] == "version" && Tokens.size() == 4) {
      // Tokens[2] = minimum
      // Tokens[3] = current

      Print(GetLogPrefix() + "map is using Warcraft 3 Map Meta Data library version [" + Tokens[3] + "]");
      optional<uint32_t> version = ToUint32(Tokens[2]);
      if (!version.has_value()) return false;
      if (version.value() > 1) {
        Print(GetLogPrefix() + "warning - parser version 1 is not compatible with this map, minimum version [" + Tokens[2] + "]");
        m_Error = true;
      }
    } else if (Tokens[1] == "pid" && Tokens.size() == 4) {
      // Tokens[2] = pid
      // Tokens[3] = name
      optional<uint32_t> PID = ToUint32(Tokens[2]);
      if (!PID.has_value()) return false;
      if (m_PIDToName.find(*PID) != m_PIDToName.end()) {
        Print(GetLogPrefix() + "overwriting previous name [" + m_PIDToName[*PID] + "] with new name [" + Tokens[3] + "] for PID [" + Tokens[2] + "]");
      }
      m_PIDToName[*PID] = Tokens[3];
    }
  } else if (actionType == "DefVarP" && Tokens.size() == 5) {
    // Tokens[1] = name
    // Tokens[2] = value type
    // Tokens[3] = goal type (ignored here)
    // Tokens[4] = suggestion (ignored here)

    if (m_DefVarPs.find(Tokens[1]) != m_DefVarPs.end()) {
      Print(GetLogPrefix() + "duplicate DefVarP [" + Tokens[1] + "] found, ignoring");
      return false;
    }
    if (Tokens[2] == "int") {
      m_DefVarPs[Tokens[1]] = MMD_VALUE_TYPE_INT;
    } else if (Tokens[2] == "real") {
      m_DefVarPs[Tokens[1]] = MMD_VALUE_TYPE_REAL;
    } else if (Tokens[2] == "string") {
      m_DefVarPs[Tokens[1]] = MMD_VALUE_TYPE_STRING;
    } else {
      Print(GetLogPrefix() + "invalid DefVarP type [" + Tokens[2] + "] found, ignoring");
      return false;
    }
  } else if (actionType == "VarP" && Tokens.size() == 5) {
    // Tokens[1] = pid
    // Tokens[2] = name
    // Tokens[3] = operation
    // Tokens[4] = value

    if (m_DefVarPs.find(Tokens[2]) == m_DefVarPs.end()) {
      Print(GetLogPrefix() + "VarP [" + Tokens[2] + "] found without a corresponding DefVarP, ignoring");
      return false;
    }
    uint8_t ValueType = m_DefVarPs[Tokens[2]];
    optional<uint32_t> PID = ToUint32(Tokens[1]);
    if (!PID.has_value()) {
      Print(GetLogPrefix() + "VarP [" + Tokens[2] + "] has invalid PID [" + Tokens[1] + "], ignoring");
      return false;
    }

    if (ValueType == MMD_VALUE_TYPE_INT) {
      VarP VP = VarP(*PID, Tokens[2]);
      optional<int32_t> intValue = ToInt32(Tokens[4]);
      if (!intValue.has_value()) {
        Print(GetLogPrefix() + "invalid int VarP [" + Tokens[2] + "] value [" + Tokens[3] + "] found, ignoring");
        return false;
      }
      if (Tokens[3] == "=") {
        m_VarPInts[VP] = *intValue;
      } else if (Tokens[3] == "+=") {
        if (m_VarPInts.find(VP) != m_VarPInts.end()) {
          m_VarPInts[VP] += *intValue;
        } else {
          m_VarPInts[VP] = *intValue;
        }
      } else if (Tokens[3] == "-=") {
        if (m_VarPInts.find(VP) != m_VarPInts.end()) {
          m_VarPInts[VP] -= *intValue;
        } else {
          m_VarPInts[VP] = -(*intValue);
        }
      } else {
        Print(GetLogPrefix() + "unknown int VarP operation [" + Tokens[3] + "] found, ignoring");
        return false;
      }
    } else if (ValueType == MMD_VALUE_TYPE_REAL) {
      VarP VP = VarP(*PID, Tokens[2]);
      optional<double> realValue = ToDouble(Tokens[4]);
      if (!realValue.has_value()) {
        Print(GetLogPrefix() + "invalid real VarP [" + Tokens[2] + "] value [" + Tokens[4] + "] found, ignoring");
        return false;
      }
      if (Tokens[3] == "=") {
        m_VarPReals[VP] = *realValue;
      } else if (Tokens[3] == "+=") {
        if (m_VarPReals.find(VP) != m_VarPReals.end()) {
          m_VarPReals[VP] += *realValue;
        } else {
          m_VarPReals[VP] = *realValue;
        }
      } else if (Tokens[3] == "-=") {
        if (m_VarPReals.find(VP) != m_VarPReals.end()) {
          m_VarPReals[VP] -= *realValue;
        } else {
          m_VarPReals[VP] = -(*realValue);
        }
      } else {
        Print(GetLogPrefix() + "unknown real VarP operation [" + Tokens[3] + "] found, ignoring");
        return false;
      }
    } else { // MMD_VALUE_TYPE_STRING
      VarP VP = VarP(*PID, Tokens[2]);
      if (Tokens[3] == "=") {
        m_VarPStrings[VP] = Tokens[4];
      } else {
        Print(GetLogPrefix() + "unknown string VarP [" + Tokens[2] + "] operation [" + Tokens[3] + "] found, ignoring");
        return false;
      }
    }
  } else if (actionType == "FlagP" && Tokens.size() == 3) {
    // Tokens[1] = pid
    // Tokens[2] = flag

    optional<uint32_t> PID = ToUint32(Tokens[1]);
    if (!PID.has_value()) {
      Print(GetLogPrefix() + "FlagP [" + Tokens[2] + "] has invalid PID [" + Tokens[1] + "], ignoring");
      return false;
    }

    if (Tokens[2] == "leaver") {
      m_FlagsLeaver[*PID] = true;
    } else if (Tokens[2] == "practicing") {
      m_FlagsPracticing[*PID] = true;
    } else {
      uint8_t result;
      if (Tokens[2] == "drawer") {
        result = MMD_RESULT_DRAWER;
      } else if (Tokens[2] == "winner") {
        result = MMD_RESULT_WINNER;
      } else if (Tokens[2] == "loser") {
        result = MMD_RESULT_LOSER;
      } else {
        Print(GetLogPrefix() + "unknown flag [" + Tokens[2] + "] found, ignoring");
        return false;
      }
      auto previousResultIt = m_Flags.find(*PID);
      if (previousResultIt != m_Flags.end()) {
        if (previousResultIt->second == result) {
          return true;
        }
        Print(GetLogPrefix() + "previous flag [" + to_string(previousResultIt->second) + "] would be overriden with new flag [" + Tokens[2] + "] for PID [" + Tokens[1] + "] - ignoring");
        return false;
      }
      m_Flags[*PID] = result;
      if (result == MMD_RESULT_WINNER) {
        m_GameOver = true;
      }
    }
  } else if (actionType == "DefEvent" && Tokens.size() >= 4) {
    // Tokens[1] = name
    // Tokens[2] = # of arguments (n)
    // Tokens[3..n+3] = arguments
    // Tokens[n+3] = format

    if (m_DefEvents.find(Tokens[1]) != m_DefEvents.end()) {
      Print(GetLogPrefix() + "duplicate DefEvent [" + Tokens[1] + "] found, ignoring");
      return false;
    }
    optional<uint32_t> arity = ToUint32(Tokens[2]);
    if (!arity.has_value()) {
      Print(GetLogPrefix() + "DefEventP invalid arity [" + Tokens[2] + "] found, ignoring");
      return false;
    }
    if (Tokens.size() != arity.value() + 4) {
      Print(GetLogPrefix() + "DefEventP [" + Tokens[2] + "] tokens missing, ignoring");
      return false;
    }
    m_DefEvents[Tokens[1]] = vector<string>(Tokens.begin() + 3, Tokens.end());
  } else if (actionType == "Event" && Tokens.size() >= 2) {
    // Tokens[1] = name
    // Tokens[2..n+2] = arguments (where n is the # of arguments in the corresponding DefEvent)
    auto defEventIt = m_DefEvents.find(Tokens[1]);
    if (defEventIt == m_DefEvents.end()) {
      Print(GetLogPrefix() + "Event [" + Tokens[1] + "] found without a corresponding DefEvent, ignoring");
      return false;
    }
    const vector<string>& DefEvent = defEventIt->second;
    if (Tokens.size() - 2 != DefEvent.size() - 1) {
      Print(GetLogPrefix() + "Event [" + Tokens[1] + "] found with " + to_string(Tokens.size() - 2) + " arguments but expected " + to_string(DefEvent.size() - 1) + " arguments, ignoring");
      return false;
    }
    if (DefEvent.empty()) {
      Print(GetLogPrefix() + "Event [" + Tokens[1] + "] triggered");
      return true;
    }

    string Format = DefEvent[DefEvent.size() - 1];

    // replace the markers in the format string with the arguments
    for (uint32_t i = 0; i < Tokens.size() - 2; ++i) {
      // check if the marker is a PID marker

      if (DefEvent[i].substr(0, 4) == "pid:") {
        // replace it with the player's name rather than their PID
        optional<uint32_t> PID = ToUint32(Tokens[i + 2]);
        if (PID.has_value()) {
          auto it = m_PIDToName.find(*PID);
          if (it == m_PIDToName.end()) {
            ReplaceText(Format, "{" + to_string(i) + "}", "PID:" + Tokens[i + 2]);
          } else {
            ReplaceText(Format, "{" + to_string(i) + "}", it->second);
          }
        }
      } else {
        ReplaceText(Format, "{" + to_string(i) + "}", Tokens[i + 2]);
      }
    }
    Print(GetLogPrefix() + "Event [" + Tokens[1] + "] triggered: " + Format);

    // Print(GetLogPrefix() + "event [" + Tokens[1] + "]");
  } else if (actionType == "Blank") {
    // ignore
  } else if (actionType == "Custom") {
    Print(GetLogPrefix() + "custom: " + JoinVector(Tokens, false));
  } else {
    Print(GetLogPrefix() + "unknown action type [" + actionType + "] found, ignoring");
  }
  return true;
}

bool CW3MMD::ProcessAction(CIncomingAction *Action)
{
  unsigned int i = 0;
  vector<uint8_t> *ActionData = Action->GetAction();
  vector<uint8_t> MissionKey;
  vector<uint8_t> Key;
  vector<uint8_t> Value;

  while (ActionData->size() >= i + 9) {
    if ((*ActionData)[i] == 'k' &&
      (*ActionData)[i + 1] == 'M' &&
      (*ActionData)[i + 2] == 'M' &&
      (*ActionData)[i + 3] == 'D' &&
      (*ActionData)[i + 4] == '.' &&
      (*ActionData)[i + 5] == 'D' &&
      (*ActionData)[i + 6] == 'a' &&
      (*ActionData)[i + 7] == 't' &&
      (*ActionData)[i + 8] == 0x00)
    {
      if (ActionData->size() >= i + 10) {
        MissionKey = ExtractCString(*ActionData, i + 9);

        if (ActionData->size() >= i + 11 + MissionKey.size()) {
          Key = ExtractCString(*ActionData, i + 10 + MissionKey.size());

          if (ActionData->size() >= i + 15 + MissionKey.size() + Key.size()) {
            Value = vector<uint8_t>(ActionData->begin() + i + 11 + MissionKey.size() + Key.size(), ActionData->begin() + i + 15 + MissionKey.size() + Key.size());
            string MissionKeyString = string(MissionKey.begin(), MissionKey.end());
            string KeyString = string(Key.begin(), Key.end());
            uint32_t ValueInt = ByteArrayToUInt32(Value, false);

            // Print("[W3MMD] DEBUG: mkey [" + MissionKeyString + "], key [" + KeyString + "], value [" + to_string(ValueInt) + "]");

            if (MissionKeyString.size() > 4 && MissionKeyString.substr(0, 4) == "val:") {
              string ValueIDString = MissionKeyString.substr(4);
              optional<uint32_t> ValueID = ToUint32(ValueIDString);
              vector<string> Tokens = TokenizeKey(KeyString);
              if (!HandleTokens(Tokens)) {
                Print(GetLogPrefix() + "error parsing [" + KeyString + "]");
              }
              ++m_NextValueID;
            } else if (MissionKeyString.size() > 4 && MissionKeyString.substr(0, 4) == "chk:") {
              string CheckIDString = MissionKeyString.substr(4);
              optional<uint32_t> CheckID = ToUint32(CheckIDString);

              // todotodo: cheat detection

               ++m_NextCheckID;
            } else {
              Print(GetLogPrefix() + "unknown mission key [" + MissionKeyString + "] found, ignoring");
            }
            i += 15 + MissionKey.size() + Key.size();
          } else {
            ++i;
          }
        } else {
          ++i;
        }
      } else {
        ++i;
      }
    }
    else {
      ++i;
    }
  }

  return m_GameOver;
}

vector<string> CW3MMD::TokenizeKey(string key) const
{
  vector<string> tokens;
  string token;
  bool escaping = false;

  for (string::iterator i = key.begin(); i != key.end(); ++i) {
    if (escaping) {
      if (*i == ' ') {
        token += ' ';
      } else if (*i == '\\') {
        token += '\\';
      } else {
        Print(GetLogPrefix() + "error tokenizing key [" + key + "], invalid escape sequence found, ignoring");
        return vector<string>();
      }
      escaping = false;
    } else {
      if (*i == ' ') {
        if (token.empty()) {
          Print(GetLogPrefix() + "error tokenizing key [" + key + "], empty token found, ignoring");
          return vector<string>();
        }
        tokens.push_back(token);
        token.clear();
      } else if (*i == '\\') {
        escaping = true;
      } else {
        token += *i;
      }
    }
  }

  if (token.empty()) {
    Print(GetLogPrefix() + "error tokenizing key [" + key + "], empty token found, ignoring");
    return vector<string>();
  }

  tokens.push_back(token);
  return tokens;
}

vector<string> CW3MMD::GetWinners() const
{
  vector<string> winners;
  for (const auto& flagEntry : m_Flags) {
    if (flagEntry.second != MMD_RESULT_WINNER) continue;
    auto nameIterator = m_PIDToName.find(flagEntry.first);
    if (nameIterator == m_PIDToName.end()) continue;
    winners.push_back(nameIterator->second);
  }
  return winners;
}

string CW3MMD::GetLogPrefix() const
{
  return "[W3MMD: " + m_Game->GetGameName() + "] ";
}
