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

#include "config/config.h"
#include "game_result.h"
#include "game_controller_data.h"
#include "map.h"

using namespace std;

//
// GameResults
//

GameResults::GameResults()
{
}

GameResults::~GameResults()
{
}

vector<string> GameResults::GetWinnersNames() const
{
  vector<string> names;
  for (const auto& winner : GetWinners()) {
    names.push_back(winner->GetName());
  }
  return names;
}

void GameResults::Confirm()
{
  for (auto& winner : winners) {
    winner->SetGameResult(GAME_RESULT_WINNER);
  }
  for (auto& loser : losers) {
    loser->SetGameResult(GAME_RESULT_LOSER);
  }
  for (auto& drawer : drawers) {
    drawer->SetGameResult(GAME_RESULT_DRAWER);
  }
}

//
// GameResultConstraints
//

GameResultConstraints::GameResultConstraints()
 : selfConsistent(true),
   canDraw(false),
   canDrawAndWin(false),
   canAllWin(false),
   canAllLose(false),
   canLeaverDraw(false),
   canLeaverWin(false),
   canTeamWithLeaverWin(false),
   requireTeamWinExceptLeaver(false),
   undecidedIsLoser(false),
   truthSource(GAME_RESULT_SOURCE_NONE)
{
}

GameResultConstraints::GameResultConstraints(const CMap* map, CConfig& CFG)
 : selfConsistent(true), // TODO
   canDraw(false),
   canDrawAndWin(false), // TODO
   canAllWin(false),
   canAllLose(false),
   canLeaverDraw(false), // TODO
   canLeaverWin(false), // TODO
   canTeamWithLeaverWin(false), // TODO
   requireTeamWinExceptLeaver(false),
   undecidedIsLoser(false),
   truthSource(GAME_RESULT_SOURCE_NONE)
{
  const vector<string> truthSourceOptions = {"none", "only-exit", "only-mmd", "prefer-exit", "prefer-mmd"};
  truthSource = map->GetMMDEnabled() ? GAME_RESULT_SOURCE_SELECT_PREFER_MMD : GAME_RESULT_SOURCE_SELECT_ONLY_LEAVECODE;
  if (CFG.Exists("map.game_result.source")) {
    truthSource = CFG.GetStringIndex("map.game_result.source", truthSourceOptions, truthSource);
    CFG.FailIfErrorLast();
  } else {
    CFG.SetString("map.game_result.source", truthSourceOptions[truthSource]);
  }

  canDraw = !map->GetMapIsMelee();
  if (CFG.Exists("map.game_result.draw.allowed")) {
    canDraw = CFG.GetBool("map.game_result.draw.allowed", canDraw);
  } else {
    CFG.SetBool("map.game_result.draw.allowed", canDraw);
  }

  /*
  canWinMultiplePlayers = true;
  if (CFG.Exists("map.game_result.shared_winners.players.allowed")) {
    canWinMultiplePlayers = CFG.GetBool("map.game_result.shared_winners.players.allowed", canWinMultiplePlayers);
  } else {
    CFG.SetBool("map.game_result.shared_winners.players.allowed", canWinMultiplePlayers);
  }

  canWinMultipleTeams = !map->GetMapIsMelee() || !(map->GetMapFlags() & MAPFLAG_FIXEDTEAMS);
  if (CFG.Exists("map.game_result.shared_winners.teams.allowed")) {
    canWinMultipleTeams = CFG.GetBool("map.game_result.shared_winners.teams.allowed", canWinMultipleTeams);
  } else {
    CFG.SetBool("map.game_result.shared_winners.teams.allowed", canWinMultipleTeams);
  }
  */

  canAllWin = false; // not allowed in a rated game
  if (CFG.Exists("map.game_result.shared_winners.all.allowed")) {
    canAllWin = CFG.GetBool("map.game_result.shared_winners.all.allowed", canAllWin);
  } else {
    CFG.SetBool("map.game_result.shared_winners.all.allowed", canAllWin);
  }

  canAllLose = false; // not allowed in a rated game
  if (CFG.Exists("map.game_result.shared_losers.all.allowed")) {
    canAllLose = CFG.GetBool("map.game_result.shared_losers.all.allowed", canAllLose);
  } else {
    CFG.SetBool("map.game_result.shared_losers.all.allowed", canAllLose);
  }

  undecidedIsLoser = true;
  if (CFG.Exists("map.game_result.undecided.is_loser")) {
    undecidedIsLoser = CFG.GetBool("map.game_result.undecided.is_loser", undecidedIsLoser);
  } else {
    CFG.SetBool("map.game_result.undecided.is_loser", undecidedIsLoser);
  }
}

GameResultConstraints::~GameResultConstraints()
{
}

