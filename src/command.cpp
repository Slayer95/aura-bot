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

#include "command.h"
#include "bnetprotocol.h"
#include "gameprotocol.h"
#include "gamesetup.h"
#include "hash.h"

#include <random>
#include <tuple>
#include <future>
#include <algorithm>

#ifndef DISABLE_DPP
#include <dpp/dpp.h>
#endif

using namespace std;

//
// CCommandContext
//

/* In-game command */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* game, CGamePlayer* player, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),

    m_SourceRealm(player->GetRealm(false)),
    m_TargetRealm(nullptr),
    m_SourceGame(game),
    m_TargetGame(game),
    m_Player(player),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(player->GetName()),
    m_FromIdentifier(0),
    m_FromWhisper(false),
    m_FromType(FROM_GAME),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(0),

    m_ServerName(player->GetRealmHostName()),

    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* Command received from BNET but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CRealm* fromRealm, string& fromName, const bool& isWhisper, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(fromRealm),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_Player(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(fromName),
    m_FromIdentifier(0),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_BNET),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(0),

    m_ServerName(fromRealm->GetServer()),

    m_ChannelName(isWhisper ? string() : fromRealm->GetCurrentChannel()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* Command received from IRC but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, CIRC* ircNetwork, string& channelName, string& userName, const bool& isWhisper, string& reverseHostName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_Player(nullptr),
    m_IRC(ircNetwork),
    m_DiscordAPI(nullptr),

    m_FromName(userName),
    m_FromIdentifier(0),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_IRC),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(0),

    m_ServerName(ircNetwork->m_Config->m_HostName),
    m_ReverseHostName(reverseHostName),
    m_ChannelName(channelName),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

#ifndef DISABLE_DPP
/* Command received from Discord but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, dpp::slashcommand_t* discordAPI, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_Player(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(discordAPI),

    m_FromName(discordAPI->command.get_issuing_user().username),
    m_FromIdentifier(discordAPI->command.get_issuing_user().id),
    m_FromWhisper(false),
    m_FromType(FROM_DISCORD),
    m_IsBroadcast(true),

    m_Permissions(0),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
  try {
    m_ServerName = discordAPI->command.get_guild().name;
    m_ChannelName = discordAPI->command.get_channel().name;
  } catch (...) {
    m_FromWhisper = true;
  }
}
#endif

/* Command received from elsewhere but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CGame* targetGame, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_Player(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(string()),
    m_FromIdentifier(0),
    m_FromWhisper(false),
    m_FromType(FROM_OTHER),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(0),

    m_ServerName(string()),
    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* BNET command */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CRealm* fromRealm, string& fromName, const bool& isWhisper, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(fromRealm),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_Player(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(fromName),
    m_FromIdentifier(0),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_BNET),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(0),


    m_ServerName(fromRealm->GetServer()),

    m_ChannelName(isWhisper ? string() : fromRealm->GetCurrentChannel()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* IRC command */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, CIRC* ircNetwork, string& channelName, string& userName, const bool& isWhisper, string& reverseHostName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_Player(nullptr),
    m_IRC(ircNetwork),
    m_DiscordAPI(nullptr),

    m_FromName(userName),
    m_FromIdentifier(0),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_IRC),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(0),

    m_ServerName(ircNetwork->m_Config->m_HostName),
    m_ReverseHostName(reverseHostName),
    m_ChannelName(channelName),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

