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
#include "gameuser.h"
#include "irc.h"
#include "map.h"
#include "net.h"

#include <set>
#include <optional>
#include <ostream>
#include <utility>
#ifndef DISABLE_DPP
#include <dpp/dpp.h>
#endif

#define FROM_GAME (1 << 0)
#define FROM_BNET (1 << 1)
#define FROM_IRC (1 << 2)
#define FROM_DISCORD (1 << 3)
#define FROM_OTHER (1 << 7)

#define CHAT_SEND_SOURCE_ALL (1 << 0)
#define CHAT_SEND_TARGET_ALL (1 << 1)
#define CHAT_LOG_CONSOLE (1 << 2)
#define CHAT_TYPE_INFO (1 << 3)
#define CHAT_TYPE_DONE (1 << 4)
#define CHAT_TYPE_ERROR (1 << 5)
#define CHAT_WRITE_TARGETS (CHAT_SEND_SOURCE_ALL | CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE)
#define CHAT_RESPONSE_TYPES (CHAT_TYPE_INFO | CHAT_TYPE_DONE | CHAT_TYPE_ERROR)

#define CHAT_ORIGIN_NONE 0
#define CHAT_ORIGIN_GAME 1
#define CHAT_ORIGIN_REALM 2
#define CHAT_ORIGIN_IRC 3
#define CHAT_ORIGIN_DISCORD 4
#define CHAT_ORIGIN_INVALID 255

#define USER_PERMISSIONS_GAME_PLAYER (1 << 0)
#define USER_PERMISSIONS_GAME_OWNER (1 << 1)
#define USER_PERMISSIONS_CHANNEL_VERIFIED (1 << 2)
#define USER_PERMISSIONS_CHANNEL_ADMIN (1 << 3)
#define USER_PERMISSIONS_CHANNEL_ROOTADMIN (1 << 4)
#define USER_PERMISSIONS_BOT_SUDO_SPOOFABLE (1 << 6)
#define USER_PERMISSIONS_BOT_SUDO_OK (1 << 7)

#define SET_USER_PERMISSIONS_ALL (0xFFFF)

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
  CGameUser*                    m_GameUser;
  CIRC*                         m_IRC;
#ifndef DISABLE_DPP
  dpp::slashcommand_t*          m_DiscordAPI;
#else
  void*                         m_DiscordAPI;
#endif

protected:
  std::string                   m_FromName;
  uint64_t                      m_FromIdentifier;
  bool                          m_FromWhisper;
  uint8_t                       m_FromType;
  bool                          m_IsBroadcast;
  char                          m_Token;
  uint16_t                      m_Permissions;

  std::string                   m_ServerName;
  std::string                   m_ReverseHostName; // user hostname, reversed from their IP (received from IRC chat)
  std::string                   m_ChannelName;
  std::string                   m_ActionMessage;

  std::ostream*                 m_Output;

  std::optional<bool>           m_OverrideVerified;
  std::optional<uint8_t>        m_OverridePermissions;

  uint16_t                      m_RefCount; // How many pointers exist to this CCommandContext? The one in CAura::m_ActiveContexts is not counted.
  bool                          m_PartiallyDestroyed;

public:
  // Game
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* game, CGameUser* user, const bool& nIsBroadcast, std::ostream* outputStream);

  // Realm, Realm->Game
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CRealm* fromRealm, std::string& fromName, const bool& isWhisper, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CRealm* fromRealm, std::string& fromName, const bool& isWhisper, const bool& nIsBroadcast, std::ostream* outputStream);

  // IRC, IRC->Game
  CCommandContext(CAura* nAura, CCommandConfig* config, CIRC* ircNetwork, std::string& channelName, std::string& userName, const bool& isWhisper, std::string& reverseHostName, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CIRC* ircNetwork, std::string& channelName, std::string& userName, const bool& isWhisper, std::string& reverseHostName, const bool& nIsBroadcast, std::ostream* outputStream);

#ifndef DISABLE_DPP
  // Discord, Discord->Game
  CCommandContext(CAura* nAura, CCommandConfig* config, dpp::slashcommand_t* discordAPI, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, dpp::slashcommand_t* discordAPI, std::ostream* outputStream);
#endif

  // Arbitrary, Arbitrary->Game
  CCommandContext(CAura* nAura, const bool& nIsBroadcast, std::ostream* outputStream);
  CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, const bool& nIsBroadcast, std::ostream* outputStream);

  inline bool GetWritesToStdout() const { return m_FromType == FROM_OTHER; }

  std::string GetUserAttribution();
  std::string GetUserAttributionPreffix();
  std::ostream* GetOutputStream() { return m_Output; }

  inline bool GetIsWhisper() const { return m_FromWhisper; }
  inline std::string GetSender() const { return m_FromName; }
  inline std::string GetChannelName() const { return m_ChannelName; }
  inline CRealm* GetSourceRealm() const { return m_SourceRealm; }
  inline CGame* GetSourceGame() const { return m_SourceGame; }
  inline CIRC* GetSourceIRC() const { return m_IRC; }
#ifndef DISABLE_DPP
  inline dpp::slashcommand_t* GetDiscordAPI() const { return m_DiscordAPI; }
