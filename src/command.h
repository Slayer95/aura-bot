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

#ifndef AURA_COMMAND_H_
#define AURA_COMMAND_H_

#include "aura.h"
#include "auradb.h"
#include "config_commands.h"
#include "realm.h"
#include "game.h"
#include "gameplayer.h"
#include "irc.h"
#include "map.h"
#include "net.h"

#include <optional>
#include <ostream>
#include <utility>

#define FROM_GAME (1 << 0)
#define FROM_BNET (1 << 1)
#define FROM_IRC (1 << 2)
#define FROM_OTHER (1 << 7)

#define CHAT_SEND_SOURCE_ALL (1 << 0)
#define CHAT_SEND_TARGET_ALL (1 << 1)
#define CHAT_LOG_CONSOLE (1 << 2)

#define USER_PERMISSIONS_GAME_PLAYER (1 << 0)
#define USER_PERMISSIONS_GAME_OWNER (1 << 1)
#define USER_PERMISSIONS_CHANNEL_VERIFIED (1 << 2)
#define USER_PERMISSIONS_CHANNEL_ADMIN (1 << 3)
#define USER_PERMISSIONS_CHANNEL_ROOTADMIN (1 << 4)
#define USER_PERMISSIONS_BOT_SUDO_SPOOFABLE (1 << 6)
#define USER_PERMISSIONS_BOT_SUDO_OK (1 << 7)

#define COMMAND_TOKEN_MATCH_NONE 0
#define COMMAND_TOKEN_MATCH_PRIVATE 1
#define COMMAND_TOKEN_MATCH_BROADCAST 2

//
// CCommandContext
//

class CCommandContext
{
public:
  CAura*                        m_Aura;
  CCommandConfig*               m_Config;
  CRealm*                       m_SourceRealm;
  CRealm*                       m_TargetRealm;
  CGame*                        m_SourceGame;
  CGame*                        m_TargetGame;
  CGamePlayer*                  m_Player;
  CIRC*                         m_IRC;
  bool                          m_IsBroadcast;

protected:
  std::string                   m_FromName;
  bool                          m_FromWhisper;
  uint8_t                       m_FromType;
  char                          m_Token;
  uint16_t                      m_Permissions;

  std::string                   m_HostName;
  std::string                   m_ReverseHostName; // user hostname, reversed from their IP (received from IRC chat)
  std::string                   m_ChannelName;
  std::string                   m_ActionMessage;

  std::ostream*                 m_Output;

  std::optional<uint8_t>        m_OverrideVerified;
  std::optional<uint8_t>        m_OverridePermissions;

  uint16_t                      m_RefCount; // How many pointers exist to this CCommandContext? The one in CAura::m_ActiveContexts is not counted.
  bool                          m_PartiallyDestroyed;

public:
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* game, CGamePlayer* player, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CRealm* fromRealm, std::string& fromName, const bool& isWhisper, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CIRC* ircNetwork, std::string& channelName, std::string& userName, const bool& isWhisper, std::string& reverseHostName, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CRealm* fromRealm, std::string& fromName, const bool& isWhisper, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CIRC* ircNetwork, std::string& channelName, std::string& userName, const bool& isWhisper, std::string& reverseHostName, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, const bool& nIsBroadcast, std::ostream* outputStream);

  inline bool GetWritesToStdout() const { return m_FromType == FROM_OTHER; };

  std::string GetUserAttribution();
  std::string GetUserAttributionPreffix();

  bool SetIdentity(const std::string& userName, const std::string& realmId);
  void SetAuthenticated(const bool& nAuthenticated);
  void SetPermissions(const uint8_t nPermissions);
  void UpdatePermissions();
  void ClearActionMessage() { m_ActionMessage.clear(); }

  std::optional<bool> CheckPermissions(const uint8_t nPermissionsRequired) const;
  bool CheckPermissions(const uint8_t nPermissionsRequired, const uint8_t nAutoPermissions) const;
  std::optional<std::pair<std::string, std::string>> CheckSudo(const std::string& message);
  bool CheckActionMessage(const std::string& nMessage) { return m_ActionMessage == nMessage; }

  void SendReplyNoFlags(const std::string& message);
  void SendReplyCustomFlags(const std::string& message, const uint8_t ccFlags);
  void SendReply(const std::string& message);
  void SendReply(const std::string& message, const uint8_t ccFlags);
  void ErrorReply(const std::string& message);
  void ErrorReply(const std::string& message, const uint8_t ccFlags);
  void SendAll(const std::string& message);
  void ErrorAll(const std::string& message);
  CGamePlayer* GetTargetPlayer(const std::string& target);
  CGamePlayer* GetTargetPlayerOrSelf(const std::string& target);
  CRealm* GetTargetRealmOrCurrent(const std::string& target);
  CGame* GetTargetGame(const std::string& target);
  void UseImplicitHostedGame();
  void Run(const std::string& token, const std::string& command, const std::string& payload);
  void Ref() { ++m_RefCount; }
  bool Unref() { return --m_RefCount == 0; }
  void SetPartiallyDestroyed() { m_PartiallyDestroyed = true; }
  bool GetPartiallyDestroyed() const { return m_PartiallyDestroyed; }

  ~CCommandContext();
};

