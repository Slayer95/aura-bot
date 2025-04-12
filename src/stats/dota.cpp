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

CDotaStats::CDotaStats(shared_ptr<CGame> nGame)
  : m_Game(ref(*nGame)),
    m_Winner(DOTA_WINNER_UNDECIDED),
    m_Time(make_pair<uint32_t, uint32_t>(0u, 0u)),
    m_SwitchEnabled(false)
{
  Print("[STATS] using dota stats");

  for (auto& dotaPlayer : m_Players) {
    dotaPlayer = nullptr;
  }
}

CDotaStats::~CDotaStats()
{
  for (auto& dotaPlayer : m_Players) {
    if (dotaPlayer) {
      delete dotaPlayer;
    }
  }
}

bool CDotaStats::EventGameCache(const uint8_t /*fromUID*/, const std::string& fileName, const std::string& missionKey, const std::string& key, const uint32_t cacheValue)
{
  if (fileName != "dr.x") {
    return true;
  }

  //Print( "[STATS] " + missionKey + ", " + key + ", " + to_string( cacheValue ) );

  if (missionKey == "Data")
  {
    // these are received during the game
    // you could use these to calculate killing sprees and double or triple kills (you'd have to make up your own time restrictions though)
    // you could also build a table of "who killed who" data

    if (key.size() >= 5 && key.compare(0, 4, "Hero") == 0)
    {
      // a hero died
      optional<uint32_t> killerColor;
      optional<uint32_t> victimColor = ToUint32(key.substr(4));

      if (GetIsPlayerColor(cacheValue) || cacheValue == 0 || cacheValue == 6) {
        killerColor = cacheValue;
      }
      if (!killerColor.has_value() || (!GetIsPlayerColor(*killerColor) && *killerColor != 0 && *killerColor != 6)) {
        return true;
      }
      if (!victimColor.has_value() || !GetIsPlayerColor(*victimColor)) {
        return true;
      }


      GameUser::CGameUser* killerUser = nullptr;
      if (GetIsPlayerColor(*killerColor)) {
        killerUser = m_Game.get().GetUserFromColor(*killerColor);
        if (!m_Players[*killerColor]) {
          m_Players[*killerColor] = new CDBDotAPlayer();
        }
      }
      GameUser::CGameUser* victimUser = m_Game.get().GetUserFromColor(*victimColor);
      if (!m_Players[*victimColor]) {
        m_Players[*victimColor] = new CDBDotAPlayer();
      }

      if (killerUser) {
        if (victimUser) {
          // check for denies
          if (GetAreSameTeamColors(*killerColor, *victimColor)) {
            Print(GetLogPrefix() + "[" + killerUser->GetName() + "] denied player [" + victimUser->GetName() + "]");
          } else {
            // non-leaver killed a non-leaver
            m_Players[*killerColor]->IncKills();
            m_Players[*victimColor]->IncDeaths();
            Print(GetLogPrefix() + "[" + killerUser->GetName() + "] killed player [" + victimUser->GetName() + "]");
          }
        } else {
          // Scourge/Sentinel/leaver killed a non-leaver
          m_Players[*victimColor]->IncDeaths();
        }
      } else if (GetIsSentinelCreepsColor(*killerColor)) {
        Print(GetLogPrefix() + "[" + killerUser->GetName() + "] the Sentinel killed player [" + victimUser->GetName() + "]");
      } else if (GetIsScourgeCreepsColor(*killerColor)) {
        Print(GetLogPrefix() + "[" + killerUser->GetName() + "] the Scourge killed player [" + victimUser->GetName() + "]");
      }
    }
    else if (key.size() >= 7 && key.compare(0, 6, "Assist") == 0)
    {
      // check if the assist was on a non-leaver

      if (m_Game.get().GetUserFromColor(static_cast<uint8_t>(cacheValue)))
      {
        optional<uint32_t> assisterColor = ToUint32(key.substr(6));
        if (assisterColor.has_value() && GetIsPlayerColor(*assisterColor)) {
          if (!m_Players[*assisterColor])
            m_Players[*assisterColor] = new CDBDotAPlayer();

          m_Players[*assisterColor]->IncAssists();
        }
      }
    }

    else if (key.size() >= 8 && key.compare(0, 5, "Tower") == 0)
    {
      // a tower died

      if (GetIsPlayerColor(cacheValue))
      {
        if (!m_Players[cacheValue])
          m_Players[cacheValue] = new CDBDotAPlayer();

        m_Players[cacheValue]->IncTowerKills();
      }
    }
    else if (key.size() >= 6 && key.compare(0, 3, "Rax") == 0)
    {
      // a rax died

      if (GetIsPlayerColor(cacheValue))
      {
        if (!m_Players[cacheValue])
          m_Players[cacheValue] = new CDBDotAPlayer();

        m_Players[cacheValue]->IncRaxKills();
      }
    }
    else if (key.size() >= 8 && key.compare(0, 7, "Courier") == 0)
    {
      // a courier died

      if (GetIsPlayerColor(cacheValue))
      {
        if (!m_Players[cacheValue])
          m_Players[cacheValue] = new CDBDotAPlayer();

        m_Players[cacheValue]->IncCourierKills();
      }
    }

    else if (key.size() >= 6 && key.compare(0, 6, "Throne") == 0)
    {
      // the frozen throne got hurt
      Print(GetLogPrefix() + "the Frozen Throne is now at " + to_string(cacheValue) + "% HP");
    }

    else if (key.size() >= 4 && key.compare(0, 4, "Tree") == 0)
    {
      // the world tree got hurt
      Print(GetLogPrefix() + "the World Tree is now at " + to_string(cacheValue) + "% HP");
    }

    else if (key.size() >= 5 && key.compare(0, 2, "CK") == 0)
    {
      // a player disconnected

      if (GetIsPlayerColor(cacheValue)) {
        string::size_type denyIndex = key.find('D');
        if (denyIndex == string::npos || denyIndex < 2) return true;
        string::size_type neutralIndex = key.find('N', denyIndex + 1);
        if (neutralIndex == string::npos) return true;

        string creepKillsString = key.substr(2, denyIndex - 2);
        string creepDeniesString = key.substr(denyIndex + 1, neutralIndex - (denyIndex + 1));
        string neutralKillsString = key.substr(neutralIndex + 1);

        optional<uint32_t> creepKills = ToUint32(creepKillsString);
        optional<uint32_t> creepDenies = ToUint32(creepDeniesString);
        optional<uint32_t> neutralKills = ToUint32(neutralKillsString);
        if (!creepKills.has_value() || !creepDenies.has_value() || !neutralKills.has_value()) {
          return true;
        }

        if (!m_Players[cacheValue]) {
          m_Players[cacheValue] = new CDBDotAPlayer();
          m_Players[cacheValue]->SetColor(cacheValue);
        }

        m_Players[cacheValue]->SetCreepKills(*creepKills);
        m_Players[cacheValue]->SetCreepDenies(*creepDenies);
        m_Players[cacheValue]->SetNeutralKills(*neutralKills);
      }

      Print(GetLogPrefix() + "player disconnected");
    }

    else if (key.size() >= 9 && key.compare(0, 9, "GameStart") == 0)
    {
      // Zero time in the game, creeps spawn.
      if (cacheValue == 1) {
        //m_Game.get().SetCreepSpawnTime(GetTime());
        Print(GetLogPrefix() + "creeps spawned");
      }
    }

    else if (key.size() >= 8 && key.compare(0, 4, "SWAP") == 0)
    {
      // swap players
      string::size_type firstUnderscore = key.find('_');
      if (firstUnderscore == string::npos) return true;
      string::size_type secondUnderscore = key.find('_', firstUnderscore + 1);
      if (secondUnderscore == string::npos) return true;
      string fromString = key.substr(firstUnderscore + 1, secondUnderscore - (firstUnderscore + 1));
      string toString = key.substr(secondUnderscore + 1);
      optional<uint32_t> fromColor = ToUint32(fromString);
      optional<uint32_t> toColor = ToUint32(toString);
      if (!fromColor.has_value() || !toColor.has_value()) return true;
      if (!GetIsPlayerColor(*fromColor) || !GetIsPlayerColor(*toColor)) return true;
      GameUser::CGameUser* fromPlayer = m_Game.get().GetUserFromColor(*fromColor);
      GameUser::CGameUser* toPlayer = m_Game.get().GetUserFromColor(*toColor);
      
      if (!m_SwitchEnabled && GetIsSentinelPlayerColor(*fromColor) != GetIsSentinelPlayerColor(*toColor)) {
        m_Players[*toColor]->SetNewColor(*fromColor);
        m_Players[*fromColor]->SetNewColor(*toColor);
        
        CDBDotAPlayer* tmp = m_Players[*toColor];
        m_Players[*toColor] = m_Players[*fromColor];
        m_Players[*fromColor] = tmp;
        
        if (fromPlayer) fromString = fromPlayer->GetName();
        if (toPlayer) toString = toPlayer->GetName();

        Print(GetLogPrefix() + "swap players from [" + fromString + "] to [" + toString + "].");
      }
    }
    else if (key.size() >= 4 && key.compare(0, 4, "Mode") == 0)
    {
      Print(GetLogPrefix() + "detected game mode: [" + key.substr(4) + "]");
      // Game mode
      string::size_type KeyStringSize = key.size();
      if (KeyStringSize % 2 != 0) KeyStringSize--;
      for (string::size_type i = 4; i < KeyStringSize; i += 2) {
        if (key[i] == 's' && key[i + 1] == 'o') {
          m_SwitchEnabled = true;
          Print(GetLogPrefix() + "detected so mode.");
        }
      }
    }

    // Unhandled keys:
    // CSK  Creep kills by the period in valueInt. Periodic.
    // NK   Neutral creep kills by the period in valueInt. Periodic.
    // CSD  Creep denies by the period in valueInt. Periodic.
    // PUI_ Hero pick up an item.
    // DRI_ Hero drop an item.

  }
  else if (missionKey == "Global")
  {
    // these are only received at the end of the game

    if (key == "Winner") {
      // Value 1 -> sentinel
      // Value 2 -> scourge

      uint8_t winner = static_cast<uint8_t>(cacheValue);
      if (winner == DOTA_WINNER_SENTINEL || winner == DOTA_WINNER_SCOURGE) {
        m_Winner = winner;
      }
      m_GameOverTime = m_Game.get().GetEffectiveTicks() / 1000;

      if (m_Winner == DOTA_WINNER_SENTINEL)
        Print(GetLogPrefix() + "detected winner: Sentinel");
      else if (m_Winner == DOTA_WINNER_SCOURGE)
        Print(GetLogPrefix() + "detected winner: Scourge");
      else
        Print(GetLogPrefix() + "detected winner: " + to_string(cacheValue));
    } else if (key == "m") {
      m_Time.first = cacheValue;
    } else if (key == "s") {
      m_Time.second = cacheValue;
    }
  }
  else if (missionKey.size() <= 2 && missionKey.find_first_not_of("1234567890") == string::npos)
  {
    // these are only received at the end of the game

    uint8_t ID = 0;
    try {
      ID = static_cast<uint8_t>(stoul(missionKey));
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

      switch (key[0]) {
        case '1':
          m_Players[ID]->SetKills(cacheValue);
          break;
        case '2':
          m_Players[ID]->SetDeaths(cacheValue);
          break;
        case '3':
          m_Players[ID]->SetCreepKills(cacheValue);
          break;
        case '4':
          m_Players[ID]->SetCreepDenies(cacheValue);
          break;
        case '5':
          m_Players[ID]->SetAssists(cacheValue);
          break;
        case '6':
          m_Players[ID]->SetGold(cacheValue);
          break;
        case '7':
          m_Players[ID]->SetNeutralKills(cacheValue);
          break;
        case '8': {
          array<uint8_t, 4> valueBE = CreateFixedByteArray(cacheValue, true);
          // 8_0 to 8_1
          if (key.size() >= 3 && key[1] == '_' && (0x30 <= key[2] && key[2] <= 0x35)) {
            m_Players[ID]->SetItem(key[2] - 0x30, string(valueBE.begin(), valueBE.end()));
          }
          break;
        }
        case '9': {
          array<uint8_t, 4> valueBE = CreateFixedByteArray(cacheValue, true);
          m_Players[ID]->SetHero(string(valueBE.begin(), valueBE.end()));
          break;
        }
        case 'i':
          if (key.size() >= 2 && key[1] == 'd' && 1 <= cacheValue && cacheValue <= 10) {
            // DotA sends id values from 1-10 with 1-5 being sentinel players and 6-10 being scourge players
            // unfortunately the actual player colours are from 1-5 and from 7-11 so we need to deal with this case here
            if (cacheValue >= 6)
              m_Players[ID]->SetNewColor(static_cast<uint8_t>(cacheValue + 1));
            else
              m_Players[ID]->SetNewColor(static_cast<uint8_t>(cacheValue));
          }
          break;

        default:
          break;
      }
    }
  }

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
    CGameController* controllerData = m_Game.get().GetGameControllerFromColor(color);
    if (!controllerData) continue;
    controllers.push_back(controllerData);
  }
  return controllers;
}

vector<CGameController*> CDotaStats::GetScourgeControllers() const
{
  vector<CGameController*> controllers;
  for (uint8_t color = 7, end = 11; color <= end; color++) {
    CGameController* controllerData = m_Game.get().GetGameControllerFromColor(color);
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
  return "[DOTA: " + m_Game.get().GetGameName() + "] ";
}

void CDotaStats::LogMetaData(int64_t gameTicks, const string& text) const
{
  m_Game.get().Log(text, gameTicks);
}