#endif

  bool SetIdentity(const std::string& userName);
  void SetAuthenticated(const bool& nAuthenticated);
  void SetPermissions(const uint16_t nPermissions);
  void UpdatePermissions();
  void ClearActionMessage() { m_ActionMessage.clear(); }

  std::optional<bool> CheckPermissions(const uint8_t nPermissionsRequired) const;
  bool CheckPermissions(const uint8_t nPermissionsRequired, const uint8_t nAutoPermissions) const;
  std::optional<std::pair<std::string, std::string>> CheckSudo(const std::string& message);
  bool GetIsSudo() const;
  bool CheckActionMessage(const std::string& nMessage) { return m_ActionMessage == nMessage; }
  bool CheckConfirmation(const std::string& cmdToken, const std::string& cmd, const std::string& payload, const std::string& errorMessage);

  std::vector<std::string> JoinReplyListCompact(const std::vector<std::string>& stringList) const;

  void SendPrivateReply(const std::string& message, const uint8_t ctxFlags = 0);
  void SendReplyCustomFlags(const std::string& message, const uint8_t ctxFlags);
  void SendReply(const std::string& message, const uint8_t ctxFlags = 0);
  void InfoReply(const std::string& message, const uint8_t ctxFlags = 0);
  void DoneReply(const std::string& message, const uint8_t ctxFlags = 0);
  void ErrorReply(const std::string& message, const uint8_t ctxFlags = 0);
  void SendAll(const std::string& message);
  void InfoAll(const std::string& message);
  void DoneAll(const std::string& message);
  void ErrorAll(const std::string& message);
  void SendAllUnlessHidden(const std::string& message);
  CGameUser* GetTargetUser(const std::string& target);
  CGameUser* RunTargetPlayer(const std::string& target);
  CGameUser* GetTargetUserOrSelf(const std::string& target);
  CGameUser* RunTargetPlayerOrSelf(const std::string& target);
  bool GetParsePlayerOrSlot(const std::string& target, uint8_t& SID, CGameUser*& user);
  bool RunParsePlayerOrSlot(const std::string& target, uint8_t& SID, CGameUser*& user);
  bool GetParseNonPlayerSlot(const std::string& target, uint8_t& SID);
  bool RunParseNonPlayerSlot(const std::string& target, uint8_t& SID);
  CRealm* GetTargetRealmOrCurrent(const std::string& target);
  bool GetParseTargetRealmUser(const std::string& target, std::string& nameFragment, std::string& realmFragment, CRealm*& realm, bool allowNoRealm = false, bool searchHistory = false);
  uint8_t GetParseTargetServiceUser(const std::string& target, std::string& nameFragment, std::string& locationFragment, void*& location);
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
    case '$':
      return " (dollar.)";
    case '%':
      return " (percent.)";
  }
  return std::string();
}

inline std::string HelpMissingComma(const std::string& payload) {
  if (payload.find(',') == std::string::npos) return " - did you miss the comma?";
  return std::string();
}

inline bool ExtractMessageTokens(const std::string& message, const std::string& token, bool& matchPadding, std::string& matchCmd, std::string& matchPayload)
{
  if (message.empty()) return false;
  std::string::size_type tokenSize = token.length();
  if (message.length() > tokenSize && message.substr(0, tokenSize) == token) {
    std::string::size_type cmdStart = message.find_first_not_of(' ', tokenSize);
    matchPadding = cmdStart > tokenSize;
    if (cmdStart != std::string::npos) {
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
          std::string::size_type payloadEnd = message.find_last_not_of(' ');
          if (payloadEnd == std::string::npos) {
            matchPayload.clear();
          } else {
            matchPayload = message.substr(payloadStart, payloadEnd + 1 - payloadStart);
          }
        }
      }
      return true;
    }
  }
  return false;
}

inline uint8_t ExtractMessageTokensAny(const std::string& message, const std::string& privateToken, const std::string& broadcastToken, std::string& matchToken, std::string& matchCmd, std::string& matchPayload)
{
  uint8_t result = COMMAND_TOKEN_MATCH_NONE;
  if (message.empty()) return result;
  if (!privateToken.empty()) {
    bool matchPadding = false;
    if (ExtractMessageTokens(message, privateToken, matchPadding, matchCmd, matchPayload)) {
      result = COMMAND_TOKEN_MATCH_PRIVATE;
      if (matchPadding) {
        matchToken = privateToken + " ";
      } else {
        matchToken = privateToken;
      }
    }
  }    
  if (result == COMMAND_TOKEN_MATCH_NONE && !broadcastToken.empty()) {
    bool matchPadding = false;
    if (ExtractMessageTokens(message, broadcastToken, matchPadding, matchCmd, matchPayload)) {
      result = COMMAND_TOKEN_MATCH_BROADCAST;
      if (matchPadding) {
        matchToken = broadcastToken + " ";
      } else {
        matchToken = broadcastToken;
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

inline bool ParseBoolean(const std::string& payload, std::optional<bool>& result)
{
  if (payload.empty()) return true;
  std::string inputLower = payload;
  std::transform(std::begin(inputLower), std::end(inputLower), std::begin(inputLower), [](char c) { return static_cast<char>(std::tolower(c)); });
  if (inputLower == "enable" || inputLower == "on" || inputLower == "yes") {
    result = true;
    return true;
  } else if (inputLower == "disable" || inputLower == "off" || inputLower == "no") {
    result = false;
    return true;
  }
  return false;
}

#endif