inline std::string GetTokenName(const std::string& token) {
  if (token.length() != 1) return std::string();
  switch (token[0]) {
    case '!':
      return " (exclamation mark.)";
    case '?':
      return " (question mark.)";
    case '.':
      return " (period.)";
    case ',':
      return " (comma.)";
    case '~':
      return " (tilde.)";
    case '-':
      return " (hyphen.)";
    case '#':
      return " (hashtag.)";
    case '@':
      return " (at.)";
    case '&':
      return " (ampersand.)";
    case '$':
      return " (dollar.)";
    case '%':
      return " (percent.)";
  }
  return std::string();
}

inline uint8_t ExtractMessageTokens(const std::string& message, const std::string& privateToken, const std::string& broadcastToken, std::string& matchToken, std::string& matchCmd, std::string& matchPayload)
{
  uint8_t result = COMMAND_TOKEN_MATCH_NONE;
  if (message.empty()) return result;
  if (!privateToken.empty()) {
    std::string::size_type tokenSize = privateToken.length();
    if (message.length() > tokenSize && message.substr(0, tokenSize) == privateToken) {
      std::string::size_type cmdStart = message.find_first_not_of(' ', tokenSize);
      if (cmdStart != std::string::npos) {
        matchToken = privateToken;
        result = COMMAND_TOKEN_MATCH_PRIVATE;
        std::string::size_type cmdEnd = message.find_first_of(' ', cmdStart);
        if (cmdEnd == std::string::npos) {
          matchCmd = message.substr(cmdStart);
          matchPayload.clear();
        } else {
          matchCmd = message.substr(cmdStart, cmdEnd - cmdStart);
          std::string::size_type payloadStart = message.find_first_not_of(' ', cmdEnd);
          if (payloadStart == std::string::npos) {
            matchPayload.clear();
          } else {
            matchPayload = message.substr(cmdEnd, payloadStart - cmdEnd);
          }
        }
      }
    }
  }    
  if (!result && !broadcastToken.empty()) {
    std::string::size_type tokenSize = broadcastToken.length();
    if (message.length() > tokenSize && message.substr(0, tokenSize) == broadcastToken) {
      size_t cmdStart = message[tokenSize] == ' ' ? tokenSize + 1 : tokenSize;
      if (cmdStart != std::string::npos) {
        matchToken = broadcastToken;
        result = COMMAND_TOKEN_MATCH_BROADCAST;
        std::string::size_type cmdEnd = message.find_first_of(' ', cmdStart);
        if (cmdEnd == std::string::npos) {
          matchCmd = message.substr(cmdStart);
          matchPayload.clear();
        } else {
          matchCmd = message.substr(cmdStart, cmdEnd - cmdStart);
          std::string::size_type payloadStart = message.find_first_not_of(' ', cmdEnd);
          if (payloadStart == std::string::npos) {
            matchPayload.clear();
          } else {
            matchPayload = message.substr(cmdEnd, payloadStart - cmdEnd);
          }
        }
      }
    }
  }

  if (result != COMMAND_TOKEN_MATCH_NONE) {
    std::transform(std::begin(matchCmd), std::end(matchCmd), std::begin(matchCmd), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  }

  return result;
}


#endif
