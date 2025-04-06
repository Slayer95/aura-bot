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

#include "dota.h"
#include "../aura.h"
#include "../game_controller_data.h"
#include "../game_result.h"
#include "../auradb.h"
#include "../game.h"
#include "../game_user.h"
#include "../protocol/game_protocol.h"
#include "../util.h"

using namespace std;

//
// CDotaStats
//

CDotaStats::CDotaStats(CGame* nGame)
  : m_Game(nGame),
    m_Winner(DOTA_WINNER_UNDECIDED)
{
  Print("[STATS] using dota stats");

  for (auto& dotaPlayer : m_Players)
    dotaPlayer = nullptr;
}

CDotaStats::~CDotaStats()
{
  for (auto& dotaPlayer : m_Players)
  {
    if (dotaPlayer)
      delete dotaPlayer;
  }
}

bool CDotaStats::RecvAction(uint8_t UID, const CIncomingAction& action)
{
  size_t                      i          = 0;
  const std::vector<uint8_t>& ActionData = action.GetImmutableAction();
  std::vector<uint8_t>        Data, Key, Value;

  // dota actions with real time replay data start with 0x6b then the nullptr terminated string "dr.x"
  // unfortunately more than one action can be sent in a single packet and the length of each action isn't explicitly represented in the packet
  // so we have to either parse all the actions and calculate the length based on the type or we can search for an identifying sequence
  // parsing the actions would be more correct but would be a lot more difficult to write for relatively little gain
  // so we take the easy route (which isn't always guaranteed to work) and search the data for the sequence "6b 64 72 2e 78 00" and hope it identifies an action

  do
  {
    if (ActionData[i] == ACTION_SYNC_INT &&
      ActionData[i + 1] == 'd' &&
      ActionData[i + 2] == 'r' &&
      ActionData[i + 3] == '.' &&
      ActionData[i + 4] == 'x' &&
      ActionData[i + 5] == 0x00)
    {
      // we think we've found an action with real time replay data (but we can't be 100% sure)
      // next we parse out two nullptr terminated strings and a 4 byte integer

      if (ActionData.size() >= i + 7)
      {
        // the first nullptr terminated string should either be the strings "Data" or "Global" or a player id in ASCII representation, e.g. "1" or "2"

        Data = ExtractCString(ActionData, i + 6);

        if (ActionData.size() >= i + 8 + Data.size())
        {
          // the second nullptr terminated string should be the key

          Key = ExtractCString(ActionData, i + 7 + Data.size());

          if (ActionData.size() >= i + 12 + Data.size() + Key.size())
          {
            // the 4 byte integer should be the value

            Value                     = std::vector<uint8_t>(ActionData.begin() + i + 8 + Data.size() + Key.size(), ActionData.begin() + i + 12 + Data.size() + Key.size());
            const string   DataString = string(begin(Data), end(Data));
            const string   KeyString  = string(begin(Key), end(Key));
            const uint32_t ValueInt   = static_cast<uint8_t>(ByteArrayToUInt32(Value, false));

            //Print( "[STATS] " + DataString + ", " + KeyString + ", " + to_string( ValueInt ) );

            if (DataString == "Data")
            {
              // these are received during the game
              // you could use these to calculate killing sprees and double or triple kills (you'd have to make up your own time restrictions though)
              // you could also build a table of "who killed who" data

              if (KeyString.size() >= 5 && KeyString.compare(0, 4, "Hero") == 0)
              {
                // a hero died

                const string   VictimName   = KeyString.substr(4);
                uint8_t  KillerColor       = static_cast<uint8_t>(ValueInt);
                uint8_t  VictimColor = 0;
                try {
                  VictimColor = static_cast<uint8_t>(stoul(VictimName));
                } catch (...) {
                }
                GameUser::CGameUser* Killer = m_Game->GetUserFromColor(KillerColor);
                GameUser::CGameUser* Victim = m_Game->GetUserFromColor(VictimColor);

                if (!m_Players[KillerColor])
                  m_Players[KillerColor] = new CDBDotAPlayer();

                if (!m_Players[VictimColor])
                  m_Players[VictimColor] = new CDBDotAPlayer();

                if (Victim)
                {
                  if (Killer)
                  {
                    // check for hero denies

                    if (!GetAreSameTeamColors(KillerColor, VictimColor))
                    {
                      // non-leaver killed a non-leaver

                      m_Players[KillerColor]->IncKills();
                      m_Players[VictimColor]->IncDeaths();
                    }
                  }
                  else
                  {
                    // Scourge/Sentinel/leaver killed a non-leaver

                    m_Players[VictimColor]->IncDeaths();
                  }
                }
              }
              else if (KeyString.size() >= 7 && KeyString.compare(0, 6, "Assist") == 0)
              {
                // check if the assist was on a non-leaver

                if (m_Game->GetUserFromColor(static_cast<uint8_t>(ValueInt)))
                {
                  string         AssisterName   = KeyString.substr(6);
                  uint8_t  AssisterColor       = 0;
                  try {
                    AssisterColor = static_cast<uint8_t>(stoul(AssisterName));
                  } catch (...) {
                  }

                  if (!m_Players[AssisterColor])
                    m_Players[AssisterColor] = new CDBDotAPlayer();

                  m_Players[AssisterColor]->IncAssists();
                }
              }
              else if (KeyString.size() >= 8 && KeyString.compare(0, 5, "Tower") == 0)
              {
                // a tower died

                if (GetIsPlayerColor(ValueInt))
                {
                  if (!m_Players[ValueInt])
                    m_Players[ValueInt] = new CDBDotAPlayer();

                  m_Players[ValueInt]->IncTowerKills();
                }
              }
              else if (KeyString.size() >= 6 && KeyString.compare(0, 3, "Rax") == 0)
              {
                // a rax died

                if (GetIsPlayerColor(ValueInt))
                {
                  if (!m_Players[ValueInt])
                    m_Players[ValueInt] = new CDBDotAPlayer();

                  m_Players[ValueInt]->IncRaxKills();
                }
              }
              else if (KeyString.size() >= 8 && KeyString.compare(0, 7, "Courier") == 0)
              {
                // a courier died

                if (GetIsPlayerColor(ValueInt))
                {
                  if (!m_Players[ValueInt])
                    m_Players[ValueInt] = new CDBDotAPlayer();

                  m_Players[ValueInt]->IncCourierKills();
                }
              }
            }
            else if (DataString == "Global")
            {
              // these are only received at the end of the game

              if (KeyString == "Winner")
              {
                // Value 1 -> sentinel
                // Value 2 -> scourge

                uint8_t winner = static_cast<uint8_t>(ValueInt);
                if (winner == DOTA_WINNER_SENTINEL || winner == DOTA_WINNER_SCOURGE) {
                  m_Winner = winner;
                }
                m_GameOverTime = m_Game->GetEffectiveTicks() / 1000;

                if (m_Winner == DOTA_WINNER_SENTINEL)
                  Print(GetLogPrefix() + "detected winner: Sentinel");
                else if (m_Winner == DOTA_WINNER_SCOURGE)
                  Print(GetLogPrefix() + "detected winner: Scourge");
                else
                  Print(GetLogPrefix() + "detected winner: " + to_string(ValueInt));
              }
            }
            else if (DataString.size() <= 2 && DataString.find_first_not_of("1234567890") == string::npos)
            {
              // these are only received at the end of the game

              uint8_t ID = 0;
              try {
                ID = static_cast<uint8_t>(stoul(DataString));
              } catch (...) {
              }

              if (GetIsPlayerColor(ID))
              {
                if (!m_Players[ID])
                {
                  m_Players[ID] = new CDBDotAPlayer();
                  m_Players[ID]->SetColor(ID);
                }

                // Key "3"		-> Creep Kills
                // Key "4"		-> Creep Denies
                // Key "7"		-> Neutral Kills
                // Key "id"     -> ID (1-5 for sentinel, 6-10 for scourge, accurate after using -sp and/or -switch)

                switch (KeyString[0])
                {
                  case '3':
                    m_Players[ID]->SetCreepKills(ValueInt);
                    break;

                  case '4':
                    m_Players[ID]->SetCreepDenies(ValueInt);
                    break;

                  case '7':
                    m_Players[ID]->SetNeutralKills(ValueInt);
                    break;

                  case 'i':
                    if (KeyString[1] == 'd')
                    {
                      // DotA sends id values from 1-10 with 1-5 being sentinel players and 6-10 being scourge players
                      // unfortunately the actual player colours are from 1-5 and from 7-11 so we need to deal with this case here

                      if (ValueInt >= 6)
                        m_Players[ID]->SetNewColor(ValueInt + 1);
                      else
                        m_Players[ID]->SetNewColor(ValueInt);
                    }

                    break;

                  default:
                    break;
                }
              }
            }

            i += 12 + Data.size() + Key.size();
          }
          else
            ++i;
        }
        else
          ++i;
      }
      else
        ++i;
    }
    else
      ++i;
  } while (ActionData.size() >= i + 6);

  return true;
}