#ifndef DISABLE_DPP
/* Discord command */
CCommandContext::CCommandContext(CAura* nAura, CCommandConfig* config, dpp::slashcommand_t* discordAPI, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_Player(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(discordAPI),

    m_FromName(discordAPI->command.get_issuing_user().username),
    m_FromIdentifier(discordAPI->command.get_issuing_user().id),
    m_FromWhisper(false),
    m_FromType(FROM_DISCORD),
    m_IsBroadcast(true),
    m_Permissions(0),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
  try {
    m_ServerName = discordAPI->command.get_guild().name;
    m_ChannelName = discordAPI->command.get_channel().name;
    Print("[DISCORD] Received slash command in " + m_ServerName + "'s server - channel " + m_ChannelName);
  } catch (...) {
    Print("[DISCORD] Received slash command on " + m_FromName + "'s DM");
    m_ServerName = "users.discord.com";
    m_FromWhisper = true;
  }
}
#endif

/* Generic command */
CCommandContext::CCommandContext(CAura* nAura, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(nAura->m_CommandDefaultConfig),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),
    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_Player(nullptr),
    m_IRC(nullptr),
    m_DiscordAPI(nullptr),

    m_FromName(string()),
    m_FromIdentifier(0),
    m_FromWhisper(false),
    m_FromType(FROM_OTHER),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(0),

    m_ServerName(string()),

    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

bool CCommandContext::SetIdentity(const string& userName)
{
  m_FromName = userName;
  return true;
}

string CCommandContext::GetUserAttribution()
{
  if (m_Player) {
    return m_FromName + "@" + (m_ServerName.empty() ? "@@LAN/VPN" : m_ServerName);
  } else if (m_SourceRealm) {
    return m_FromName + "@" + m_SourceRealm->GetServer();
  } else if (m_IRC) {
    return m_FromName + "@" + m_ServerName;
  } else if (m_DiscordAPI) {
    return m_FromName + "@[" + m_ServerName + "].discord.com";
  } else if (!m_FromName.empty()) {
    return m_FromName;
  } else {
    return "[Anonymous]";
  }
}

string CCommandContext::GetUserAttributionPreffix()
{
  if (m_Player) {
    return m_TargetGame->GetLogPrefix() + "Player [" + m_FromName + "@" + (m_ServerName.empty() ? "@@LAN/VPN" : m_ServerName) + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (m_SourceRealm) {
    return m_SourceRealm->GetLogPrefix() + "User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (m_IRC) {
    return "[IRC] User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (m_DiscordAPI) {
    return "[DISCORD] User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (!m_FromName.empty()) {
    return "[SYSTEM] User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else {
    return "[ANONYMOUS] (Mode " + ToHexString(m_Permissions) + ") ";
  }
}

void CCommandContext::SetAuthenticated(const bool& nAuthenticated)
{
  m_OverrideVerified = nAuthenticated;
}

void CCommandContext::SetPermissions(const uint8_t nPermissions)
{
  m_OverridePermissions = nPermissions;
}

void CCommandContext::UpdatePermissions()
{
  if (m_OverridePermissions.has_value()) {
    m_Permissions = m_OverridePermissions.value();
    return;
  }

  m_Permissions = 0;

  if (m_IRC) {
    string::size_type suffixSize = m_IRC->m_Config->m_VerifiedDomain.length();
    if (suffixSize > 0) {
      if (m_ReverseHostName.length() > suffixSize &&
        m_ReverseHostName.substr(0, m_ReverseHostName.length() - suffixSize) == m_IRC->m_Config->m_VerifiedDomain
      ) {
        m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
      }
      const bool IsCreatorIRC = (
        m_TargetGame && m_TargetGame->GetCreatedFromType() == GAMESETUP_ORIGIN_IRC &&
        reinterpret_cast<CIRC*>(m_TargetGame->GetCreatedFrom()) == m_IRC
      );
      if ((!m_TargetGame || IsCreatorIRC) && m_IRC->GetIsModerator(m_ReverseHostName)) m_Permissions |= USER_PERMISSIONS_CHANNEL_ADMIN;
      if (m_IRC->GetIsSudoer(m_ReverseHostName)) m_Permissions |= USER_PERMISSIONS_BOT_SUDO_SPOOFABLE;
    }
    return;
  }
  if (m_DiscordAPI) {
#ifndef DISABLE_DPP
    if (m_Aura->m_Discord->GetIsSudoer(m_FromIdentifier)) {
      m_Permissions = 0xFFFF &~ (USER_PERMISSIONS_BOT_SUDO_OK);
    } else if (m_DiscordAPI->command.get_issuing_user().is_verified()) {
      m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
    }
#endif
    return;
  }

  bool IsRealmVerified = false;
  if (m_OverrideVerified.has_value()) {
    IsRealmVerified = m_OverrideVerified.value();
  } else {
    IsRealmVerified = m_Player ? m_Player->IsRealmVerified() : m_SourceRealm != nullptr;
  }

  // Trust PvPGN servers on player identities for admin powers. Their impersonation is not a threat we worry about.
  // However, do NOT trust them regarding sudo access, since those commands may cause data deletion or worse.
  // Note also that sudo permissions must be ephemeral, since neither WC3 nor PvPGN TCP connections are secure.
  bool IsOwner = false;
  if (m_Player && m_Player->m_Game == m_TargetGame) {
    IsOwner = m_Player->GetIsOwner(m_OverrideVerified);
  } else if (m_TargetGame) {
    IsOwner = IsRealmVerified && m_TargetGame->MatchOwnerName(m_FromName) && m_ServerName == m_TargetGame->GetOwnerRealm();
  }
  bool IsCreatorRealm = m_TargetGame && m_SourceRealm && m_TargetGame->MatchesCreatedFrom(GAMESETUP_ORIGIN_REALM, reinterpret_cast<void*>(m_SourceRealm));
  bool IsRootAdmin = IsRealmVerified && m_SourceRealm != nullptr && (!m_TargetGame || IsCreatorRealm) && m_SourceRealm->GetIsAdmin(m_FromName);
  bool IsAdmin = IsRootAdmin || (IsRealmVerified && m_SourceRealm != nullptr && (!m_TargetGame || IsCreatorRealm) && m_SourceRealm->GetIsModerator(m_FromName));
  bool IsSudoSpoofable = IsRealmVerified && m_SourceRealm != nullptr && m_SourceRealm->GetIsSudoer(m_FromName);

  // Owners are always treated as players if the game hasn't started yet. Even if they haven't joined.
  if (m_Player || (IsOwner && m_TargetGame && m_TargetGame->GetIsLobby())) {
    m_Permissions |= USER_PERMISSIONS_GAME_PLAYER;
  }

  // Leaver or absent owners are automatically demoted.
  if (IsRealmVerified) {
    m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
  }
  if (IsOwner && (m_Player || (m_TargetGame && m_TargetGame->GetIsLobby()))) {
    m_Permissions |= USER_PERMISSIONS_GAME_OWNER;
  }
  if (IsAdmin) m_Permissions |= USER_PERMISSIONS_CHANNEL_ADMIN;
  if (IsRootAdmin) m_Permissions |= USER_PERMISSIONS_CHANNEL_ROOTADMIN;

  // Sudo is a permission system separate from channels.
  if (IsSudoSpoofable) m_Permissions |= USER_PERMISSIONS_BOT_SUDO_SPOOFABLE;
}

optional<bool> CCommandContext::CheckPermissions(const uint8_t requiredPermissions) const
{
  optional<bool> result;
  switch (requiredPermissions) {
    case COMMAND_PERMISSIONS_DISABLED:
      result = false;
      break;
    case COMMAND_PERMISSIONS_SUDO:
      result = (m_Permissions & USER_PERMISSIONS_BOT_SUDO_OK) > 0;
      break;
    case COMMAND_PERMISSIONS_SUDO_UNSAFE:
      result = (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE) > 0;
      break;
    case COMMAND_PERMISSIONS_ROOTADMIN:
      result = (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) > 0;
      break;
    case COMMAND_PERMISSIONS_ADMIN:
      result = (m_Permissions & (USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) > 0;
      break;
    case COMMAND_PERMISSIONS_VERIFIED_OWNER:
      result = (
        ((m_Permissions & (USER_PERMISSIONS_CHANNEL_VERIFIED | USER_PERMISSIONS_BOT_SUDO_OK)) > 0) &&
        ((m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0)
      );
      // Note: It's possible to be owner without being verified, if they belong to a LAN realm.
      break;
    case COMMAND_PERMISSIONS_OWNER:
      result = (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      break;
    case COMMAND_PERMISSIONS_VERIFIED:
      result = (m_Permissions & (USER_PERMISSIONS_CHANNEL_VERIFIED | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      break;
    case COMMAND_PERMISSIONS_AUTO:
      // Let commands special-case nullopt, or call CheckPermissions().value_or(true)
      break;
    case COMMAND_PERMISSIONS_POTENTIAL_OWNER:
      if (m_TargetGame && !m_TargetGame->HasOwnerSet()) {
        result = (m_Permissions & (USER_PERMISSIONS_GAME_PLAYER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      } else {
        result = (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      }
      break;
    case COMMAND_PERMISSIONS_UNVERIFIED:
      result = true;
      break;
  }

  return result;
}

bool CCommandContext::CheckPermissions(const uint8_t requiredPermissions, const uint8_t autoPermissions) const
{
  // autoPermissions must not be COMMAND_PERMISSIONS_AUTO, nor other unhandled cases
  // (that results in infinite recursion)
  optional<bool> result = CheckPermissions(requiredPermissions);
  if (result.has_value()) return result.value();
  return CheckPermissions(autoPermissions).value_or(false);
}

bool CCommandContext::CheckConfirmation(const string& cmdToken, const string& cmd, const string& payload, const string& errorMessage)
{
  string message = cmdToken + cmd + payload;
  if (m_Player) {
    if (m_Player->GetLastCommand() == message) {
      m_Player->ClearLastCommand();
      return true;
    } else {
      m_Player->SetLastCommand(message);
    }
  }
  ErrorReply(errorMessage + "Send the command again to confirm.");
  return false;
}

optional<pair<string, string>> CCommandContext::CheckSudo(const string& message)
{
  optional<pair<string, string>> Result;
  // Allow !su for LAN connections
  if (!m_ServerName.empty() && !(m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
    return Result;
  }
  if (!m_Aura->m_SudoContext) {
    return Result;
  }
  if (m_Aura->m_SudoContext->GetPartiallyDestroyed()) {
    m_Aura->UnholdContext(m_Aura->m_SudoContext);
    m_Aura->m_SudoContext = nullptr;
    m_Aura->m_SudoAuthPayload.clear();
    m_Aura->m_SudoExecCommand.clear();
    return Result;
  }
  bool isValidCaller = (
    m_FromName == m_Aura->m_SudoContext->m_FromName &&
    m_SourceRealm == m_Aura->m_SudoContext->m_SourceRealm &&
    m_TargetRealm == m_Aura->m_SudoContext->m_TargetRealm &&
    m_SourceGame == m_Aura->m_SudoContext->m_SourceGame &&
    m_TargetGame == m_Aura->m_SudoContext->m_TargetGame &&
    m_Player == m_Aura->m_SudoContext->m_Player &&
    m_IRC == m_Aura->m_SudoContext->m_IRC
    // TODO: Discord !aura su
  );
  if (isValidCaller && message == m_Aura->m_SudoAuthPayload) {
    LogStream(*m_Output, "[AURA] Confirmed " + m_FromName + " command \"" + m_Aura->m_SudoExecCommand + "\"");
    size_t PayloadStart = m_Aura->m_SudoExecCommand.find(' ');
    string Command, Payload;
    if (PayloadStart != string::npos) {
      Command = m_Aura->m_SudoExecCommand.substr(0, PayloadStart);
      Payload = m_Aura->m_SudoExecCommand.substr(PayloadStart + 1);
    } else {
      Command = m_Aura->m_SudoExecCommand;
    }
    transform(begin(Command), end(Command), begin(Command), [](char c) { return static_cast<char>(std::tolower(c)); });
    Result = make_pair(Command, Payload);
    //m_Permissions |= USER_PERMISSIONS_BOT_SUDO_OK;
    m_Permissions = 0xFFFF;
  }
  m_Aura->UnholdContext(m_Aura->m_SudoContext);
  m_Aura->m_SudoContext = nullptr;
  m_Aura->m_SudoAuthPayload.clear();
  m_Aura->m_SudoExecCommand.clear();
  return Result;
}

bool CCommandContext::GetIsSudo() const
{
  return (0 != (m_Permissions & (USER_PERMISSIONS_BOT_SUDO_OK)));
}

vector<string> CCommandContext::JoinReplyListCompact(const vector<string>& stringList) const
{
  vector<string> result;

  if (m_FromType == FROM_GAME && m_TargetGame) {
    string bufferedLine;
    for (const auto& element : stringList) {
      if (element.size() > 100) {
        if (!bufferedLine.empty()) {
          result.push_back(bufferedLine);
          bufferedLine.clear();
        }
        string leftElement = element;
        do {
          result.push_back(leftElement.substr(0, 100));
          leftElement = leftElement.substr(100);
        } while (leftElement.length() > 100);
        if (!leftElement.empty()) {
          result.push_back(leftElement);
        }
      } else if (bufferedLine.size() + element.size() > 97) {
        result.push_back(bufferedLine);
        bufferedLine = element;
      } else if (bufferedLine.empty()) {
        bufferedLine = element;
      } else {
        bufferedLine += " | " + element;
      }
    }
    if (!bufferedLine.empty()) {
      result.push_back(bufferedLine);
      bufferedLine.clear();
    }
  } else {
    result.push_back(JoinVector(stringList, false));
  }

  return result;
}

void CCommandContext::SendPrivateReply(const string& message, const uint8_t ctxFlags)
{
  if (message.empty())
    return;

  switch (m_FromType) {
    case FROM_GAME: {
      if (!m_TargetGame) break;
      if (message.length() <= 100) {
        m_TargetGame->SendChat(m_Player, message);
      } else {
        string leftMessage = message;
        do {
          m_TargetGame->SendChat(m_Player, leftMessage.substr(0, 100));
          leftMessage = leftMessage.substr(100);
        } while (leftMessage.length() > 100);
        if (!leftMessage.empty()) {
          m_TargetGame->SendChat(m_Player, leftMessage);
        }
      }
      break;
    }

    case FROM_BNET: {
      if (m_SourceRealm) {
        m_SourceRealm->TryQueueChat(message, m_FromName, true, this, ctxFlags);
      }
      break;
    }

    case FROM_IRC: {
      if (m_IRC) {
        m_IRC->SendUser(message, m_FromName);
      }
      break;
    }

#ifndef DISABLE_DPP
    case FROM_DISCORD: {
      if (m_DiscordAPI) {
        m_Aura->m_Discord->SendUser(message, m_FromIdentifier);
      }
      break;
    }
#endif

    default: {
      LogStream(*m_Output, "[AURA] " + message);
    }
  }
}

void CCommandContext::SendReplyCustomFlags(const string& message, const uint8_t ctxFlags) {
  bool AllTarget = ctxFlags & CHAT_SEND_TARGET_ALL;
  bool AllSource = ctxFlags & CHAT_SEND_SOURCE_ALL;
  bool AllSourceSuccess = false;
  if (AllTarget) {
    if (m_TargetGame) {
      m_TargetGame->SendAllChat(message);
      if (m_TargetGame == m_SourceGame) {
        AllSourceSuccess = true;
      }
    }
    if (m_TargetRealm) {
      m_TargetRealm->TryQueueChat(message, m_FromName, false, this, ctxFlags);
      if (m_TargetRealm == m_SourceRealm) {
        AllSourceSuccess = true;
      }
    }
    // IRC/Discord are not valid targets, only sources.
  }
  if (AllSource && !AllSourceSuccess && !m_FromWhisper) {
    if (m_SourceGame) {
      m_SourceGame->SendAllChat(message);
      AllSourceSuccess = true;
    }
    if (m_SourceRealm && !AllSourceSuccess) {
      m_SourceRealm->TryQueueChat(message, m_FromName, false, this, ctxFlags);
      AllSourceSuccess = true;
    }
    if (m_IRC) {
      m_IRC->SendChannel(message, m_ChannelName);
      AllSourceSuccess = true;
    }
#ifndef DISABLE_DPP
    if (m_DiscordAPI) {
      m_DiscordAPI->edit_original_response(dpp::message(message));
      AllSourceSuccess = true;
    }
#endif
  }
  if (!AllSourceSuccess) {
    SendPrivateReply(message, ctxFlags);
  }

  // Write to console if CHAT_LOG_CONSOLE, but only if we haven't written to it in SendPrivateReply
  if (m_FromType != FROM_OTHER && (ctxFlags & CHAT_LOG_CONSOLE)) {
    if (m_TargetGame) {
      LogStream(*m_Output, m_TargetGame->GetLogPrefix() + message);
    } else if (m_SourceRealm) {
      LogStream(*m_Output, m_SourceRealm->GetLogPrefix() + message);
    } else if (m_IRC) {
      LogStream(*m_Output, "[IRC] " + message);
    } else if (m_DiscordAPI) {
      LogStream(*m_Output, "[DISCORD] " + message);
    } else {
      LogStream(*m_Output, "[AURA] " + message);
    }
  }
}

void CCommandContext::SendReply(const string& message, const uint8_t ctxFlags) {
  if (message.empty()) return;

  if (m_IsBroadcast) {
    SendReplyCustomFlags(message, ctxFlags | CHAT_SEND_SOURCE_ALL);
  } else {
    SendReplyCustomFlags(message, ctxFlags);
  }
}

void CCommandContext::InfoReply(const string& message, const uint8_t ctxFlags) {
  if (message.empty()) return;
  SendReply(message, ctxFlags | CHAT_TYPE_INFO);
}

void CCommandContext::DoneReply(const string& message, const uint8_t ctxFlags) {
  if (message.empty()) return;
  SendReply("Done: " + message, ctxFlags | CHAT_TYPE_DONE);
}

void CCommandContext::ErrorReply(const string& message, const uint8_t ctxFlags) {
  if (message.empty()) return;
  SendReply("Error: " + message, ctxFlags | CHAT_TYPE_ERROR);
}

void CCommandContext::SendAll(const string& message)
{
  if (message.empty()) return;
  SendReply(message, CHAT_SEND_TARGET_ALL);
}

void CCommandContext::InfoAll(const string& message)
{
  if (message.empty()) return;
  InfoReply(message, CHAT_SEND_TARGET_ALL);
}

void CCommandContext::DoneAll(const string& message)
{
  if (message.empty()) return;
  DoneReply(message, CHAT_SEND_TARGET_ALL);
}

void CCommandContext::ErrorAll(const string& message)
{
  if (message.empty()) return;
  ErrorReply(message, CHAT_SEND_TARGET_ALL);
}

CGamePlayer* CCommandContext::GetTargetPlayer(const string& target)
{
  CGamePlayer* TargetPlayer = nullptr;
  if (!m_TargetGame) {
    return TargetPlayer;
  }

  uint8_t Matches = m_TargetGame->GetPlayerFromNamePartial(target, TargetPlayer);
  if (Matches > 1) {
    ErrorReply("Player [" + target + "] ambiguous.");
  } else if (Matches == 0) {
    ErrorReply("Player [" + target + "] not found.");
  }
  return TargetPlayer;
}

CGamePlayer* CCommandContext::GetTargetPlayerOrSelf(const string& target)
{
  if (target.empty()) {
    return m_Player;
  }

  CGamePlayer* targetPlayer = nullptr;
  if (!m_TargetGame) return targetPlayer;
  uint8_t Matches = m_TargetGame->GetPlayerFromNamePartial(target, targetPlayer);
  if (Matches > 1) {
    targetPlayer = nullptr;
  }
  return targetPlayer;
}

bool CCommandContext::ParsePlayerOrSlot(const std::string& target, uint8_t& SID, CGamePlayer*& player)
{
  if (!m_TargetGame || target.empty()) {
    ErrorReply("Please provide a player @name or #slot.");
    return false;
  }
  switch (target[0]) {
    case '#': {
      uint8_t testSID = ParseSID(target.substr(1));
      const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
      if (!slot) {
        ErrorReply("Slot " + ToDecString(testSID + 1) + " not found.");
        return false;
      }
      SID = testSID;
      player = m_TargetGame->GetPlayerFromPID(slot->GetPID());
      return true;
    }

    case '@': {
      player = GetTargetPlayer(target.substr(1));
      return player != nullptr;
    }

    default: {
      uint8_t testSID = ParseSID(target);
      const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
      CGamePlayer* testPlayer = nullptr;
      uint8_t matches = m_TargetGame->GetPlayerFromNamePartial(target.substr(1), testPlayer);
      if ((slot && matches > 0) || (!slot && matches == 0)) {
        ErrorReply("Please provide a player @name or #slot.");
        return false;
      }
      if (matches > 1) {
        ErrorReply("Player [" + target + "] ambiguous.");
        return false;
      }
      if (matches == 0) {
        SID = testSID;
        player = m_TargetGame->GetPlayerFromPID(slot->GetPID());
      } else {
        SID = m_TargetGame->GetSIDFromPID(testPlayer->GetPID());
        player = testPlayer;
      }
      return true;
    }
  }
}

bool CCommandContext::ParseNonPlayerSlot(const std::string& target, uint8_t& SID)
{
  if (!m_TargetGame || target.empty()) {
    ErrorReply("Please provide a player #slot.");
    return false;
  }

  uint8_t testSID = 0xFF;
  if (target[0] == '#') {
    testSID = ParseSID(target.substr(1));
  } else {
    testSID = ParseSID(target);
  }

  const CGameSlot* slot = m_TargetGame->InspectSlot(testSID);
  if (!slot) {
    ErrorReply("Slot [" + target + "] not found.");
    return false;
  }
  if (m_TargetGame->GetIsPlayerSlot(testSID)) {
    ErrorReply("Slot is occupied by a player.");
    return false;
  }
  SID = testSID;
  return true;
}

CRealm* CCommandContext::GetTargetRealmOrCurrent(const string& target)
{
  if (target.empty()) {
    return m_SourceRealm;
  }
  string realmId = target;
  transform(begin(realmId), end(realmId), begin(realmId), [](char c) { return static_cast<char>(std::tolower(c)); });
  CRealm* exactMatch = m_Aura->GetRealmByInputId(realmId);
  if (exactMatch) return exactMatch;
  return m_Aura->GetRealmByHostName(realmId);
}

CGame* CCommandContext::GetTargetGame(const string& target)
{
  if (target.empty()) {
    return nullptr;
  }
  string gameId = target;
  transform(begin(gameId), end(gameId), begin(gameId), [](char c) { return static_cast<char>(std::tolower(c)); });
  if (gameId == "lobby") {
    return m_Aura->m_CurrentLobby;
  }
  if (gameId.substr(0, 5) == "game#") {
    gameId = gameId.substr(5);
  }
  uint32_t GameNumber = 0;
  try {
    GameNumber = stoul(gameId);
  } catch (...) {
  }
  if (GameNumber == 0 || GameNumber > m_Aura->m_Games.size()) {
    return nullptr;
  }
  return m_Aura->m_Games[GameNumber - 1];
}

void CCommandContext::UseImplicitHostedGame()
{
  if (m_TargetGame) return;

  m_TargetGame = m_Aura->m_CurrentLobby;
  if (m_TargetGame && !GetIsSudo()) {
    UpdatePermissions();
  }
}

void CCommandContext::Run(const string& cmdToken, const string& command, const string& payload)
{
  const static string emptyString;

  UpdatePermissions();

  string Command = command;
  string Payload = payload;

  if (HasNullOrBreak(command) || HasNullOrBreak(payload)) {
    ErrorReply("Invalid input");
    return;
  }

  uint64_t CommandHash = HashCode(Command);

  if (CommandHash == HashCode("su")) {
    // Allow !su for LAN connections
    if (!m_ServerName.empty() && !(m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
      ErrorReply("Forbidden");
      return;
    }
    m_Aura->HoldContext(this);
    m_Aura->m_SudoContext = this;
    // TODO(IceSandslash): GetSudoAuthPayload should only be executed after some local input.
    m_Aura->m_SudoAuthPayload = m_Aura->GetSudoAuthPayload(Payload);
    m_Aura->m_SudoExecCommand = Payload;
    SendReply("Sudo command requested. See Aura's console for further steps.");
    Print("[AURA] Sudoer " + GetUserAttribution() + " requests command \"" + cmdToken + Payload + "\"");
    if (m_SourceRealm && m_FromWhisper) {
      Print("[AURA] Confirm from [" + m_ServerName + "] with: \"/w " + m_SourceRealm->GetLoginName() + " " + cmdToken + "sudo " + m_Aura->m_SudoAuthPayload + "\"");
    } else if (m_IRC || m_DiscordAPI) {
      Print("[AURA] Confirm from [" + m_ServerName + "] with: \"" + cmdToken + "sudo " + m_Aura->m_SudoAuthPayload + "\"");
    } else {
      Print("[AURA] Confirm from the game client with: \"" + cmdToken + "sudo " + m_Aura->m_SudoAuthPayload + "\"");
    }
    return;
  }

  if (CommandHash == HashCode("sudo")) {
    optional<pair<string, string>> RunOverride = CheckSudo(Payload);
    if (RunOverride.has_value()) {
      Command = RunOverride.value().first;
      Payload = RunOverride.value().second;
      CommandHash = HashCode(Command);
    } else {
      if (m_Aura->m_SudoExecCommand.empty()) {
        Print("[AURA] " + GetUserAttribution() + " sent command [" + cmdToken + command + "] with payload [" + payload + "], but " + cmdToken + "sudo was not requested.");
      } else {
        Print("[AURA] " + GetUserAttribution() + " failed sudo authentication.");
      }
      ErrorReply("Sudo check failure.");
      return;
    }
  }

  if (m_TargetGame && m_TargetGame->m_Locked && 0 == (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
    LogStream(*m_Output, m_TargetGame->GetLogPrefix() + "Command ignored, the game is locked");
    ErrorReply("Only the game owner and root admins can run game commands when the game is locked.");
    return;
  }

  if (Payload.empty()) {
    LogStream(*m_Output, GetUserAttributionPreffix() + "sent command [" + cmdToken + command + "]");
  } else {
    LogStream(*m_Output, GetUserAttributionPreffix() + "sent command [" + cmdToken + command + "] with payload [" + payload + "]");
  }

  /*********************
   * NON ADMIN COMMANDS *
   *********************/

  switch (CommandHash)
  {
    //
    // !ABOUT
    //

    case HashCode("version"):
    case HashCode("about"): {
      SendReply("Aura " + m_Aura->m_Version + " is a permissive-licensed open source project. Say hi at <" + m_Aura->m_IssuesURL + ">");
      break;
    }

    //
    // !HELP
    //

    case HashCode("git"):
    case HashCode("help"): {
      SendReply("Aura " + m_Aura->m_Version + " is a permissive-licensed open source project. Read more at <" + m_Aura->m_RepositoryURL + ">");
      break;
    }

    //
    // !SC
    //
    case HashCode("sc"): {
      SendReply("To verify your identity and use commands in game rooms, whisper me two letters: sc");
      break;
    }

    //
    // !KEY
    //
    case HashCode("key"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to inspect raw slots.");
        break;
      }

      SendReply(
        "Name <<" + m_TargetGame->GetGameName() + ">> | ID " +
        to_string(m_TargetGame->GetHostCounter()) + " (" + ToHexString(m_TargetGame->GetHostCounter()) + ")" + " | Key " +
        to_string(m_TargetGame->GetEntryKey()) + " (" + ToHexString(m_TargetGame->GetEntryKey()) + ")"
      );
      break;
    }

    //
    // !SLOT
    //

    case HashCode("slot"):
    {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to inspect raw slots.");
        break;
      }

      uint8_t SID = 0xFF;
      CGamePlayer* targetPlayer = nullptr;
      if (!ParsePlayerOrSlot(Payload, SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "slot <PLAYER>");
        break;
      }
      const CGameSlot* slot = m_TargetGame->InspectSlot(SID);
      if (targetPlayer) {
        SendReply("Player " + targetPlayer->GetName() + " (slot #" + ToDecString(SID + 1) + ") = " + ByteArrayToDecString(slot->GetByteArray()));
      } else {
        SendReply("Slot #" + ToDecString(SID + 1) + " = " + ByteArrayToDecString(slot->GetByteArray()));
      }
      break;
    }

    //
    // !CHECKME
    // !CHECK <PLAYER>
    //

    case HashCode("checkme"):
    case HashCode("check"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror()) {
        break;
      }
      CGamePlayer* targetPlayer = GetTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }
      CRealm* targetPlayerRealm = targetPlayer->GetRealm(true);
      bool IsRealmVerified = targetPlayerRealm != nullptr;
      bool IsOwner = targetPlayer->GetIsOwner(nullopt);
      bool IsRootAdmin = IsRealmVerified && targetPlayerRealm->GetIsAdmin(targetPlayer->GetName());
      bool IsAdmin = IsRootAdmin || (IsRealmVerified && targetPlayerRealm->GetIsModerator(targetPlayer->GetName()));
      string SyncStatus;
      if (m_TargetGame->GetGameLoaded()) {
        if (m_TargetGame->m_SyncPlayers[targetPlayer].size() + 1 == m_TargetGame->m_Players.size()) {
          SyncStatus = "Full";
        } else if (m_TargetGame->m_SyncPlayers[targetPlayer].empty()) {
          SyncStatus = "Alone";
        } else {
          SyncStatus = "With: ";
          for (auto& otherPlayer: m_TargetGame->m_SyncPlayers[targetPlayer]) {
            SyncStatus += otherPlayer->GetName() + ", ";
          }
          SyncStatus = SyncStatus.substr(0, SyncStatus.length() - 2);
        }
      }
      string SlotFragment;
      if (m_TargetGame->GetIsLobby()) {
        SlotFragment = "Slot #" + to_string(1 + m_TargetGame->GetSIDFromPID(targetPlayer->GetPID())) + ". ";
      }
      string GProxyFragment;
      if (targetPlayer->GetGProxyExtended()) {
        GProxyFragment = "Extended";
      } else if (targetPlayer->GetGProxyAny()) {
        GProxyFragment = "Yes";
      } else {
        GProxyFragment = "No";
      }
      string IPVersionFragment;
      if (targetPlayer->GetUsingIPv6()) {
        IPVersionFragment = ", IPv6";
      } else {
        IPVersionFragment = ", IPv4";
      }
      SendReply("[" + targetPlayer->GetName() + "]. " + SlotFragment + "Ping: " + targetPlayer->GetDelayText(true) + IPVersionFragment + ", Reconnection: " + GProxyFragment + ", From: " + m_Aura->m_DB->FromCheck(ByteArrayToUInt32(targetPlayer->GetIPv4(), true)) + (m_TargetGame->GetGameLoaded() ? ", Sync: " + SyncStatus : ""));
      SendReply("[" + targetPlayer->GetName() + "]. Realm: " + (targetPlayer->GetRealmHostName().empty() ? "LAN" : targetPlayer->GetRealmHostName()) + ", Verified: " + (IsRealmVerified ? "Yes" : "No") + ", Reserved: " + (targetPlayer->GetIsReserved() ? "Yes" : "No"));
      if (IsOwner || IsAdmin || IsRootAdmin) {
        SendReply("[" + targetPlayer->GetName() + "]. Owner: " + (IsOwner ? "Yes" : "No") + ", Admin: " + (IsAdmin ? "Yes" : "No") + ", Root Admin: " + (IsRootAdmin ? "Yes" : "No"));
      }
      break;
    }

    //
    // !PING
    //

    case HashCode("pingall"):
    case HashCode("ping"):
    case HashCode("p"): {
      // kick players with ping higher than payload if payload isn't empty
      // we only do this if the game hasn't started since we don't want to kick players from a game in progress

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      uint32_t KickedCount = 0;
      optional<uint32_t> KickPing;
      if (!Payload.empty()) {
        if (!m_TargetGame->GetIsLobby()) {
          ErrorReply("Maximum ping may only be set in a lobby.");
          break;
        }
        if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("Not allowed to set maximum ping.");
          break;
        }
        try {
          int64_t Value = stoul(Payload);
          if (Value <= 0 || 0xFFFFFFFF < Value) {
            ErrorReply("Invalid maximum ping [" + Payload + "].");
            break;
          }
          KickPing = static_cast<uint32_t>(Value);
        } catch (...) {
          ErrorReply("Invalid maximum ping [" + Payload + "].");
          break;
        }
      }

      if (KickPing.has_value())
        m_TargetGame->m_AutoKickPing = KickPing.value();

      // copy the m_Players vector so we can sort by descending ping so it's easier to find players with high pings

      vector<CGamePlayer*> SortedPlayers = m_TargetGame->m_Players;
      if (m_TargetGame->GetGameLoaded()) {
        sort(begin(SortedPlayers), end(SortedPlayers), [](const CGamePlayer* a, const CGamePlayer* b) {
          return a->GetNormalSyncCounter() < b->GetNormalSyncCounter();
        });
      } else {
        sort(begin(SortedPlayers), end(SortedPlayers), [](const CGamePlayer* a, const CGamePlayer* b) {
          return a->GetPing() > b->GetPing();
        });
      }
      vector<string> pingsText;
      uint32_t maxPing = 0;

      for (auto i = begin(SortedPlayers); i != end(SortedPlayers); ++i) {
        pingsText.push_back((*i)->GetName() + ": " + (*i)->GetDelayText(false));
        size_t numPings = (*i)->GetNumPings();
        if (numPings == 0) continue;
        uint32_t ping = (*i)->GetPing();
        if (ping > maxPing) maxPing = ping;
        if (m_TargetGame->GetIsLobby() && !(*i)->GetIsReserved()) {
          if (KickPing.has_value() && ping > m_TargetGame->m_AutoKickPing) {
            (*i)->SetKickByTime(GetTime() + 5);
            (*i)->SetLeftReason("was kicked for excessive ping " + to_string(ping) + " > " + to_string(m_TargetGame->m_AutoKickPing));
            (*i)->SetLeftCode(PLAYERLEAVE_LOBBY);
            ++KickedCount;
          } else if ((*i)->GetKickQueued() && ((*i)->GetMapReady() || (*i)->GetDownloadStarted())) {
            (*i)->SetKickByTime(0);
            (*i)->SetLeftReason(emptyString);
          }
        }
      }

      SendReply(JoinVector(pingsText, false), !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);

      if (KickedCount > 0) {
        SendAll("Kicking " + to_string(KickedCount) + " players with pings greater than " + to_string(m_TargetGame->m_AutoKickPing) + "...");
      }
      if (0 < maxPing && maxPing < m_TargetGame->m_Latency && REFRESH_PERIOD_MIN < m_TargetGame->m_Latency) {
        SendAll("HINT: Using ping equalizer at " + to_string(m_TargetGame->m_Latency) + "ms. Decrease it with " + cmdToken + "latency [VALUE]");
      }

      break;
    }

    //
    // !CHECKRACE
    //

    case HashCode("checkrace"): {
      if (!m_TargetGame)
        break;

      vector<const CGamePlayer*> players = m_TargetGame->GetPlayers();
      if (players.empty()) {
        ErrorReply("No players found.");
        break;
      }

      vector<string> races;
      for (const auto& player: players) {
        const CGameSlot* slot = m_TargetGame->InspectSlot(m_TargetGame->GetSIDFromPID(player->GetPID()));
        uint8_t race = slot->GetRaceFixed();
        races.push_back(player->GetName() + ": " + GetRaceName(race));
      }
      SendReply(JoinVector(races, false));
      break;
    }

    //
    // !STATSDOTA
    // !STATS
    //

    case HashCode("statsdota"):
    case HashCode("stats"): {
      if (
        !(m_SourceGame && m_SourceGame->GetIsLobby()) &&
        !CheckPermissions(m_Config->m_StatsPermissions, COMMAND_PERMISSIONS_VERIFIED)
      ) {
        ErrorReply("Not allowed to look up stats.");
        break;
      }
      CGamePlayer* targetPlayer = GetTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }
      const bool isDota = CommandHash == HashCode("statsdota") || m_TargetGame->GetClientFileName().find("DotA") != string::npos;
      const bool isUnverified = targetPlayer->GetRealm(false) != nullptr && !targetPlayer->IsRealmVerified();
      string targetIdentity = "[" + targetPlayer->GetName() + "]";
      if (isUnverified) targetIdentity += " (unverified)";

      if (isDota) {
        CDBDotAPlayerSummary* DotAPlayerSummary = m_Aura->m_DB->DotAPlayerSummaryCheck(targetPlayer->GetName());
        if (!DotAPlayerSummary) {
          SendReply(targetIdentity + " has no registered DotA games.");
          break;
        }
        const string summaryText = (
          targetIdentity +
          " - " + to_string(DotAPlayerSummary->GetTotalGames()) + " games (W/L: " +
          to_string(DotAPlayerSummary->GetTotalWins()) + "/" + to_string(DotAPlayerSummary->GetTotalLosses()) +
          ") Hero K/D/A: " + to_string(DotAPlayerSummary->GetTotalKills()) +
          "/" + to_string(DotAPlayerSummary->GetTotalDeaths()) +
          "/" + to_string(DotAPlayerSummary->GetTotalAssists()) +
          " (" + to_string(DotAPlayerSummary->GetAvgKills()) +
          "/" + to_string(DotAPlayerSummary->GetAvgDeaths()) +
          "/" + to_string(DotAPlayerSummary->GetAvgAssists()) +
          ") Creep K/D/N: " + to_string(DotAPlayerSummary->GetTotalCreepKills()) +
          "/" + to_string(DotAPlayerSummary->GetTotalCreepDenies()) +
          "/" + to_string(DotAPlayerSummary->GetTotalNeutralKills()) +
          " (" + to_string(DotAPlayerSummary->GetAvgCreepKills()) +
          "/" + to_string(DotAPlayerSummary->GetAvgCreepDenies()) +
          "/" + to_string(DotAPlayerSummary->GetAvgNeutralKills()) +
          ") T/R/C: " + to_string(DotAPlayerSummary->GetTotalTowerKills()) +
          "/" + to_string(DotAPlayerSummary->GetTotalRaxKills()) +
          "/" + to_string(DotAPlayerSummary->GetTotalCourierKills())
        );
        SendReply(summaryText);
        delete DotAPlayerSummary;
      } else {
        CDBGamePlayerSummary* GamePlayerSummary = m_Aura->m_DB->GamePlayerSummaryCheck(targetPlayer->GetName());
        if (!GamePlayerSummary) {
          SendReply(targetIdentity + " has no registered games.");
          break;
        }
        const string summaryText = (
          targetIdentity + " has played " +
          to_string(GamePlayerSummary->GetTotalGames()) + " games with this bot. Average loading time: " +
          to_string(GamePlayerSummary->GetAvgLoadingTime()) + " seconds. Average stay: " +
          to_string(GamePlayerSummary->GetAvgLeftPercent()) + " percent"
        );
        SendReply(summaryText);
        delete GamePlayerSummary;
      }

      break;
    }

    //
    // !GETPLAYERS
    // !GETOBSERVERS
    //

    case HashCode("getplayers"):
    case HashCode("getobservers"): {
      if (!Payload.empty()) {
        m_TargetGame = GetTargetGame(Payload);
      } else if (m_SourceGame) {
        m_TargetGame = m_SourceGame;
      } else {
        UseImplicitHostedGame();
      }

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (m_TargetGame->m_DisplayMode == GAME_PRIVATE && !m_Player) {
        if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("This game is private.");
          break;
        }
      }
      string Players = PlayersToNameListString(m_TargetGame->GetPlayers());
      string Observers = PlayersToNameListString(m_TargetGame->GetObservers());
      if (Players.empty() && Observers.empty()) {
        SendReply("Nobody is in the game.");
        break;
      }
      string PlayersFragment = Players.empty() ? "No players. " : "Players: " + Players + ". ";
      string ObserversFragment = Observers.empty() ? "No observers" : "Observers: " + Observers + ".";
      SendReply(PlayersFragment + ObserversFragment);
      break;
    }

    //
    // !VOTEKICK
    //

    case HashCode("votekick"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (m_TargetGame->GetCountDownStarted() && !m_TargetGame->GetGameLoaded())
        break;

      if (Payload.empty()){
        ErrorReply("Usage: " + cmdToken + "votekick <PLAYERNAME>");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        break;
      }

      if (!m_TargetGame->m_KickVotePlayer.empty()) {
        ErrorReply("Unable to start votekick. Another votekick is in progress");
        break;
      }
      if (m_TargetGame->m_Players.size() <= 2) {
        ErrorReply("Unable to start votekick. There aren't enough players in the game for a votekick");
        break;
      }
      if (targetPlayer->GetIsReserved()) {
        ErrorAll("Unable to votekick player [" + targetPlayer->GetName() + "]. That player is reserved and cannot be votekicked");
        break;
      }

      m_TargetGame->m_KickVotePlayer      = targetPlayer->GetName();
      m_TargetGame->m_StartedKickVoteTime = GetTime();

      for (auto& it : m_TargetGame->m_Players)
        it->SetKickVote(false);

      SendReply("Votekick against player [" + m_TargetGame->m_KickVotePlayer + "] started by player [" + m_FromName + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      if (m_Player && m_Player != targetPlayer) {
        m_Player->SetKickVote(true);
        SendAll("Player [" + m_Player->GetName() + "] voted to kick player [" + m_TargetGame->m_KickVotePlayer + "]. " + to_string(static_cast<uint32_t>(ceil(static_cast<float>(m_TargetGame->GetNumHumanPlayers() - 1) * static_cast<float>(m_TargetGame->m_VoteKickPercentage) / 100)) - 1) + " more votes are needed to pass");
      }
      SendAll("Type " + cmdToken + "yes or " + cmdToken + "no to vote.");

      break;
    }

    //
    // !YES
    //

    case HashCode("yes"): {
      if (!m_Player || m_TargetGame->m_KickVotePlayer.empty() || m_Player->GetKickVote().value_or(false))
        break;

      uint32_t VotesNeeded = static_cast<uint32_t>(ceil(static_cast<float>(m_TargetGame->GetNumHumanPlayers() - 1) * static_cast<float>(m_TargetGame->m_VoteKickPercentage) / 100));
      m_Player->SetKickVote(true);
      m_TargetGame->SendAllChat("Player [" + m_Player->GetName() + "] voted for kicking player [" + m_TargetGame->m_KickVotePlayer + "]. " + to_string(VotesNeeded) + " affirmative votes required to pass");
      m_TargetGame->CountKickVotes();
      break;
    }

    //
    // !NO
    //

    case HashCode("no"): {
      if (!m_Player || m_TargetGame->m_KickVotePlayer.empty() || !m_Player->GetKickVote().value_or(true))
        break;

      m_Player->SetKickVote(false);
      m_TargetGame->SendAllChat("Player [" + m_Player->GetName() + "] voted against kicking player [" + m_TargetGame->m_KickVotePlayer + "].");
      break;
    }

    //
    // !INVITE
    //

    case HashCode("invite"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetCountDownStarted()) {
        // Intentionally allows !invite to fake (mirror) lobbies.
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "invite <PLAYERNAME>@<REALM>");
        break;
      }

      const string MapPath = m_TargetGame->GetMap()->GetClientPath();
      size_t LastSlash = MapPath.rfind('\\');

      string inputName = Payload;
      string inputRealm;
      string::size_type RealmStart = inputName.find('@');
      if (RealmStart != string::npos) {
        inputRealm = TrimString(inputName.substr(RealmStart + 1));
        inputName = TrimString(inputName.substr(0, RealmStart));
      }

      if (inputName.empty()) {
        ErrorReply("Usage: " + cmdToken + "invite <PLAYERNAME>@<REALM>");
        break;
      }

      CRealm* matchingRealm = GetTargetRealmOrCurrent(inputRealm);
      if (!matchingRealm) {
        if (inputRealm.empty()) {
          ErrorReply("Usage: " + cmdToken + "invite <PLAYERNAME>@<REALM>");
        } else {
          ErrorReply(inputRealm + " is not a valid realm.");
        }
        break;
      }

      // Name of sender and receiver should be included in the message,
      // so that they can be checked in successful whisper acks from the server (CBNETProtocol::EID_WHISPERSENT)
      // Note that the server doesn't provide any way to recognize whisper targets if the whisper fails.
      if (LastSlash != string::npos && LastSlash <= MapPath.length() - 6) {
        m_ActionMessage = inputName + ", " + m_FromName + " invites you to play [" + MapPath.substr(LastSlash + 1) + "]. Join game \"" + m_TargetGame->m_GameName + "\"";
      } else {
        m_ActionMessage = inputName + ", " + m_FromName + " invites you to join game \"" + m_TargetGame->m_GameName + "\"";
      }

      matchingRealm->QueueWhisper(m_ActionMessage, inputName, this, true);
      break;
    }

    //
    // !FLIP
    //

    case HashCode("coin"):
    case HashCode("coinflip"):
    case HashCode("flip"): {
      double chance = 0.5;
      if (!Payload.empty()) {
        double chancePercent;
        try {
          chancePercent = stod(Payload);
        } catch (...) {
          ErrorReply("Usage: " + cmdToken + "flip <CHANCE%>");
          break;
        }
        if (chancePercent < 0. || chancePercent > 100.) {
          ErrorReply("Usage: " + cmdToken + "flip <CHANCE%>");
          break;
        }
        chance = chancePercent / 100.;
      }

      // Max 5pm

      std::random_device rd;
      std::mt19937 gen(rd());
      std::bernoulli_distribution bernoulliDist(chance);
      bool result = bernoulliDist(gen);

      SendReply(m_FromName + " flipped a coin and got " + (result ? "heads" : "tails") + ".", !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !ROLL
    //

    case HashCode("roll"): {
      size_t diceStart = Payload.find('d');
      uint16_t rollFaces = 0;
      uint8_t rollCount = 1;

      string rawRollCount = diceStart == string::npos ? "1" : Payload.substr(0, diceStart);
      string rawRollFaces = Payload.empty() ? "100" : diceStart == string::npos ? Payload : Payload.substr(diceStart + 1);

      try {
        rollCount = static_cast<uint8_t>(stoi(rawRollCount));
        rollFaces = static_cast<uint16_t>(stoi(rawRollFaces));
      } catch (...) {
        ErrorReply("Usage: " + cmdToken + "roll <FACES>");
        break;
      }

      if (!(0 < rollCount && rollCount <= 8)) {
        ErrorReply("Invalid dice count: " + to_string(rollCount));
        break;
      }
      if (!(0 < rollFaces && rollFaces <= 10000)) {
        ErrorReply("Invalid dice faces: " + to_string(rollFaces));
        break;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, rollFaces);

      vector<string> gotRolls;
      for (uint8_t i = 1; i <= rollCount; ++i) {
        gotRolls.push_back(to_string(distribution(gen)));
      }

      if (Payload.empty()) {
        SendReply(m_FromName + " rolled " + gotRolls[0] + ".", !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      } else {
        SendReply(m_FromName + " rolled " + to_string(rollCount) + "d" + to_string(rollFaces) + ". Got: " + JoinVector(gotRolls, false) + ".", !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      }
      break;
    }

    //
    // !PICK
    //

    case HashCode("pick"): {
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "pick <OPTION> , <OPTION> , <OPTION>, ...");
        break;
      }

      vector<string> options = SplitArgs(Payload, 1, 24);
      if (options.empty()) {
        ErrorReply("Empty options list.");
        break;
      }
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(options.size()));

      string randomPick = options[distribution(gen) - 1];
      SendReply("Randomly picked: " + randomPick, !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PICKRACE
    //

    case HashCode("pickrace"): {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(0, 3);
      const uint8_t race = 1 << distribution(gen);
      string randomPick = GetRaceName(race);
      SendReply("Randomly picked: " + randomPick + " race", !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PICKPLAYER
    //

    case HashCode("pickplayer"): {
      if (!m_TargetGame)
        break;

      vector<const CGamePlayer*> players = m_TargetGame->GetPlayers();
      if (players.empty()) {
        ErrorReply("No players found.");
        break;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(players.size()));
      const CGamePlayer* pickedPlayer = players[distribution(gen) - 1];
      string randomPick = pickedPlayer->GetName();
      SendReply("Randomly picked: " + randomPick, !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PICKOBS
    //

    case HashCode("pickobserver"):
    case HashCode("pickobs"): {
      if (!m_TargetGame)
        break;

      vector<const CGamePlayer*> players = m_TargetGame->GetObservers();
      if (players.empty()) {
        ErrorReply("No observers found.");
        break;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(players.size()));
      const CGamePlayer* pickedPlayer = players[distribution(gen) - 1];
      string randomPick = pickedPlayer->GetName();
      SendReply("Randomly picked: " + randomPick, !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    case HashCode("twrpg"): {
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "twrpg <NOMBRE>");
        break;
      }

      string name = Payload;
      uint8_t matchType = m_Aura->m_DB->FindData(MAP_TYPE_TWRPG, MAP_DATA_TYPE_ANY, name, false);
      if (matchType == MAP_DATA_TYPE_NONE) {
        vector<string> words = Tokenize(name, ' ');
        if (words.size() <= 1) {
          ErrorReply("[" + Payload + "] not found.");
          break;
        }
        string intermediate = words[0];
        words[0] = words[words.size() - 1];
        words[words.size() - 1] = intermediate;
        name = JoinVector(words, " ", false);
        matchType = m_Aura->m_DB->FindData(MAP_TYPE_TWRPG, MAP_DATA_TYPE_ANY, name, false);
        if (matchType == MAP_DATA_TYPE_NONE) {
          ErrorReply("[" + Payload + "] not found.");
          break;
        }
      }
      if (matchType == MAP_DATA_TYPE_ANY) {
        ErrorReply("Did you mean any of these? " + name);
        break;
      }

      vector<string> descriptionLines = m_Aura->m_DB->GetDescription(MAP_TYPE_TWRPG, matchType, name);
      if (descriptionLines.empty()) {
        ErrorReply("Item description not found.");
        break;
      }

      vector<string> replyLines = JoinReplyListCompact(descriptionLines);
      for (const auto& line : replyLines) {
        SendReply(line);
      }
      break;
    }

    /*****************
     * ADMIN COMMANDS *
     ******************/
     
    //
    // !FROM
    //

    case HashCode("where"):
    case HashCode("from"):
    case HashCode("f"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_CommonBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        ErrorReply("Not allowed to check players geolocalization.");
        break;
      }

      string Froms;

      for (auto i = begin(m_TargetGame->m_Players); i != end(m_TargetGame->m_Players); ++i) {
        // we reverse the byte order on the IP because it's stored in network byte order

        Froms += (*i)->GetName();
        Froms += ": (";
        Froms += m_Aura->m_DB->FromCheck(ByteArrayToUInt32((*i)->GetIPv4(), true));
        Froms += ")";

        if (i != end(m_TargetGame->m_Players) - 1)
          Froms += ", ";
      }

      SendReply(Froms, !m_Player || m_Player->GetCanUsePublicChat() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !BANLAST
    //

    case HashCode("banlast"):
    case HashCode("bl"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (!m_TargetGame->m_DBBanLast) {
        ErrorReply("No ban candidates stored.");
        break;
      }
      if (m_Aura->m_Realms.empty()) {
        ErrorReply("No realms joined.");
        break;
      }

      m_Aura->m_DB->BanAdd(m_TargetGame->m_DBBanLast->GetServer(), m_TargetGame->m_DBBanLast->GetName(), m_FromName, Payload);
      SendAll("Player [" + m_TargetGame->m_DBBanLast->GetName() + "] was banned by player [" + m_FromName + "] on server [" + m_TargetGame->m_DBBanLast->GetServer() + "]");
      break;
    }

    //
    // !CLOSE (close slot)
    //

    case HashCode("close"):
    case HashCode("c"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        if (!m_TargetGame->CloseSlot()) {
          ErrorReply("No slots are open.");
        } else {
          SendReply("One slot closed.");
        }
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, m_Aura->m_MaxSlots);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "c <SLOTNUM>");
        break;
      }

      vector<string> failedSlots;
      for (auto& elem : Args) {
        if (elem == 0 || elem > m_Aura->m_MaxSlots) {
          ErrorReply("Usage: " + cmdToken + "c <SLOTNUM>");
          break;
        }
        uint8_t SID = static_cast<uint8_t>(elem) - 1;
        if (!m_TargetGame->CloseSlot(SID, CommandHash == HashCode("close"))) {
          failedSlots.push_back(to_string(elem));
        }
      }
      if (Args.size() == failedSlots.size()) {
        ErrorReply("Failed to close slot.");
      } else if (failedSlots.empty()) {
        if (Args.size() == 1) {
          SendReply("Closed slot #" + to_string(Args[0]) + ".");
        } else {
          SendReply("Closed " + to_string(Args.size()) + " slot(s).");
        }
      } else {
        ErrorReply("Slot(s) " + JoinVector(failedSlots, false) + " cannot be closed.");
      }
      break;
    }

    //
    // !END
    //

    case HashCode("end"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot end the game.");
        break;
      }

      LogStream(*m_Output, m_TargetGame->GetLogPrefix() + "is over (admin ended game) [" + m_FromName + "]");
      m_TargetGame->SendAllChat("Ending the game.");
      m_TargetGame->StopPlayers("was disconnected (admin ended game)", false);
      break;
    }

    //
    // !URL
    //

    case HashCode("url"):
    case HashCode("link"): {
      if (!m_TargetGame)
        break;

      const string TargetUrl = TrimString(Payload);

      if (TargetUrl.empty()) {
        if (m_TargetGame->GetMapSiteURL().empty()) {
          SendAll("Download URL unknown");
        } else {
          SendAll("Visit  <" + m_TargetGame->GetMapSiteURL() + "> to download [" + m_TargetGame->GetClientFileName() + "]");
        }
        break;
      }

      if (!m_TargetGame->GetIsLobby())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      m_TargetGame->SetMapSiteURL(TargetUrl);
      SendAll("Download URL set to [" + TargetUrl + "]");
      break;
    }

    //
    // !GPROXY
    //

    case HashCode("reconnect"):
    case HashCode("gproxy"): {
      SendReply("Protect against disconnections using GProxyDLL, a Warcraft III plugin. See: <" + m_Aura->m_Net->m_Config->m_AnnounceGProxySite + ">");
      break;
    }

    //
    // !MODE
    //

    
    case HashCode("hcl"):
    case HashCode("mode"): {
      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (Payload.empty()) {
        SendReply("Game mode (HCL) is [" + m_TargetGame->m_HCLCommandString + "]");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (Payload.size() > m_TargetGame->m_Slots.size()) {
        ErrorReply("Unable to set mode (HCL) because it's too long - it must not exceed the amount of occupied game slots");
        break;
      }

      const string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";
      size_t IllegalIndex = Payload.find_first_not_of(HCLChars);

      if (IllegalIndex != string::npos) {
        ErrorReply("Unable to set mode (HCL) because it contains invalid character [" + cmdToken + "]");
        break;
      }

      m_TargetGame->m_HCLCommandString = Payload;
      SendAll("Game mode (HCL) set to [" + m_TargetGame->m_HCLCommandString + "]");
      break;
    }

    //
    // !HOLD (hold a slot for someone)
    //

    case HashCode("hold"): {
      UseImplicitHostedGame();
      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, m_Aura->m_MaxSlots);

      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "hold <PLAYER1> , <PLAYER2>, ...");
        break;
      }

      for (auto& PlayerName: Args) {
        if (PlayerName.empty())
          continue;
        m_TargetGame->AddToReserved(PlayerName);
      }

      SendAll("Added player(s) to the hold list: " + JoinVector(Args, false));
      break;
    }

    //
    // !UNHOLD
    //

    case HashCode("unhold"): {
      UseImplicitHostedGame();
      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, m_Aura->m_MaxSlots);

      if (Args.empty()) {
        m_TargetGame->RemoveAllReserved();
        SendAll("Cleared the reservations list.");
        break;
      }

      for (auto& PlayerName: Args) {
        if (PlayerName.empty())
          continue;
        m_TargetGame->RemoveFromReserved(PlayerName);
      }
      SendAll("Removed player(s) from the reservations list: " + JoinVector(Args, false));
      break;
    }

    //
    // !KICK (kick a player)
    //

    case HashCode("closekick"):
    case HashCode("ckick"):
    case HashCode("kick"):
    case HashCode("k"): {
      UseImplicitHostedGame();
      if (!m_TargetGame || m_TargetGame->GetIsMirror() || (m_TargetGame->GetCountDownStarted() && !m_TargetGame->GetGameLoaded()))
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "kick <PLAYERNAME>");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        break;
      }

      targetPlayer->SetDeleteMe(true);
      targetPlayer->SetLeftReason("was kicked by player [" + m_FromName + "]");

      if (m_TargetGame->GetIsLobby())
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
      else
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOST);

      if (m_TargetGame->GetIsLobby()) {
        bool KickAndClose = CommandHash == HashCode("ckick") || CommandHash == HashCode("closekick");
        if (KickAndClose && !m_TargetGame->GetIsRestored()) {
          m_TargetGame->CloseSlot(m_TargetGame->GetSIDFromPID(targetPlayer->GetPID()), false);
        } else {
          m_TargetGame->OpenSlot(m_TargetGame->GetSIDFromPID(targetPlayer->GetPID()), false);
        }
      }

      break;
    }

    //
    // !LATENCY (set game latency)
    //

    case HashCode("latency"): {
      UseImplicitHostedGame();      
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (Payload.empty()) {
        SendReply("The game latency is " + to_string(m_TargetGame->m_Latency) + " ms");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot modify the game latency.");
        break;
      }

      string lower = Payload;
      transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lower == "default" || lower == "reset") {
        m_TargetGame->ResetLatency();
        SendAll("Latency settings reset to default.");
        break;
      } else if (lower == "ignore" || lower == "bypass" || lower == "normal") {
        if (!m_TargetGame->GetGameLoaded()) {
          ErrorReply("This command must be used after the game has loaded.");
          break;
        }
        m_TargetGame->NormalizeSyncCounters();
        SendAll("Ignoring lagging players. (They may not be able to control their units.)");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "latency [REFRESH]");
        ErrorReply("Usage: " + cmdToken + "latency [REFRESH], [TOLERANCE]");
        break;
      }

      if (Args[0] <= 0 || Args[0] > 60000) {
        // WC3 clients disconnect after a minute without network activity.
        ErrorReply("Invalid game refresh period [" + to_string(Args[0]) + "ms].");
        break;
      }

      double refreshTime = static_cast<double>(Args[0]);
      optional<double> tolerance;
      if (Args.size() >= 2) {
        tolerance = static_cast<double>(Args[1]);
        if (tolerance.value() <= LAG_TOLERANCE_MIN_TIME) {
          ErrorReply("Minimum spike tolerance is " + to_string(LAG_TOLERANCE_MIN_TIME) + " ms.");
          break;
        }
        if (tolerance.value() >= LAG_TOLERANCE_MAX_TIME) {
          ErrorReply("Maximum spike tolerance is " + to_string(LAG_TOLERANCE_MAX_TIME) + " ms.");
          break;
        }
      }

      if (refreshTime < REFRESH_PERIOD_MIN) {
        refreshTime = REFRESH_PERIOD_MIN;
      } else if (refreshTime > REFRESH_PERIOD_MAX) {
        refreshTime = REFRESH_PERIOD_MAX;
      }

      const double oldRefresh = m_TargetGame->m_Latency;
      const double oldSyncLimit = m_TargetGame->m_SyncLimit;
      const double oldSyncLimitSafe = m_TargetGame->m_SyncLimitSafe;

      double syncLimit, syncLimitSafe;
      double resolvedTolerance = (
        tolerance.has_value() ?
        tolerance.value() :
        oldSyncLimit * oldRefresh
      );
      syncLimit = resolvedTolerance / refreshTime;
      syncLimitSafe = oldSyncLimitSafe * syncLimit / oldSyncLimit;
      if (syncLimit < 4) syncLimit = 4;
      if (syncLimitSafe < syncLimit / 2) syncLimitSafe = syncLimit / 2;
      if (syncLimitSafe < 1) syncLimitSafe = 1;

      m_TargetGame->m_Latency = static_cast<uint16_t>(refreshTime);
      m_TargetGame->m_SyncLimit = static_cast<uint16_t>(syncLimit);
      m_TargetGame->m_SyncLimitSafe = static_cast<uint16_t>(syncLimitSafe);

      const uint32_t finalToleranceMilliseconds = (
        static_cast<uint32_t>(m_TargetGame->m_Latency) *
        static_cast<uint32_t>(m_TargetGame->m_SyncLimit)
      );

      if (refreshTime == REFRESH_PERIOD_MIN) {
        SendAll("Game will be updated at the fastest rate (every " + to_string(m_TargetGame->m_Latency) + " ms)");
      } else if (refreshTime == REFRESH_PERIOD_MAX) {
        SendAll("Game will be updated at the slowest rate (every " + to_string(m_TargetGame->m_Latency) + " ms)");
      } else {
        SendAll("Game will be updated with a delay of " + to_string(m_TargetGame->m_Latency) + "ms.");
      }
      SendAll("Spike tolerance set to " + to_string(finalToleranceMilliseconds) + "ms.");
      break;
    }

    //
    // !OPEN (open slot)
    //

    case HashCode("open"):
    case HashCode("o"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        if (!m_TargetGame->OpenSlot()) {
          ErrorReply("Cannot open further slots.");
        } else {
          SendReply("One slot opened.");
        }
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, m_Aura->m_MaxSlots);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "o <SLOTNUM>");
        break;
      }

      vector<string> failedSlots;
      for (auto& elem : Args) {
        if (elem == 0 || elem > m_Aura->m_MaxSlots) {
          ErrorReply("Usage: " + cmdToken + "o <SLOTNUM>");
          break;
        }
        const uint8_t SID = static_cast<uint8_t>(elem) - 1;
        const CGameSlot* slot = m_TargetGame->GetSlot(SID);
        if (!slot || slot->GetSlotStatus() == SLOTSTATUS_OPEN) {
          failedSlots.push_back(to_string(elem));
          continue;
        }
        if (!m_TargetGame->OpenSlot(SID, CommandHash == HashCode("open"))) {
          failedSlots.push_back(to_string(elem));
        }
      }
      if (Args.size() == failedSlots.size()) {
        ErrorReply("Failed to open slot.");
      } else if (failedSlots.empty()) {
        if (Args.size() == 1) {
          SendReply("Opened slot #" + to_string(Args[0]) + ".");
        } else {
          SendReply("Opened " + to_string(Args.size()) + " slot(s).");
        }
      } else {
        ErrorReply("Slot(s) " + JoinVector(failedSlots, false) + " cannot be opened.");
      }
      break;
    }

    //
    // !PUB (create or recreate as public game)
    // !PRIV (create or recreate as private game)
    //

    case HashCode("priv"):
    case HashCode("pub"): {
      if (m_TargetGame) {
        if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted()) {
          break;
        }
        if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("You are not the game owner, and therefore cannot rehost the game.");
          break;
        }
      } else {
        if (!CheckPermissions(m_Config->m_HostPermissions, COMMAND_PERMISSIONS_ADMIN)) {
          ErrorReply("Not allowed to host games.");
          break;
        }
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
          break;
        }
        if (Payload.empty()) {
          ErrorReply("Usage: " + cmdToken + "pub <GAMENAME>");
          ErrorReply("Usage: " + cmdToken + "priv <GAMENAME>");
          break;
        }
      }

      if (m_SourceRealm && m_Aura->m_GameVersion != m_SourceRealm->GetGameVersion() &&
        find(m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.begin(), m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end(), m_SourceRealm->GetGameVersion()) == m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end()
        && !GetIsSudo()) {
        ErrorReply("Hosting games on v1." + to_string(m_SourceRealm->GetGameVersion()) + " is disabled.");
        break;
      }

      if (Payload.length() >= 31) {
        ErrorReply("Unable to create game [" + Payload + "]. The game name is too long (the maximum is 31 characters)");
        break;
      }

      if (m_TargetGame) {
        SendReply("Trying to rehost with name [" + Payload + "].", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      }

      bool IsPrivate = CommandHash == HashCode("priv");
      if (m_TargetGame) {
        m_TargetGame->m_DisplayMode  = IsPrivate ? GAME_PRIVATE : GAME_PUBLIC;
        m_TargetGame->m_GameName     = Payload;
        m_TargetGame->m_HostCounter  = m_Aura->NextHostCounter();
        m_TargetGame->m_RealmRefreshError = false;

        for (auto& realm : m_Aura->m_Realms) {
          if (m_TargetGame->m_IsMirror && realm->GetIsMirror()) {
            continue;
          }
          if (!realm->GetLoggedIn()) {
            continue;
          }
          if (m_TargetGame->m_RealmsExcluded.find(realm->GetServer()) != m_TargetGame->m_RealmsExcluded.end()) {
            continue;
          }

          // unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
          // this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
          // we assume this won't happen very often since the only downside is a potential false positive

          realm->QueueGameUncreate();
          realm->SendEnterChat();

          if (!realm->GetPvPGN())
            realm->SendEnterChat();
        }

        m_TargetGame->m_CreationTime = m_TargetGame->m_LastRefreshTime = GetTime();
      } else {
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
          break;
        }
        if (m_Aura->m_CurrentLobby) {
          ErrorReply("Already hosting a game.");
          break;
        }
        m_Aura->m_GameSetup->SetContext(this);
        m_Aura->m_GameSetup->SetBaseName(Payload);
        m_Aura->m_GameSetup->SetDisplayMode(IsPrivate ? GAME_PRIVATE : GAME_PUBLIC);

        m_Aura->m_GameSetup->SetOwner(m_FromName, m_SourceRealm);
        if (m_SourceRealm) {
          m_Aura->m_GameSetup->SetCreator(m_FromName, m_SourceRealm);
        } else if (m_IRC) {
          m_Aura->m_GameSetup->SetCreator(m_FromName, m_IRC);
        } else if (m_DiscordAPI) {
          m_Aura->m_GameSetup->SetCreator(m_FromName, m_Aura->m_Discord);
        }

        m_Aura->m_GameSetup->RunHost();
      }
      break;
    }

    //
    // !PUBBY (create public game by other player)
    // !PRIVBY (create private game by other player)
    //

    case HashCode("privby"):
    case HashCode("pubby"): {
      if (!CheckPermissions(m_Config->m_HostPermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to host games.");
        break;
      }
      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      string gameName;
      if (Args.empty() || (gameName = TrimString(Args[1])).empty()) {
        ErrorReply("Usage: " + cmdToken + "pubby <OWNER> , <GAMENAME>");
        ErrorReply("Usage: " + cmdToken + "privby <OWNER> , <GAMENAME>");
        break;
      }

      if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
        break;
      }

      if (m_Aura->m_CurrentLobby) {
        ErrorReply("Already hosting a game.");
        break;
      }

      if (m_SourceRealm && m_Aura->m_GameVersion != m_SourceRealm->GetGameVersion() &&
        find(m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.begin(), m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end(), m_SourceRealm->GetGameVersion()) == m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end()
        && !GetIsSudo()) {
        ErrorReply("Hosting games on v1." + to_string(m_SourceRealm->GetGameVersion()) + " is disabled.");
        break;
      }

      bool IsPrivate = CommandHash == HashCode("privby");
      string ownerRealmName;
      string ownerName = Args[0];
      string::size_type realmStart = ownerName.find('@');
      if (realmStart != string::npos) {
        ownerRealmName = TrimString(ownerName.substr(realmStart + 1));
        ownerName = TrimString(ownerName.substr(0, realmStart));
      }
      m_Aura->m_GameSetup->SetContext(this);
      m_Aura->m_GameSetup->SetBaseName(gameName);
      m_Aura->m_GameSetup->SetDisplayMode(IsPrivate ? GAME_PRIVATE : GAME_PUBLIC);
      m_Aura->m_GameSetup->SetCreator(m_FromName, m_SourceRealm);
      m_Aura->m_GameSetup->SetOwner(ownerName, ownerRealmName.empty() ? m_SourceRealm : m_Aura->GetRealmByInputId(ownerRealmName));
      m_Aura->m_GameSetup->RunHost();
      break;
    }

    //
    // !REMAKE
    //

    case HashCode("remake"):
    case HashCode("rmk"): {
      if (!m_TargetGame) {
        ErrorReply("No game is selected.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot remake the game.");
        break;
      }

      if (!m_TargetGame->GetGameLoading() && !m_TargetGame->GetGameLoaded()) {
        ErrorReply("This game has not started yet.");
        break;
      }

      if (m_Aura->m_CurrentLobby) {
        ErrorReply("There is already a lobby hosted.");
        break;
      }

      if (!m_TargetGame->GetIsRemakeable()) {
        ErrorReply("This game cannot be remade.");
        break;
      }
      m_TargetGame->SendAllChat("Please rejoin the remade game <<" + m_TargetGame->GetGameName() + ">>.");
      m_TargetGame->StopPlayers("was disconnected (admin remade game)", true);
      m_TargetGame->SendEveryoneElseLeft();
      m_TargetGame->Remake();
      break;
    }

    //
    // !START
    //

    case HashCode("start"):
    case HashCode("vs"):
    case HashCode("go"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        ErrorReply("You are not allowed to start this game.");
        break;
      }

      uint32_t ConnectionCount = m_TargetGame->GetNumHumanOrFakeControllers();
      if (ConnectionCount == 0) {
        ErrorReply("Not enough players have joined.");
        break;
      }

      bool IsForce = Payload == "force" || Payload == "f";
      if (IsForce && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot forcibly start it.");
        break;
      }

      m_TargetGame->StartCountDown(true, IsForce);
      break;
    }

    //
    // !QUICKSTART
    //

    case HashCode("sn"):
    case HashCode("startn"):
    case HashCode("quickstart"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot quickstart it.");
        break;
      }

      if (GetTicks() - m_TargetGame->m_LastPlayerLeaveTicks < 2000) {
        ErrorReply("Someone left the game less than two seconds ago.");
        break;
      }
      m_TargetGame->StartCountDown(true, true);
      if (m_TargetGame->GetCountDownStarted()) {
        // 500 ms countdown
        m_TargetGame->m_CountDownCounter = 1;
      }
      break;
    }

    //
    // !FREESTART
    //

    case HashCode("freestart"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit the game permissions.");
        break;
      }

      optional<bool> TargetValue;
      if (Payload.empty() || Payload == "on" || Payload == "ON") {
        TargetValue = true;
      } else if (Payload == "off" || Payload == "OFF") {
        TargetValue = false;
      }
      if (!TargetValue.has_value()) {
        ErrorReply("Unrecognized setting [" + Payload + "].");
        break;
      }
      m_TargetGame->m_PublicStart = TargetValue.value();
      if (m_TargetGame->m_PublicStart) {
        SendAll("Anybody may now use the " + cmdToken + "start command.");
      } else {
        SendAll("Only the game owner may now use the " + cmdToken + "start command.");
      }
      break;
    }

    //
    // !AUTOSTART
    // !AS
    //

    case HashCode("addas"):
    case HashCode("as"):
    case HashCode("autostart"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (Payload.empty()) {
        m_TargetGame->SendAllAutoStart();
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "autostart <slots> , <minutes>");
        break;
      }

      uint8_t minReadyControllers = static_cast<uint8_t>(Args[0]);
      uint32_t MinMinutes = 0;
      if (Args.size() >= 2) {
        MinMinutes = Args[1];
      }

      if (minReadyControllers > m_TargetGame->GetMap()->GetMapNumControllers()) {
        ErrorReply("This map does not allow " + to_string(minReadyControllers) + " players.");
        break;
      }

      if (minReadyControllers <= m_TargetGame->m_ControllersReadyCount) {
        // Misuse protection. Make sure the user understands AI players are added.
        ErrorReply("There are already " + to_string(m_TargetGame->m_ControllersReadyCount) + " players ready. Use " + cmdToken + "start instead.");
        break;
      }

      int64_t time = GetTime();
      int64_t dueTime = time + static_cast<int64_t>(MinMinutes) * 60;
      if (dueTime < time) {
        ErrorReply("Failed to set timed start after " + to_string(MinMinutes) + " minutes.");
        break;
      }
      if (CommandHash != HashCode("addas")) {
        m_TargetGame->m_AutoStartRequirements.clear();
      }
      m_TargetGame->m_AutoStartRequirements.push_back(make_pair(minReadyControllers, dueTime));
      m_TargetGame->SendAllAutoStart();
      break;
    }

    //
    // !CLEARAUTOSTART
    //

    case HashCode("clearas"):
    case HashCode("clearautostart"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (m_TargetGame->m_AutoStartRequirements.empty()) {
        ErrorReply("There are no active autostart conditions.");
        break;
      }
      m_TargetGame->m_AutoStartRequirements.clear();
      SendReply("Autostart removed.");
      break;
    }

    //
    // !SWAP (swap slots)
    //

    case HashCode("swap"):
    case HashCode("sw"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      bool onlyDraft = false;
      if (!GetIsSudo()) {
        if ((m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_CUSTOMFORCES) && (onlyDraft = m_TargetGame->GetIsDraftMode())) {
          if (!m_Player || !m_Player->GetIsDraftCaptain()) {
            ErrorReply("Draft mode is enabled. Only draft captains may assign teams.");
            break;
          }
        } else if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "swap <PLAYER> , <PLAYER>");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "swap <PLAYER> , <PLAYER>" + HelpMissingComma(Payload));
        break;
      }

      CGamePlayer* playerOne = nullptr;
      CGamePlayer* playerTwo = nullptr;
      uint8_t slotNumOne = 0xFF;
      uint8_t slotNumTwo = 0xFF;
      if (!ParsePlayerOrSlot(Args[0], slotNumOne, playerOne) || !ParsePlayerOrSlot(Args[1], slotNumTwo, playerTwo)) {
        break;
      }
      if (slotNumOne == slotNumTwo) {
        ErrorReply("Usage: " + cmdToken + "swap <PLAYER> , <PLAYER>");
        break;
      }

      if (onlyDraft) {

        //
        // Swapping is allowed in these circumstances
        // 1. Both slots belong to own authorized team.
        // 2. The following conditions hold simultaneously:
        //     i. One slot belongs to own authorized team, and the other slot is on a different team.
        //    ii. One slot is controlled by an actual player, and the other slot is empty. 
        //

        const CGameSlot* slotOne = m_TargetGame->GetSlot(slotNumOne);
        const CGameSlot* slotTwo = m_TargetGame->GetSlot(slotNumTwo);

        if (slotOne->GetSlotStatus() == SLOTSTATUS_CLOSED || slotTwo->GetSlotStatus() == SLOTSTATUS_CLOSED) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }
        if (slotOne->GetSlotStatus() == SLOTSTATUS_OPEN && slotTwo->GetSlotStatus() == SLOTSTATUS_OPEN) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }

        // Targetting either
        // - OCCUPIED, OCCUPIED
        // - OPEN, OCCUPIED
        // - OCCUPIED, OPEN

        if (slotOne->GetTeam() != slotTwo->GetTeam()) {
          // Ensure user is already captain of the targetted player, or captain of the team we want to move it to.
          if (!m_Player->GetIsDraftCaptainOf(slotOne->GetTeam()) && !m_Player->GetIsDraftCaptainOf(slotTwo->GetTeam())) {
            // Attempting to swap two slots of different unauthorized teams.
            ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
            break;
          }
          // One targetted slot is authorized. The other one belongs to another team.
          // Allow snatching or donating the player, but not trading it for another player.
          // Non-players cannot be transferred.
          if ((playerOne == nullptr) == (playerTwo == nullptr)) {
            ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
            break;
          }
          if (playerOne == nullptr) {
            // slotOne is guaranteed to be occupied by a non-player
            // slotTwo is guaranteed to be occupied by a player
            if (slotOne->GetSlotStatus() != SLOTSTATUS_OPEN) {
              ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
              break;
            }
          } else {
            // slotTwo is guaranteed to be occupied by a non-player
            // slotOne is guaranteed to be occupied by a player
            if (slotTwo->GetSlotStatus() != SLOTSTATUS_OPEN) {
              ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
              break;
            }
          }
        } else if (!m_Player->GetIsDraftCaptainOf(slotOne->GetTeam())) {
          // Both targetted slots belong to the same team, but not to the authorized team.
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        } else {
          // OK. Attempting to swap two slots within own team.
          // They could do it themselves tbh. But let's think of the afks.
        }
      }

      if (!m_TargetGame->SwapSlots(slotNumOne, slotNumTwo)) {
        ErrorReply("These slots cannot be swapped.");
        break;
      }
      m_TargetGame->ResetLayoutIfNotMatching();
      if ((playerOne != nullptr) && (playerTwo != nullptr)) {
        SendReply("Swapped " + playerOne->GetName() + " with " + playerTwo->GetName() + ".");
      } else if (!playerOne && !playerTwo) {
        SendReply("Swapped slots " + ToDecString(slotNumOne + 1) + " and " + ToDecString(slotNumTwo + 1) + ".");
      } else if (playerOne) {
        SendReply("Swapped player [" + playerOne->GetName() + "] to slot " + ToDecString(slotNumTwo + 1) + ".");
      } else {
        SendReply("Swapped player [" + playerTwo->GetName() + "] to slot " + ToDecString(slotNumOne + 1) + ".");
      }
      break;
    }

    //
    // !UNHOST
    //

    case HashCode("unhost"):
    case HashCode("uh"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetCountDownStarted()) {
        // Intentionally allows !unhost for fake (mirror) lobbies.
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      LogStream(*m_Output, m_TargetGame->GetLogPrefix() + "is over (admin cancelled game) [" + m_FromName + "]");
      SendReply("Aborting " + m_TargetGame->GetDescription());
      m_TargetGame->m_Exiting = true;
      m_TargetGame->StopPlayers("was disconnected (admin cancelled game)", false);
      break;
    }

    //
    // !DOWNLOAD
    // !DL
    //

    case HashCode("download"):
    case HashCode("dl"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobby())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      bool IsMapAvailable = !m_TargetGame->GetMap()->GetMapData()->empty() && !m_TargetGame->GetMap()->HasMismatch();
      if (m_Aura->m_Net->m_Config->m_AllowTransfers == MAP_TRANSFERS_NEVER || !IsMapAvailable || m_Aura->m_Games.size() >= m_Aura->m_Config->m_MaxGames) {
        if (m_TargetGame->GetMapSiteURL().empty()) {
          ErrorAll("Cannot transfer the map.");
        } else {
          ErrorAll("Cannot transfer the map. Please download it from <" + m_TargetGame->GetMapSiteURL() + ">");
        }
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }

      if (targetPlayer->GetDownloadStarted() || targetPlayer->GetDownloadFinished()) {
        ErrorReply("Player [" + targetPlayer->GetName() + "] is already downloading the map.");
        break;
      }

      const CGameSlot* slot = m_TargetGame->InspectSlot(m_TargetGame->GetSIDFromPID(targetPlayer->GetPID()));
      if (!slot || slot->GetDownloadStatus() == 100) {
        ErrorReply("Map transfer failed unexpectedly.", CHAT_LOG_CONSOLE);
        break;
      }

      SendReply("Map download started for player [" + targetPlayer->GetName() + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      m_TargetGame->Send(targetPlayer, m_TargetGame->GetProtocol()->SEND_W3GS_STARTDOWNLOAD(m_TargetGame->GetHostPID()));
      targetPlayer->SetDownloadAllowed(true);
      targetPlayer->SetDownloadStarted(true);
      targetPlayer->SetStartedDownloadingTicks(GetTicks());
      targetPlayer->SetKickByTime(0);
      break;
    }

    //
    // !DROP
    //

    case HashCode("drop"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!m_TargetGame->GetLagging()) {
        ErrorReply("No player is currently lagging.");
        break;
      }

      m_TargetGame->StopLaggers("lagged out (dropped by admin)");
      break;
    }

    //
    // !REFEREE
    //

    case HashCode("referee"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror() || (m_TargetGame->GetCountDownStarted() && !m_TargetGame->GetGameLoaded()))
        break;

      CGamePlayer* targetPlayer = GetTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }
      if (!targetPlayer->GetIsObserver()) {
        ErrorReply("[" + targetPlayer->GetName() + "] is not an observer.");
        break;
      }

      m_TargetGame->SetUsesCustomReferees(true);
      for (auto& otherPlayer: m_TargetGame->m_Players) {
        if (otherPlayer->GetIsObserver())
          otherPlayer->SetPowerObserver(false);
      }
      targetPlayer->SetPowerObserver(true);
      SendAll("Player [" + targetPlayer->GetName() + "] was promoted to referee by [" + m_FromName + "] (Other observers may only use observer chat)");
      break;
    }

    //
    // !MUTE
    //

    case HashCode("mute"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot mute people.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "mute [PLAYERNAME]");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        break;
      }
      if (!GetIsSudo()) {
        if (targetPlayer == m_Player) {
          ErrorReply("Cannot mute yourself.");
          break;
        }
        if (m_SourceRealm && (
          m_SourceRealm->GetIsAdmin(targetPlayer->GetName()) || (
            m_SourceRealm->GetIsModerator(targetPlayer->GetName()) &&
            !CheckPermissions(m_Config->m_AdminBasePermissions, COMMAND_PERMISSIONS_ROOTADMIN)
          )
        )) {
          ErrorReply("User [" + targetPlayer->GetName() + "] is an admin on server [" + m_ServerName + "]");
          break;
        }
      }
      if (targetPlayer->GetMuted()) {
        ErrorReply("Player [" + targetPlayer->GetName() + "] is already muted.");
        break;
      }
      targetPlayer->SetMuted(true);
      SendAll("Player [" + targetPlayer->GetName() + "] was muted by player [" + m_FromName + "]");
      break;
    }

    //
    // !MUTEALL
    //

    case HashCode("muteall"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror() || !m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot disable \"All\" chat channel.");
        break;
      }

      if (m_TargetGame->m_MuteAll) {
        ErrorReply("Global chat is already muted.");
        break;
      }

      SendAll("Global chat muted (allied and private chat is unaffected)");
      m_TargetGame->m_MuteAll = true;
      break;
    }

    //
    // !ABORT (abort countdown)
    // !A
    //

    case HashCode("abort"):
    case HashCode("a"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror() || m_TargetGame->GetGameLoading() || m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot abort game start.");
        break;
      }

      if (m_TargetGame->GetCountDownStarted()) {
        m_TargetGame->m_CountDownStarted = false;
        if (m_TargetGame->GetIsAutoStartDue()) {
          m_TargetGame->m_AutoStartRequirements.clear();
          SendAll("Countdown stopped by " + m_FromName + ". Autostart removed.");
        } else {
          SendAll("Countdown stopped by " + m_FromName + ".");
        }
      } else {
        m_TargetGame->m_AutoStartRequirements.clear();
        SendAll("Autostart removed.");
      }
      break;
    }

    //
    // !CHECKNETWORK
    //
    case HashCode("checknetwork"): {
      UseImplicitHostedGame();

      uint16_t gamePort = m_TargetGame ? m_TargetGame->GetHostPortForDiscoveryInfo(AF_INET) : m_Aura->m_Net->NextHostPort();
      uint8_t checkMode = HEALTH_CHECK_ALL | HEALTH_CHECK_VERBOSE;
      if (!m_Aura->m_Net->m_SupportTCPOverIPv6) {
        checkMode &= ~HEALTH_CHECK_PUBLIC_IPV6;
        checkMode &= ~HEALTH_CHECK_LOOPBACK_IPV6;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        ErrorReply("Not allowed to check network status.");
        break;
      }
      const bool TargetAllRealms = Payload == "*" || (Payload.empty() && !m_SourceRealm);
      CRealm* targetRealm = nullptr;
      if (!TargetAllRealms) {
        targetRealm = GetTargetRealmOrCurrent(Payload);
        if (!targetRealm) {
          ErrorReply("Usage: " + cmdToken + "checknetwork *");
          ErrorReply("Usage: " + cmdToken + "checknetwork <REALM>");
          break;
        }
      }

      if (!m_Aura->m_Net->QueryHealthCheck(this, checkMode, targetRealm, gamePort)) {
        ErrorReply("Already testing the network.");
        break;
      }

      SendReply("Testing network connectivity...");
      break;
    }

    //
    // !PORTFORWARD
    //

