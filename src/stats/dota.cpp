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
using namespace Dota;

optional<uint8_t> Dota::EnsureHeroColor(uint32_t input)
{
  optional<uint8_t> result;
  if (GetIsHeroColor(input)) {
    result = static_cast<uint8_t>(input);
  }
  return result;
}

optional<uint8_t> Dota::EnsureActorColor(uint32_t input)
{
  optional<uint8_t> result;
  if (input <= MAX_SLOTS_LEGACY) {
    result = static_cast<uint8_t>(input);
  }
  return result;
}

optional<uint8_t> Dota::ParseHeroColor(const string& input)
{
  optional<uint8_t> result;
  optional<uint32_t> maybeColor = ToUint32(input);
  if (GetIsHeroColor(*maybeColor)) {
    result = static_cast<uint8_t>(*maybeColor);
  }
  return result;
}

optional<uint8_t> Dota::ParseActorColor(const string& input)
{
  optional<uint8_t> result;
  optional<uint32_t> maybeColor = ToUint32(input);
  if (*maybeColor <= MAX_SLOTS_LEGACY) {
    result = static_cast<uint8_t>(*maybeColor);
  }
  return result;
}

string Dota::GetLaneName(const uint8_t code)
{
  switch (code) {
    case 0: return "Top Lane";
    case 1: return "Mid Lane";
    case 2: return "Bot Lane";
  }
  return "Invalid Lane";
}

string Dota::GetTeamNameBaseZero(const uint8_t code)
{
  switch (code) {
    case 0: return "Sentinel";
    case 1: return "Scourge";
  }
  return string();
}

string Dota::GetTeamNameBaseOne(const uint8_t code)
{
  switch (code) {
    case 1: return "Sentinel";
    case 2: return "Scourge";
  }
  return string();
}

string Dota::GetRuneName(const uint8_t code)
{
  switch (code) {
    case 1: return "Haste";
    case 2: return "Regeneration";
    case 3: return "Double Damage";
    case 4: return "Illusion";
    case 5: return "Invisibility";
    case 6: return "Bounty";
    case 8: return "Wisdom";
    default: return "#" + ToDecString(code);
  }
}

//
// CDotaStats
//

CDotaStats::CDotaStats(shared_ptr<CGame> nGame)
  : m_Game(ref(*nGame)),
    m_Winner(Dota::WINNER_UNDECIDED),
    m_SwitchEnabled(false),
    m_Time(make_pair<uint32_t, uint32_t>(0u, 0u))
    