bool CDotaStats::UpdateQueue()
{
  return true;
}

void CDotaStats::FlushQueue()
{
}

vector<CGameController*> CDotaStats::GetSentinelControllers() const
{
  vector<CGameController*> controllers;
  for (uint8_t color = 1, end = 5; color <= end; color++) {
    CGameController* controllerData = m_Game->GetGameControllerFromColor(color);
    if (!controllerData) continue;
    controllers.push_back(controllerData);
  }
  return controllers;
}

vector<CGameController*> CDotaStats::GetScourgeControllers() const
{
  vector<CGameController*> controllers;
  for (uint8_t color = 7, end = 11; color <= end; color++) {
    CGameController* controllerData = m_Game->GetGameControllerFromColor(color);
    if (!controllerData) continue;
    controllers.push_back(controllerData);
  }
  return controllers;
}

optional<GameResults> CDotaStats::GetGameResults(const GameResultConstraints& /*constraints*/) const
{
  optional<GameResults> gameResults;
  if (m_Winner == DOTA_WINNER_UNDECIDED) {
    return gameResults;
  }

  vector<CGameController*> sentinel = GetSentinelControllers();
  vector<CGameController*> scourge = GetScourgeControllers();

  gameResults.emplace();

  if (m_Winner == DOTA_WINNER_SENTINEL) {
    gameResults->winners.swap(sentinel);
    gameResults->losers.swap(scourge);
  } else {
    gameResults->winners.swap(scourge);
    gameResults->losers.swap(sentinel);
  }

  auto it = gameResults->winners.begin();
  while (it != gameResults->winners.end()) {
    // 5 seconds of grace period for players to quit just before the game ends
    if ((*it)->GetHasLeftGame() && (*it)->GetLeftGameTime() + 5u < GetGameOverTime()) {
      gameResults->losers.push_back(*it);
      it = gameResults->winners.erase(it);
    } else {
      ++it;
    }
  }

  return gameResults;
}

string CDotaStats::GetLogPrefix() const
{
  return "[DOTA: " + m_Game->GetGameName() + "] ";
}

void CDotaStats::LogMetaData(int64_t gameTicks, const string& text) const
{
  m_Game->Log(text, gameTicks);
}
