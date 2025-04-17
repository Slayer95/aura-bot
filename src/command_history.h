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

#ifndef AURA_COMMAND_HISTORY_H_
#define AURA_COMMAND_HISTORY_H_

#include "includes.h"

//
// CommandHistory
//

struct CommandHistory
{
  bool m_UsedAnyCommands;
  bool m_SentAutoCommandsHelp;
  uint8_t m_SmartCommand;
  std::string m_LastCommand;
  std::optional<int64_t> m_SudoMode;                          // if the player has enabled sudo mode, its expiration time
  
  CommandHistory();
  ~CommandHistory();

  [[nodiscard]] inline std::string                            GetLastCommand() const { return m_LastCommand; }
  inline void                                                 ClearLastCommand() { m_LastCommand.clear(); }
  inline void                                                 SetLastCommand(const std::string nLastCommand) { m_LastCommand = nLastCommand; }

  [[nodiscard]] inline bool                                   GetUsedAnyCommands() const { return m_UsedAnyCommands; }
  inline void                                                 SetUsedAnyCommands(const bool nValue) { m_UsedAnyCommands = nValue; }

  [[nodiscard]] inline bool                                   GetSentAutoCommandsHelp() const { return m_SentAutoCommandsHelp; }
  inline void                                                 SetSentAutoCommandsHelp(const bool nValue) { m_SentAutoCommandsHelp = nValue; }

  [[nodiscard]] inline uint8_t                                GetSmartCommand() const { return m_SmartCommand; }
  inline void                                                 SetSmartCommand(const uint8_t nValue) { m_SmartCommand = nValue; }
  inline void                                                 ClearSmartCommand() { m_SmartCommand = SMART_COMMAND_NONE; }

  bool                                                        GetIsSudoMode() const;
  bool                                                        CheckSudoMode(CAura* nAura, std::shared_ptr<CGame> game, const std::string& userName);
  void                                                        SudoModeStart(CAura* nAura, std::shared_ptr<CGame> game, const std::string& userName);
  void                                                        SudoModeEnd(CAura* nAura, std::shared_ptr<CGame> game, const std::string& userName);
};

#endif