{
  Print("[STATS] using dota stats");
  m_Players.fill(nullptr);

  for (unsigned int i = 0; i < m_Players.size(); ++i) {
    if (GetIsHeroColor(i)) {
      m_Players[i] = new CDBDotAPlayer();
    }
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

bool CDotaStats::EventGameCacheInteger(const uint8_t /*fromUID*/, const std::string& fileName, const std::string& missionKey, const std::string& key, const uint32_t cacheValue)
{
  if (fileName != "dr.x") {
    return true;
  }

  if (missionKey == "Data") {
    // these are received during the game
    // you could use these to calculate killing sprees and double or triple kills (you'd have to make up your own time restrictions though)
    // you could also build a table of "who killed who" data

    string eventName = key;
    string eventStringData;

    if (key.size() >= 4 && key.compare(0, 4, "Mode") == 0) {
      eventStringData = key.substr(4);
      eventName = "Mode";
    } else {
      string::size_type keyNumIndex = key.find_first_of("1234567890_");
      if (keyNumIndex != string::npos) {
        eventStringData = key.substr(keyNumIndex);
        eventName = key.substr(0, keyNumIndex);
      }
    }

    uint64_t eventHash = HashCode(eventName);

    switch (eventHash) {
      case HashCode("Hero"): {
        // a hero died
        optional<uint8_t> killerColor = EnsureActorColor(cacheValue);
        optional<uint8_t> victimColor = ParseHeroColor(eventStringData);
        if (!killerColor.has_value() || !victimColor.has_value()) {
          break;
        }

        GameUser::CGameUser* killerUser = GetUserFromColor(*killerColor);
        GameUser::CGameUser* victimUser = GetUserFromColor(*victimColor);
        bool isFriendlyFire = GetAreSameTeamColors(*killerColor, *victimColor);

        // only count kills from non-leavers
        if (killerUser) m_Players[*killerColor]->IncKills();
        m_Players[*victimColor]->IncDeaths();

        string action = "killed";
        if (isFriendlyFire) {
          action = "denied";
        }
        if (victimUser) {
          action.append(" player");
        } else {
          action.append(" leaver");
        }

        Print(GetLogPrefix() + GetActorNameFromColor(*killerColor) + " " + action + " [" + victimUser->GetName() + "]");
        break;
      }

      case HashCode("Assist"): {
        optional<uint8_t> assisterColor = ParseHeroColor(eventStringData);
        optional<uint8_t> victimColor = EnsureHeroColor(cacheValue);
        if (assisterColor.has_value() && victimColor.has_value()) {
          GameUser::CGameUser* assisterUser = GetUserFromColor(*assisterColor);
          if (assisterUser) { // only count assists from non-leavers
            m_Players[*assisterColor]->IncAssists();
            Print(GetLogPrefix() + GetActorNameFromColor(*assisterColor) + " assisted on killing [" + GetUserNameFromColor(*victimColor) + "]");
          }
        }
        break;
      }

      case HashCode("Tower"): {
        optional<uint8_t> siegeColor = EnsureActorColor(cacheValue);
        vector<uint8_t> towerId = SplitNumeral(eventStringData);
        if (siegeColor.has_value() && towerId.size() == 3) {
          GameUser::CGameUser* siegeUser = GetUserFromColor(*siegeColor);
          if (siegeUser) { // only count tower destroyed by non-leavers
            m_Players[cacheValue]->IncTowerKills();
          }

          bool isFriendlyFire = towerId[0] == 0 == GetIsSentinelHeroColor(*siegeColor);
          string action = "destroyed";
          if (isFriendlyFire) {
            action = "denied";
          }

          Print(GetLogPrefix() + GetActorNameFromColor(cacheValue) + " " + action + GetTeamNameBaseZero(towerId[0]) + "'s " + ToOrdinalName((size_t)(towerId[1])) + " tower at " + GetLaneName(towerId[2]));
        }

        break;
      }

      case HashCode("Rax"): {
        optional<uint8_t> siegeColor = EnsureActorColor(cacheValue);
        vector<uint8_t> raxId = SplitNumeral(eventStringData);
        if (siegeColor.has_value() && raxId.size() == 3) {
          GameUser::CGameUser* siegeUser = GetUserFromColor(*siegeColor);
          if (siegeUser) { // only count tower destroyed by non-leavers
            m_Players[cacheValue]->IncRaxKills();
          }

          bool isFriendlyFire = raxId[0] == 0 == GetIsSentinelHeroColor(*siegeColor);
          string action = "destroyed";
          if (isFriendlyFire) {
            action = "denied";
          }

          string typeName = "melee";
          if (raxId[2] == 1) {
            typeName = "ranged";
          }

          Print(GetLogPrefix() + GetActorNameFromColor(cacheValue) + " " + action + " " + GetTeamNameBaseZero(raxId[0]) + "'s " + typeName + " Barracks at " + GetLaneName(raxId[1]));
        }
        break;
      }

      case HashCode("Courier"): {
        optional<uint8_t> killerColor = EnsureActorColor(cacheValue);
        optional<uint8_t> victimColor = ParseHeroColor(eventStringData);
        if (killerColor.has_value() && victimColor.has_value()) {
          GameUser::CGameUser* killerUser = GetUserFromColor(*killerColor);
          if (killerUser) { // only count couriers killed by non-leavers
            m_Players[cacheValue]->IncCourierKills();
          }

          bool isFriendlyFire = false;
          string action = "killed";
          if (isFriendlyFire) {
            action = "denied";
          }

          Print(GetLogPrefix() + GetActorNameFromColor(cacheValue) + " " + action + " [" + GetUserNameFromColor(*victimColor) + "]'s courier");
        }
        break;
      }

      case HashCode("Throne"): {
        if (!eventStringData.empty()) break;
        // the frozen throne got hurt
        Print(GetLogPrefix() + "the Frozen Throne is now at " + to_string(cacheValue) + "% HP");
        break;
      }

      case HashCode("Tree"): {
        if (!eventStringData.empty()) break;
        // the world tree got hurt
        Print(GetLogPrefix() + "the World Tree Tree is now at " + to_string(cacheValue) + "% HP");
        break;
      }

      case HashCode("CSS"): {
        // incremental creeping performance stats sent every 5 minutes
        // supersedes CSK, NK, CSD
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          uint32_t creepData = cacheValue;
          uint32_t creepDenies = creepData & 0xFF; // 8 bits
          creepData >>= 8;
          uint32_t neutralKills = creepData & 0xFFF; // 12 bits
          creepData >>= 12;
          uint32_t creepKills = creepData; // 12 bits

          m_Players[*heroColor]->AddCreepKills(creepKills);
          m_Players[*heroColor]->AddCreepDenies(creepDenies);
          m_Players[*heroColor]->AddNeutralKills(neutralKills);

          string playerName = GetUserNameFromColor(*heroColor);
          //Print(GetLogPrefix() + "[" + playerName + "] " + to_string(creepKills) + " creeps killed, " + to_string(creepDenies) + " denied, " + to_string(neutralKills) + "neutrals");
        }
        break;
      }

      case HashCode("CK"): {
        // a player disconnected - the map sends a final creep performance stats update
        if (eventStringData.size() < 5) break; 
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        string::size_type denyIndex = eventStringData.find('D');
        if (denyIndex == string::npos) return true;
        string::size_type neutralIndex = eventStringData.find('N', denyIndex + 1);
        if (neutralIndex == string::npos) return true;

        string creepKillsString = eventStringData.substr(0, denyIndex);
        string creepDeniesString = eventStringData.substr(denyIndex + 1, neutralIndex - (denyIndex + 1));
        string neutralKillsString = eventStringData.substr(neutralIndex + 1);

        optional<uint32_t> creepKills = ToUint32(creepKillsString);
        optional<uint32_t> creepDenies = ToUint32(creepDeniesString);
        optional<uint32_t> neutralKills = ToUint32(neutralKillsString);
        if (!creepKills.has_value() || !creepDenies.has_value() || !neutralKills.has_value()) {
          return true;
        }

        m_Players[*heroColor]->SetCreepKills(*creepKills);
        m_Players[*heroColor]->SetCreepDenies(*creepDenies);
        m_Players[*heroColor]->SetNeutralKills(*neutralKills);

        string playerName = GetUserNameFromColor(*heroColor);
        Print(GetLogPrefix() + "[" + playerName + "] disconnected");
        break;
      }

      case HashCode("XGPM"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          /*
          uint32_t xgData = cacheValue;
          string playerName = GetUserNameFromColor(*heroColor);
          //Print(GetLogPrefix() + "[" + playerName + "] " + to_string(creepKills) + " creeps killed, " + to_string(creepDenies) + " denied, " + to_string(neutralKills) + "neutrals");
          */
        }
        break;
      }

      case HashCode("Level"): {
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] advanced to Lv. " + eventStringData);
        }
        break;
      }

      case HashCode("Talent"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          uint32_t tier = cacheValue / 10;
          uint32_t variant = cacheValue % 10;
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] chose option #" + to_string(variant) + " as their " + ToOrdinalName(tier) + " talent");
        }
        break;
      }

      case HashCode("RuneUse"): {
        if (eventStringData.empty()) break;
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] used a " + GetRuneName((uint8_t)eventStringData[0]) + " rune");
        }        
        break;
      }

      case HashCode("RuneStore"): {
        if (eventStringData.empty()) break;
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] stored a " + GetRuneName((uint8_t)eventStringData[0]) + " rune");
        }        
        break;
      }

      case HashCode("AfkDetect"): {
        optional<uint8_t> heroColor = ParseHeroColor(eventStringData);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(*heroColor);
          Print(GetLogPrefix() + "[" + playerName + "] detected as AFK");
        }
        break;
      }

      case HashCode("MHSuspected"): {
        optional<uint8_t> heroColor = EnsureHeroColor(cacheValue);
        if (heroColor.has_value()) {
          string playerName = GetUserNameFromColor(cacheValue);
          Print(GetLogPrefix() + "[" + playerName + "] suspected map hacker");
        }
        break;
      }

      case HashCode("GameStart"): {
        if (cacheValue == 1) {
          //m_Game.get().SetCreepSpawnTime(GetTime());
          Print(GetLogPrefix() + "creeps spawned");
        }
        break;
      }

      case HashCode("SWAP"): {
        // swap players
        string::size_type firstUnderscore = eventStringData.find('_');
        if (firstUnderscore == string::npos) return true;
        string::size_type secondUnderscore = eventStringData.find('_', firstUnderscore + 1);
        if (secondUnderscore == string::npos) return true;
        string fromString = eventStringData.substr(firstUnderscore + 1, secondUnderscore - (firstUnderscore + 1));
        string toString = eventStringData.substr(secondUnderscore + 1);
        optional<uint32_t> fromColor = ToUint32(fromString);
        optional<uint32_t> toColor = ToUint32(toString);
        if (!fromColor.has_value() || !toColor.has_value()) return true;
        if (!GetIsHeroColor(*fromColor) || !GetIsHeroColor(*toColor)) return true;
        GameUser::CGameUser* fromPlayer = m_Game.get().GetUserFromColor(*fromColor);
        GameUser::CGameUser* toPlayer = m_Game.get().GetUserFromColor(*toColor);
        
        if (!m_SwitchEnabled && GetIsSentinelHeroColor(*fromColor) != GetIsSentinelHeroColor(*toColor)) {
          m_Players[*toColor]->SetNewColor(*fromColor);
          m_Players[*fromColor]->SetNewColor(*toColor);
          
          CDBDotAPlayer* tmp = m_Players[*toColor];
          m_Players[*toColor] = m_Players[*fromColor];
          m_Players[*fromColor] = tmp;
          
          if (fromPlayer) fromString = fromPlayer->GetName();
          if (toPlayer) toString = toPlayer->GetName();

          Print(GetLogPrefix() + "swap players from [" + fromString + "] to [" + toString + "].");
        } else {
          Print(GetLogPrefix() + "swap players from [" + fromString + "] to [" + toString + "] (switch enabled).");
        }
        break;
      }

      case HashCode("APBan"): {
        break;
      }

      case HashCode("Mode"): {
        Print(GetLogPrefix() + "mode -" + eventStringData);
        // Game mode
        string::size_type KeyStringSize = eventStringData.size();
        if (KeyStringSize % 2 != 0) KeyStringSize--;
        for (string::size_type i = 0; i < KeyStringSize; i += 2) {
          if (eventStringData[i] == 's' && eventStringData[i + 1] == 'o') {
            m_SwitchEnabled = true;
            Print(GetLogPrefix() + "Switch On");
          }
        }
        break;
      }

      case HashCode("PUI"):
      case HashCode("DRI"): {
        if (eventStringData.size() < 2) break;
        optional<uint32_t> heroColor = ParseHeroColor(eventStringData.substr(1));
        if (!heroColor.has_value()) break;
        break;
      }

      default: {
        if (m_Game.get().m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
          Print(GetLogPrefix() + "unhandled dota event: [" + key + "] for " + to_string(cacheValue));
        }

        // Unhandled keys:
        // CSK  Creep kills by the period in valueInt. Periodic. Superseded by CSS.
        // NK   Neutral creep kills by the period in valueInt. Periodic. Superseded by CSS.
        // CSD  Creep denies by the period in valueInt. Periodic. Superseded by CSS.
        // PUI_ Hero pick up an item.
        // DRI_ Hero drop an item.
        //
        // Unknown keys:
        // Buyback?
      }
    }
  }
  else if (missionKey == "Global")
  {
    // these are only received at the end of the game

    if (key == "Winner") {
      // Value 1 -> sentinel
      // Value 2 -> scourge

      uint8_t winner = static_cast<uint8_t>(cacheValue);
      if (winner == Dota::WINNER_SENTINEL || winner == Dota::WINNER_SCOURGE) {
        m_Winner = winner;
      }
      m_GameOverTime = m_Game.get().GetEffectiveTicks() / 1000;

      if (m_Winner == Dota::WINNER_SENTINEL)
        Print(GetLogPrefix() + "detected winner: Sentinel");
      else if (m_Winner == Dota::WINNER_SCOURGE)
        Print(GetLogPrefix() + "detected winner: Scourge");
      else
        Print(GetLogPrefix() + "detected winner: " + to_string(cacheValue));
    } else if (key == "m") {
      m_Time.first = cacheValue;
    } else if (key == "s") {
      m_Time.second = cacheValue;
    }
  }
  /*else if (missionKey == "DLL_State" || missionKey == "bonus" || missionKey == "bonush" || missionKey == "DonRepeatFail" || missionKey == "SyncCounter<XY>.000")
  {
  }*/
  else if (missionKey.size() <= 2 && missionKey.find_first_not_of("1234567890") == string::npos)
  {
    // these are only received at the end of the game

    optional<uint8_t> heroColor = ParseHeroColor(missionKey);
    if (heroColor.has_value())
    {
      // Key "3"		-> Creep Kills
      // Key "4"		-> Creep Denies
      // Key "7"		-> Neutral Kills
      // Key "id"     -> *heroColor (1-5 for sentinel, 6-10 for scourge, accurate after using -sp and/or -switch)

      switch (key[0]) {
        case '1':
          m_Players[*heroColor]->SetKills(cacheValue);
          break;
        case '2':
          m_Players[*heroColor]->SetDeaths(cacheValue);
          break;
        case '3':
          m_Players[*heroColor]->SetCreepKills(cacheValue);
          break;
        case '4':
          m_Players[*heroColor]->SetCreepDenies(cacheValue);
          break;
        case '5':
          m_Players[*heroColor]->SetAssists(cacheValue);
          break;
        case '6':
          m_Players[*heroColor]->SetGold(cacheValue);
          break;
        case '7':
          m_Players[*heroColor]->SetNeutralKills(cacheValue);
          break;
        case '8': {
          array<uint8_t, 4> valueBE = CreateFixedByteArray(cacheValue, true);
          // 8_0 to 8_1
          if (key.size() >= 3 && key[1] == '_' && (0x30 <= key[2] && key[2] <= 0x35)) {
            m_Players[*heroColor]->SetItem(key[2] - 0x30, string(valueBE.begin(), valueBE.end()));
          }
          break;
        }
        case '9': {
          array<uint8_t, 4> valueBE = CreateFixedByteArray(cacheValue, true);
          m_Players[*heroColor]->SetHero(string(valueBE.begin(), valueBE.end()));
          break;
        }
        case 'i':
          if (key.size() >= 2 && key[1] == 'd' && 1 <= cacheValue && cacheValue <= 10) {
            // DotA sends id values from 1-10 with 1-5 being sentinel players and 6-10 being scourge players
            // unfortunately the actual player colours are from 1-5 and from 7-11 so we need to deal with this case here
            if (cacheValue >= 6)
              m_Players[*heroColor]->SetNewColor(static_cast<uint8_t>(cacheValue + 1));
            else
              m_Players[*heroColor]->SetNewColor(static_cast<uint8_t>(cacheValue));
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

[[nodiscard]] GameUser::CGameUser* CDotaStats::GetUserFromColor(const uint8_t color) const
{
  if (!GetIsHeroColor(color)) return nullptr;
  return m_Game.get().GetUserFromColor(color);
}

[[nodiscard]] std::string CDotaStats::GetUserNameFromColor(const uint8_t color) const
{
  switch (color) {
    case SENTINEL_CREEPS_COLOR:
      return "Sentinel"; // should never happen - use GetActorNameFromColor() instead
    case SCOURGE_CREEPS_COLOR:
      return "Scourge"; // should never happen - use GetActorNameFromColor() instead
    case NEUTRAL_CREEPS_COLOR:
      return "Neutral Creeps";
    default: {
      GameUser::CGameUser* user = GetUserFromColor(color);
      if (user) return user->GetName();
      return "Player " + ToDecString(color);
    }
  }
}

[[nodiscard]] std::string CDotaStats::GetActorNameFromColor(const uint8_t color) const
{
  switch (color) {
    case SENTINEL_CREEPS_COLOR:
      return "the Sentinel";
    case SCOURGE_CREEPS_COLOR:
      return "the Scourge";
    case NEUTRAL_CREEPS_COLOR:
      return "Neutral Creeps";
    default: {
      GameUser::CGameUser* user = GetUserFromColor(color);
      if (user) return "Player [" + user->GetName() + "]";
      return "Player " + ToDecString(color);
    }
  }
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
  if (m_Winner == Dota::WINNER_UNDECIDED) {
    return gameResults;
  }

  vector<CGameController*> sentinel = GetSentinelControllers();
  vector<CGameController*> scourge = GetScourgeControllers();

  gameResults.emplace();

  if (m_Winner == Dota::WINNER_SENTINEL) {
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
