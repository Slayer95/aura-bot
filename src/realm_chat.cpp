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

#include "realm.h"
#include "command.h"
#include "aura.h"
#include "util.h"
#include "fileutil.h"
#include "config.h"
#include "config_realm.h"
#include "realm_chat.h"
#include "includes.h"

#include <algorithm>
#include <cmath>

using namespace std;

//
// CQueuedChatMessage
//

CQueuedChatMessage::CQueuedChatMessage(CRealm* nRealm)
  : m_Realm(nRealm),
    m_Protocol(nRealm->GetProtocol()),
    m_QueuedTime(0),
    m_ReceiverSelector(0),
    m_MessageValue(0),

    m_ProxySenderCtx(nullptr)
{
    //m_ReceiverName = vector<uint8_t>();
    //m_Message = vector<uint8_t>(),
}

CQueuedChatMessage::~CQueuedChatMessage()
{
  if (m_ProxySenderCtx) {
    m_Realm->m_Aura->UnholdContext(m_ProxySenderCtx);
    m_ProxySenderCtx = nullptr;
  }
}

int64_t CQueuedChatMessage::GetQueuedDuration() const
{
  return GetTime() - m_QueuedTime;
}

bool CQueuedChatMessage::GetIsStale() const
{
  if (m_Validator.empty()) return false;
  switch (m_Validator[0]) {
    case CHAT_VALIDATOR_CURRENT_LOBBY:
      if (!m_Realm->m_Aura->m_CurrentLobby) return true;
      return m_Realm->m_Aura->m_CurrentLobby->GetHostCounter() != ByteArrayToUInt32(m_Validator, false, 1);
    default:
      return false;
  }
}

vector<uint8_t> CQueuedChatMessage::GetMessage() const
{
  return m_Protocol->SEND_SID_CHAT_PUBLIC(m_Message);
}

vector<uint8_t> CQueuedChatMessage::GetWhisper() const
{
  return m_Protocol->SEND_SID_CHAT_WHISPER(m_Message, m_ReceiverName);
}

uint8_t CQueuedChatMessage::QuerySelection(const std::string& currentChannel) const
{
  switch (m_ReceiverSelector) {
    case RECV_SELECTOR_ONLY_WHISPER:
      return CHAT_RECV_SELECTED_WHISPER;
    case RECV_SELECTOR_ONLY_PUBLIC:
      if (currentChannel.empty()) return CHAT_RECV_SELECTED_NONE;
      return CHAT_RECV_SELECTED_PUBLIC;
    case RECV_SELECTOR_ONLY_PUBLIC_OR_DROP:
      if (currentChannel.empty()) return CHAT_RECV_SELECTED_DROP;
      return CHAT_RECV_SELECTED_PUBLIC;
    case RECV_SELECTOR_PREFER_PUBLIC:
      if (currentChannel.empty()) return CHAT_RECV_SELECTED_WHISPER;
      return CHAT_RECV_SELECTED_PUBLIC;
    default:
      // Should never happen
      return CHAT_RECV_SELECTED_DROP;
  }
}

vector<uint8_t> CQueuedChatMessage::SelectBytes(const std::string& currentChannel, uint8_t& selectType) const
{
  selectType = QuerySelection(currentChannel);
  switch (selectType) {
    case CHAT_RECV_SELECTED_WHISPER:
      return GetWhisper();
    case CHAT_RECV_SELECTED_PUBLIC:
      return GetMessage();
    default:
      return vector<uint8_t>();
  }
}

uint8_t CQueuedChatMessage::SelectSize(const size_t wrapSize, const std::string& currentChannel) const
{
  uint8_t selectType = QuerySelection(currentChannel);
  switch (selectType) {
    case CHAT_RECV_SELECTED_WHISPER:
    case CHAT_RECV_SELECTED_PUBLIC:
      return GetVirtualSize(wrapSize, selectType);
    default:
      // m_Message.size() > 0 => GetVirtualSize(...) > 0
      return 0;
  }
}

uint8_t CQueuedChatMessage::GetVirtualSize(const size_t wrapSize, const uint8_t selectType) const
{
  // according to realm's antiflood parameters
  size_t result;
  if (selectType == CHAT_RECV_SELECTED_WHISPER) {
    result = (m_Protocol->GetWhisperSize(m_Message, m_ReceiverName) + wrapSize - 1) / wrapSize;
  } else {
    result = (m_Protocol->GetMessageSize(m_Message) + wrapSize - 1) / wrapSize;
  }
  // Note that PvPGN antiflood accepts no more than 100 virtual lines in any given message.
  if (result > 0xFF) return 0xFF;
  return static_cast<uint8_t>(result);
}

std::pair<bool, uint8_t> CQueuedChatMessage::OptimizeVirtualSize(const size_t wrapSize) const
{
  // m_ReceiverSelector must be accounted externally
  const uint8_t minSize = GetVirtualSize(wrapSize, CHAT_RECV_SELECTED_PUBLIC);
  return make_pair(GetVirtualSize(wrapSize, CHAT_RECV_SELECTED_WHISPER) > minSize, minSize);
}