#ifndef DISABLE_MINIUPNP
    case HashCode("portforward"): {
      UseImplicitHostedGame();

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      string protocol = "TCP";
      vector<uint32_t> Args = SplitNumericArgs(Payload, 1, 2);
      if (Args.size() == 1) {
        if (Args[0] == 0 || Args[0] > 0xFFFF) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
        Args.push_back(Args[0]);
      } else if (Args.empty()) {
        if (!Payload.empty() || !m_TargetGame) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
        Args.push_back(m_TargetGame->GetHostPort());
        Args.push_back(m_TargetGame->GetHostPort());
      } else {
        if (Args[0] == 0 || Args[0] > 0xFFFF || Args[1] == 0 || Args[1] > 0xFFFF) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
      }

      uint16_t extPort = static_cast<uint16_t>(Args[0]);
      uint16_t intPort = static_cast<uint16_t>(Args[1]);

      SendReply("Trying to forward external port " + to_string(extPort) + " to internal port " + to_string(intPort) + "...");
      uint8_t result = m_Aura->m_Net->RequestUPnP(protocol, extPort, intPort);
      if (result == 0) {
        ErrorReply("Universal Plug and Play is not supported by the host router.");
      } else if (0 != (result & 1)) {
        SendReply("Opened port " + to_string(extPort) + " with Universal Plug and Play");
      } else {
        SendReply("Unknown results. Try " + cmdToken + "checknetwork");
      }
      break;
    }
