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

#include "game_structs.h"
#include "game_controller_data.h"
#include "game_user.h"


using namespace std;

//
// CGameLogRecord
//

CGameLogRecord::CGameLogRecord(int64_t gameTicks, string text)
  : m_Ticks(gameTicks),
    m_Text(move(text))
{
}

string CGameLogRecord::ToString() const
{
  int64_t gameTicks = m_Ticks;
  int64_t hours = gameTicks / 3600000;
  gameTicks -= hours * 3600000;
  int64_t mins = gameTicks / 60000;
  gameTicks -= mins * 60000;
  int64_t seconds = gameTicks / 1000;
  string hh = to_string(hours);
  string mm = to_string(mins);
  string ss = to_string(seconds);
  if (hours < 10) hh = "0" + hh;
  if (mins < 10) mm = "0" + mm;
  if (seconds < 10) ss = "0" + ss;
  return "[" + hh + ":" + mm + ":" + ss + "] " + m_Text;
}

CGameLogRecord::~CGameLogRecord() = default;

//
// CQueuedActionsFrame
//

CQueuedActionsFrame::CQueuedActionsFrame()
 : callback(ON_SEND_ACTIONS_NONE),
   pauseUID(0xFF),
   bufferSize(0),
   activeQueue(nullptr)
 {
   activeQueue = &actions.emplace_back();
 }

CQueuedActionsFrame::~CQueuedActionsFrame() = default;

void CQueuedActionsFrame::AddAction(CIncomingAction&& action)
{
  const uint16_t actionSize = static_cast<uint16_t>(action.GetOutgoingLength());

  // we aren't allowed to send more than 1460 bytes in a single packet but it's possible we might have more than that many bytes waiting in the queue
  // check if adding the next action to the sub actions queue would put us over the limit
  // (1452 because the INCOMING_ACTION and INCOMING_ACTION2 packets use an extra 8 bytes)
  // we'd be over the limit if we added the next action to the sub actions queue

  /*if (actionSize > 1452) {
    if (bufferSize > 0) {
      activeQueue = &actions.emplace_back();
    }
    bufferSize = actionSize;
  } else */if (bufferSize + actionSize > 1452) {
    activeQueue = &actions.emplace_back();
    activeQueue->reserve(DEFAULT_ACTIONS_PER_FRAME);
    bufferSize = actionSize;
  } else {
    bufferSize += actionSize;
  }
  activeQueue->push_back(std::move(action));
}

vector<uint8_t> CQueuedActionsFrame::GetBytes(const uint16_t sendInterval) const
{
  vector<uint8_t> packet;

  // the W3GS_INCOMING_ACTION2 packet handles the overflow but it must be sent *before*
  // the corresponding W3GS_INCOMING_ACTION packet

  auto it = actions.begin();
  auto back = actions.end() - 1;
  while (it != back) {
    const vector<uint8_t> subPacket = GameProtocol::SEND_W3GS_INCOMING_ACTION2(*it);
    AppendByteArrayFast(packet, subPacket);
    ++it;
  }

  {
    const vector<uint8_t> subPacket = GameProtocol::SEND_W3GS_INCOMING_ACTION(*it, sendInterval);
    AppendByteArrayFast(packet, subPacket);
  }

  // Note: Must ensure Reset() is called afterwards
  return packet;
}

void CQueuedActionsFrame::Reset()
{
  actions.clear();
  callback = ON_SEND_ACTIONS_NONE;
  bufferSize = 0;
  activeQueue = &actions.emplace_back();
  activeQueue->reserve(DEFAULT_ACTIONS_PER_FRAME);
  leavers.clear();
}

bool CQueuedActionsFrame::GetIsEmpty() const
{
  if (callback != ON_SEND_ACTIONS_NONE) return false;
  if (!leavers.empty()) return false;
  if (bufferSize != 0) return false;
  if (actions.empty() && activeQueue == nullptr) return true;
  return actions.size() == 1 && activeQueue == &actions.front() && activeQueue->empty();
}

size_t CQueuedActionsFrame::GetActionCount() const
{
  if (actions.empty()) return 0;
  uint32_t count = 0;
  for (const ActionQueue& actionQueue : actions) {
    count += actionQueue.size();
  }
  return count;
}

void CQueuedActionsFrame::MergeFrame(CQueuedActionsFrame& frame)
{
  if (frame.bufferSize == 0) return;

  callback = frame.callback;

  for (auto& user : frame.leavers) {
    leavers.push_back(user);
  }

  auto it = frame.actions.begin();
  while (it != frame.actions.end()) {
    ActionQueue& subActions = (*it);
    auto it2 = subActions.begin();
    while (it2 != subActions.end()) {
      AddAction(std::move(*it2));
      ++it2;
    }
    // subActions is now in indeterminate state, so use erase() instead of clear()
    subActions.erase(subActions.begin(), subActions.end());
    ++it;
  }
  frame.Reset();
}

bool CQueuedActionsFrame::GetHasActionsBy(const uint8_t UID) const
{
  for (const ActionQueue& actionQueue : actions) {
    for (const auto& action : actionQueue) {
      if (action.GetUID() == UID) {
        return true;
      }
    }
  }
  return false;
}

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
