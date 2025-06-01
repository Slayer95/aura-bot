/*

  Copyright [2025] [Leonardo Julca]

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

#include "command_history.h"
#include "aura.h"
#include "game.h"

using namespace std;

//
// CommandHistory
//

CommandHistory::CommandHistory()
 : m_UsedAnyCommands(false),
   m_SentAutoCommandsHelp(false),
   m_SmartCommand(SMART_COMMAND_NONE)
{}

CommandHistory::~CommandHistory()
{
}

bool CommandHistory::GetIsSudoMode() const
{
  if (!m_SudoMode.has_value()) return false;
  return GetTime() < m_SudoMode.value();
}

bool CommandHistory::CheckSudoMode(CAura* nAura, shared_ptr<CGame> game, const string& userName)
{
  if (GetIsSudoMode()) return true;
  if (m_SudoMode.has_value()) {
    m_SudoMode.reset();
    if (nAura->MatchLogLevel(LogLevel::kWarning)) {
      string prefix;
      if (game) {
        prefix = game->GetLogPrefix();
      } else {
        prefix = "[SPECTATOR]";
      }
      Print(prefix + "sudo session expired for [" + userName + "]");
    }
  }
  return false;
}

void CommandHistory::SudoModeStart(CAura* nAura, shared_ptr<CGame> game, const string& userName)
{
  if (nAura->MatchLogLevel(LogLevel::kWarning)) {
    string prefix;
    if (game) {
      prefix = game->GetLogPrefix();
    } else {
      prefix = "[SPECTATOR]";
    }
    Print(prefix + "sudo session started by [" + userName + "]");
  }
  m_SudoMode = GetTime() + 600;
}

void CommandHistory::SudoModeEnd(CAura* nAura, shared_ptr<CGame> game, const string& userName)
{
  if (!GetIsSudoMode()) {
    return;
  }
  if (nAura->MatchLogLevel(LogLevel::kWarning)) {
    string prefix;
    if (game) {
      prefix = game->GetLogPrefix();
    } else {
      prefix = "[SPECTATOR]";
    }
    Print(prefix + "sudo session ended by [" + userName + "]");
  }
  m_SudoMode.reset();
}
