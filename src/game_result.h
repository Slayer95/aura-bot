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

//
// GameResultConstraints
//

struct GameResultConstraints
{
  bool selfConsistent;

  bool canDraw;
  bool canDrawAndWin;
  bool canAllWin;
  bool canAllLose;
  bool canLeaverDraw;
  bool canLeaverWin;
  bool canTeamWithLeaverWin;
  bool requireTeamWinExceptLeaver;
  bool undecidedIsLoser;

  uint8_t truthSource;

  std::optional<uint8_t> minPlayers;
  std::optional<uint8_t> maxPlayers;
  std::optional<uint8_t> minWinnerPlayers;
  std::optional<uint8_t> maxWinnerPlayers;
  std::optional<uint8_t> minLoserPlayers;
  std::optional<uint8_t> maxLoserPlayers;

  std::optional<uint8_t> minTeams;
  std::optional<uint8_t> maxTeams;
  std::optional<uint8_t> minTeamsWithWinners;
  std::optional<uint8_t> maxTeamsWithWinners;
  std::optional<uint8_t> minTeamsWithNoWinners;
  std::optional<uint8_t> maxTeamsWithNoWinners;

  GameResultConstraints();
  GameResultConstraints(const CMap* map, CConfig& CFG);
  ~GameResultConstraints();
};

#endif // AURA_GAME_RESULTS_H
