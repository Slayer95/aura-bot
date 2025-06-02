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

#ifndef AURA_GAME_RESULTS_H_
#define AURA_GAME_RESULTS_H_

#include "includes.h"
#include "list.h"
#include "protocol/game_protocol.h"

struct GameResults
{
  std::vector<CGameController*> winners;
  std::vector<CGameController*> losers;
  std::vector<CGameController*> drawers;
  std::vector<CGameController*> undecided;

  GameResults();
  ~GameResults();

  inline const std::vector<CGameController*>& GetWinners() const { return winners; }
  inline const std::vector<CGameController*>& GetLosers() const { return losers; }
  inline const std::vector<CGameController*>& GetDrawers() const { return drawers; }
  inline const std::vector<CGameController*>& GetUndecided() const { return drawers; }
  std::vector<std::string> GetWinnersNames() const;
  void Confirm();
};

struct GameResultTeamAnalysis
{
  std::bitset<MAX_SLOTS_MODERN> winnerTeams; // teams with at least one winner
  std::bitset<MAX_SLOTS_MODERN> loserTeams; // teams with at least one loser
  std::bitset<MAX_SLOTS_MODERN> drawerTeams; // teams with at least one drawer
  std::bitset<MAX_SLOTS_MODERN> leaverTeams; // teams with at least one leaver
  std::bitset<MAX_SLOTS_MODERN> undecidedVirtualTeams; // teams with at least one undecided virtual user
  std::bitset<MAX_SLOTS_MODERN> undecidedUserTeams; // teams with at least one undecided user
  std::bitset<MAX_SLOTS_MODERN> undecidedComputerTeams; // teams with at least one undecided computer

  GameResultTeamAnalysis() = default;
  ~GameResultTeamAnalysis() = default;
};

//
// GameResultConstraints
//

struct GameResultConstraints
{
  bool m_IsValid;

  bool m_CanDraw;
  bool m_CanDrawAndWin;
  bool m_CanAllWin;
  bool m_CanAllLose;
  bool m_CanLeaverDraw;
  bool m_CanLeaverWin;
  bool m_CanTeamWithLeaverWin;
  bool m_RequireTeamWinExceptLeaver;

  uint8_t m_UndecidedVirtualHandler;
  uint8_t m_UndecidedUserHandler; // Note: We ignore "leaver" flag and just treat them as undecided
  uint8_t m_UndecidedComputerHandler;
  uint8_t m_ConflictHandler;
  GameResultSourceSelect m_SourceOfTruth;

  uint8_t m_MinPlayers;
  uint8_t m_MaxPlayers;
  uint8_t m_MinWinnerPlayers;
  uint8_t m_MaxWinnerPlayers;
  uint8_t m_MinLoserPlayers;
  uint8_t m_MaxLoserPlayers;

  uint8_t m_MinTeams;
  uint8_t m_MaxTeams;
  uint8_t m_MinTeamsWithWinners;
  uint8_t m_MaxTeamsWithWinners;
  uint8_t m_MinTeamsWithNoWinners;
  uint8_t m_MaxTeamsWithNoWinners;

  GameResultConstraints();
  GameResultConstraints(const CMap* map, CConfig& CFG);
  ~GameResultConstraints();

  [[nodiscard]] inline bool GetIsValid() const { return m_IsValid; }

  [[nodiscard]] inline bool GetCanDraw() const { return m_CanDraw; }
  [[nodiscard]] inline bool GetCanDrawAndWin() const { return m_CanDrawAndWin; }
  [[nodiscard]] inline bool GetCanAllWin() const { return m_CanAllWin; }
  [[nodiscard]] inline bool GetCanAllLose() const { return m_CanAllLose; }
  [[nodiscard]] inline bool GetCanLeaverDraw() const { return m_CanLeaverDraw; }
  [[nodiscard]] inline bool GetCanLeaverWin() const { return m_CanLeaverWin; }
  [[nodiscard]] inline bool GetCanTeamWithLeaverWin() const { return m_CanTeamWithLeaverWin; }
  [[nodiscard]] inline bool GetRequireTeamWinExceptLeaver() const { return m_RequireTeamWinExceptLeaver; }

  [[nodiscard]] inline uint8_t GetUndecidedVirtualHandler() const { return m_UndecidedVirtualHandler; }
  [[nodiscard]] inline uint8_t GetUndecidedUserHandler() const { return m_UndecidedUserHandler; }
  [[nodiscard]] inline uint8_t GetUndecidedComputerHandler() const { return m_UndecidedComputerHandler; }
  [[nodiscard]] inline uint8_t GetConflictHandler() const { return m_ConflictHandler; }
  [[nodiscard]] inline GameResultSourceSelect GetSourceOfTruth() const { return m_SourceOfTruth; }
};

#endif // AURA_GAME_RESULTS_H
