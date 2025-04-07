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
  : m_IsValid(true),
    m_CanDraw(false),
    m_CanDrawAndWin(false),
    m_CanAllWin(false),
    m_CanAllLose(false),
    m_CanLeaverDraw(false),
    m_CanLeaverWin(false),
    // A "winner team" is a team with at least 1 winner
    m_CanTeamWithLeaverWin(false),
    m_RequireTeamWinExceptLeaver(false),
    m_UndecidedVirtualHandler(GAME_RESULT_VIRTUAL_UNDECIDED_HANDLER_AUTO),
    m_UndecidedUserHandler(GAME_RESULT_USER_UNDECIDED_HANDLER_LOSER_SELF),
    m_UndecidedComputerHandler(GAME_RESULT_COMPUTER_UNDECIDED_HANDLER_AUTO),
    m_ConflictHandler(GAME_RESULT_CONFLICT_HANDLER_MAJORITY_OR_VOID),
    m_SourceOfTruth(GAME_RESULT_SOURCE_NONE),

    m_MinPlayers(2),
    m_MaxPlayers(MAX_SLOTS_MODERN),
    m_MinWinnerPlayers(0),
    m_MaxWinnerPlayers(MAX_SLOTS_MODERN),
    m_MinLoserPlayers(0),
    m_MaxLoserPlayers(MAX_SLOTS_MODERN),

    m_MinTeams(2),
    m_MaxTeams(MAX_SLOTS_MODERN),
    // A "winner team" is a team with at least 1 winner
    m_MinTeamsWithWinners(0),
    m_MaxTeamsWithWinners(MAX_SLOTS_MODERN),
    m_MinTeamsWithNoWinners(0),
    m_MaxTeamsWithNoWinners(MAX_SLOTS_MODERN)
{
}

