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

#ifndef AURA_REALM_CHAT_H_
#define AURA_REALM_CHAT_H_

#include <utility>
#include <string>
#include <vector>

#include "includes.h"
#include "aura.h"
#include "realm.h"
#include "config_realm.h"
#include "command.h"
#include "bnetprotocol.h"

#define RECV_SELECTOR_SYSTEM 1
#define RECV_SELECTOR_ONLY_WHISPER 2
#define RECV_SELECTOR_ONLY_PUBLIC 3
#define RECV_SELECTOR_ONLY_PUBLIC_OR_DROP 4
#define RECV_SELECTOR_PREFER_PUBLIC 5

#define CHAT_RECV_SELECTED_NONE 0
#define CHAT_RECV_SELECTED_SYSTEM 1
#define CHAT_RECV_SELECTED_PUBLIC 2
#define CHAT_RECV_SELECTED_WHISPER 3
#define CHAT_RECV_SELECTED_DROP 4

#define CHAT_CALLBACK_NONE 0
#define CHAT_CALLBACK_REFRESH_GAME 1

#define CHAT_VALIDATOR_NONE 0
#define CHAT_VALIDATOR_CURRENT_LOBBY 1

class CRealm;
class CBNETProtocol;
class CCommandContext;

class CQueuedChatMessage
{
private:
  CRealm*               m_Realm;
  CBNETProtocol*        m_Protocol;
  int64_t               m_QueuedTime;
  std::string           m_Channel; // Empty if whisper-only.
  uint8_t               m_ReceiverSelector; // force whisper, prefer channel, wait for channel, channel or drop
  std::vector<uint8_t>  m_ReceiverName; // Empty if the message cannot fall back to whispering.
  std::vector<uint8_t>  m_Message;
  uint8_t               m_MessageValue; // If m_Message is too long, Aura MAY replace it by a shorter one, respecting this value.

  CCommandContext*      m_ProxySenderCtx;
  std::vector<uint8_t>  m_ProxySenderName; // !whois, !tell, !invite, !say, !announce
  std::string           m_EarlyFeedback;

  std::vector<uint8_t>  m_Validator; // First byte CHAT_VALIDATOR_NONE, CHAT_VALIDATOR_CURRENT_LOBBY. Rest is parsed.
  uint8_t               m_Callback;

public:
  void SetMessage(const std::string& body) {
    m_Message = std::vector<uint8_t>(body.begin(), body.end());
  }
  void SetMessage(const std::vector<uint8_t>& body) {
    m_Message = body;
  }
  void SetMessage(const uint8_t status, const std::string& body) {
    m_MessageValue = status;
    m_Message = std::vector<uint8_t>(body.begin(), body.end());
  }
  void SetReceiver(const uint8_t selector) {
    m_ReceiverSelector = selector;
  }
  void SetReceiver(const uint8_t selector, const std::string& name) {
    m_ReceiverSelector = selector;
    m_ReceiverName = std::vector<uint8_t>(name.begin(), name.end());
  }
  void SetReceiver(const uint8_t selector, const std::vector<uint8_t>& name) {
    m_ReceiverSelector = selector;
    m_ReceiverName = name;
  }
  void SetChannel(const std::string& nChannel) { m_Channel = nChannel; }
  void SetCallback(const uint8_t callback) { m_Callback = callback; }
  void SetValidator(const uint8_t validator);
  int64_t GetQueuedDuration() const;
  bool GetIsStale() const;
  std::vector<uint8_t> GetMessage() const;
  std::vector<uint8_t> GetWhisper() const;
  inline std::string GetInnerMessage() const { return std::string(m_Message.begin(), m_Message.end()); }
  uint8_t QuerySelection(const std::string& currentChannel) const;
  std::vector<uint8_t> SelectBytes(const std::string& currentChannel, uint8_t& selectType) const;
  uint8_t GetVirtualSize(const size_t wrapSize, const uint8_t selectType) const;
  uint8_t SelectSize(const size_t wrapSize, const std::string& currentChannel) const;
  std::pair<bool, uint8_t> OptimizeVirtualSize(const size_t wrapSize) const;
  bool GetSendsEarlyFeedback() const;
  void SendEarlyFeedback() const;
  void SetEarlyFeedback(const std::string& body) { m_EarlyFeedback = body; }
  inline std::string GetEarlyFeedback() const { return m_EarlyFeedback; }
  bool IsProxySent() const { return m_ProxySenderCtx != nullptr; }
  CCommandContext* GetProxyCtx() const { return m_ProxySenderCtx; }
  std::string GetReceiver() const { return std::string(m_ReceiverName.begin(), m_ReceiverName.end()); }
  inline uint8_t GetCallback() const { return m_Callback; }

  CQueuedChatMessage(CRealm* nRealm, CCommandContext* nCtx, const bool isProxy);
  ~CQueuedChatMessage();
};

#endif // AURA_REALM_CHAT_H_
