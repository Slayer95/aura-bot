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

#ifndef AURA_STATS_H_
#define AURA_STATS_H_

#include "../includes.h"
#include "../game_structs.h"

//
// CDotaStats
//

// the stats class is passed a copy of every player action in ProcessAction when it's received
// then when the game is over the Save function is called
// so the idea is that you parse the actions to gather data about the game, storing the results in any member variables you need in your subclass
// and in the Save function you write the results to the database
// e.g. for dota the number of kills/deaths/assists, etc...

class CDotaStats
{
public:
  CGame*                                        m_Game;
  CDBDotAPlayer*                                m_Players[12];
  uint8_t                                       m_Winner;
  std::optional<uint64_t>                       m_GameOverTime;

  explicit CDotaStats(CGame* nGame);
  ~CDotaStats();
  CDotaStats(CDotaStats&) = delete;

  bool RecvAction(uint8_t UID, const CIncomingAction& action);
  bool UpdateQueue();
  void FlushQueue();
  void Save(CAura* nAura, CAuraDB* nDB);
  [[nodiscard]] inline bool GetIsSentinelPlayerColor(const uint8_t color) { return 1 <= color && color <= 5; }
  [[nodiscard]] inline bool GetIsScourgePlayerColor(const uint8_t color) { return 7 <= color && color <= 11; }
  [[nodiscard]] inline bool GetIsPlayerColor(const uint8_t color) { return GetIsSentinelPlayerColor(color) || GetIsScourgePlayerColor(color); }
  [[nodiscard]] inline bool GetAreSameTeamColors(const uint8_t a, const uint8_t b) { return (a <= 5 && b <= 5) || (a >= 7 && b >= 7); }
  [[nodiscard]] std::vector<CDBGamePlayer*> GetSentinelPlayers() const;
  [[nodiscard]] std::vector<CDBGamePlayer*> GetScourgePlayers() const;
  [[nodiscard]] std::optional<GameResults> GetGameResults(const bool undecidedIsLoser) const;
  [[nodiscard]] inline bool GetIsGameOver() const { return m_GameOverTime.has_value(); }
  [[nodiscard]] inline uint64_t GetGameOverTime() const { return m_GameOverTime.value(); }
  [[nodiscard]] std::string GetLogPrefix() const;
  void LogMetaData(int64_t recvTicks, const std::string& text) const;
};

#endif // AURA_STATS_H_