GameResultConstraints::GameResultConstraints(const CMap* map, CConfig& CFG)
  : m_IsValid(true),
    m_CanDraw(!map->GetMapIsMelee()),
    m_CanDrawAndWin(false),
    m_CanAllWin(false),
    m_CanAllLose(false),
    m_CanLeaverDraw(false),
    m_CanLeaverWin(false),
    // A "winner team" is a team with at least 1 winner
    m_CanTeamWithLeaverWin(false),
    m_RequireTeamWinExceptLeaver(false),
    m_UndecidedVirtualHandler(GAME_RESULT_VIRTUAL_UNDECIDED_HANDLER_AUTO),
    m_UndecidedUserHandler(GAME_RESULT_USER_UNDECIDED_HANDLER_LOSER_SELF),
    m_UndecidedComputerHandler(GAME_RESULT_COMPUTER_UNDECIDED_HANDLER_AUTO),
    m_ConflictHandler(GAME_RESULT_CONFLICT_HANDLER_MAJORITY_OR_VOID),
    m_SourceOfTruth(map->GetMMDEnabled() ? GAME_RESULT_SOURCE_SELECT_PREFER_MMD : GAME_RESULT_SOURCE_SELECT_ONLY_LEAVECODE),

    m_MinPlayers(2),
    m_MaxPlayers(map->GetVersionMaxSlots()),
    m_MinWinnerPlayers(0),
    m_MaxWinnerPlayers(map->GetVersionMaxSlots()),
    m_MinLoserPlayers(0),
    m_MaxLoserPlayers(map->GetVersionMaxSlots()),

    m_MinTeams(2),
    m_MaxTeams(map->GetVersionMaxSlots()),
    // A "winner team" is a team with at least 1 winner
    m_MinTeamsWithWinners(0),
    m_MaxTeamsWithWinners(map->GetVersionMaxSlots()),
    m_MinTeamsWithNoWinners(0),
    m_MaxTeamsWithNoWinners(map->GetVersionMaxSlots())
{
  const uint8_t maxSlots = map->GetVersionMaxSlots();
  const vector<string> truthSourceOptions = {"none", "only-exit", "only-mmd", "prefer-exit", "prefer-mmd"};
  if (CFG.Exists("map.game_result.source")) {
    m_SourceOfTruth = CFG.GetStringIndex("map.game_result.source", truthSourceOptions, m_SourceOfTruth);
    CFG.FailIfErrorLast();
  } else {
    CFG.SetString("map.game_result.source", truthSourceOptions[m_SourceOfTruth]);
  }

  // Generic
  vector<string> undecidedVirtualHandlers = {"none", "loser-self", "auto"};
  if (CFG.Exists("map.game_result.resolution.undecided_handler.virtual")) {
    m_UndecidedVirtualHandler = CFG.GetStringIndex("map.game_result.resolution.undecided_handler.virtual", undecidedVirtualHandlers, m_UndecidedVirtualHandler);
  } else {
    CFG.SetString("map.game_result.resolution.undecided_handler.virtual", undecidedVirtualHandlers[m_UndecidedVirtualHandler]);
  }

  // Generic
  vector<string> undecidedUserHandlers = {"none", "loser-self", "loser-self-and-non-user-allies"};
  if (CFG.Exists("map.game_result.resolution.undecided_handler.user")) {
    m_UndecidedUserHandler = CFG.GetStringIndex("map.game_result.resolution.undecided_handler.user", undecidedUserHandlers, m_UndecidedUserHandler);
  } else {
    CFG.SetString("map.game_result.resolution.undecided_handler.user", undecidedUserHandlers[m_UndecidedUserHandler]);
  }

  // Generic
  vector<string> undecidedComputerHandlers = {"none", "loser-self", "auto"};
  if (CFG.Exists("map.game_result.resolution.undecided_handler.computer")) {
    m_UndecidedComputerHandler = CFG.GetStringIndex("map.game_result.resolution.undecided_handler.computer", undecidedComputerHandlers, m_UndecidedComputerHandler);
  } else {
    CFG.SetString("map.game_result.resolution.undecided_handler.computer", undecidedComputerHandlers[m_UndecidedComputerHandler]);
  }

  // MMD-specific
  const vector<string> conflictHandlerOptions = {"void", "pessimistic", "optimistic", "majority-or-void", "majority-or-pessimistic", "majority-or-optimistic"};
    if (CFG.Exists("map.game_result.resolution.conflict_handler")) {
    m_ConflictHandler = CFG.GetStringIndex("map.game_result.resolution.conflict_handler", conflictHandlerOptions, m_ConflictHandler);
    CFG.FailIfErrorLast();
  } else {
    CFG.SetString("map.game_result.resolution.conflict_handler", conflictHandlerOptions[m_ConflictHandler]);
  }

  if (CFG.Exists("map.game_result.constraints.draw.allowed")) {
    m_CanDraw = CFG.GetBool("map.game_result.constraints.draw.allowed", m_CanDraw);
  } else {
    CFG.SetBool("map.game_result.constraints.draw.allowed", m_CanDraw);
  }

  if (CFG.Exists("map.game_result.constraints.draw_and_win.allowed")) {
    m_CanDrawAndWin = CFG.GetBool("map.game_result.constraints.draw_and_win.allowed", m_CanDrawAndWin);
  } else {
    CFG.SetBool("map.game_result.constraints.draw_and_win.allowed", m_CanDrawAndWin);
  }

  if (CFG.Exists("map.game_result.constraints.shared_winners.all.allowed")) {
    m_CanAllWin = CFG.GetBool("map.game_result.constraints.shared_winners.all.allowed", m_CanAllWin);
  } else {
    CFG.SetBool("map.game_result.constraints.shared_winners.all.allowed", m_CanAllWin);
  }

  if (CFG.Exists("map.game_result.constraints.shared_losers.all.allowed")) {
    m_CanAllLose = CFG.GetBool("map.game_result.constraints.shared_losers.all.allowed", m_CanAllLose);
  } else {
    CFG.SetBool("map.game_result.constraints.shared_losers.all.allowed", m_CanAllLose);
  }

  if (CFG.Exists("map.game_result.constraints.drawing_leavers.allowed")) {
    m_CanLeaverDraw = CFG.GetBool("map.game_result.constraints.drawing_leavers.allowed", m_CanLeaverDraw);
  } else {
    CFG.SetBool("map.game_result.constraints.drawing_leavers.allowed", m_CanLeaverDraw);
  }

  m_CanLeaverWin = false; // TODO: This should be configurable with a grace period
  if (CFG.Exists("map.game_result.constraints.winning_leavers.allowed")) {
    m_CanLeaverWin = CFG.GetBool("map.game_result.constraints.winning_leavers.allowed", m_CanLeaverWin);
  } else {
    CFG.SetBool("map.game_result.constraints.winning_leavers.allowed", m_CanLeaverWin);
  }

  if (CFG.Exists("map.game_result.constraints.winner_has_allied_leaver.allowed")) {
    m_CanTeamWithLeaverWin = CFG.GetBool("map.game_result.constraints.winner_has_allied_leaver.allowed", m_CanTeamWithLeaverWin);
  } else {
    CFG.SetBool("map.game_result.constraints.winner_has_allied_leaver.allowed", m_CanTeamWithLeaverWin);
  }

  if (CFG.Exists("map.game_result.constraints.allied_victory_except_leaver.required")) {
    m_RequireTeamWinExceptLeaver = CFG.GetBool("map.game_result.constraints.allied_victory_except_leaver.required", m_RequireTeamWinExceptLeaver);
  } else {
    CFG.SetBool("map.game_result.constraints.allied_victory_except_leaver.required", m_RequireTeamWinExceptLeaver);
  }

  bool wasStrictMode = CFG.GetStrictMode();
  if (CFG.Exists("map.game_result.constraints.controllers.min")) {
    m_MinPlayers = CFG.GetSlot("map.game_result.constraints.controllers.min", maxSlots, m_MinPlayers) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.controllers.min", m_MinPlayers);
  }
  if (CFG.Exists("map.game_result.constraints.controllers.max")) {
    m_MaxPlayers = CFG.GetSlot("map.game_result.constraints.controllers.max", maxSlots, m_MaxPlayers) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.controllers.max", m_MaxPlayers);
  }
  if (CFG.Exists("map.game_result.constraints.controllers.winners.min")) {
    m_MinWinnerPlayers = CFG.GetSlot("map.game_result.constraints.controllers.winners.min", maxSlots, m_MinWinnerPlayers) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.controllers.winners.min", m_MinWinnerPlayers);
  }
  if (CFG.Exists("map.game_result.constraints.controllers.winners.max")) {
    m_MaxWinnerPlayers = CFG.GetSlot("map.game_result.constraints.controllers.winners.max", maxSlots, m_MaxWinnerPlayers) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.controllers.winners.max", m_MaxWinnerPlayers);
  }
  if (CFG.Exists("map.game_result.constraints.controllers.losers.min")) {
    m_MinLoserPlayers = CFG.GetSlot("map.game_result.constraints.controllers.losers.min", maxSlots, m_MinLoserPlayers) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.controllers.losers.min", m_MinLoserPlayers);
  }
  if (CFG.Exists("map.game_result.constraints.controllers.losers.max")) {
    m_MaxLoserPlayers = CFG.GetSlot("map.game_result.constraints.controllers.losers.max", maxSlots, m_MaxLoserPlayers) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.controllers.losers.max", m_MaxLoserPlayers);
  }

  if (CFG.Exists("map.game_result.constraints.teams.min")) {
    m_MinTeams = CFG.GetSlot("map.game_result.constraints.teams.min", maxSlots, m_MinTeams) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.teams.min", m_MinTeams);
  }
  if (CFG.Exists("map.game_result.constraints.teams.max")) {
    m_MaxTeams = CFG.GetSlot("map.game_result.constraints.teams.max", maxSlots, m_MaxTeams) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.teams.max", m_MaxTeams);
  }
  if (CFG.Exists("map.game_result.constraints.teams.winners.min")) {
    m_MinTeamsWithWinners = CFG.GetSlot("map.game_result.constraints.teams.winners.min", maxSlots, m_MinTeamsWithWinners) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.teams.winners.min", m_MinTeamsWithWinners);
  }
  if (CFG.Exists("map.game_result.constraints.teams.winners.max")) {
    m_MaxTeamsWithWinners = CFG.GetSlot("map.game_result.constraints.teams.winners.max", maxSlots, m_MaxTeamsWithWinners) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.teams.winners.max", m_MaxTeamsWithWinners);
  }
  if (CFG.Exists("map.game_result.constraints.teams.losers.min")) {
    m_MinTeamsWithNoWinners = CFG.GetSlot("map.game_result.constraints.teams.losers.min", maxSlots, m_MinTeamsWithNoWinners) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.teams.losers.min", m_MinTeamsWithNoWinners);
  }
  if (CFG.Exists("map.game_result.constraints.teams.losers.max")) {
    m_MaxTeamsWithNoWinners = CFG.GetSlot("map.game_result.constraints.teams.losers.max", maxSlots, m_MaxTeamsWithNoWinners) + 1;
  } else {
    CFG.SetUint8("map.game_result.constraints.teams.losers.max", m_MaxTeamsWithNoWinners);
  }

  CFG.SetStrictMode(wasStrictMode);

  if (
    m_UndecidedUserHandler == GAME_RESULT_USER_UNDECIDED_HANDLER_LOSER_SELF_AND_ALLIES && (
      m_UndecidedVirtualHandler != GAME_RESULT_VIRTUAL_UNDECIDED_HANDLER_AUTO ||
      m_UndecidedComputerHandler != GAME_RESULT_COMPUTER_UNDECIDED_HANDLER_AUTO
    )
  ) {
    Print("[MAP] <map.game_result.resolution.undecided_handler.user = loser-self-and-non-user-allies> requires other handlers set to auto");
    m_IsValid = false;
  }

  if (m_MinPlayers > m_MaxPlayers) m_IsValid = false;
  if (m_MinWinnerPlayers > m_MaxWinnerPlayers) m_IsValid = false;
  if (m_MinLoserPlayers > m_MaxLoserPlayers) m_IsValid = false;
  if (m_MinTeams > m_MaxTeams) m_IsValid = false;
  if (m_MinTeamsWithWinners > m_MaxTeamsWithWinners) m_IsValid = false;
  if (m_MinTeamsWithNoWinners > m_MaxTeamsWithNoWinners) m_IsValid = false;

  if (!m_IsValid) {
    CFG.SetFailed();
  }
}

GameResultConstraints::~GameResultConstraints()
{
}