#endif

    //
    // !CHECKBAN
    //

    case HashCode("checkban"): {
      if (m_Aura->m_Realms.empty())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to check ban status.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "checkban <PLAYERNAME>");
        break;
      }

      vector<string> CheckResult;
      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;

        CDBBan* Ban = m_Aura->m_DB->BanCheck(bnet->GetDataBaseID(), Payload);
        if (Ban) {
          CheckResult.push_back("[" + bnet->GetServer() + "] - banned by [" + Ban->GetModerator() + "] because [" + Ban->GetReason() + "] (" + Ban->GetDate() + ")");
          delete Ban;
        }
      }

      if (CheckResult.empty()) {
        SendAll("[" + Payload + "] is not banned from any server.");
      } else {
        SendAll("[" + Payload + "] is banned from " + to_string(CheckResult.size()) + " server(s): " + JoinVector(CheckResult, false));
      }
      break;
    }

    //
    // !LISTBANS
    //

    case HashCode("listbans"): {
      if (!CheckPermissions(m_Config->m_AdminBasePermissions, COMMAND_PERMISSIONS_ROOTADMIN)) {
        ErrorReply("Only root admins may list bans.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "listbans <REALM>");
        break;
      }
      if (targetRealm != m_SourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to list bans in arbitrary realms.");
        break;
      }
      vector<string> bannedUsers = m_Aura->m_DB->ListBans(targetRealm->GetDataBaseID());
      if (bannedUsers.empty()) {
        SendReply("No users are banned on " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      SendReply("Banned: " + JoinVector(bannedUsers, false));
      break;
    }

    //
    // !IMPORT
    //

    case HashCode("import"): {
      if (!CheckPermissions(m_Config->m_ImportPermissions, COMMAND_PERMISSIONS_SUDO)) {
        ErrorReply("Not allowed to import files to the database.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage " + cmdToken + "import [DATA TYPE]. Supported data types are: aliases");
        break;
      }
      string lower = Payload;
      transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lower == "aliases") {
        m_Aura->LoadMapAliases();
        SendReply("Commited map aliases to SQLite.");
      } else {
        ErrorReply("Usage " + cmdToken + "import [DATA TYPE]. Supported data types are: aliases");
      }
      break;
    }

    //
    // !ALIAS
    //

    case HashCode("alias"): {
      if (!CheckPermissions(m_Config->m_AliasPermissions, COMMAND_PERMISSIONS_SUDO)) {
        ErrorReply("Not allowed to update map aliases.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [FILE NAME]");
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [MAP IDENTIFIER]");
        break;
      }
      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [FILE NAME]");
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [MAP IDENTIFIER]");
        break;
      }
      string alias = Args[0];
      transform(begin(alias), end(alias), begin(alias), [](char c) { return static_cast<char>(std::tolower(c)); });
      if (!m_Aura->m_DB->AliasAdd(alias, Args[1])) {
        ErrorReply("Failed to add alias [" + alias + "]");
      } else {
        SendReply("Added [" + alias + "] as alias to [" + Args[1] + "].");
      }
      break;
    }
    
    //
    // !ADDBAN
    // !BAN
    //

    case HashCode("addban"):
    case HashCode("ban"): {
      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (m_Aura->m_Realms.empty()) {
        ErrorReply("No realms joined.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>");
        break;
      }

      // extract the victim and the reason
      // e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

      string       Victim, Reason;
      stringstream SS;
      SS << Payload;
      SS >> Victim;

      if (!SS.eof())
      {
        getline(SS, Reason);
        string::size_type Start = Reason.find_first_not_of(' ');

        if (Start != string::npos)
          Reason = Reason.substr(Start);
      }

      if (m_TargetGame && m_TargetGame->GetGameLoaded()) {
        string VictimLower = Victim;
        transform(begin(VictimLower), end(VictimLower), begin(VictimLower), [](char c) { return static_cast<char>(std::tolower(c)); });
        uint32_t Matches   = 0;
        CDBBan*  LastMatch = nullptr;

        // try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")
        // we use the m_DBBans vector for this in case the player already left and thus isn't in the m_Players vector anymore

        for (auto& ban : m_TargetGame->m_DBBans) {
          string TestName = ban->GetName();
          transform(begin(TestName), end(TestName), begin(TestName), [](char c) { return static_cast<char>(std::tolower(c)); });

          if (TestName.find(VictimLower) != string::npos) {
            ++Matches;
            LastMatch = ban;

            // if the name matches exactly stop any further matching

            if (TestName == VictimLower) {
              Matches = 1;
              break;
            }
          }
        }

        if (Matches == 0) {
          ErrorReply("Unable to ban player [" + Victim + "]. No matches found");
        } else if (Matches > 1) {
          ErrorReply("Unable to ban player [" + Victim + "]. Found more than one match");
        } else {
          m_Aura->m_DB->BanAdd(LastMatch->GetServer(), LastMatch->GetName(), m_FromName, Reason);
          SendAll("Player [" + LastMatch->GetName() + "] was banned by player [" + m_FromName + "] on server [" + LastMatch->GetServer() + "]");
        }
        break;
      } else if (m_TargetGame) {
        CGamePlayer* targetPlayer = GetTargetPlayer(Victim);
        if (!targetPlayer) {
          break;
        }
        m_Aura->m_DB->BanAdd(targetPlayer->GetRealmDataBaseID(false), targetPlayer->GetName(), m_FromName, Reason);
        SendAll("Player [" + targetPlayer->GetName() + "] was banned by player [" + m_FromName + "] on server [" + targetPlayer->GetRealmHostName() + "]");
        break;
      } else if (m_SourceRealm) {
        if (m_SourceRealm->GetIsAdmin(Victim) || (m_SourceRealm->GetIsModerator(Victim) &&
          !CheckPermissions(m_Config->m_AdminBasePermissions, COMMAND_PERMISSIONS_ROOTADMIN))
        ) {
          ErrorReply("User [" + Victim + "] is an admin on server [" + m_ServerName + "]");
          break;
        }

        if (m_SourceRealm->IsBannedName(Victim)) {
          ErrorReply("User [" + Victim + "] is already banned on server [" + m_ServerName + "]");
          break;
        }
        if (m_SourceRealm->GetIsModerator(Victim)) {
          if (!m_Aura->m_DB->ModeratorRemove(m_SourceRealm->GetDataBaseID(), Victim)) {
            ErrorReply("Failed to ban user [" + Victim + "] on server [" + m_ServerName + "]");
            break;
          }
        }
        if (!m_Aura->m_DB->BanAdd(m_SourceRealm->GetDataBaseID(), Victim, m_FromName, Reason)) {
          ErrorReply("Failed to ban user [" + Victim + "] on server [" + m_ServerName + "]");
          break;
        }
        SendAll("User [" + Victim + "] banned on server [" + m_ServerName + "]");
        break;
      } else {
        ErrorReply("Realm unknown.");
        break;
      }
      break;
    }

    //
    // !UNBAN
    //

    case HashCode("unban"): {
      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (m_Aura->m_Realms.empty()) {
        ErrorReply("No realms joined.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "unban <PLAYERNAME>");
        break;
      }

      if (!m_Aura->m_DB->BanRemove(Payload)) {
        ErrorReply("Unbanning user [" + Payload + "] on all realms.");
        break;
      }

      SendReply("Unbanned user [" + Payload + "] on all realms.");
      break;
    }

    //
    // !CLEARHCL
    //

    case HashCode("clearhcl"): {
      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (m_TargetGame->m_HCLCommandString.empty()) {
        ErrorReply("There was no game mode set.");
        break;
      }

      m_TargetGame->m_HCLCommandString.clear();
      SendAll("Game mode reset");
      break;
    }

    //
    // !STATUS
    //

    case HashCode("status"): {
      string message = "Status: ";

      for (const auto& bnet : m_Aura->m_Realms)
        message += "[" + bnet->GetUniqueDisplayName() + (bnet->GetLoggedIn() ? " - online] " : " - offline] ");

      if (m_Aura->m_IRC) {
        message += "[" + m_Aura->m_IRC->m_Config->m_HostName + (!m_Aura->m_IRC->m_WaitingToConnect ? " - online]" : " - offline]");
      }

      if (m_Aura->m_Discord) {
        //TODO: Support Discord in !status
        //message += m_Aura->m_DiscordAPI->m_Config->m_ServerName + (m_Aura->m_DiscordAPI->GetConnected() ? " [online]" : " [offline]");
      }

      SendReply(message);
      break;
    }

    //
    // !SENDLAN
    //

    case HashCode("sendlan"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetCountDownStarted()) {
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (m_TargetGame->GetIsMirror()) {
        // This is not obvious.
        ErrorReply("Mirrored games cannot be broadcast to LAN");
        break;
      }

      optional<bool> TargetValue;
      if (Payload.empty() || Payload == "on" || Payload == "ON") {
        TargetValue = true;
      } else if (Payload == "off" || payload == "OFF") {
        TargetValue = false;
      }

      if (!TargetValue.has_value()) {
        optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(Payload);
        if (!maybeAddress.has_value()) {
          ErrorReply("Usage: " + cmdToken + "sendlan ON/OFF");
          ErrorReply("Usage: " + cmdToken + "sendlan <IP>");
          break;
        }
        sockaddr_storage* address = &(maybeAddress.value());
        if (GetInnerIPVersion(address) == AF_INET6 && !(m_Aura->m_Net->m_SupportUDPOverIPv6 && m_Aura->m_Net->m_SupportTCPOverIPv6)) {
          ErrorReply("IPv6 support hasn't been enabled. Set <net.ipv6.tcp.enabled = yes>, and <net.udp_ipv6.enabled = yes> if you want to enable it.");
          break;
        }
        if ((address->ss_family == AF_INET6 && isSpecialIPv6Address(reinterpret_cast<struct sockaddr_in6*>(address))) ||
          (address->ss_family == AF_INET && isSpecialIPv4Address(reinterpret_cast<struct sockaddr_in*>(address)))) {
          ErrorReply("Special IP address rejected. Add it to <net.game_discovery.udp.extra_clients.ip_addresses> if you are sure about this.");
          break;
        }
        if (m_TargetGame->m_ExtraDiscoveryAddresses.find(Payload) != m_TargetGame->m_ExtraDiscoveryAddresses.end()) {
          ErrorReply("Already sending game info to " + Payload);
          break;
        }
        if (!m_TargetGame->m_UDPEnabled)
          SendReply("This lobby will now be displayed in the Local Area Network game list");
        m_TargetGame->m_UDPEnabled = true;
        m_TargetGame->m_ExtraDiscoveryAddresses.insert(Payload);
        SendReply("This lobby will be displayed in the Local Area Network game list for IP " + Payload + ". Make sure your peer has done UDP hole-punching.");
        break;
      }

      m_TargetGame->m_UDPEnabled = TargetValue.value();
      if (TargetValue) {
        m_TargetGame->SendGameDiscoveryCreate();
        m_TargetGame->SendGameDiscoveryRefresh();
        if (!m_Aura->m_Net->m_UDPMainServerEnabled)
          m_TargetGame->SendGameDiscoveryInfo(); // Since we won't be able to handle incoming GAME_SEARCH packets
      }
      if (m_TargetGame->m_UDPEnabled) {
        SendReply("This lobby will now be displayed in the Local Area Network game list");
      } else {
        SendReply("This lobby will no longer be displayed in the Local Area Network game list");
      }
      break;
    }

    //
    // !SENDLANINFO
    //

    case HashCode("sendlaninfo"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetCountDownStarted()) {
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (!Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "sendlaninfo");
        ErrorReply("You may want !sendlan <IP> or !sendlan <ON/OFF> instead");
        break;
      }

      if (m_TargetGame->GetIsMirror()) {
        // This is not obvious.
        ErrorReply("Mirrored games cannot be broadcast to LAN");
        break;
      }

      m_TargetGame->m_UDPEnabled = true;
      m_TargetGame->SendGameDiscoveryInfo();
      SendReply("Sent game info to peers.");
      break;
    }

    //
    // !OWNER (set game owner)
    //

    case HashCode("owner"): {
      UseImplicitHostedGame();

      if (!m_TargetGame) {
        ErrorReply("No game found.");
        break;
      }

      if ((!m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted() || m_TargetGame->m_OwnerLess) && !GetIsSudo()) {
        ErrorReply("Cannot take ownership of this game.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        if (Payload.empty() && m_TargetGame->HasOwnerSet()) {
          SendReply("The owner is [" + m_TargetGame->m_OwnerName + "@" + (m_TargetGame->m_OwnerRealm.empty() ? "@@LAN/VPN" : m_TargetGame->m_OwnerRealm) + "]");
        }
        ErrorReply("You are not allowed to change the owner of this game.");
        break;
      }

      CGamePlayer* TargetPlayer = GetTargetPlayerOrSelf(Payload);
      string TargetName = TargetPlayer ? TargetPlayer->GetName() : Payload;
      if (TargetName.empty()) {
        ErrorReply("Usage: " + cmdToken + "owner <PLAYERNAME>");
        break;
      }
      string TargetRealm = TargetPlayer ? TargetPlayer->GetRealmHostName() : m_ServerName;
      if (!TargetPlayer && TargetRealm.empty()) {
        ErrorReply("Usage: " + cmdToken + "owner <PLAYERNAME>");
        break;
      }
      if (!TargetPlayer && !CheckConfirmation(cmdToken, command, payload, "Player [" + TargetName + "] is not in this game lobby. ")) {
        break;
      }
      if ((TargetPlayer && TargetPlayer != m_Player && !TargetRealm.empty() && !TargetPlayer->IsRealmVerified()) &&
        !CheckConfirmation(cmdToken, command, payload, "Player [" + TargetName + "] has not been verified by " + TargetRealm + ". ")) {
        break;
      }
      if (m_TargetGame->m_OwnerName == TargetName && m_TargetGame->m_OwnerRealm == TargetRealm) {
        SendAll(TargetName + "@" + (TargetRealm.empty() ? "@@LAN/VPN" : TargetRealm) + " is already the owner of this game.");
      } else {
        m_TargetGame->SetOwner(TargetName, TargetRealm);
        SendReply("Setting game owner to [" + TargetName + "@" + (TargetRealm.empty() ? "@@LAN/VPN" : TargetRealm) + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      }
      if (TargetPlayer) {
        TargetPlayer->SetWhoisShouldBeSent(true);
        TargetPlayer->SetOwner(true);
        m_TargetGame->SendOwnerCommandsHelp(cmdToken, TargetPlayer);
      }
      break;
    }

    //
    // !SAY
    //

    case HashCode("say"): {
      if (m_Aura->m_Realms.empty())
        break;

      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("You are not allowed to advertise.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "say <REALM> , <MESSAGE>");
        break;
      }

      string::size_type MessageStart = Payload.find(',');
      string RealmId = TrimString(Payload.substr(0, MessageStart));
      string Message = TrimString(Payload.substr(MessageStart + 1));
      if (Message.empty()) {
        ErrorReply("Usage: " + cmdToken + "say <REALM> , <MESSAGE>");
        break;
      }
      bool IsCommand = Message[0] == '/';
      if (IsCommand && !GetIsSudo()) {
        ErrorReply("You are not allowed to send bnet commands.");
        break;
      }
      if (!IsCommand && m_Aura->m_CurrentLobby) {
        ErrorReply("Cannot send bnet chat messages while the bot is hosting a game lobby.");
        break;
      }
      transform(begin(RealmId), end(RealmId), begin(RealmId), [](char c) { return static_cast<char>(std::tolower(c)); });

      const bool ToAllRealms = RealmId.length() == 1 && RealmId[0] == '*';
      bool Success = false;

      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;
        if (ToAllRealms || bnet->GetInputID() == RealmId) {
          if (IsCommand) {
            bnet->QueueCommand(Message)->SetEarlyFeedback("Command sent.");
          } else {
            bnet->QueueChatChannel(Message)->SetEarlyFeedback("Message sent.");
          }
          Success = true;
        }
      }

      if (!Success) {
        ErrorReply("Realm [" + RealmId + "] not found.");
        break;
      }
      break;
    }

    //
    // !ANNOUNCE
    //

    case HashCode("announce"): {
      UseImplicitHostedGame();

      if (m_Aura->m_Realms.empty())
        break;

      if (!m_TargetGame || !m_TargetGame->GetIsLobby())
        break;

      if (m_TargetGame->m_DisplayMode == GAME_PRIVATE) {
        ErrorReply("This game is private.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot advertise.");
        break;
      }

      bool ToAllRealms = Payload == "*";
      CRealm* targetRealm = nullptr;
      if (ToAllRealms) {
        if (0 != (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
          ErrorReply("Announcing on all realms requires sudo permissions."); // But not really
          break;
        }
      } else {
        targetRealm = GetTargetRealmOrCurrent(Payload);
        if (!targetRealm) {
          ErrorReply("Usage: " + cmdToken + "announce <REALM>");
          break;
        }
        if (!m_TargetGame->GetIsSupportedGameVersion(targetRealm->GetGameVersion())) {
          ErrorReply("Crossplay is not enabled. [" + targetRealm->GetCanonicalDisplayName() + "] is running v1." + to_string(targetRealm->GetGameVersion()));
          break;
        }
      }

      m_TargetGame->m_DisplayMode = GAME_PUBLIC;
      m_TargetGame->m_RealmRefreshError = false;
      string earlyFeedback = "Announcement sent.";
      if (ToAllRealms) {
        for (auto& bnet : m_Aura->m_Realms) {
          if (!m_TargetGame->GetIsSupportedGameVersion(bnet->GetGameVersion())) continue;
          bnet->QueueGameUncreate(); //?
          bnet->QueueGameChatAnnouncement(m_TargetGame, this, true)->SetEarlyFeedback(earlyFeedback);
          bnet->SendEnterChat();
        }
      } else {
        targetRealm->QueueGameUncreate();
        targetRealm->QueueGameChatAnnouncement(m_TargetGame, this, true)->SetEarlyFeedback(earlyFeedback);
        targetRealm->SendEnterChat();
      }
      break;
    }

    //
    // !CLOSEALL
    //

    case HashCode("closeall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (m_TargetGame->CloseAllSlots()) {
        // Also sent if there was no player, and so all slots except one were closed.
        SendReply("Closed all slots.");
      } else {
        ErrorReply("There are no open slots.");
      }
      
      break;
    }

    //
    // !COMP (computer slot)
    //

    case HashCode("comp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        // ignore layout, don't override computers
        if (!m_TargetGame->ComputerNSlots(SLOTCOMP_HARD, m_TargetGame->GetNumComputers() + 1, true, false)) {
          ErrorReply("No slots available.");
        } else {
          SendReply("Insane computer added.");
        }
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "comp <SLOT> , <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }

      uint8_t SID = 0xFF;
      if (!ParseNonPlayerSlot(Args[0], SID)) {
        ErrorReply("Usage: " + cmdToken + "comp <SLOT> , <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }
      uint8_t skill = SLOTCOMP_HARD;
      if (Args.size() >= 2) {
        skill = ParseComputerSkill(Args[1]);
      }
      if (!m_TargetGame->ComputerSlot(SID, skill, false)) {
        ErrorReply("Cannot add computer on that slot.");
        break;
      }
      m_TargetGame->ResetLayoutIfNotMatching();
      SendReply("Computer slot added.");
      break;
    }

    //
    // !DELETECOMP (remove computer slots)
    //

    case HashCode("deletecomp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (!m_TargetGame->ComputerNSlots(SLOTCOMP_HARD, 0)) {
        ErrorReply("Failed to remove computer slots.");
        break;
      }
      SendReply("Computer slots removed.");
      break;
    }

    //
    // !COLOR (computer colour change)
    //

    case HashCode("color"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12" + HelpMissingComma(Payload));
        break;
      }

      uint8_t SID = 0xFF;
      CGamePlayer* targetPlayer = nullptr;
      if (!ParsePlayerOrSlot(Args[0], SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12");
        break;
      }

      uint8_t color = ParseColor(Args[1]);
      if (color >= m_Aura->m_MaxSlots) {
        color = ParseSID(Args[1]);

        if (color >= m_Aura->m_MaxSlots) {
          ErrorReply("Color identifier \"" + Args[1] + "\" is not valid.");
          break;
        }
      }

      if (!m_TargetGame->SetSlotColor(SID, color, true)) {
        ErrorReply("Cannot change slot #" + Args[0] + " color to " + Args[1]);
        break;
      }

      SendReply("Color changed.");
      break;
    }

    //
    // !HANDICAP (handicap change)
    //

    case HashCode("handicap"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100" + HelpMissingComma(Payload));
        break;
      }

      uint8_t SID = 0xFF;
      CGamePlayer* targetPlayer = nullptr;
      if (!ParsePlayerOrSlot(Args[0], SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }

      vector<uint32_t> handicap = SplitNumericArgs(Args[1], 1u, 1u);
      if (handicap.empty() || handicap[0] % 10 != 0 || !(50 <= handicap[0] && handicap[0] <= 100)) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }

      if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        // The WC3 client is incapable of modifying handicap when Fixed Player Settings is enabled.
        // However, the GUI misleads users into thinking that it can be modified.
        // While it's indeed editable for HCL purposes,
        // we don't intend to forcibly allow its edition outside of HCL context.
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      CGameSlot* slot = m_TargetGame->GetSlot(SID);
      if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + Args[0] + " is empty.");
        break;
      }

      if (slot->GetHandicap() == static_cast<uint8_t>(handicap[0])) {
        ErrorReply("Handicap is already at " + Args[1] + "%");
        break;
      }
      slot->SetHandicap(static_cast<uint8_t>(handicap[0]));
      m_TargetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (targetPlayer) {
        SendReply("Player [" + targetPlayer->GetName() + "] handicap is now [" + Args[1] + "].");
      } else {
        SendReply("Race updated.");
      }
      break;
    }

    //
    // !RACE (race change)
    //

    case HashCode("comprace"):
    case HashCode("race"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll" + HelpMissingComma(Payload));
        break;
      }

      uint8_t SID = 0xFF;
      CGamePlayer* targetPlayer = nullptr;
      if (!ParsePlayerOrSlot(Args[0], SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll");
        break;
      }

      uint8_t Race = ParseRace(Args[1]);
      if (Race == SLOTRACE_INVALID) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll");
        break;
      }
      if (Race == SLOTRACE_PICKRANDOM) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distribution(0, 3);
        Race = 1 << distribution(gen);
      }

      if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      if (m_TargetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
        ErrorReply("This game has Random Races enabled.");
        break;
      }

      CGameSlot* slot = m_TargetGame->GetSlot(SID);
      if (!slot || slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED || slot->GetTeam() == m_Aura->m_MaxSlots) {
        ErrorReply("Slot " + ToDecString(SID + 1) + " is not playable.");
        break;
      }
      if (Race == (slot->GetRace() & Race)) {
        ErrorReply("Slot " + ToDecString(SID + 1) + " is already [" + GetRaceName(Race) + "] race.");
        break;
      }
      slot->SetRace(Race | SLOTRACE_SELECTABLE);
      m_TargetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (targetPlayer) {
        SendReply("Player [" + targetPlayer->GetName() + "] race is now [" + GetRaceName(Race) + "].");
      } else {
        SendReply("Race updated.");
      }
      break;
    }

    //
    // !TEAM (forced team change)
    //

    case HashCode("compteam"):
    case HashCode("team"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      bool onlyDraft = false;
      if (!GetIsSudo()) {
        if ((onlyDraft = m_TargetGame->GetIsDraftMode())) {
          if (!m_Player || !m_Player->GetIsDraftCaptain()) {
            ErrorReply("Draft mode is enabled. Only draft captains may assign teams.");
            break;
          }
        } else if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }
      }

      if (Payload.empty()) {
        if (m_Player) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 2u);
      if (Args.empty()) {
        if (m_Player) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }

      uint8_t SID = 0xFF;
      CGamePlayer* targetPlayer = nullptr;
      if (!ParsePlayerOrSlot(Args[0], SID, targetPlayer)) {
        if (m_Player) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }

      uint8_t targetTeam = 0xFF;
      if (Args.size() >= 2) {
        targetTeam = ParseSID(Args[1]);
      } else {      
        if (!m_Player) {
          ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
          break;
        }
        const CGameSlot* slot = m_TargetGame->InspectSlot(m_TargetGame->GetSIDFromPID(m_Player->GetPID()));
        if (!slot) {
          ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
          break;
        }

        // If there are several admins in the game,
        // let them directly pick their team members with e.g. !team Arthas
        targetTeam = slot->GetTeam();
      }
      if (targetTeam > m_Aura->m_MaxSlots + 1) { // accept 13/25 as observer
        ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }
      if (onlyDraft && !m_Player->GetIsDraftCaptainOf(targetTeam)) {
        ErrorReply("You are not the captain of team " + ToDecString(targetTeam + 1));
        break;
      }

      if (m_TargetGame->GetSlot(SID)->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + Args[0] + " is empty.");
        break;
      }

      if (targetTeam == m_Aura->m_MaxSlots) {
        if (m_TargetGame->GetMap()->GetMapObservers() != MAPOBS_ALLOWED && m_TargetGame->GetMap()->GetMapObservers() != MAPOBS_REFEREES) {
          ErrorReply("This game does not have observers enabled.");
          break;
        }
        if (m_TargetGame->m_Slots[SID].GetIsComputer()) {
          ErrorReply("Computer slots cannot be moved to observers team.");
          break;
        }
      } else if (targetTeam > m_TargetGame->GetMap()->GetMapNumTeams()) {
        ErrorReply("This map does not allow Team #" + ToDecString(targetTeam + 1) + ".");
        break;
      }

      if (!m_TargetGame->SetSlotTeam(SID, targetTeam, true)) {
        if (targetPlayer) {
          ErrorReply("Cannot transfer [" + targetPlayer->GetName() + "] to team " + ToDecString(targetTeam + 1) + ".");
        } else {
          ErrorReply("Cannot transfer to team " + ToDecString(targetTeam + 1) + ".");
        }
      } else {
        m_TargetGame->ResetLayoutIfNotMatching();
        if (targetPlayer) {
          SendReply("[" + targetPlayer->GetName() + "] is now in team " + ToDecString(targetTeam + 1) + ".");
        } else {
          SendReply("Transferred to team " + ToDecString(targetTeam + 1) + ".");
        }
      }
      break;
    }

    //
    // !OBSERVER (forced change)
    //

    case HashCode("obs"):
    case HashCode("observer"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "observer <PLAYER>");
        break;
      }

      uint8_t SID = 0xFF;
      CGamePlayer* targetPlayer = nullptr;
      if (!ParsePlayerOrSlot(Payload, SID, targetPlayer)) {
        ErrorReply("Usage: " + cmdToken + "observer <PLAYER>");
        break;
      }

      if (!(m_TargetGame->GetMap()->GetMapObservers() == MAPOBS_ALLOWED || m_TargetGame->GetMap()->GetMapObservers() == MAPOBS_REFEREES)) {
        ErrorReply("This lobby does not allow observers.");
        break;
      }
      if (m_TargetGame->m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + Payload + " is empty.");
        break;
      }
      if (m_TargetGame->m_Slots[SID].GetIsComputer()) {
        ErrorReply("Computer slots cannot be moved to observers team.");
        break;
      }

      if (!m_TargetGame->SetSlotTeam(SID, m_Aura->m_MaxSlots, true)) {
        if (targetPlayer) {
          ErrorReply("Cannot turn [" + targetPlayer->GetName() + "] into an observer.");
        } else {
          ErrorReply("Cannot turn the player into an observer.");
        }
      } else {
        m_TargetGame->ResetLayoutIfNotMatching();
        if (targetPlayer) {
          SendReply("[" + targetPlayer->GetName() + "] is now an observer.");
        } else {
          SendReply("Moved to observers team.");
        }
      }
      break;
    }

    //
    // !FILL (fill all open slots with computers)
    //

    case HashCode("compall"):
    case HashCode("fill"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      uint8_t targetSkill = Payload.empty() ? SLOTCOMP_HARD : ParseComputerSkill(Payload);
      if (targetSkill == SLOTCOMP_INVALID) {
        ErrorReply("Usage: " + cmdToken + "fill <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }
      if (!m_TargetGame->ComputerAllSlots(targetSkill)) {
        if (m_TargetGame->GetCustomLayout() == CUSTOM_LAYOUT_HUMANS_VS_AI) {
          ErrorReply("No remaining slots available (lobby is set to humans vs AI.");
        } else {
          ErrorReply("No remaining slots available.");
        }
      }
      SendReply("Computers added.");
      break;
    }

    //
    // !DRAFT
    //

    case HashCode("draft"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
        ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, m_TargetGame->GetMap()->GetMapNumTeams());
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
        ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
        break;
      }

      if (Args.size() == 1) {
        optional<bool> targetValue;
        if (!ParseBoolean(Payload, targetValue)) {
          ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
          ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
          break;
        }
        if (targetValue.value_or(true)) {
          if (m_TargetGame->GetIsDraftMode()) {
            ErrorReply("Draft mode is already enabled.");
            break;
          }
          m_TargetGame->SetDraftMode(true);

          // Only has effect if observers are allowed.
          m_TargetGame->ResetTeams(false);

          SendReply("Draft mode enabled. Only draft captains may assign teams.");
        } else {
          if (!m_TargetGame->GetIsDraftMode()) {
            ErrorReply("Draft mode is already disabled.");
            break;
          }
          m_TargetGame->ResetDraft();
          m_TargetGame->SetDraftMode(false);
          SendReply("Draft mode disabled. Everyone may choose their own team.");
        }
        break;
      }

      m_TargetGame->ResetDraft();
      vector<string> failPlayers;

      uint8_t team = static_cast<uint8_t>(Args.size());
      while (team--) {
        CGamePlayer* player = GetTargetPlayer(Args[team]);
        if (player) {
          const uint8_t SID = m_TargetGame->GetSIDFromPID(player->GetPID());
          if (m_TargetGame->SetSlotTeam(SID, team, true) ||
            m_TargetGame->InspectSlot(SID)->GetTeam() == team) {
            player->SetDraftCaptain(team + 1);
          } else {
            failPlayers.push_back(player->GetName());
          }
        } else {
          failPlayers.push_back(Args[team]);
        }
      }

      // Only has effect if observers are allowed.
      m_TargetGame->ResetTeams(false);

      if (failPlayers.empty()) {
        SendReply("Draft captains assigned.");
      } else {
        ErrorReply("Draft mode enabled, but failed to assign captains: " + JoinVector(failPlayers, false));
      }
      break;
    }

    //
    //
    // !FFA
    //

    case HashCode("ffa"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetValue;
      if (!ParseBoolean(Payload, targetValue)) {
        ErrorReply("Usage: " + cmdToken + "ffa <ON|OFF>");
        break;
      }
      if (!targetValue.value_or(true)) {
        m_TargetGame->ResetLayout(true);
        SendReply("FFA mode disabled.");
        break;
      }
      if (m_TargetGame->GetMap()->GetMapNumControllers() <= 2) {
        ErrorReply("This map does not support FFA mode.");
        break;
      }
      if (!m_TargetGame->SetLayoutFFA()) {
        m_TargetGame->ResetLayout(true);
        ErrorReply("Cannot arrange a FFA match.");
      } else {
        SendReply("Game set to free-for-all.");
      }
      break;
    }

    //
    // !VSALL
    //

    case HashCode("pro"):
    case HashCode("lynch"):
    case HashCode("vsall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetValue;
      if (ParseBoolean(Payload, targetValue) && targetValue.value_or(true) == false) {
        // Branching here means that you can't actually set a player named "Disable" against everyone else.
        m_TargetGame->ResetLayout(true);
        SendReply("One-VS-All mode disabled.");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }

      const uint8_t othersCount = m_TargetGame->GetNumPotentialControllers() - 1;
      if (othersCount < 2) {
        ErrorReply("There are too few players in the game.");
        break;
      }

      if (!m_TargetGame->SetLayoutOneVsAll(targetPlayer)) {
        m_TargetGame->ResetLayout(true);
        ErrorReply("Cannot arrange a " + ToDecString(othersCount) + "-VS-1 match.");
        
      } else {
        SendReply("Game set to everyone against " + targetPlayer->GetName());
      }

      break;
    }

    //
    // !TERMINATOR
    //

    case HashCode("vsai"):
    case HashCode("terminator"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetValue;
      if (!ParseBoolean(Payload, targetValue)) {
        vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 1u);
        // Special-case max slots so that if someone careless enough types !terminator 12, it just works.
        if (Args.empty() || Args[0] <= 0 || (Args[0] >= m_TargetGame->GetMap()->GetMapNumControllers() && Args[0] != m_Aura->m_MaxSlots)) {
          ErrorReply("Usage: " + cmdToken + "terminator <ON|OFF>");
          ErrorReply("Usage: " + cmdToken + "terminator <NUMBER>");
          break;
        }
        m_TargetGame->ResetLayout(false);
        uint8_t computerCount = static_cast<uint8_t>(Args[0]);
        if (computerCount == m_Aura->m_MaxSlots) --computerCount; // Fix 1v12 into 1v11
        // ignore layout, don't override computers
        if (!m_TargetGame->ComputerNSlots(SLOTCOMP_HARD, computerCount, true, false)) {
          ErrorReply("Not enough open slots for " + ToDecString(computerCount) + " computers.");
          break;
        }
      }
      if (!targetValue.value_or(true)) {
        m_TargetGame->ResetLayout(true);
        SendReply("Humans-VS-AI mode disabled.");
        break;
      }
      const uint8_t computersCount = m_TargetGame->GetNumComputers();
      if (computersCount == 0) {
        ErrorReply("No computer slots found. Use [" + cmdToken + "terminator NUMBER] to play against one or more insane computers.");
        break;
      }
      const uint8_t humansCount = static_cast<uint8_t>(m_TargetGame->GetNumHumanOrFakeControllers());
      pair<uint8_t, uint8_t> matchedTeams;
      if (!m_TargetGame->FindHumanVsAITeams(humansCount, computersCount, matchedTeams)) {
        ErrorReply("Not enough open slots to host " + ToDecString(humansCount) + " humans VS " + ToDecString(computersCount) + " computers game.");
        break;
      }

      if (!m_TargetGame->SetLayoutHumansVsAI(matchedTeams.first, matchedTeams.second)) {
        m_TargetGame->ResetLayout(true);
        ErrorReply("Cannot arrange a " + ToDecString(humansCount) + " humans VS " + ToDecString(computersCount) + " computers match.");
      } else {
        SendReply("Game set to versus AI.");
      }
      break;
    }

    //
    // !TEAMS
    //

    case HashCode("teams"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }
      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }
      optional<bool> targetValue;
      if (!ParseBoolean(Payload, targetValue)) {
        ErrorReply("Usage: " + cmdToken + "teams <ON|OFF>");
        break;
      }
      if (!targetValue.value_or(true)) {
        m_TargetGame->ResetLayout(true);
        // This doesn't have any effect, since
        // both CUSTOM_LAYOUT_COMPACT nor CUSTOM_LAYOUT_ISOPLAYERS are
        // missing from CUSTOM_LAYOUT_LOCKTEAMS mask.
        SendReply("No longer enforcing teams.");
        break;
      }
      if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
        if (m_TargetGame->GetMap()->GetMapNumTeams() == 2) {
          // This is common enough to warrant a special case.
          if (m_TargetGame->SetLayoutTwoTeams()) {
            SendReply("Teams automatically arranged.");
            break;
          }
        }
      } else {
        if (m_TargetGame->SetLayoutCompact()) {
          SendReply("Teams automatically arranged.");
          break;
        }
      }
      m_TargetGame->ResetLayout(true);
      ErrorReply("Failed to automatically assign teams.");
      break;
    }

    //
    // !FAKEPLAYER
    //

    case HashCode("fakeplayer"):
    case HashCode("fp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      string inputLower = Payload;
      transform(begin(inputLower), end(inputLower), begin(inputLower), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (inputLower == "fill") {
        m_TargetGame->DeleteVirtualHost();
        m_TargetGame->FakeAllSlots();
        break;
      }

      const bool isToggle = inputLower == "disable" || inputLower == "enable";

      if (!isToggle && !Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "fp");
        ErrorReply("Usage: " + cmdToken + "fp <ON|OFF>");
        break;
      }

      if (!isToggle && m_TargetGame->GetIsRestored()) {
        ErrorReply("Usage: " + cmdToken + "fp <ON|OFF>");
        break;
      }

      m_TargetGame->SetAutoVirtualPlayers(!isToggle || inputLower != "disable");
      if (!isToggle && !m_TargetGame->CreateFakePlayer(false)) {
        ErrorReply("Cannot add another virtual player");
      } else if (isToggle) {
        if (m_TargetGame->GetIsAutoVirtualPlayers()) {
          SendReply("Automatic virtual players enabled.");
        } else {
          SendReply("Automatic virtual players disabled.");
        }
      }
      break;
    }

    //
    // !DELETEFP

    case HashCode("deletefake"):
    case HashCode("deletefp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (m_TargetGame->m_FakePlayers.empty()) {
        ErrorReply("No virtual players found.");
        break;
      }

      m_TargetGame->DeleteFakePlayers();
      break;
    }

    //
    // !FILLFAKE
    //

    case HashCode("fillfake"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      bool Success = false;
      while (m_TargetGame->CreateFakePlayer(false)) {
        Success = true;
      }

      if (!Success) {
        ErrorReply("Cannot add another virtual player");
      }
      break;
    }

    //
    // !PAUSE
    //

    case HashCode("pause"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot pause the game.");
        break;
      }

      if (m_TargetGame->GetPaused()) {
        ErrorReply("Game already paused.");
        break;
      }

      if (m_TargetGame->m_FakePlayers.empty()) {
        ErrorReply("This game does not support the " + cmdToken + "pause command. Use the game menu instead.");
        break;
      }

      if (!m_TargetGame->Pause(m_Player, false)) {
        ErrorReply("Max pauses reached.");
        break;
      }

      SendReply("Game paused.");
      break;
    }

    //
    // !SAVE
    //

    case HashCode("autosave"):
    case HashCode("save"):
    {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!m_Player && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot save the game.");
        break;
      }

      if (m_TargetGame->m_FakePlayers.empty()) {
        ErrorReply("This game does not support the " + cmdToken + "save command. Use the game menu instead.");
        break;
      }

      if (Payload.empty()) {
        if (!m_TargetGame->Save(m_Player, false)) {
          ErrorReply("Can only save once per player per game session.");
          break;
        }
        SendReply("Saving game...");
        break;
      }

      string lower = Payload;
      transform(begin(Command), end(Command), begin(Command), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lower == "enable") {
        m_TargetGame->SetSaveOnLeave(SAVE_ON_LEAVE_ALWAYS);
        SendReply("Autosave on disconnections enabled.");
      } else if (lower == "disable") {
        m_TargetGame->SetSaveOnLeave(SAVE_ON_LEAVE_NEVER);
        SendReply("Autosave on disconnections disabled.");
      } else {
        ErrorReply("Usage: " + cmdToken + "save");
        ErrorReply("Usage: " + cmdToken + "save <ON|OFF>");
      }
      break;
    }

    //
    // !RESUME
    //

    case HashCode("resume"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot resume the game.");
        break;
      }

      if (!m_TargetGame->GetPaused()) {
        ErrorReply("Game is not paused.");
        break;
      }

      if (m_TargetGame->m_FakePlayers.empty()) {
        ErrorReply("This game does not support the " + cmdToken + "resume command. Use the game menu instead.");
        break;
      }

      m_TargetGame->Resume();
      SendReply("Resuming game...");
      break;
    }

    //
    // !SP
    //

    case HashCode("sp"):
    case HashCode("shuffle"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      m_TargetGame->ShuffleSlots();
      SendAll("Players shuffled");
      break;
    }

    //
    // !LOCK
    //

    case HashCode("lock"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot lock the game.");
        break;
      }

      string Warning = m_TargetGame->m_OwnerRealm.empty() ? " (Owner joined over LAN - will get removed if they leave.)" : "";
      SendAll("Game locked. Only the game owner and root admins may run game commands." + Warning);
      m_TargetGame->m_Locked = true;
      break;
    }

    //
    // !OPENALL
    //

    case HashCode("openall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame)
        break;

      if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetIsRestored() || m_TargetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      m_TargetGame->OpenAllSlots();
      break;
    }

    //
    // !UNLOCK
    //

    case HashCode("unlock"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner nor a root admin, and therefore cannot unlock the game.");
        break;
      }

      SendAll("Game unlocked. All admins may now run game commands");
      m_TargetGame->m_Locked = false;
      break;
    }

    //
    // !UNMUTE
    //

    case HashCode("unmute"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unmute people.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "unmute <PLAYERNAME>");
        break;
      }
      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        break;
      }
      if (!targetPlayer->GetMuted()) {
        // Let this be transparent info.
        // And, more crucially, don't show "cannot unmute yourself" to people who aren't muted.
        ErrorReply("Player [" + targetPlayer->GetName() + "] is not muted.");
        break;
      }
      if (targetPlayer == m_Player && !GetIsSudo()) {
        ErrorReply("Cannot unmute yourself.");
        break;
      }

      targetPlayer->SetMuted(false);
      SendAll("Player [" + targetPlayer->GetName() + "] was unmuted by player [" + m_FromName + "]");
      break;
    }

    //
    // !UNMUTEALL
    //

    case HashCode("unmuteall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot enable \"All\" chat channel.");
        break;
      }

      if (m_TargetGame->m_MuteAll) {
        ErrorReply("Global chat is not muted.");
        break;
      }

      SendAll("Global chat unmuted");
      m_TargetGame->m_MuteAll = false;
      break;
    }

    //
    // !VOTECANCEL
    //

    case HashCode("votecancel"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot cancel a votekick.");
        break;
      }

      if (m_TargetGame->m_KickVotePlayer.empty()) {
        ErrorReply("There is no active votekick.");
        break;
      }

      SendReply("A votekick against player [" + m_TargetGame->m_KickVotePlayer + "] has been cancelled by [" + m_FromName + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      m_TargetGame->m_KickVotePlayer.clear();
      m_TargetGame->m_StartedKickVoteTime = 0;
      break;
    }

    //
    // !W
    //

    case HashCode("tell"):
    case HashCode("w"): {
      UseImplicitHostedGame();

      if (!CheckPermissions(m_Config->m_TellPermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("You are not allowed to send whispers.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      string::size_type MessageStart = Payload.find(',');

      if (MessageStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      string inputName = TrimString(Payload.substr(0, MessageStart));
      string subMessage = TrimString(Payload.substr(MessageStart + 1));
      string inputLocation;
      if (inputName.empty() || subMessage.empty()) {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      string::size_type RealmStart = inputName.find('@');
      if (RealmStart != string::npos) {
        inputLocation = TrimString(inputName.substr(RealmStart + 1));
        inputName = TrimString(inputName.substr(0, RealmStart));
      }

      if (inputName.empty()) {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      CRealm* matchingRealm = GetTargetRealmOrCurrent(inputLocation);
      if (matchingRealm) {
        if (inputName == matchingRealm->GetLoginName()) {
          ErrorReply("Cannot PM myself.");
          break;
        }

        // Name of sender and receiver should be included in the message,
        // so that they can be checked in successful whisper acks from the server (CBNETProtocol::EID_WHISPERSENT)
        // Note that the server doesn't provide any way to recognize whisper targets if the whisper fails.
        if (m_ServerName.empty()) {
          m_ActionMessage = inputName + ", " + m_FromName + " tells you: <<" + subMessage + ">>";
        } else {
          m_ActionMessage = inputName + ", " + m_FromName + " at " + m_ServerName + " tells you: <<" + subMessage + ">>";
        }

        matchingRealm->QueueWhisper(m_ActionMessage, inputName, this, true);
      } else {
        CGame* matchingGame = GetTargetGame(inputLocation);
        bool success = false;
        if (matchingGame) {
          if (matchingGame->GetGameLoaded() && !GetIsSudo()) {
            ErrorReply("Cannot send messages to a game that has already started.");
            break;
          }
          CGamePlayer* targetPlayer = nullptr;
          if (matchingGame->GetPlayerFromNamePartial(inputName, targetPlayer) != 1) {
            ErrorReply("Player [" + inputName + "] not found in <<" + matchingGame->GetGameName() + ">>.");
            break;
          }
          if (targetPlayer) {
            success = true;
            if (m_ServerName.empty()) {
              matchingGame->SendChat(targetPlayer, inputName + ", " + m_FromName + " tells you: <<" + subMessage + ">>");
            } else {
              matchingGame->SendChat(targetPlayer, inputName + ", " + m_FromName + " at " + m_ServerName + " tells you: <<" + subMessage + ">>");
            }
            SendReply("Message sent to " + targetPlayer->GetName() + ".");
          }
        }
        if (!success) {
          if (inputLocation.empty()) {
            ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
          } else {
            ErrorReply(inputLocation + " is not a valid location.");
          }
        }
      }
      break;
    }
    //
    // !WHOIS
    //

    case HashCode("whois"): {
      UseImplicitHostedGame();

      if (!CheckPermissions(m_Config->m_WhoisPermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot ask for /whois.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "whois <PLAYERNAME>");
        break;
      }

      string Name = Payload;
      string TargetRealm;

      string::size_type RealmStart = Name.find('@');
      if (RealmStart != string::npos) {
        TargetRealm = TrimString(Name.substr(RealmStart + 1));
        Name = TrimString(Name.substr(0, RealmStart));
      }

      if (Name.empty() || Name.length() > MAX_PLAYER_NAME_SIZE) {
        ErrorReply("Usage: " + cmdToken + "whois <PLAYERNAME>");
        break;
      }

      bool ToAllRealms = TargetRealm.empty() || (TargetRealm.length() == 1 && TargetRealm[0] == '*');
      const string Message = "/whois " + Name;

      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;
        if (ToAllRealms || bnet->GetInputID() == TargetRealm) {
          bnet->QueueCommand(Message);
        }
      }

      break;
    }

    //
    // !VIRTUALHOST
    //

    case HashCode("virtualhost"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot send whispers.");
        break;
      }

      if (Payload.empty() || Payload.length() > 15) {
        ErrorReply("Usage: " + cmdToken + "virtualhost <PLAYERNAME>");
        break;
      }
      if (m_TargetGame->m_LobbyVirtualHostName == Payload) {
        ErrorReply("Virtual host "+ Payload + " is already in the game.");
        break;
      }

      m_TargetGame->m_LobbyVirtualHostName = Payload;
      if (m_TargetGame->DeleteVirtualHost()) {
        m_TargetGame->CreateVirtualHost();
      }
      break;
    }

  /*****************
   * SUDOER COMMANDS *
   ******************/

    // !GETCLAN
    case HashCode("getclan"): {
      if (!m_SourceRealm)
        break;

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      m_SourceRealm->SendGetClanList();
      SendReply("Fetching clan member list from " + m_ServerName + "...");
      break;
    }

    // !GETFRIENDS
    case HashCode("getfriends"): {
      if (!m_SourceRealm)
        break;

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      m_SourceRealm->SendGetFriendsList();
      SendReply("Fetching friends list from " + m_ServerName + "...");
      break;
    }

    case HashCode("game"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      size_t CommandStart = Payload.find(',');
      if (CommandStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "game <GAMEID> , <COMMAND> - See game ids with !listgames");
        break;
      }
      string GameId = TrimString(Payload.substr(0, CommandStart));
      string ExecString = TrimString(Payload.substr(CommandStart + 1));
      if (GameId.empty() || ExecString.empty()) {
        ErrorReply("Usage: " + cmdToken + "game <GAMEID> , <COMMAND> - See game ids with !listgames");
        break;
      }
      CGame* targetGame = GetTargetGame(GameId);
      if (!targetGame) {
        ErrorReply("Game [" + GameId + "] not found. - See game ids with !listgames");
        break;
      }
      size_t PayloadStart = ExecString.find(' ');
      const string SubCmd = PayloadStart == string::npos ? ExecString : ExecString.substr(0, PayloadStart);
      const string SubPayload = PayloadStart == string::npos ? string() : ExecString.substr(PayloadStart + 1);
      CCommandContext* ctx;
      if (m_IRC) {
        ctx = new CCommandContext(m_Aura, m_Config, targetGame, m_IRC, m_ChannelName, m_FromName, m_FromWhisper, m_ServerName, m_IsBroadcast, &std::cout);
      } else if (m_DiscordAPI) {
        ctx = new CCommandContext(m_Aura, m_Config, targetGame, m_DiscordAPI, &std::cout);
      } else if (m_SourceRealm) {
        ctx = new CCommandContext(m_Aura, m_Config, targetGame, m_SourceRealm, m_FromName, m_FromWhisper, m_IsBroadcast, &std::cout);
      } else {
        ctx = new CCommandContext(m_Aura, m_Config, targetGame, m_IsBroadcast, &std::cout);
      }
      ctx->Run(cmdToken, SubCmd, SubPayload);
      m_Aura->UnholdContext(ctx);
      break;
    }

    case HashCode("cachemaps"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      vector<filesystem::path> AllMaps = FilesMatch(m_Aura->m_Config->m_MapPath, FILE_EXTENSIONS_MAP);
      int goodCounter = 0;
      int badCounter = 0;
      
      for (const auto& fileName : AllMaps) {
        string nameString = PathToString(fileName);
        if (nameString.empty()) continue;
        if (CommandHash != HashCode("synccfg") && m_Aura->m_CachedMaps.find(nameString) != m_Aura->m_CachedMaps.end()) {
          continue;
        }
        if (nameString.find(Payload) == string::npos) {
          continue;
        }

        CConfig MapCFG;
        MapCFG.Set("cfg_partial", "1");
        MapCFG.Set("map_path", R"(Maps\Download\)" + nameString);
        MapCFG.Set("map_localpath", nameString);
        if (nameString.find("_evrgrn3") != string::npos) {
          MapCFG.Set("map_site", "https://www.hiveworkshop.com/threads/351924/");
        } else {
          MapCFG.Set("map_site", "");
        }
        MapCFG.Set("map_url", "");
        if (nameString.find("_evrgrn3") != string::npos) {
          MapCFG.Set("map_shortdesc", "This map uses Warcraft 3: Reforged game mechanics.");
        } else {
          MapCFG.Set("map_shortdesc", "");
        }
        MapCFG.Set("downloaded_by", m_FromName);

        if (nameString.find("DotA") != string::npos)
          MapCFG.Set("map_type", "dota");

        CMap* ParsedMap = new CMap(m_Aura, &MapCFG, true);
        const bool isValid = ParsedMap->GetValid();
        if (!isValid) {
          Print("[AURA] warning - map [" + nameString + "] is not valid.");
          delete ParsedMap;
          badCounter++;
          continue;
        }

        string CFGName = "local-" + nameString + ".cfg";
        filesystem::path CFGPath = m_Aura->m_Config->m_MapCachePath / filesystem::path(CFGName);

        vector<uint8_t> OutputBytes = MapCFG.Export();
        FileWrite(CFGPath, OutputBytes.data(), OutputBytes.size());
        m_Aura->m_CachedMaps[nameString] = CFGName;
        goodCounter++;
      }
      SendReply("Initialized " + to_string(goodCounter) + " map config files. " + to_string(badCounter) + " invalid maps found.");
      return;
    }

    //
    // !COUNTMAPS
    // !COUNTCFGS
    //

    case HashCode("countmaps"):
    case HashCode("countcfgs"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      const auto MapCount = FilesMatch(m_Aura->m_Config->m_MapPath, FILE_EXTENSIONS_MAP).size();
      const auto CFGCount = FilesMatch(m_Aura->m_Config->m_MapCFGPath, FILE_EXTENSIONS_CONFIG).size();

      SendReply(to_string(MapCount) + " maps on disk, " + to_string(CFGCount) + " presets on disk, " + to_string(m_Aura->m_CachedMaps.size()) + " preloaded.");
      return;
    }

    //
    // !DELETECFG
    // !DELETEMAP
    //

    case HashCode("deletecfg"):
    case HashCode("deletemap"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      if (Payload.empty())
        return;

      string DeletionType = CommandHash == HashCode("deletecfg") ? "cfg" : "map";
      filesystem::path Folder = DeletionType == "cfg" ? m_Aura->m_Config->m_MapCFGPath : m_Aura->m_Config->m_MapPath;

      if ((DeletionType == "cfg" && !IsValidCFGName(Payload)) || (DeletionType == "map" &&!IsValidMapName(Payload))) {
        ErrorReply("Removal failed");
        break;
      }
      filesystem::path FileFragment = Payload;
      if (FileFragment.is_absolute() || FileFragment != FileFragment.filename()) {
        ErrorReply("Removal failed");
        break;
      }

      string InvalidChars = "/\\\0\"'*?:|<>;,";
      if (FileFragment.string().find_first_of(InvalidChars) != string::npos) {
        ErrorReply("Removal failed");
        break;
      }

      filesystem::path TargetPath = (Folder / FileFragment).lexically_normal();
      if (TargetPath.parent_path() != Folder.parent_path() || PathHasNullBytes(TargetPath) || TargetPath.filename().empty()) {
        ErrorReply("Removal failed");
        break;
      }

      if (!FileDelete(TargetPath)) {
        ErrorReply("Removal failed");
        break;
      }
      SendReply("Deleted [" + Payload + "]");
      break;
    }

    //
    // !SAYGAME
    //

    case HashCode("saygameraw"):
    case HashCode("saygame"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      string::size_type MessageStart = Payload.find(',');
      if (MessageStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "saygame <GAMEID> , <MESSAGE>");
        break;
      }
      bool IsRaw = CommandHash == HashCode("saygameraw");
      string GameId = TrimString(Payload.substr(0, MessageStart));
      string Message = TrimString(Payload.substr(MessageStart + 1));
      if (!IsRaw) Message = "[ADMIN] " + Message;
      if (GameId == "*") {
        bool Success = false;
        if (m_Aura->m_CurrentLobby && !m_Aura->m_CurrentLobby->GetIsMirror()) {
          Success = true;
          m_Aura->m_CurrentLobby->SendAllChat(Message);
        }
        if (m_Aura->m_Games.size()) {
          Success = true;
          for (auto& targetGame : m_Aura->m_Games)
            targetGame->SendAllChat(Message);
        }
        if (!Success) {
          ErrorReply("No games found.");
          break;
        }
      } else {
        CGame* targetGame = GetTargetGame(GameId);
        if (!targetGame) {
          ErrorReply("Game [" + GameId + "] not found.");
          break;
        }
        if (targetGame->GetIsMirror()) {
          ErrorReply("Game [" + GameId + "] is a mirror game.");
          break;
        }
        targetGame->SendAllChat(Message);
      }
      break;
    }

    //
    // !DISABLE
    //

    case HashCode("disable"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      SendReply("Creation of new games has been disabled. (Active lobbies will not be unhosted.)");
      m_Aura->m_Config->m_Enabled = false;
      break;
    }

    //
    // !ENABLE
    //

    case HashCode("enable"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      SendReply("Creation of new games has been enabled");
      m_Aura->m_Config->m_Enabled = true;
      break;
    }

    //
    // !DISABLEPUB
    //

    case HashCode("disablepub"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may toggle public game creation.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "disablepub <REALM>");
        break;
      }
      if (targetRealm != m_SourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to toggle game creation in arbitrary realms.");
        break;
      }
      if (!targetRealm->m_Config->m_CommandCFG->m_Enabled) {
        ErrorReply("All commands are already completely disabled in " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      SendReply("Creation of new games has been temporarily disabled for non-staff at " + targetRealm->GetCanonicalDisplayName() + ". (Active lobbies will not be unhosted.)");
      targetRealm->m_Config->m_CommandCFG->m_HostPermissions = COMMAND_PERMISSIONS_VERIFIED;
      break;
    }

    //
    // !ENABLEPUB
    //

    case HashCode("enablepub"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may toggle public game creation.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "enablepub <REALM>");
        break;
      }
      if (targetRealm != m_SourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to toggle game creation in arbitrary realms.");
        break;
      }
      if (!targetRealm->m_Config->m_CommandCFG->m_Enabled) {
        ErrorReply("All commands are completely disabled in " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      SendReply("Creation of new games has been enabled for non-staff at " + targetRealm->GetCanonicalDisplayName());
      targetRealm->m_Config->m_CommandCFG->m_HostPermissions = COMMAND_PERMISSIONS_VERIFIED;
      break;
    }

    //
    // !MAPTRANSFERS
    //

    case HashCode("setdownloads"):
    case HashCode("maptransfers"): {
      if (Payload.empty()) {
        if (m_Aura->m_Net->m_Config->m_AllowTransfers == MAP_TRANSFERS_NEVER) {
          SendReply("Map transfers are disabled");
        } else if (m_Aura->m_Net->m_Config->m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC) {
          SendReply("Map transfers are enabled");
        } else {
          SendReply("Map transfers are set to manual");
        }
        break;
      }

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      optional<int32_t> TargetValue;
      try {
        TargetValue = stol(Payload);
      } catch (...) {
      }

      if (!TargetValue.has_value()) {
        ErrorReply("Usage: " + cmdToken + "maptransfers <MODE>: Mode is 0/1/2.");
        break;
      }

      if (TargetValue.value() != MAP_TRANSFERS_NEVER && TargetValue.value() != MAP_TRANSFERS_AUTOMATIC && TargetValue.value() != MAP_TRANSFERS_MANUAL) {
        ErrorReply("Usage: " + cmdToken + "maptransfers <MODE>: Mode is 0/1/2.");
        break;
      }

      m_Aura->m_Net->m_Config->m_AllowTransfers = static_cast<uint8_t>(TargetValue.value());
      if (m_Aura->m_Net->m_Config->m_AllowTransfers == MAP_TRANSFERS_NEVER) {
        SendAll("Map transfers disabled.");
      } else if (m_Aura->m_Net->m_Config->m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC) {
        SendAll("Map transfers enabled.");
      } else {
        SendAll("Map transfers set to manual.");
      }
      break;
    }

    //
    // !RELOAD
    //

    case HashCode("reload"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      SendReply("Reloading configuration files...");
      if (m_Aura->ReloadConfigs()) {
        SendReply("Reloaded successfully.");
      } else {
        ErrorReply("Reload failed. See the console output.");
      }
      return;
    }

    //
    // !EXIT
    // !QUIT
    //

    case HashCode("exit"):
    case HashCode("quit"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      if (m_Aura->m_CurrentLobby || !m_Aura->m_Games.empty()) {
        if (Payload != "force" && Payload != "f") {
          ErrorReply("At least one game is in progress, or a lobby is hosted. Use '"+ cmdToken + "exit force' to shutdown anyway");
          break;
        }
      }
      m_Aura->m_Exiting = true;
      break;
    }

    //
    // !RESTART
    //

    case HashCode("restart"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      if (m_Aura->m_CurrentLobby || !m_Aura->m_Games.empty()) {
        if (Payload != "force" && Payload != "f") {
          ErrorReply("At least one game is in progress, or a lobby is hosted. Use '" + cmdToken + "restart force' to restart anyway");
          break;
        }
      }

      m_Aura->m_Exiting = true;

      // gRestart is defined in aura.cpp
      extern bool gRestart;
      gRestart = true;
      break;
    }

    /*********************
     * ROOTADMIN COMMANDS *
     *********************/

    //
    // !CHECKSTAFF
    //

    case HashCode("checkadmin"):
    case HashCode("checkstaff"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list staff.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "checkstaff <NAME>");
        break;
      }

      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      bool IsRootAdmin = m_SourceRealm->GetIsAdmin(Payload);
      bool IsAdmin = IsRootAdmin || m_SourceRealm->GetIsModerator(Payload);
      if (!IsAdmin && !IsRootAdmin)
        SendReply("User [" + Payload + "] is not staff on server [" + m_ServerName + "]");
      else if (IsRootAdmin)
        SendReply("User [" + Payload + "] is a root admin on server [" + m_ServerName + "]");
      else
        SendReply("User [" + Payload + "] is a moderator on server [" + m_ServerName + "]");

      break;
    }

    //
    // !LISTSTAFF
    //

    case HashCode("liststaff"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list staff.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "liststaff <REALM>");
        break;
      }
      if (targetRealm != m_SourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to list staff in arbitrary realms.");
        break;
      }
      vector<string> admins = vector<string>(targetRealm->m_Config->m_Admins.begin(), targetRealm->m_Config->m_Admins.end());
      vector<string> moderators = m_Aura->m_DB->ListModerators(m_SourceRealm->GetDataBaseID());
      if (admins.empty() && moderators.empty()) {
        ErrorReply("No staff has been designated in " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      if (!admins.empty()) SendReply("Root admins: " + JoinVector(admins, false));
      if (!moderators.empty()) SendReply("Moderators: " + JoinVector(moderators, false));
      break;
    }

    //
    // !STAFF
    //

    case HashCode("admin"):
    case HashCode("staff"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        if (m_SourceGame && !m_SourceGame->m_OwnerLess) {
          ErrorReply("Only root admins may add staff. Did you mean to acquire control of this game? Use " + cmdToken + "owner");
        } else {
          ErrorReply("Only root admins may add staff.");
        }
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "addstaff <NAME>");
        break;
      }
      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (m_SourceRealm->GetIsModerator(Payload)) {
        ErrorReply("User [" + Payload + "] is already staff on server [" + m_ServerName + "]");
        break;
      }
      if (!m_Aura->m_DB->ModeratorAdd(m_SourceRealm->GetDataBaseID(), Payload)) {
        ErrorReply("Failed to add user [" + Payload + "] as moderator [" + m_ServerName + "]");
        break;
      }
      SendReply("Added user [" + Payload + "] to the moderator database on server [" + m_ServerName + "]");
      break;
    }

    //
    // !DELSTAFF
    //

    case HashCode("delstaff"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may change staff.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "delstaff <NAME>");
        break;
      }

      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (m_SourceRealm->GetIsAdmin(Payload)) {
        ErrorReply("User [" + Payload + "] is a root admin on server [" + m_ServerName + "]");
        break;
      }
      if (!m_SourceRealm->GetIsModerator(Payload)) {
        ErrorReply("User [" + Payload + "] is not staff on server [" + m_ServerName + "]");
        break;
      }
      if (!m_Aura->m_DB->ModeratorRemove(m_SourceRealm->GetDataBaseID(), Payload)) {
        ErrorReply("Error deleting user [" + Payload + "] from the moderator database on server [" + m_ServerName + "]");
        break;
      }
      SendReply("Deleted user [" + Payload + "] from the moderator database on server [" + m_ServerName + "]");
      break;
    }

    /*********************
     * ADMIN COMMANDS *
     *********************/

    //
    // !LOADCFG (load config file)
    //

    case HashCode("loadcfg"): {
      if (Payload.empty()) {
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          SendReply("There is no map/config file loaded.");
          break;
        }
        SendReply("The currently loaded map/config file is: [" + m_Aura->m_GameSetup->GetInspectName() + "]");
        break;
      }
      if (!CheckPermissions(m_Config->m_HostPermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to host games.");
        break;
      }
      if (m_Aura->m_GameSetup && m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("Another user is hosting a map.");
        break;
      }

      if (!FileExists(m_Aura->m_Config->m_MapCFGPath)) {
        ErrorReply("Map config path doesn't exist", CHAT_LOG_CONSOLE);
        break;
      }

      CGameSetup* gameSetup = new CGameSetup(m_Aura, this, Payload, SEARCH_TYPE_ONLY_CONFIG, SETUP_PROTECT_ARBITRARY_TRAVERSAL, SETUP_PROTECT_ARBITRARY_TRAVERSAL, true, true);
      if (!gameSetup) {
        ErrorReply("Unable to host game");
        break;
      }
      gameSetup->SetActive();
      gameSetup->LoadMap();
      break;
    }

    //
    // !FLUSHDNS
    //

    case HashCode("flushdns"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      m_Aura->m_Net->FlushDNSCache();
      SendReply("Cleared DNS entries");
      break;
    }

    //
    // !NETINFO
    //

    case HashCode("netinfo"): {
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "netinfo <REALM>");
        break;
      }
      if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Requires sudo permissions."); // But not really
        break;
      }

      targetRealm->QueueCommand("/netinfo");
      break;
    }

    //
    // !PRINTGAMES
    //

    case HashCode("printgames"): {
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "printgames <REALM>");
        break;
      }
      if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Requires sudo permissions."); // But not really
        break;
      }

      targetRealm->QueueCommand("/games");
      break;
    }

    //
    // !QUERYGAMES
    //

    case HashCode("querygames"): {
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "querygames <REALM>");
        break;
      }
      if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Requires sudo permissions."); // But not really
        break;
      }

      int64_t Time = GetTime();
      if (Time - targetRealm->m_LastGameListTime >= 30) {
        targetRealm->SendGetGamesList();
      }
      break;
    }

    // !CHANNEL (change channel)
    //

    case HashCode("channel"): {
      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to invite the bot to another channel.");
        break;
      }
      if (m_Aura->m_CurrentLobby)  {
        ErrorReply("Cannot join a chat channel while hosting a lobby.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "channel <CHANNEL>");
        break;
      }
      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (!m_SourceRealm->QueueCommand("/join " + Payload)) {
        ErrorReply("Failed to join channel.");
        break;
      }
      break;
    }

    //
    // !LISTGAMES
    //

    case HashCode("listgames"):
    case HashCode("getgames"): {
      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to list games.");
        break;
      }
      vector<string> CurrentGames;
      if (m_Aura->m_CurrentLobby) {
        CurrentGames.push_back(string("Lobby: ") + m_Aura->m_CurrentLobby->GetDescription());
      }
      for (size_t i = 0; i < m_Aura->m_Games.size(); ++i) {
        CGame* game = m_Aura->m_Games[i];
        CurrentGames.push_back(string("Game#") + to_string(i) + ": " + game->GetDescription());
      }
      if (CurrentGames.empty()) {
        SendReply("No games are active.");
        break;
      }
      SendReply(JoinVector(CurrentGames, false), CHAT_LOG_CONSOLE);
      break;
    }

    //
    // !MAP (load map file)
    // !HOST (create game)
    //

    case HashCode("map"):
    case HashCode("load"):
    case HashCode("host"): {
      if (!CheckPermissions(m_Config->m_HostPermissions, (
        m_SourceGame == m_Aura->m_CurrentLobby && m_Aura->m_CanReplaceLobby ?
        COMMAND_PERMISSIONS_UNVERIFIED :
        COMMAND_PERMISSIONS_ADMIN
      ))) {
        ErrorReply("Not allowed to host games.");
        break;
      }
      bool isHostCommand = CommandHash == HashCode("host");
      vector<string> Args = isHostCommand ? SplitArgs(Payload, 1, 6) : SplitArgs(Payload, 1, 5);

      if (Args.empty() || Args[0].empty() || (isHostCommand && Args[Args.size() - 1].empty())) {
        if (isHostCommand) {
          ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <GAME NAME>");
          if (m_Player || !m_SourceRealm || m_SourceRealm->GetIsFloodImmune()) {
            ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <OBSERVERS> , <GAME NAME>");
            ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <OBSERVERS> , <VISIBILITY> , <GAME NAME>");
            ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <OBSERVERS> , <VISIBILITY> , <RANDOM RACES> , <GAME NAME>");
            ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <OBSERVERS> , <VISIBILITY> , <RANDOM RACES> , <RANDOM HEROES> , <GAME NAME>");
          }
          break;
        }
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          SendReply("There is no map/config file loaded.", CHAT_SEND_SOURCE_ALL);
          break;
        }
        SendReply("The currently loaded map/config file is: [" + m_Aura->m_GameSetup->GetInspectName() + "]", CHAT_SEND_SOURCE_ALL);
        break;
      }
      if (m_SourceRealm && m_Aura->m_GameVersion != m_SourceRealm->GetGameVersion() &&
        find(m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.begin(), m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end(), m_SourceRealm->GetGameVersion()) == m_Aura->m_GameDefaultConfig->m_SupportedGameVersions.end()
        && !GetIsSudo()) {
        ErrorReply("Hosting games on v1." + to_string(m_SourceRealm->GetGameVersion()) + " is disabled.");
        break;
      }
      if (m_Aura->m_GameSetup && m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("Another user is hosting a game.");
        break;
      }
      if (m_Aura->m_CurrentLobby && (!m_Aura->m_CanReplaceLobby || (!GetIsSudo() && (
        // This block defines under which circumstances a replaceable lobby can actually be replaced.
        // Do not allow replacements when using !host from another game, unless sudo.
        (m_SourceGame && m_SourceGame != m_Aura->m_CurrentLobby)
        // The following check defines whether players in the replaceable lobby get priority,
        // or external users (PvPGN/IRC/Discord) get it.
        // By commenting it out, external users get priority.
        // (Replaceable lobbies are NOT admin games.)
        //|| (!m_SourceGame && m_Aura->m_CurrentLobby->GetHasAnyPlayer())
      )))) {
        ErrorReply("Another user is hosting a game.");
        break;
      }

      string gameName;
      if (isHostCommand) {
        if (Args.size() >= 2) {
          gameName = Args[Args.size() - 1];
          Args.pop_back();
        } else {
          gameName = m_FromName + "'s " + Args[0];
          if (gameName.length() > m_Aura->m_MaxGameNameSize) {
            gameName = m_FromName + "'s game";
            if (gameName.length() > m_Aura->m_MaxGameNameSize) {
              ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <GAME NAME>");
              break;
            }
          }
        }
      }
      CGameExtraOptions* options = new CGameExtraOptions();
      if (2 <= Args.size()) options->ParseMapObservers(Args[1]);
      if (3 <= Args.size()) options->ParseMapVisibility(Args[2]);
      if (4 <= Args.size()) options->ParseMapRandomRaces(Args[3]);
      if (5 <= Args.size()) options->ParseMapRandomHeroes(Args[4]);

      CGameSetup* gameSetup = new CGameSetup(m_Aura, this, Args[0], SEARCH_TYPE_ANY, SETUP_PROTECT_ARBITRARY_TRAVERSAL, SETUP_PROTECT_ARBITRARY_TRAVERSAL, isHostCommand /* lucky mode */, true /* skip version check for convenience */);
      if (!gameSetup) {
        delete options;
        ErrorReply("Unable to host game", CHAT_SEND_SOURCE_ALL);
        break;
      }
      if (isHostCommand) {
        gameSetup->SetMapReadyCallback(MAP_ONREADY_HOST, gameName);
      }
      gameSetup->SetMapExtraOptions(options);
      gameSetup->SetActive();
      gameSetup->LoadMap();
      break;
    }

    //
    // !MIRROR (mirror game from another server)
    //

    case HashCode("mirror"): {
      if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Not allowed to mirror games.");
        break;
      }

      if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("A map must first be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + cmdToken + "mirror <EXCLUDESERVER> , <IP> , <PORT> , <GAMEID> , <GAMEKEY> , <GAMENAME> - GAMEID, GAMEKEY expected hex.");
        break;
      }

      if (m_Aura->m_CurrentLobby) {
        ErrorReply("Already hosting a game.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 6);

      uint16_t gamePort = 6112;
      uint32_t gameHostCounter = 1;

      CRealm* excludedServer = m_Aura->GetRealmByInputId(Args[0]);

      optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(Args[1], ACCEPT_IPV4);
      if (!maybeAddress.has_value()) {
        ErrorReply("Not a IPv4 address.");
        break;
      }

      try {
        gamePort = static_cast<uint16_t>(stoul(Args[2]));
        size_t posId;
        gameHostCounter = stoul(Args[3], &posId, 16);
        if (posId != Args[3].length()) {
          ErrorReply("Usage: " + cmdToken + "mirror <EXCLUDESERVER> , <IP> , <PORT> , <GAMEID> , <GAMEKEY> , <GAMENAME> - GAMEID expected hex.");
          break;
        }
      } catch (...) {
        ErrorReply("Usage: " + cmdToken + "mirror <EXCLUDESERVER> , <IP> , <PORT> , <GAMEID> , <GAMEKEY> , <GAMENAME> - GAMEID expected hex.");
        break;
      }

      string gameName = Args[5];
      SetAddressPort(&(maybeAddress.value()), gamePort);
      m_Aura->m_GameSetup->SetContext(this);
      m_Aura->m_GameSetup->SetMirrorSource(maybeAddress.value(), gameHostCounter);
      m_Aura->m_GameSetup->SetBaseName(gameName);
      if (excludedServer) m_Aura->m_GameSetup->AddIgnoredRealm(excludedServer);
      m_Aura->m_GameSetup->RunHost();
      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet != excludedServer && !bnet->GetIsMirror()) {
          bnet->ResetConnection(false);
          bnet->SetReconnectNextTick(true);
        }
      }
      break;
    }

    case HashCode("nick"): {
      if (!m_IRC) {
        ErrorReply("This is an IRC-exclusive command.");
        break;
      }
      if (0 == (m_Permissions & USER_PERMISSIONS_CHANNEL_ADMIN)) {
        ErrorReply("You are not allowed to change my nickname.");
        break;
      }
      m_IRC->Send("NICK :" + Payload);
      m_IRC->m_NickName = Payload;
      break;
    }

    case HashCode("discord"): {
      if (!m_Aura->m_Discord->m_Client) {
        ErrorReply("Not connected to Discord.");
        break;
      }

      if (m_Aura->m_Discord->m_Config->m_InviteUrl.empty()) {
        ErrorReply("This bot is invite-only. Ask the owner for an invitation link.");
        break;
      }

      switch (m_Aura->m_Discord->m_Config->m_FilterJoinServersMode) {
        case FILTER_DENY_ALL:
          SendReply("Install me to your user (DM-only) at <" + m_Aura->m_Discord->m_Config->m_InviteUrl + ">");
          break;
        case FILTER_ALLOW_LIST:
          SendReply("Install me to your server (requires approval) at <" + m_Aura->m_Discord->m_Config->m_InviteUrl + ">");
          break;
        default:
          SendReply("Install me to your server at <" + m_Aura->m_Discord->m_Config->m_InviteUrl + ">");
      }
      break;
    }

    case HashCode("v"): {
      SendReply("v" + cmdToken);
      break;
    }

    case HashCode("afk"):
    case HashCode("unready"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || !m_Player)
        break;

      if (m_TargetGame->GetCountDownStarted()/* && !m_TargetGame->GetCountDownUserInitiated()*/) {
        // Stopping the countdown here MAY be sensible behavior,
        // but only if it's not a manually initiated countdown, and if ...
        break;
      }

      uint8_t readyMode = m_TargetGame->GetPlayersReadyMode();
      bool isAlwaysReadyMode = readyMode == READY_MODE_FAST;
      if (readyMode == READY_MODE_EXPECT_RACE) {
        if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
          isAlwaysReadyMode = true;
        } else if (m_TargetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
          isAlwaysReadyMode = true;
        }
      }
      if (isAlwaysReadyMode) {
        ErrorReply("You are always assumed to be ready. Please don't go AFK.");
        break;
      }
      m_Player->SetUserReady(false);
      SendAll("Player [" + m_FromName + "] no longer ready to start the game. When you are, use " + cmdToken + "ready");
      break;
    }

    case HashCode("ready"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || !m_Player) {
        break;
      }

      if (m_TargetGame->GetCountDownStarted()) {
        break;
      }

      uint8_t readyMode = m_TargetGame->GetPlayersReadyMode();
      bool isAlwaysReadyMode = readyMode == READY_MODE_FAST;
      if (readyMode == READY_MODE_EXPECT_RACE) {
        if (m_TargetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
          isAlwaysReadyMode = true;
        } else if (m_TargetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
          isAlwaysReadyMode = true;
        }
      }
      if (isAlwaysReadyMode) {
        ErrorReply("You are always assumed to be ready. Please don't go AFK.");
        break;
      }
      m_Player->SetUserReady(true);
      SendAll("Player [" + m_FromName + "] ready to start the game. Please don't go AFK.");
      break;
    }

    case HashCode("checkready"):
    case HashCode("askready"):
    case HashCode("readystatus"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobby()) {
        break;
      }
      if (m_TargetGame->GetCountDownStarted()) {
        break;
      }
      SendReply(m_TargetGame->GetReadyStatusText());
      break;
    }

    default: {
      bool hasLetter = false;
      for (const auto& c : command) {
        if (97 <= c && c <= 122) {
          hasLetter = true;
          break;
        }
      }
      if (hasLetter) {
        ErrorReply("Unrecognized command [" + command + "].");
      }
      break;
    }
  }
}


CCommandContext::~CCommandContext()
{
  m_Aura = nullptr;
  m_SourceGame = nullptr;
  m_TargetGame = nullptr;
  m_Player = nullptr;
  m_SourceRealm = nullptr;
  m_TargetRealm = nullptr;
  m_IRC = nullptr;
#ifndef DISABLE_DPP
  delete m_DiscordAPI;
  m_DiscordAPI = nullptr;
#endif
  m_Output = nullptr;
}
