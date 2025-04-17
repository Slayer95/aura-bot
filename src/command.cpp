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

#include "aura.h"
#include "auradb.h"
#include "action.h"
#include "protocol/bnet_protocol.h"
#include "command.h"
#include "config/config_commands.h"
#include "hash.h"
#include "realm.h"
#include "game.h"
#include "protocol/game_protocol.h"
#include "game_setup.h"
#include "game_user.h"
#include "integration/irc.h"
#include "map.h"
#include "net.h"
#include "realm_chat.h"

#include <random>
#include <tuple>
#include <future>

#ifndef DISABLE_DPP
#include <dpp/dpp.h>
#endif

#ifndef NDEBUG
#define CHECK_SERVICE_TYPE(T) \
  do {\
    CheckServiceType(T);\
  } while (0);
#else
#define CHECK_SERVICE_TYPE(T)
#endif

using namespace std;

//
// CCommandContext
//

/* In-game command */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, shared_ptr<CGame> game, GameUser::CGameUser* user, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),

    m_TargetGame(game),

    m_GameSource(GameSource(user)),
    m_ServiceSource(SERVICE_TYPE_LAN, user->GetName()), // may be changed to SERVICE_TYPE_REALM
    m_FromWhisper(false),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(USER_PERMISSIONS_NONE),

    m_ServerName(user->GetRealmHostName()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  shared_ptr<CRealm> sourceRealm = user->GetRealm(false);
  if (sourceRealm) {
    m_ServiceSource = ServiceUser(SERVICE_TYPE_REALM, m_ServiceSource.GetUser(), sourceRealm);
  }
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

/* Spectator command */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, shared_ptr<CGame> game, CAsyncObserver* spectator, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),

    m_TargetGame(game),

    m_GameSource(GameSource(spectator)),
    m_ServiceSource(SERVICE_TYPE_LAN, spectator->GetName()), // may be changed to SERVICE_TYPE_REALM
    m_FromWhisper(false),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(USER_PERMISSIONS_NONE),

    m_ServerName(string()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  shared_ptr<CRealm> sourceRealm = spectator->GetRealm();
  if (sourceRealm) {
    m_ServiceSource = ServiceUser(SERVICE_TYPE_REALM, m_ServiceSource.GetUser(), sourceRealm);
    m_ServerName = sourceRealm->GetServer();
  }
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

/* Command received from BNET but targetting a game */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, shared_ptr<CGame> targetGame, shared_ptr<CRealm> fromRealm, const string& fromName, const bool& isWhisper, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_TargetGame(targetGame),

    m_GameSource(GameSource()),
    m_ServiceSource(SERVICE_TYPE_REALM, fromName, fromRealm),
    m_FromWhisper(isWhisper),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(USER_PERMISSIONS_NONE),

    m_ServerName(fromRealm->GetServer()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  m_Aura->m_ActiveContexts.push_back(weak_from_this());

  if (!isWhisper) {
    m_ServiceSource.AddNested(fromRealm->GetCurrentChannel());
  }
}

/* Command received from IRC but targetting a game */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, shared_ptr<CGame> targetGame, const string& channelName, const string& userName, const bool& isWhisper, const string& reverseHostName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_TargetGame(targetGame),

    m_GameSource(GameSource()),
    m_ServiceSource(SERVICE_TYPE_IRC, userName),
    m_FromWhisper(isWhisper),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(USER_PERMISSIONS_NONE),

    m_ServerName(nAura->m_IRC.m_Config.m_HostName),
    m_ReverseHostName(reverseHostName),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
  m_ServiceSource.AddNested(channelName);
}

#ifndef DISABLE_DPP
/* Command received from Discord but targetting a game */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, shared_ptr<CGame> targetGame, dpp::slashcommand_t* discordAPI, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_TargetGame(targetGame),

    m_GameSource(GameSource()),
    m_ServiceSource(SERVICE_TYPE_DISCORD, discordAPI->command.get_issuing_user().id, discordAPI->command.get_issuing_user().username, discordAPI),
    m_FromWhisper(false),
    m_IsBroadcast(true),

    m_Permissions(USER_PERMISSIONS_NONE),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  string serverName, channelName;
  dpp::snowflake serverId, channelId;
  try {
    serverName = discordAPI->command.get_guild().name;
    channelName = discordAPI->command.get_channel().name;
    serverId = discordAPI->command.get_guild().id;
    channelId = discordAPI->command.get_channel().id;
  } catch (...) {
    m_FromWhisper = true;
    serverName = "users.discord.com";
  }
  m_ServerName = serverName;

  if (m_FromWhisper) {
    Print("[DISCORD] Received slash command on " + GetSender() + "'s DM");
  } else {
    m_ServiceSource.AddNested(serverName, (uint64_t)serverId)->AddNested(channelName, (uint64_t)channelId);
    Print("[DISCORD] Received slash command in " + serverName + "'s server - channel " + channelName);
  }
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}
#endif

/* Command received from elsewhere but targetting a game */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, shared_ptr<CGame> targetGame, const string& nFromName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),
    m_TargetGame(targetGame),

    m_GameSource(GameSource()),
    m_ServiceSource(SERVICE_TYPE_CLI, nFromName),
    m_FromWhisper(false),
    m_IsBroadcast(nIsBroadcast),

    m_Permissions(USER_PERMISSIONS_NONE),

    m_ServerName(string()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

/* BNET command */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, shared_ptr<CRealm> fromRealm, const string& fromName, const bool& isWhisper, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),

    m_GameSource(GameSource()),
    m_ServiceSource(SERVICE_TYPE_REALM, fromName, fromRealm),
    m_FromWhisper(isWhisper),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(USER_PERMISSIONS_NONE),

    m_ServerName(fromRealm->GetServer()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  m_Aura->m_ActiveContexts.push_back(weak_from_this());

  if (!isWhisper) {
    m_ServiceSource.AddNested(fromRealm->GetCurrentChannel());
  }
}

/* IRC command */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, const string& channelName, const string& userName, const bool& isWhisper, const string& reverseHostName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),

    m_GameSource(GameSource()),
    m_ServiceSource(SERVICE_TYPE_IRC, userName),
    m_FromWhisper(isWhisper),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(USER_PERMISSIONS_NONE),

    m_ServerName(nAura->m_IRC.m_Config.m_HostName),
    m_ReverseHostName(reverseHostName),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
  m_ServiceSource.AddNested(channelName);
}

#ifndef DISABLE_DPP
/* Discord command */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, CCommandConfig* config, dpp::slashcommand_t* discordAPI, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(config),

    m_GameSource(GameSource()),
    m_ServiceSource(SERVICE_TYPE_DISCORD, discordAPI->command.get_issuing_user().id, discordAPI->command.get_issuing_user().username, discordAPI),
    m_FromWhisper(false),
    m_IsBroadcast(true),
    m_Permissions(USER_PERMISSIONS_NONE),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  string serverName, channelName;
  dpp::snowflake serverId, channelId;
  try {
    serverName = discordAPI->command.get_guild().name;
    channelName = discordAPI->command.get_channel().name;
    serverId = discordAPI->command.get_guild().id;
    channelId = discordAPI->command.get_channel().id;
  } catch (...) {
    m_FromWhisper = true;
    serverName = "users.discord.com";
  }
  m_ServerName = serverName;

  if (m_FromWhisper) {
    Print("[DISCORD] Received slash command on " + GetSender() + "'s DM");
  } else {
    m_ServiceSource.AddNested(serverName, (uint64_t)serverId)->AddNested(channelName, (uint64_t)channelId);
    Print("[DISCORD] Received slash command in " + serverName + "'s server - channel " + channelName);
  }
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}
#endif

/* Generic command */
CCommandContext::CCommandContext(uint8_t serviceType, CAura* nAura, const string& nFromName, const bool& nIsBroadcast, ostream* nOutputStream)
  : m_Aura(nAura),
    m_Config(nAura->m_CommandDefaultConfig),

    m_GameSource(GameSource()),
    m_ServiceSource(SERVICE_TYPE_CLI, nFromName),

    m_FromWhisper(false),
    m_IsBroadcast(nIsBroadcast),
    m_Permissions(USER_PERMISSIONS_NONE),

    m_ServerName(string()),

    m_Output(nOutputStream),
    m_PartiallyDestroyed(false)
{
  CHECK_SERVICE_TYPE(serviceType);
  m_Aura->m_ActiveContexts.push_back(weak_from_this());
}

#undef CHECK_SERVICE_TYPE

void CCommandContext::SetIdentity(const string& userName)
{
  GetServiceSource().SetName(userName);
}

string CCommandContext::GetUserAttribution()
{
  switch (GetServiceSourceType()) {
    case SERVICE_TYPE_IRC:
      return GetSender() + "@" + m_ServerName;
    case SERVICE_TYPE_DISCORD:
      return GetSender() + "@[" + m_ServerName + "].discord.com";
    default: {
      if (auto sourceRealm = GetSourceRealm()) {
        return GetSender() + "@" + sourceRealm->GetServer();
      }
      if (!GetGameSource().GetIsEmpty()) {
        return GetSender() + "@" + ToFormattedRealm(m_ServerName);
      }
      return GetServiceSource().GetUserOrAnon();
    }
  }
}

string CCommandContext::GetUserAttributionPreffix()
{
  switch (GetServiceSourceType()) {
    case SERVICE_TYPE_IRC:
      return "[IRC] User [" + GetSender() + "] (Mode " + ToHexString(m_Permissions) + ") ";
    case SERVICE_TYPE_DISCORD:
      return "[DISCORD] User [" + GetSender() + "]@[" + m_ServerName + "].discord.com (Mode " + ToHexString(m_Permissions) + ") ";
    default: {
      if (auto sourceRealm = GetSourceRealm()) {
        return sourceRealm->GetLogPrefix() + "User [" + GetSender() + "] (Mode " + ToHexString(m_Permissions) + ") ";
      }
      auto gameSource = GetGameSource();
      if (!gameSource.GetIsEmpty()) {
        return gameSource.GetGame()->GetLogPrefix() + "Player [" + GetSender() + "@" + ToFormattedRealm(m_ServerName) + "] (Mode " + ToHexString(m_Permissions) + ") ";
      }
      if (!GetIsAnonymous()) {
        return "[SYSTEM] User [" + GetSender() + "] (Mode " + ToHexString(m_Permissions) + ") ";
      }
      return "[ANONYMOUS] (Mode " + ToHexString(m_Permissions) + ") ";
    }
  }
}

CommandHistory* CCommandContext::GetCommandHistory() const
{
  if (InspectGameSource().GetIsEmpty()) {
    return nullptr;
  }

  CConnection* userOrSpectator = InspectGameSource().GetUserOrSpectator();
  if (GetIsGameUser()) {
    return static_cast<GameUser::CGameUser*>(userOrSpectator)->GetCommandHistory();
  } else {
    return static_cast<CAsyncObserver*>(userOrSpectator)->GetCommandHistory();
  }
}

string CCommandContext::GetChannelName() const
{
  const SimpleNestedLocation* subLoc = nullptr;
  switch (GetServiceSourceType()) {
    case SERVICE_TYPE_IRC:
    case SERVICE_TYPE_REALM:
      subLoc = InspectServiceSource().InspectSubLocation();
      if (subLoc) return subLoc->GetName();
      break;
    case SERVICE_TYPE_DISCORD:
      subLoc = InspectServiceSource().InspectSubLocation();
      if (subLoc) {
        subLoc = subLoc->InspectSubLocation();
        if (subLoc) return subLoc->GetName();
      }
      break;
    default:
      return string();
  }
  return string();
}

shared_ptr<CRealm> CCommandContext::GetSourceRealm() const
{
  if (m_ServiceSource.GetServiceType() != SERVICE_TYPE_REALM) {
    return nullptr;
  }
  return m_ServiceSource.GetService<CRealm>();
}

shared_ptr<CGame> CCommandContext::GetSourceGame() const
{
  if (m_GameSource.GetIsEmpty()) return nullptr;
  return m_GameSource.GetGame();
}

GameUser::CGameUser* CCommandContext::GetGameUser() const
{
  if (!InspectGameSource().GetIsUser()) return nullptr;
  return InspectGameSource().GetUser();
}

CConnection* CCommandContext::GetGameUserOrSpectator() const
{
  if (InspectGameSource().GetIsEmpty()) return nullptr;
  return InspectGameSource().GetUser();
}

void CCommandContext::SetAuthenticated(const bool& nAuthenticated)
{
  m_OverrideVerified = nAuthenticated;
}

void CCommandContext::SetPermissions(const uint16_t nPermissions)
{
  m_OverridePermissions = nPermissions;
}

void CCommandContext::UpdatePermissions()
{
  if (m_OverridePermissions.has_value()) {
    m_Permissions = m_OverridePermissions.value();
    return;
  }

  m_Permissions = USER_PERMISSIONS_NONE;

  if (GetServiceSourceType() == SERVICE_TYPE_IRC) {
    string::size_type suffixSize = m_Aura->m_IRC.m_Config.m_VerifiedDomain.length();
    if (suffixSize > 0) {
      if (m_ReverseHostName.length() > suffixSize &&
        m_ReverseHostName.substr(0, m_ReverseHostName.length() - suffixSize) == m_Aura->m_IRC.m_Config.m_VerifiedDomain
      ) {
        m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
      }
      const bool IsCreatorIRC = (
        !m_TargetGame.expired() && m_TargetGame.lock()->GetCreatedFromType() == SERVICE_TYPE_IRC
      );
      if ((m_TargetGame.expired() || IsCreatorIRC) && m_Aura->m_IRC.GetIsModerator(m_ReverseHostName)) m_Permissions |= USER_PERMISSIONS_CHANNEL_ADMIN;
      if (m_Aura->m_IRC.GetIsSudoer(m_ReverseHostName)) m_Permissions |= USER_PERMISSIONS_BOT_SUDO_SPOOFABLE;
    }
    return;
  }
  if (GetServiceSourceType() == SERVICE_TYPE_DISCORD) {
#ifndef DISABLE_DPP
    if (m_Aura->m_Discord.GetIsSudoer(GetServiceSource().GetUserIdentifier())) {
      m_Permissions = SET_USER_PERMISSIONS_ALL &~ (USER_PERMISSIONS_BOT_SUDO_OK);
    } else if (GetServiceSource().GetDiscordAPI()->command.get_issuing_user().is_verified()) {
      m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
    }
#endif
    return;
  }

  shared_ptr<CGame> targetGame = GetTargetGame();
  shared_ptr<CRealm> sourceRealm = GetSourceRealm();
  bool isRealmVerified = false;
  if (m_OverrideVerified.has_value()) {
    isRealmVerified = m_OverrideVerified.value();
  } else {
    isRealmVerified = !GetGameSource().GetIsEmpty() ? (GetIsGameUser() && GetGameUser()->GetIsRealmVerified()) : (sourceRealm != nullptr);
  }

  // Trust PvPGN servers on users identities for admin powers. Their impersonation is not a threat we worry about.
  // However, do NOT trust them regarding sudo access, since those commands may cause data deletion or worse.
  // Note also that sudo permissions must be ephemeral, since neither WC3 nor PvPGN TCP connections are secure.
  bool IsOwner = false;
  if (GetIsGameUser() && (&(GetGameUser()->m_Game.get()) == targetGame.get())) {
    IsOwner = GetGameUser()->GetIsOwner(m_OverrideVerified);
  } else if (targetGame) {
    IsOwner = isRealmVerified && targetGame->MatchOwnerName(GetSender()) && m_ServerName == targetGame->GetOwnerRealm();
  }
  bool IsCreatorRealm = targetGame && sourceRealm && targetGame->MatchesCreatedFromRealm(sourceRealm);
  bool IsRootAdmin = isRealmVerified && sourceRealm != nullptr && (m_TargetGame.expired() || IsCreatorRealm) && sourceRealm->GetIsAdmin(GetSender());
  bool IsAdmin = IsRootAdmin || (isRealmVerified && sourceRealm != nullptr && (m_TargetGame.expired() || IsCreatorRealm) && sourceRealm->GetIsModerator(GetSender()));
  bool IsSudoSpoofable = isRealmVerified && sourceRealm != nullptr && sourceRealm->GetIsSudoer(GetSender());

  // GOTCHA: Owners are always treated as players if the game hasn't started yet. Even if they haven't joined.
  if (GetIsGameUser() || (IsOwner && targetGame && targetGame->GetIsLobbyStrict())) {
    m_Permissions |= USER_PERMISSIONS_GAME_PLAYER;
  }

  // Leaver or absent owners are automatically demoted.
  if (isRealmVerified) {
    m_Permissions |= USER_PERMISSIONS_CHANNEL_VERIFIED;
  }
  if (IsOwner && (GetIsGameUser() || (targetGame && targetGame->GetIsLobbyStrict()))) {
    m_Permissions |= USER_PERMISSIONS_GAME_OWNER;
  }
  if (IsAdmin) m_Permissions |= USER_PERMISSIONS_CHANNEL_ADMIN;
  if (IsRootAdmin) m_Permissions |= USER_PERMISSIONS_CHANNEL_ROOTADMIN;

  // Sudo is a permission system separate from channels.
  if (IsSudoSpoofable) m_Permissions |= USER_PERMISSIONS_BOT_SUDO_SPOOFABLE;
  if (GetHasCommandHistory() && GetCommandHistory()->CheckSudoMode(m_Aura, GetSourceGame(), GetSender())) {
    m_Permissions = SET_USER_PERMISSIONS_ALL;
  }
}

void CCommandContext::CheckServiceType(uint8_t serviceType)
{
  // Runtime check that I didn't mess function signatures
  if (GetServiceSourceType() != serviceType) {
    Print("[COMMAND] error - command instantiated with type [" + ToDecString(serviceType) + "] but used signature for type [" + ToDecString(GetServiceSourceType()) + "]");
  }
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
      if (!m_TargetGame.expired() && !m_TargetGame.lock()->HasOwnerSet()) {
        result = (m_Permissions & (USER_PERMISSIONS_GAME_PLAYER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      } else {
        result = (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_BOT_SUDO_OK)) > 0;
      }
      break;
    case COMMAND_PERMISSIONS_START_GAME:
      if (!m_TargetGame.expired() && (m_TargetGame.lock()->m_PublicStart || !m_TargetGame.lock()->HasOwnerSet())) {
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

bool CCommandContext::CheckConfirmation(const string& cmdToken, const string& cmd, const string& target, const string& errorMessage)
{
  CommandHistory* cmdHistory = GetCommandHistory();
  if (!cmdHistory) {
    ErrorReply(errorMessage);
    return false;
  }

  string message = cmdToken + cmd + target;
  
  if (cmdHistory->GetLastCommand() == message) {
    cmdHistory->ClearLastCommand();
    return true;
  }

  cmdHistory->SetLastCommand(message);
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
    m_Aura->m_SudoContext.reset();
    m_Aura->m_SudoAuthTarget.clear();
    m_Aura->m_SudoExecCommand.clear();
    return Result;
  }
  bool isValidCaller = (
    GetSender() == m_Aura->m_SudoContext->GetSender() &&
    GetServiceSourceType() == m_Aura->m_SudoContext->GetServiceSourceType() &&
    m_TargetRealm.lock() == m_Aura->m_SudoContext->m_TargetRealm.lock() &&
    m_TargetGame.lock() == m_Aura->m_SudoContext->m_TargetGame.lock() &&
    (memcmp(&GetServiceSource(), &m_Aura->m_SudoContext->GetServiceSource(), sizeof(ServiceUser)) == 0) &&
    (memcmp(&GetGameSource(), &m_Aura->m_SudoContext->GetGameSource(), sizeof(GameSource)) == 0)
  );
  if (isValidCaller && message == m_Aura->m_SudoAuthTarget) {
    LogStream(*m_Output, "[AURA] Confirmed " + GetSender() + " command \"" + m_Aura->m_SudoExecCommand + "\"");
    size_t TargetStart = m_Aura->m_SudoExecCommand.find(' ');
    string cmd, target;
    if (TargetStart != string::npos) {
      cmd = m_Aura->m_SudoExecCommand.substr(0, TargetStart);
      target = m_Aura->m_SudoExecCommand.substr(TargetStart + 1);
    } else {
      cmd = m_Aura->m_SudoExecCommand;
    }
    Result.emplace(ToLowerCase(cmd), target);
    //m_Permissions |= USER_PERMISSIONS_BOT_SUDO_OK;
    m_Permissions = SET_USER_PERMISSIONS_ALL;
  }
  m_Aura->m_SudoContext.reset();
  m_Aura->m_SudoAuthTarget.clear();
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

  if (GetSourceGame()) {
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

  const shared_ptr<CGame> sourceGame = GetSourceGame();
  const shared_ptr<CGame> targetGame = GetTargetGame();
  const shared_ptr<CRealm> sourceRealm = GetSourceRealm();
  //const shared_ptr<CRealm> targetRealm = GetTargetRealm();

  if (GetIsGameUser()) {
    if (message.length() <= 100) {
      sourceGame->SendChat(GetGameUser(), message);
    } else {
      string leftMessage = message;
      do {
        sourceGame->SendChat(GetGameUser(), leftMessage.substr(0, 100));
        leftMessage = leftMessage.substr(100);
      } while (leftMessage.length() > 100);
      if (!leftMessage.empty()) {
        sourceGame->SendChat(GetGameUser(), leftMessage);
      }
    }
    return;
  }

  if (GetGameSource().GetIsSpectator()) {
    CAsyncObserver* spectator = GetGameSource().GetSpectator();
    if (message.length() <= 100) {
      spectator->SendChat(message);
    } else {
      string leftMessage = message;
      do {
        spectator->SendChat(leftMessage.substr(0, 100));
        leftMessage = leftMessage.substr(100);
      } while (leftMessage.length() > 100);
      if (!leftMessage.empty()) {
        spectator->SendChat(leftMessage);
      }
    }
    return;
  }

  switch (GetServiceSourceType()) {
    case SERVICE_TYPE_REALM:
      if (sourceRealm) {
        sourceRealm->TryQueueChat(message, GetSender(), true, shared_from_this(), ctxFlags);
      }
      break;
    case SERVICE_TYPE_IRC:
      m_Aura->m_IRC.SendUser(message, GetSender());
      break;
#ifndef DISABLE_DPP
    case SERVICE_TYPE_DISCORD:
      m_Aura->m_Discord.SendUser(message, GetServiceSource().GetUserIdentifier());
      break;
#endif
    default:
      LogStream(*m_Output, "[AURA] " + message);
  }
}

void CCommandContext::SendReplyCustomFlags(const string& message, const uint8_t ctxFlags) {
  bool AllTarget = ctxFlags & CHAT_SEND_TARGET_ALL;
  bool AllSource = ctxFlags & CHAT_SEND_SOURCE_ALL;
  bool AllSourceSuccess = false;

  const shared_ptr<CGame> sourceGame = GetSourceGame();
  const shared_ptr<CGame> targetGame = GetTargetGame();
  const shared_ptr<CRealm> sourceRealm = GetSourceRealm();
  const shared_ptr<CRealm> targetRealm = GetTargetRealm();

  if (AllTarget) {
    if (targetGame) {
      targetGame->SendAllChat(message);
      if (targetGame == sourceGame) {
        AllSourceSuccess = true;
      }
    }
    if (targetRealm) {
      targetRealm->TryQueueChat(message, GetSender(), false, shared_from_this(), ctxFlags);
      if (targetRealm == sourceRealm) {
        AllSourceSuccess = true;
      }
    }
    // IRC/Discord are not valid targets, only sources.
  }
  if (AllSource && !AllSourceSuccess && !m_FromWhisper) {
    if (sourceGame) {
      sourceGame->SendAllChat(message);
      AllSourceSuccess = true;
    }
    if (sourceRealm && !AllSourceSuccess) {
      sourceRealm->TryQueueChat(message, GetSender(), false, shared_from_this(), ctxFlags);
      AllSourceSuccess = true;
    }
    if (GetServiceSourceType() == SERVICE_TYPE_IRC) {
      m_Aura->m_IRC.SendChannel(message, GetChannelName());
      AllSourceSuccess = true;
    }
#ifndef DISABLE_DPP
    if (GetServiceSourceType() == SERVICE_TYPE_DISCORD) {
      GetServiceSource().GetDiscordAPI()->edit_original_response(dpp::message(message));
      AllSourceSuccess = true;
    }
#endif
  }
  if (!AllSourceSuccess) {
    SendPrivateReply(message, ctxFlags);
  }

  // Write to console if CHAT_LOG_INCIDENT, but only if we haven't written to it in SendPrivateReply
  if (GetServiceSourceType() != SERVICE_TYPE_CLI && (ctxFlags & CHAT_LOG_INCIDENT)) {
    if (!m_TargetGame.expired()) {
      LogStream(*m_Output, m_TargetGame.lock()->GetLogPrefix() + message);
    } else if (sourceRealm) {
      LogStream(*m_Output, sourceRealm->GetLogPrefix() + message);
    } else if (GetServiceSourceType() == SERVICE_TYPE_IRC) {
      LogStream(*m_Output, "[IRC] " + message);
    } else if (GetServiceSourceType() == SERVICE_TYPE_DISCORD) {
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

void CCommandContext::SendAllUnlessHidden(const string& message)
{
  if (m_TargetGame.expired()) {
    SendAll(message);
  } else {
    SendReply(message, m_TargetGame.lock()->GetIsLobbyStrict() || !m_TargetGame.lock()->GetIsHiddenPlayerNames() ? CHAT_SEND_TARGET_ALL : 0);
  }
}

GameUser::CGameUser* CCommandContext::GetTargetUser(const string& target)
{
  if (m_TargetGame.expired()) {
    return nullptr;
  }
  return GetTargetGame()->GetUserFromDisplayNamePartial(target).user;
}

GameUser::CGameUser* CCommandContext::RunTargetUser(const string& target)
{
  if (m_TargetGame.expired()) {
    return nullptr;
  }

  GameUserSearchResult searchResult = GetTargetGame()->GetUserFromDisplayNamePartial(target);
  if (searchResult.matchCount > 1) {
    ErrorReply("Player [" + target + "] ambiguous.");
  } else if (searchResult.matchCount == 1) {
    ErrorReply("Player [" + target + "] not found.");
  }
  return searchResult.user;
}

GameUser::CGameUser* CCommandContext::GetTargetUserOrSelf(const string& target)
{
  if (m_TargetGame.expired()) {
    return nullptr;
  }
  if (target.empty()) {
    return GetGameUser();
  }

  GameUserSearchResult searchResult = GetTargetGame()->GetUserFromDisplayNamePartial(target);
  return searchResult.user;
}

GameUser::CGameUser* CCommandContext::RunTargetUserOrSelf(const string& target)
{
  if (target.empty()) {
    return GetGameUser();
  }

  shared_ptr<const CGame> targetGame = GetTargetGame();
  if (!targetGame) {
    ErrorReply("Please specify target user.");
    return nullptr;
  }
  GameUserSearchResult searchResult = targetGame->GetUserFromDisplayNamePartial(target);
  if (!searchResult.user) {
    ErrorReply("Player [" + target + "] not found.");
  }
  return searchResult.user;
}

GameControllerSearchResult CCommandContext::GetParseController(const std::string& target)
{
  if (m_TargetGame.expired() || target.empty()) {
    return {};
  }
  shared_ptr<const CGame> targetGame = GetTargetGame();

  GameUser::CGameUser* user = nullptr;

  switch (target[0]) {
    case '#': {
      uint8_t testSID = ParseSID(target.substr(1));
      const CGameSlot* slot = targetGame->InspectSlot(testSID);
      if (!slot) {
        return {};
      }
      return GameControllerSearchResult(testSID, targetGame->GetUserFromUID(slot->GetUID()));
    }

    case '@': {
      user = GetTargetUser(target.substr(1));
      if (user == nullptr) {
        return {};
      }
      return GameControllerSearchResult(targetGame->GetSIDFromUID(user->GetUID()), user);
    }

    default: {
      uint8_t testSID = ParseSID(target);
      const CGameSlot* slot = targetGame->InspectSlot(testSID);
      GameUser::CGameUser* testPlayer = GetTargetUser(target.substr(1));
      if ((slot == nullptr) == (testPlayer == nullptr)) {
        return {};
      }
      if (testPlayer == nullptr) {
        return GameControllerSearchResult(testSID, targetGame->GetUserFromUID(slot->GetUID()));
      } else {
        return GameControllerSearchResult(targetGame->GetSIDFromUID(testPlayer->GetUID()), testPlayer);
      }
    }
  }
}

GameControllerSearchResult CCommandContext::RunParseController(const std::string& target)
{
  if (m_TargetGame.expired() || target.empty()) {
    ErrorReply("Please provide a user @name or #slot.");
    return {};
  }
  shared_ptr<const CGame> targetGame = GetTargetGame();

  GameUser::CGameUser* user = nullptr;

  switch (target[0]) {
    case '#': {
      uint8_t testSID = ParseSID(target.substr(1));
      const CGameSlot* slot = targetGame->InspectSlot(testSID);
      if (!slot) {
        ErrorReply("Slot " + ToDecString(testSID + 1) + " not found.");
        return {};
      }
      return GameControllerSearchResult(testSID, targetGame->GetUserFromUID(slot->GetUID()));
    }

    case '@': {
      user = RunTargetUser(target.substr(1));
      if (user == nullptr) {
        return {};
      }
      return GameControllerSearchResult(targetGame->GetSIDFromUID(user->GetUID()), user);
    }

    default: {
      uint8_t testSID = ParseSID(target);
      const CGameSlot* slot = targetGame->InspectSlot(testSID);
      GameUser::CGameUser* testPlayer = GetTargetUser(target.substr(1));
      if ((slot == nullptr) == (testPlayer == nullptr)) {
        ErrorReply("Please provide a user @name or #slot.");
        return {};
      }
      if (testPlayer == nullptr) {
        return GameControllerSearchResult(testSID, targetGame->GetUserFromUID(slot->GetUID()));
      } else {
        return GameControllerSearchResult(targetGame->GetSIDFromUID(testPlayer->GetUID()), testPlayer);
      }
    }
  }
}

optional<uint8_t> CCommandContext::GetParseNonPlayerSlot(const std::string& target)
{
  if (m_TargetGame.expired() || target.empty()) {
    return nullopt;
  }
  shared_ptr<const CGame> targetGame = GetTargetGame();

  uint8_t testSID = 0xFF;
  if (target[0] == '#') {
    testSID = ParseSID(target.substr(1));
  } else {
    testSID = ParseSID(target);
  }

  const CGameSlot* slot = targetGame->InspectSlot(testSID);
  if (!slot) {
    return nullopt;
  }
  if (targetGame->GetIsRealPlayerSlot(testSID)) {
    return nullopt;
  }

  return optional<uint8_t>(testSID);
}

optional<uint8_t> CCommandContext::RunParseNonPlayerSlot(const std::string& target)
{
  if (m_TargetGame.expired() || target.empty()) {
    ErrorReply("Please provide a user #slot.");
    return nullopt;
  }
  shared_ptr<const CGame> targetGame = GetTargetGame();

  uint8_t testSID = 0xFF;
  if (target[0] == '#') {
    testSID = ParseSID(target.substr(1));
  } else {
    testSID = ParseSID(target);
  }

  const CGameSlot* slot = targetGame->InspectSlot(testSID);
  if (!slot) {
    ErrorReply("Slot [" + target + "] not found.");
    return nullopt;
  }
  if (targetGame->GetIsRealPlayerSlot(testSID)) {
    ErrorReply("Slot is occupied by a player.");
    return nullopt;
  }

  return optional<uint8_t>(testSID);
}

shared_ptr<CRealm> CCommandContext::GetTargetRealmOrCurrent(const string& target)
{
  if (target.empty()) {
    return GetSourceRealm();
  }
  string realmId = TrimString(target);
  transform(begin(realmId), end(realmId), begin(realmId), [](char c) { return static_cast<char>(std::tolower(c)); });
  shared_ptr<CRealm> exactMatch = m_Aura->GetRealmByInputId(realmId);
  if (exactMatch) return exactMatch;
  return m_Aura->GetRealmByHostName(realmId);
}

RealmUserSearchResult CCommandContext::GetParseTargetRealmUser(const string& inputTarget, bool allowNoRealm, bool searchHistory)
{
  if (inputTarget.empty()) {
    return {};
  }

  string nameFragment;
  string realmFragment;
  shared_ptr<CRealm> realm = nullptr;

  string target = inputTarget;
  string::size_type realmStart = inputTarget.find('@');
  const bool isFullyQualified = realmStart != string::npos;
  if (isFullyQualified) {
    realmFragment = TrimString(inputTarget.substr(realmStart + 1));
    nameFragment = TrimString(inputTarget.substr(0, realmStart));
    if (!nameFragment.empty() && nameFragment.size() <= MAX_PLAYER_NAME_SIZE) {
      if (allowNoRealm && realmFragment.empty()) {
        return RealmUserSearchResult(nameFragment, realmFragment, realm);
      }
      realm = GetTargetRealmOrCurrent(realmFragment);
      if (realm) {
        realmFragment = realm->GetServer();
      }
      if (realm == nullptr) return RealmUserSearchResult(nameFragment, realmFragment);
      return RealmUserSearchResult(nameFragment, realmFragment, realm);
    }
    // Handle @PLAYER
    target = realmFragment;
    realmFragment.clear();
    nameFragment.clear();
    //isFullyQualified = false;
  }

  if (/*!isFullyQualified && */GetIsGameUser()) {
    auto sourceGame = GetSourceGame();
    if (!sourceGame || sourceGame->GetIsHiddenPlayerNames()) {
      return {};
    }
    if (searchHistory) {
      BannableUserSearchResult bannableUserSearchResult = sourceGame->GetBannableFromNamePartial(target);
      if (!bannableUserSearchResult.GetSuccess()) {
        return {};
      }
      realmFragment = bannableUserSearchResult.bannable->GetServer();
      nameFragment = bannableUserSearchResult.bannable->GetName();
    } else {
      GameUser::CGameUser* targetPlayer = GetTargetUser(target);
      if (!targetPlayer) {
        return {};
      }
      realmFragment = targetPlayer->GetRealmHostName();
      nameFragment = targetPlayer->GetName();
    }
  }

  if (GetServiceSourceType() == SERVICE_TYPE_REALM) {
    realmFragment = m_ServerName;
    nameFragment = TrimString(target);
  } else {
    return {};
  }

  if (nameFragment.empty() || nameFragment.size() > MAX_PLAYER_NAME_SIZE) {
    return {};
  }

  if (realmFragment.empty()) {
    return RealmUserSearchResult(nameFragment, realmFragment, realm);
  } else {
    realm = GetTargetRealmOrCurrent(realmFragment);
    if (realm) {
      realmFragment = realm->GetServer();
    }
    if (realm == nullptr) return RealmUserSearchResult(nameFragment, realmFragment);
    return RealmUserSearchResult(nameFragment, realmFragment, realm);
  }
}

uint8_t CCommandContext::GetParseTargetServiceUser(const std::string& target, std::string& nameFragment, std::string& locationFragment, void*& location)
{
  RealmUserSearchResult realmSearchResult = GetParseTargetRealmUser(target);
  if (realmSearchResult.GetSuccess()) {
    nameFragment = realmSearchResult.userName;
    locationFragment = realmSearchResult.hostName;
    location = realmSearchResult.GetRealm().get();
    return SERVICE_TYPE_REALM;
  }
  shared_ptr<CGame> matchingGame = GetTargetGame(locationFragment);
  if (matchingGame) {
    if (nameFragment.size() > MAX_PLAYER_NAME_SIZE) {
      location = reinterpret_cast<CGame*>(matchingGame.get());
      return SERVICE_TYPE_GAME;
    }
  }
  return SERVICE_TYPE_NONE;
}

shared_ptr<CGame> CCommandContext::GetTargetGame(const string& rawInput)
{
  return m_Aura->GetGameByString(rawInput);
}

void CCommandContext::UseImplicitReplaceable()
{
  if (!m_TargetGame.expired()) return;

  for (auto it = m_Aura->m_Lobbies.rbegin(); it != m_Aura->m_Lobbies.rend(); ++it) {
    if ((*it)->GetIsReplaceable() && !(*it)->GetCountDownStarted()) {
      m_TargetGame = *it;
    }
  }

  if (!m_TargetGame.expired() && !GetIsSudo()) {
    UpdatePermissions();
  }
}

void CCommandContext::UseImplicitHostedGame()
{
  if (!m_TargetGame.expired()) return;

  m_TargetGame = m_Aura->GetMostRecentLobbyFromCreator(GetSender());
  if (!m_TargetGame.expired()) m_TargetGame = m_Aura->GetMostRecentLobby();

  if (!m_TargetGame.expired() && !GetIsSudo()) {
    UpdatePermissions();
  }
}

void CCommandContext::Run(const string& cmdToken, const string& baseCommand, const string& baseTarget)
{
  const static string emptyString;

  UpdatePermissions();

  string cmd = baseCommand;
  string target = baseTarget;

  if (HasNullOrBreak(baseCommand) || HasNullOrBreak(baseTarget)) {
    ErrorReply("Invalid input");
    return;
  }

  uint64_t cmdHash = HashCode(cmd);

  const shared_ptr<CGame> baseSourceGame = GetSourceGame();
  const shared_ptr<CGame> baseTargetGame = GetTargetGame();
  const shared_ptr<CRealm> baseSourceRealm = GetSourceRealm();
  const shared_ptr<CRealm> baseTargetRealm = GetTargetRealm();

  if (cmdHash == HashCode("su")) {
    // Allow !su for LAN connections
    if (!m_ServerName.empty() && !(m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
      ErrorReply("Forbidden");
      return;
    }
    m_Aura->m_SudoContext = shared_from_this();
    m_Aura->m_SudoAuthTarget = m_Aura->GetSudoAuthTarget(target);
    m_Aura->m_SudoExecCommand = target;
    SendReply("Sudo command requested. See Aura's console for further steps.");

    string notificationText = "[AURA] Sudoer " + GetUserAttribution() + " requests command \"" + cmdToken + target + "\"";
    Print(notificationText);
    m_Aura->LogPersistent(notificationText);

    string confirmationText;
    if (baseSourceRealm && m_FromWhisper) {
      confirmationText = "[AURA] Confirm from [" + m_ServerName + "] with: \"/w " + baseSourceRealm->GetLoginName() + " " + cmdToken + m_Aura->m_Config.m_SudoKeyWord + " " + m_Aura->m_SudoAuthTarget + "\"";
    } else {
      switch (GetServiceSourceType()) {
        case SERVICE_TYPE_IRC:
        case SERVICE_TYPE_DISCORD:
          confirmationText = "[AURA] Confirm from [" + m_ServerName + "] with: \"" + cmdToken + m_Aura->m_Config.m_SudoKeyWord + " " + m_Aura->m_SudoAuthTarget + "\"";
          break;
        default:
          confirmationText = "[AURA] Confirm from the game client with: \"" + cmdToken + m_Aura->m_Config.m_SudoKeyWord + " " + m_Aura->m_SudoAuthTarget + "\"";
      }
    }
    if (!confirmationText.empty()) {
      Print(confirmationText);
      m_Aura->LogPersistent(confirmationText);
    }
    return;
  }

  if (cmdHash == HashCode(m_Aura->m_Config.m_SudoKeyWord)) {
    optional<pair<string, string>> runOverride = CheckSudo(target);
    if (runOverride.has_value()) {
      cmd = runOverride->first;
      target = runOverride->second;
      cmdHash = HashCode(cmd);
    } else {
      if (m_Aura->m_SudoExecCommand.empty()) {
        Print("[AURA] " + GetUserAttribution() + " sent command [" + cmdToken + baseCommand + "] with target [" + baseTarget + "], but " + cmdToken + "su was not requested.");
      } else {
        Print("[AURA] " + GetUserAttribution() + " failed sudo authentication.");
      }
      ErrorReply("Sudo check failure.");
      return;
    }
  }

  const bool isLocked = baseTargetGame && ((GetIsGameUser() && GetGameUser()->GetIsActionLocked()) || baseTargetGame->GetLocked());
  if (isLocked && 0 == (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
    LogStream(*m_Output, baseTargetGame->GetLogPrefix() + "Command ignored, the game is locked");
    ErrorReply("Only the game owner and root admins can run game commands when the game is locked.");
    return;
  }

  if (target.empty()) {
    LogStream(*m_Output, GetUserAttributionPreffix() + "sent command [" + cmdToken + baseCommand + "]");
  } else {
    LogStream(*m_Output, GetUserAttributionPreffix() + "sent command [" + cmdToken + baseCommand + "] with target [" + baseTarget + "]");
  }

  /*********************
   * NON ADMIN COMMANDS *
   *********************/

  switch (cmdHash)
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to inspect game net configuration.");
        break;
      }

      SendReply(
        "Name <<" + targetGame->GetGameName() + ">> | ID " +
        to_string(targetGame->GetHostCounter()) + " (" + ToHexString(targetGame->GetHostCounter()) + ")" + " | Key " +
        to_string(targetGame->GetEntryKey()) + " (" + ToHexString(targetGame->GetEntryKey()) + ")"
      );
      break;
    }

    //
    // !SLOT
    //

    case HashCode("slot"):
    {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to inspect raw slots.");
        break;
      }

      auto searchResult = RunParseController(target);
      if (!searchResult.GetSuccess()) {
        ErrorReply("Usage: " + cmdToken + "slot <PLAYER>");
        break;
      }
      uint8_t SID = searchResult.SID;
      GameUser::CGameUser* targetPlayer = searchResult.user;

      const CGameSlot* slot = targetGame->InspectSlot(SID);
      if (targetPlayer) {
        SendReply("Player " + targetPlayer->GetName() + " (slot #" + ToDecString(SID + 1) + ") = " + ByteArrayToDecString(slot->GetByteArray()));
      } else {
        SendReply("Slot #" + ToDecString(SID + 1) + " = " + ByteArrayToDecString(slot->GetByteArray()));
      }
      break;
    }

    //
    // !APM
    // !APM <PLAYER>
    //

    case HashCode("apm"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror() || !targetGame->GetGameLoaded()) {
        break;
      }
      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(target);
      if (!targetPlayer) {
        break;
      }
      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games.");
        break;
      }
      if (targetPlayer != GetGameUser() && !CheckPermissions(m_Config->m_ImportPermissions, COMMAND_PERMISSIONS_SUDO)) {
        ErrorReply("Not allowed to view other players' APM.");
        break;
      }
      if (targetPlayer->GetIsObserver()) {
        ErrorReply("[" + targetPlayer->GetName() + "] is an observer.");
        break;
      }
      string currentAPMFragment = "APM: " + to_string(static_cast<uint64_t>(round(targetPlayer->GetAPM())));
      string maxAPMFragment, holdActionsFragment;
      if (targetPlayer->GetHasAPMQuota()) {
        maxAPMFragment = " / " + to_string(targetPlayer->GetAPMQuota().GetTokensPerMinute());
      }
      if (targetPlayer->GetOnHoldActionsAny()) {
        holdActionsFragment = " - Restricted: " + to_string(targetPlayer->GetOnHoldActionsCount()) + " actions";
      }
      SendReply("[" + targetPlayer->GetName() + "] " + currentAPMFragment + maxAPMFragment + holdActionsFragment);
      break;
    }

    //
    // !APMTRAINER <VALUE>
    //

    case HashCode("apmtrainer"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror() || !GetIsGameUser()) {
        break;
      }
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "apmtrainer <APM>");
        break;
      }
      if (GetGameUser()->GetIsObserver()) {
        ErrorReply("Only players may enable APM trainer.");
        break;
      }
      optional<int64_t> targetAPM;
      try {
        int64_t parsedValue = stol(target);
        if (0 <= parsedValue && parsedValue <= 1000) {
          targetAPM = parsedValue;
        }
      } catch (...) {
      }
      if (!targetAPM.has_value()) {
        ErrorReply("Usage: " + cmdToken + "apmtrainer <APM>");
        break;
      }
      if (targetAPM.value() >= 1) {
        GetGameUser()->SetAPMTrainer((double)targetAPM.value());
        SendReply("APM Trainer set to " + to_string(targetAPM.value()));
        break;
      }
      if (!GetGameUser()->GetHasAPMTrainer()) {
        ErrorReply("APM Trainer is already disabled");
        break;
      }
      GetGameUser()->DisableAPMTrainer();
      SendReply("APM Trainer disabled.");
      break;
    }
    

    //
    // !CHECKME
    // !CHECK <PLAYER>
    //

    case HashCode("checkme"):
    case HashCode("check"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror()) {
        break;
      }
      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(target);
      if (!targetPlayer) {
        break;
      }
      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games.");
        break;
      }
      shared_ptr<CRealm> targetPlayerRealm = targetPlayer->GetRealm(true);
      bool GetIsRealmVerified = targetPlayerRealm != nullptr;
      bool IsOwner = targetPlayer->GetIsOwner(nullopt);
      bool IsRootAdmin = GetIsRealmVerified && targetPlayerRealm->GetIsAdmin(targetPlayer->GetName());
      bool IsAdmin = IsRootAdmin || (GetIsRealmVerified && targetPlayerRealm->GetIsModerator(targetPlayer->GetName()));
      string SyncStatus;
      if (targetGame->GetGameLoaded()) {
        if (targetGame->m_SyncPlayers[targetPlayer].size() + 1 == targetGame->m_Users.size()) {
          SyncStatus = "Full";
        } else if (targetGame->m_SyncPlayers[targetPlayer].empty()) {
          SyncStatus = "Alone";
        } else {
          SyncStatus = "With: ";
          for (auto& otherPlayer: targetGame->m_SyncPlayers[targetPlayer]) {
            SyncStatus += otherPlayer->GetName() + ", ";
          }
          SyncStatus = SyncStatus.substr(0, SyncStatus.length() - 2);
        }
      }
      string SlotFragment, ReadyFragment;
      if (targetGame->GetIsLobbyStrict()) {
        SlotFragment = "Slot #" + to_string(1 + targetGame->GetSIDFromUID(targetPlayer->GetUID())) + ". ";
        if (targetPlayer->GetIsReady()) {
          ReadyFragment = "Ready. ";
        } else {
          ReadyFragment = "Not ready. ";
        }
      }
      
      string IPVersionFragment;
      if (targetPlayer->GetUsingIPv6()) {
        IPVersionFragment = ", IPv6";
      } else {
        IPVersionFragment = ", IPv4";
      }
      string FromFragment;
      if (m_Aura->m_Net.m_Config.m_EnableGeoLocalization) {
        FromFragment = ", From: " + m_Aura->m_DB->FromCheck(ByteArrayToUInt32(targetPlayer->GetIPv4(), true));
      }
      string realmFragment = "Realm: " + (targetPlayer->GetRealmHostName().empty() ? "LAN" : targetPlayer->GetRealmHostName());
      string versionFragment;
      if (targetGame->m_SupportedGameVersionsMin != targetGame->m_SupportedGameVersionsMax) {
        versionFragment = " (" + targetPlayer->GetGameVersionString() + ")";
      }
      SendReply("[" + targetPlayer->GetName() + "]. " + SlotFragment + ReadyFragment + "Ping: " + targetPlayer->GetDelayText(true) + IPVersionFragment + ", Reconnection: " + targetPlayer->GetReconnectionText() + FromFragment + (targetGame->GetGameLoaded() ? ", Sync: " + SyncStatus : ""));
      SendReply("[" + targetPlayer->GetName() + "]. " + realmFragment + versionFragment + ", Verified: " + (GetIsRealmVerified ? "Yes" : "No") + ", Reserved: " + (targetPlayer->GetIsReserved() ? "Yes" : "No"));
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      // kick players with ping higher than target if target isn't empty
      // we only do this if the game hasn't started since we don't want to kick players from a game in progress

      if (!targetGame || targetGame->GetIsMirror())
        break;

      optional<uint32_t> kickPing;
      if (!target.empty()) {
        if (!targetGame->GetIsLobbyStrict()) {
          ErrorReply("Maximum ping may only be set in a lobby.");
          break;
        }
        if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("Not allowed to set maximum ping.");
          break;
        }
        try {
          int64_t Value = stoul(target);
          if (Value <= 0 || 0xFFFFFFFF < Value) {
            ErrorReply("Invalid maximum ping [" + target + "].");
            break;
          }
          kickPing = static_cast<uint32_t>(Value);
        } catch (...) {
          ErrorReply("Invalid maximum ping [" + target + "].");
          break;
        }
      }

      if (kickPing.has_value())
        targetGame->m_Config.m_AutoKickPing = kickPing.value();

      // copy the m_Users vector so we can sort by descending ping so it's easier to find players with high pings

      vector<GameUser::CGameUser*> SortedPlayers = targetGame->m_Users;
      if (targetGame->GetGameLoaded()) {
        sort(begin(SortedPlayers), end(SortedPlayers), &GameUser::SortUsersByKeepAlivesAscending);
      } else {
        sort(begin(SortedPlayers), end(SortedPlayers), &GameUser::SortUsersByLatencyDescending);
      }
      bool anyPing = false;
      vector<string> pingsText;
      uint32_t maxPing = 0;
      for (auto i = begin(SortedPlayers); i != end(SortedPlayers); ++i) {
        pingsText.push_back((*i)->GetDisplayName() + ": " + (*i)->GetDelayText(false));
        uint32_t ping = (*i)->GetRTT();
        if (ping == 0) continue; // also skips this iteration if there is no ping data
        anyPing = true;
        if (ping > maxPing) maxPing = ping;
      }

      bool sendAll = GetGameSource().GetIsEmpty() || (GetIsGameUser() && GetGameUser()->GetCanUsePublicChat());
      if (anyPing) {
        SendReply(JoinVector(pingsText, false), sendAll ? CHAT_SEND_TARGET_ALL : 0);
      } else if (m_Aura->m_Net.m_Config.m_HasBufferBloat && targetGame->IsDownloading()) {
        SendReply("Ping not measured yet (wait for map download.)", sendAll ? CHAT_SEND_TARGET_ALL : 0);
      } else {
        SendReply("Ping not measured yet.", sendAll ? CHAT_SEND_TARGET_ALL : 0);
      }

      const uint16_t internalLatency = (uint16_t)targetGame->GetNextLatency();
      const bool suggestLowerLatency = 0 < maxPing && maxPing < internalLatency && REFRESH_PERIOD_MIN_SUGGESTED < internalLatency;
      const bool suggestHigherLatency = 0 < maxPing && internalLatency < maxPing / 4 && REFRESH_PERIOD_MAX_SUGGESTED > internalLatency;
      if (targetGame->m_Config.m_LatencyEqualizerEnabled || suggestLowerLatency || suggestHigherLatency) {
        string refreshText = "Internal latency is " + to_string(targetGame->GetNextLatency()) + "ms.";
        string equalizerHeader;
        if (targetGame->m_Config.m_LatencyEqualizerEnabled) {
          equalizerHeader = "Ping equalizer ENABLED. ";
        }
        string suggestionText;
        if (suggestLowerLatency) {
          suggestionText = " Decrease it with " + cmdToken + "latency [VALUE]";
        } else if (suggestHigherLatency) {
          suggestionText = " Increase it with " + cmdToken + "latency [VALUE]";
        }
        SendReply("HINT: " + equalizerHeader + refreshText + suggestionText, sendAll ? CHAT_SEND_TARGET_ALL : 0);
      }

      break;
    }

    //
    // !CHECKRACE
    // !RACES
    // !GAME
    //

    case HashCode("game"):
    case HashCode("races"):
    case HashCode("checkrace"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      vector<string> output;
      output.push_back("Game#" + to_string(targetGame->GetGameID()));

      // GOTCHA: /game - Leavers info is omitted. This affects games with name obfuscation.
      vector<const GameUser::CGameUser*> players = targetGame->GetPlayers();
      for (const auto& player : players) {
        const CGameSlot* slot = targetGame->InspectSlot(targetGame->GetSIDFromUID(player->GetUID()));
        uint8_t race = slot->GetRaceFixed();
        output.push_back("[" + player->GetName() + "] - " + GetRaceName(race));
      }
      vector<string> replyLines = JoinReplyListCompact(output);
      for (const auto& line : replyLines) {
        SendReply(line);
      }
      break;
    }

    //
    // !STATSDOTA
    // !STATS
    //

    case HashCode("statsdota"):
    case HashCode("stats"): {
      shared_ptr<CGame> sourceGame = GetSourceGame();
      if (
        (!sourceGame || sourceGame->GetIsLobbyStrict()) &&
        !CheckPermissions(m_Config->m_StatsPermissions, COMMAND_PERMISSIONS_VERIFIED)
      ) {
        ErrorReply("Not allowed to look up stats.");
        break;
      }
      shared_ptr<CGame> targetGame = baseTargetGame;
      if (target.empty() && (!targetGame || targetGame->GetIsHiddenPlayerNames())) {
        ErrorReply("Usage: " + cmdToken + "stats <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "statsdota <PLAYER>");
        break;
      }
      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(target);
      if (!targetPlayer) {
        break;
      }
      const bool isDota = cmdHash == HashCode("statsdota") || targetGame->GetClientFileName().find("DotA") != string::npos;
      const bool isUnverified = targetPlayer->GetRealm(false) != nullptr && !targetPlayer->GetIsRealmVerified();
      string targetIdentity = "[" + targetPlayer->GetExtendedName() + "]";
      if (isUnverified) targetIdentity += " (unverified)";

      if (isDota) {
        CDBDotAPlayerSummary* DotAPlayerSummary = m_Aura->m_DB->DotAPlayerSummaryCheck(targetPlayer->GetName(), targetPlayer->GetRealmHostName());
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
        CDBGamePlayerSummary* GamePlayerSummary = m_Aura->m_DB->GamePlayerSummaryCheck(targetPlayer->GetName(), targetPlayer->GetRealmHostName());
        if (!GamePlayerSummary) {
          SendReply(targetIdentity + " has no registered games.");
          break;
        }
        const string summaryText = (
          targetIdentity + " has played " +
          to_string(GamePlayerSummary->GetTotalGames()) + " games with this bot. Average loading time: " +
          ToFormattedString(static_cast<double>(GamePlayerSummary->GetAvgLoadingTime())) + " seconds. Average stay: " +
          to_string(GamePlayerSummary->GetAvgLeftPercent()) + "%"
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
      if (!target.empty()) {
        m_TargetGame = GetTargetGame(target);
      } else if (auto sourceGame = GetSourceGame()) {
        m_TargetGame = sourceGame;
      } else {
        UseImplicitHostedGame();
      }
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (targetGame->m_DisplayMode == GAME_PRIVATE && GetSourceGame() != GetTargetGame()) {
        if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("This game is private.");
          break;
        }
      }
      string Players = ToNameListSentence(targetGame->GetPlayers());
      string Observers = ToNameListSentence(targetGame->GetObservers());
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (targetGame->GetCountDownStarted() && !targetGame->GetGameLoaded())
        break;

      if (target.empty()){
        ErrorReply("Usage: " + cmdToken + "votekick <PLAYERNAME>");
        break;
      }

      if (!targetGame->m_KickVotePlayer.empty()) {
        ErrorReply("Unable to start votekick. Another votekick is in progress");
        break;
      }
      if (targetGame->m_Users.size() <= 2) {
        ErrorReply("Unable to start votekick. There aren't enough players in the game for a votekick");
        break;
      }

      auto searchResult = RunParseController(target);
      if (!searchResult.GetSuccess()) {
        ErrorReply("Usage: " + cmdToken + "votekick <PLAYERNAME>");
        break;
      }
      uint8_t SID = searchResult.SID;
      GameUser::CGameUser* targetPlayer = searchResult.user;

      if (!targetPlayer) {
        ErrorReply("Slot #" + to_string(SID + 1) + " is not occupied by a user.");
        break;
      }
      if (targetPlayer->GetIsReserved()) {
        ErrorAll("Unable to votekick user [" + targetPlayer->GetName() + "]. That user is reserved and cannot be votekicked");
        break;
      }

      targetGame->m_KickVotePlayer = targetPlayer->GetName();
      targetGame->m_StartedKickVoteTime = GetTime();

      for (auto& it : targetGame->m_Users)
        it->SetKickVote(false);

      SendReply("Votekick against [" + targetGame->m_KickVotePlayer + "] started by [" + GetSender() + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_INCIDENT);
      if (GetIsGameUser() && GetGameUser() != targetPlayer) {
        GetGameUser()->SetKickVote(true);
        SendAll("[" + GetGameUser()->GetDisplayName() + "] voted to kick [" + targetGame->m_KickVotePlayer + "]. " + to_string(static_cast<uint32_t>(ceil(static_cast<float>(targetGame->GetNumJoinedPlayers() - 1) * static_cast<float>(targetGame->m_Config.m_VoteKickPercentage) / 100)) - 1) + " more votes are needed to pass");
      }
      SendAll("Type " + cmdToken + "yes or " + cmdToken + "no to vote.");

      break;
    }

    //
    // !YES
    //

    case HashCode("yes"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!GetIsGameUser() || targetGame->m_KickVotePlayer.empty() || GetGameUser()->GetKickVote().value_or(false))
        break;

      uint32_t VotesNeeded = static_cast<uint32_t>(ceil(static_cast<float>(targetGame->GetNumJoinedPlayers() - 1) * static_cast<float>(targetGame->m_Config.m_VoteKickPercentage) / 100));
      GetGameUser()->SetKickVote(true);
      targetGame->SendAllChat("[" + GetGameUser()->GetDisplayName() + "] voted for kicking [" + targetGame->m_KickVotePlayer + "]. " + to_string(VotesNeeded) + " affirmative votes required to pass");
      targetGame->CountKickVotes();
      break;
    }

    //
    // !NO
    //

    case HashCode("no"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!GetIsGameUser() || targetGame->m_KickVotePlayer.empty() || !GetGameUser()->GetKickVote().value_or(true))
        break;

      GetGameUser()->SetKickVote(false);
      targetGame->SendAllChat("[" + GetGameUser()->GetDisplayName() + "] voted against kicking [" + targetGame->m_KickVotePlayer + "].");
      break;
    }

    //
    // !INVITE
    //

    case HashCode("invite"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsStageAcceptingJoins()) {
        // Intentionally allows !invite to fake (mirror) lobbies.
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "invite <PLAYERNAME>@<REALM>");
        break;
      }

      const string MapPath = targetGame->GetMap()->GetClientPath();
      size_t LastSlash = MapPath.rfind('\\');

      auto realmUserResult = GetParseTargetRealmUser(target, false, true);
      if (!realmUserResult.GetSuccess()) {
        if (!realmUserResult.hostName.empty()) {
          ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "invite <PLAYERNAME>@<REALM>");
        }
        break;
      }

      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();

      // Name of sender and receiver should be included in the message,
      // so that they can be checked in successful whisper acks from the server (BNETProtocol::IncomingChatEvent::WHISPERSENT)
      // Note that the server doesn't provide any way to recognize whisper targets if the whisper fails.
      if (LastSlash != string::npos && LastSlash <= MapPath.length() - 6) {
        m_ActionMessage = targetName + ", " + GetSender() + " invites you to play [" + MapPath.substr(LastSlash + 1) + "]. Join game \"" + targetGame->m_GameName + "\"";
      } else {
        m_ActionMessage = targetName + ", " + GetSender() + " invites you to join game \"" + targetGame->m_GameName + "\"";
      }

      targetRealm->QueueWhisper(m_ActionMessage, targetName, shared_from_this(), true);
      break;
    }

    //
    // !PRD
    //

    case HashCode("markov"):
    case HashCode("prd"): {
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "prd <NOMINAL CHANCE%>");
        break;
      }
      vector<uint32_t> Args = SplitNumericArgs(target, 1u, 1u);
      if (Args.empty() || Args[0] > 100) {
        ErrorReply("Usage: " + cmdToken + "prd <NOMINAL CHANCE%>");
        break;
      }
      vector<string> factors = {"0.00380", "0.01475", "0.03221", "0.05570", "0.08475", "0.11895", "0.14628", "0.18128", "0.21867", "0.25701", "0.29509", "0.33324", "0.38109", "0.42448", "0.46134", "0.50276"};
      vector<string> chances = {"5.0", "10.0", "15.0", "20.0", "24.9", "29.9", "33.6", "37.7", "41.8", "45.7", "49.3", "53.0", "56.6", "60.1", "63.2", "66.7"};
      uint8_t nominalChance = static_cast<uint8_t>(Args[0]);
      uint8_t index = nominalChance / 5 - 1;
      if (nominalChance <= 5) {
        SendReply("Nominal chance of 5% formula: P(N) = " + factors[0] + " N. Expected value: " + chances[0] + "%");
      } else if (nominalChance >= 80) {
        SendReply("Nominal chance of 80% formula: P(N) = " + factors[15] + "N. Expected value: " + chances[15] + "%");
      } else if (nominalChance % 5 == 0) {
        SendReply("Nominal chance of " + ToDecString(nominalChance) + "% formula: P(N) = " + factors[index] + " N. Expected value: " + chances[index] + "%");
      } else {
        uint8_t lower = static_cast<uint8_t>(5 * (index + 1));
        uint8_t upper = static_cast<uint8_t>(5 * (index + 2));
        SendReply("Nominal chance of " + ToDecString(lower) + "% formula: P(N) = " + factors[index] + " N. Expected value: " + chances[index] + "%");
        SendReply("Nominal chance of " + ToDecString(upper) + "% formula: P(N) = " + factors[index + 1] + " N. Expected value: " + chances[index + 1] + "%");
      }
      break;
    }

    //
    // !PRD2
    //

    case HashCode("markov2"):
    case HashCode("prd2"): {
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "prd2 <NOMINAL CHANCE%>");
        break;
      }
      vector<uint32_t> Args = SplitNumericArgs(target, 1u, 1u);
      if (Args.empty() || Args[0] > 100) {
        ErrorReply("Usage: " + cmdToken + "prd2 <NOMINAL CHANCE%>");
        break;
      }
      vector<string> factors = {"0.00380166", "0.01474584", "0.03222091", "0.05570404", "0.08474409", "0.11894919", "0.15798310", "0.20154741", "0.24930700", "0.30210303", "0.36039785", "0.42264973", "0.48112548", "0.57142857", "0.66666667", "0.75000000", "0.82352941", "0.88888889", "0.94736842"};
      uint8_t nominalChance = static_cast<uint8_t>(Args[0]);
      uint8_t index = nominalChance / 5 - 1;
      if (nominalChance <= 5) {
        SendReply("Nominal chance of 5% formula: P(N) = " + factors[0] + " N. Expected value: 5%");
      } else if (nominalChance >= 95) {
        SendReply("Nominal chance of 95% formula: P(N) = " + factors[18] + "N. Expected value: 95%");
      } else if (nominalChance % 5 == 0) {
        SendReply("Nominal chance of " + ToDecString(nominalChance) + "% formula: P(N) = " + factors[index] + " N. Expected value: " + ToDecString(nominalChance) + "%");
      } else {
        uint8_t lower = static_cast<uint8_t>(5 * (index + 1));
        uint8_t upper = static_cast<uint8_t>(5 * (index + 2));
        SendReply("Nominal chance of " + ToDecString(lower) + "% formula: P(N) = " + factors[index] + " N. Expected value: " + ToDecString(lower) + "%");
        SendReply("Nominal chance of " + ToDecString(upper) + "% formula: P(N) = " + factors[index + 1] + " N. Expected value: " + ToDecString(upper) + "%");
      }
      break;
    }

    //
    // !FLIP
    //

    case HashCode("coin"):
    case HashCode("coinflip"):
    case HashCode("flip"): {
      double chance = 0.5;
      if (!target.empty()) {
        double chancePercent;
        try {
          chancePercent = stod(target);
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

      std::random_device rd;
      std::mt19937 gen(rd());
      std::bernoulli_distribution bernoulliDist(chance);
      bool result = bernoulliDist(gen);

      bool sendAll = GetGameSource().GetIsEmpty() || (GetIsGameUser() && GetGameUser()->GetCanUsePublicChat());
      SendReply(GetSender() + " flipped a coin and got " + (result ? "heads" : "tails") + ".", sendAll ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !ROLL
    //

    case HashCode("roll"): {
      size_t diceStart = target.find('d');
      uint16_t rollFaces = 0;
      uint8_t rollCount = 1;

      string rawRollCount = diceStart == string::npos ? "1" : target.substr(0, diceStart);
      string rawRollFaces = target.empty() ? "100" : diceStart == string::npos ? target : target.substr(diceStart + 1);

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

      bool sendAll = GetGameSource().GetIsEmpty() || (GetIsGameUser() && GetGameUser()->GetCanUsePublicChat());
      if (target.empty()) {
        SendReply(GetSender() + " rolled " + gotRolls[0] + ".", sendAll ? CHAT_SEND_TARGET_ALL : 0);
      } else {
        SendReply(GetSender() + " rolled " + to_string(rollCount) + "d" + to_string(rollFaces) + ". Got: " + JoinVector(gotRolls, false) + ".", sendAll ? CHAT_SEND_TARGET_ALL : 0);
      }
      break;
    }

    //
    // !PICK
    //

    case HashCode("pick"): {
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "pick <OPTION> , <OPTION> , <OPTION>, ...");
        break;
      }

      vector<string> options = SplitArgs(target, 1, 24);
      if (options.empty()) {
        ErrorReply("Empty options list.");
        break;
      }
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(options.size()));

      string randomPick = options[distribution(gen) - 1];
      bool sendAll = GetGameSource().GetIsEmpty() || (GetIsGameUser() && GetGameUser()->GetCanUsePublicChat());
      SendReply("Randomly picked: " + randomPick, sendAll ? CHAT_SEND_TARGET_ALL : 0);
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
      bool sendAll = GetGameSource().GetIsEmpty() || (GetIsGameUser() && GetGameUser()->GetCanUsePublicChat());
      SendReply("Randomly picked: " + randomPick + " race", sendAll ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PICKPLAYER
    //

    case HashCode("pickplayer"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      vector<const GameUser::CGameUser*> players = targetGame->GetPlayers();
      if (players.empty()) {
        ErrorReply("No players found.");
        break;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(players.size()));
      const GameUser::CGameUser* pickedPlayer = players[distribution(gen) - 1];
      string randomPick = pickedPlayer->GetName();
      bool sendAll = GetGameSource().GetIsEmpty() || (GetIsGameUser() && GetGameUser()->GetCanUsePublicChat());
      SendReply("Randomly picked: " + randomPick, sendAll ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PICKOBS
    //

    case HashCode("pickobserver"):
    case HashCode("pickobs"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      vector<const GameUser::CGameUser*> players = targetGame->GetObservers();
      if (players.empty()) {
        ErrorReply("No observers found.");
        break;
      }

      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, static_cast<int>(players.size()));
      const GameUser::CGameUser* pickedPlayer = players[distribution(gen) - 1];
      string randomPick = pickedPlayer->GetName();
      bool sendAll = GetGameSource().GetIsEmpty() || (GetIsGameUser() && GetGameUser()->GetCanUsePublicChat());
      SendReply("Randomly picked: " + randomPick, sendAll ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !ERAS
    //

    case HashCode("eras"): {
      shared_ptr<CGame> targetGame = baseTargetGame;
      if (target.empty() && targetGame && targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("Usage: " + cmdToken + "eras <PLAYER|COUNTRY|COLOR>");
        break;
      }

      string countryName = target;
      std::transform(std::begin(countryName), std::end(countryName), std::begin(countryName), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      if (countryName == "sweden" || countryName == "peru") {
        SendReply("Slot #1: " + GetColorName(0));
      } else if (countryName == "england" || countryName == "chile") {
        SendReply("Slot #2: " + GetColorName(1));
      } else if (countryName == "russia" || countryName == "bolivia") {
        SendReply("Slot #3: " + GetColorName(2));
      } else if (countryName == "italy" || countryName == "argentina" || countryName == "argentine") {
        SendReply("Slot #4: " + GetColorName(3));
      } else if (countryName == "france" || countryName == "colombia") {
        SendReply("Slot #5: " + GetColorName(4));
      } else if (countryName == "spain" || countryName == "venezuela") {
        SendReply("Slot #6: " + GetColorName(5));
      } else if (countryName == "turkey" || countryName == "turkiye" || countryName == "brasil" || countryName == "brazil") {
        SendReply("Slot #7: " + GetColorName(6));
      } else if (countryName == "poland" || countryName == "mexico") {
        SendReply("Slot #8: " + GetColorName(7));
      } else if (countryName == "germany" || countryName == "ecuador") {
        SendReply("Slot #9: " + GetColorName(8));
      } else if (countryName == "ireland") {
        SendReply("Slot #13: " + GetColorName(12));
      } else if (countryName == "norway") {
        SendReply("Slot #14: " + GetColorName(13));
      } else if (countryName == "iceland") {
        SendReply("Slot #15: " + GetColorName(14));
      } else if (countryName == "greece") {
        SendReply("Slot #16: " + GetColorName(15));
      } else if (countryName == "holland" || countryName == "netherlands") {
        SendReply("Slot #17: " + GetColorName(16));
      } else if (countryName == "romania") {
        SendReply("Slot #18: " + GetColorName(17));
      } else if (countryName == "egypt") {
        SendReply("Slot #19: " + GetColorName(18));
      } else if (countryName == "morocco") {
        SendReply("Slot #20: " + GetColorName(19));
      } else if (countryName == "congo") {
        SendReply("Slot #21: " + GetColorName(20));
      } else if (countryName == "somalia") {
        SendReply("Slot #22: " + GetColorName(21));
      } else {
        uint8_t color = ParseColor(target);
        if (color == 0xFF && targetGame) {
          GameUser::CGameUser* targetPlayer = GetTargetUserOrSelf(target);
          if (targetPlayer) {
            color = targetGame->GetSIDFromUID(targetPlayer->GetUID());
          }
        }
        if (color == 0) {
          SendReply(GetColorName(color) + " - EU: Sweden, LAT: Peru");
        } else if (color == 1) {
          SendReply(GetColorName(color) + " - EU: England, LAT: Chile");
        } else if (color == 2) {
          SendReply(GetColorName(color) + " - EU: Russia, LAT: Bolivia");
        } else if (color == 3) {
          SendReply(GetColorName(color) + " - EU: Italy, LAT: Argentina");
        } else if (color == 4) {
          SendReply(GetColorName(color) + " - EU: France, LAT: Colombia");
        } else if (color == 5) {
          SendReply(GetColorName(color) + " - EU: Spain, LAT: Venezuela");
        } else if (color == 6) {
          SendReply(GetColorName(color) + " - EU: Turkey, LAT: Brasil");
        } else if (color == 7) {
          SendReply(GetColorName(color) + " - EU: Poland, LAT: Mexico");
        } else if (color == 8) {
          SendReply(GetColorName(color) + " - EU: Germany, LAT: Ecuador");
        } else if (color == 12) {
          SendReply(GetColorName(color) + " - EU: Ireland");
        } else if (color == 13) {
          SendReply(GetColorName(color) + " - EU: Norway");
        } else if (color == 14) {
          SendReply(GetColorName(color) + " - EU: Iceland");
        } else if (color == 15) {
          SendReply(GetColorName(color) + " - EU: Greece");
        } else if (color == 16) {
          SendReply(GetColorName(color) + " - EU: Holland");
        } else if (color == 17) {
          SendReply(GetColorName(color) + " - EU: Romania");
        } else if (color == 18) {
          SendReply(GetColorName(color) + " - EU: Egypt");
        } else if (color == 19) {
          SendReply(GetColorName(color) + " - EU: Morocco");
        } else if (color == 20) {
          SendReply(GetColorName(color) + " - EU: Congo");
        } else if (color == 21) {
          SendReply(GetColorName(color) + " - EU: Somalia");
        } else if (color == 0xFF) {
          ErrorReply("Not a country identifier: [" + target + "]");
        } else {
          ErrorReply("No country matches slot #" + ToDecString(color + 1));
        }
      }
      break;
    }

#ifndef DISABLE_DPP
    case HashCode("twrpg"): {
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "twrpg <NOMBRE>");
        break;
      }

      string name = target;
      uint8_t matchType = m_Aura->m_DB->FindData(MAP_TYPE_TWRPG, MAP_DATA_TYPE_ANY, name, false);
      if (matchType == MAP_DATA_TYPE_NONE) {
        vector<string> words = Tokenize(name, ' ');
        if (words.size() <= 1) {
          ErrorReply("[" + target + "] not found.");
          break;
        }
        string intermediate = words[0];
        words[0] = words[words.size() - 1];
        words[words.size() - 1] = intermediate;
        name = JoinVector(words, " ", false);
        matchType = m_Aura->m_DB->FindData(MAP_TYPE_TWRPG, MAP_DATA_TYPE_ANY, name, false);
        if (matchType == MAP_DATA_TYPE_NONE) {
          ErrorReply("[" + target + "] not found.");
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
#endif

    /*****************
     * ADMIN COMMANDS *
     ******************/
     
    //
    // !FROM
    //

    case HashCode("where"):
    case HashCode("from"):
    case HashCode("f"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (!m_Aura->m_Net.m_Config.m_EnableGeoLocalization) {
        ErrorReply("Geolocalization is disabled.");
        break;
      }

      if (!CheckPermissions(m_Config->m_CommonBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        ErrorReply("Not allowed to check players geolocalization.");
        break;
      }

      string Froms;

      for (auto i = begin(targetGame->m_Users); i != end(targetGame->m_Users); ++i) {
        // we reverse the byte order on the IP because it's stored in network byte order

        Froms += (*i)->GetDisplayName();
        Froms += ": (";
        Froms += m_Aura->m_DB->FromCheck(ByteArrayToUInt32((*i)->GetIPv4(), true));
        Froms += ")";

        if (i != end(targetGame->m_Users) - 1)
          Froms += ", ";
      }

      bool sendAll = GetGameSource().GetIsEmpty() || (GetIsGameUser() && GetGameUser()->GetCanUsePublicChat());
      SendReply(Froms, sendAll ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !PBANLAST
    //

    case HashCode("bl"):
    case HashCode("pbl"):
    case HashCode("pbanlast"): {
      shared_ptr<CGame> targetGame = GetTargetGame();
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!targetGame || !targetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (!targetGame->m_LastLeaverBannable) {
        ErrorReply("No ban candidates stored.");
        break;
      }

      string lower = target;
      transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });
      bool isConfirm = lower == "c" || lower == "confirm";

      if (!targetGame->m_LastLeaverBannable->GetSuspect()) {
        if (isConfirm) {
          ErrorReply("Usage: " + cmdToken + "pbanlast");
          break;
        }
        targetGame->m_LastLeaverBannable->SetSuspect(true);
        SendReply("Player [" + targetGame->m_LastLeaverBannable->GetName() + "@" + targetGame->m_LastLeaverBannable->GetServer() + "] was the last leaver.");
        SendReply("Use " + cmdToken + "pbanlast confirm to ban them.");
        break;
      } else {
        if (!isConfirm) {
          ErrorReply("Usage: " + cmdToken + "pbanlast confirm");
          break;
        }
        string authServer = m_ServerName;
        if (sourceRealm) {
          authServer = sourceRealm->GetDataBaseID();
        }
        m_Aura->m_DB->BanAdd(
          targetGame->m_LastLeaverBannable->GetName(),
          targetGame->m_LastLeaverBannable->GetServer(),
          authServer,
          targetGame->m_LastLeaverBannable->GetIP(),
          GetSender(),
          "Leaver"
        );
        SendAll("Player [" + targetGame->m_LastLeaverBannable->GetName() + "@" + targetGame->m_LastLeaverBannable->GetServer() + "] was banned by [" + GetSender() + "] on server [" + targetGame->m_LastLeaverBannable->GetAuthServer() + "]");
      }
      break;
    }

    //
    // !CLOSE (close slot)
    //

    case HashCode("close"):
    case HashCode("c"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (target.empty()) {
        if (!targetGame->CloseSlot()) {
          ErrorReply("No slots are open.");
        } else {
          SendReply("One slot closed.");
        }
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(target, 1u, targetGame->GetMap()->GetVersionMaxSlots());
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "c <SLOTNUM>");
        break;
      }

      vector<string> failedSlots;
      for (auto& elem : Args) {
        if (elem == 0 || elem > targetGame->GetMap()->GetVersionMaxSlots()) {
          ErrorReply("Usage: " + cmdToken + "c <SLOTNUM>");
          break;
        }
        uint8_t SID = static_cast<uint8_t>(elem) - 1;
        if (!targetGame->CloseSlot(SID, cmdHash == HashCode("close"))) {
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot end the game.");
        break;
      }

      LogStream(*m_Output, targetGame->GetLogPrefix() + "is over (admin ended game) [" + GetSender() + "]");
      targetGame->SendAllChat("Ending the game.");
      targetGame->StopPlayers("was disconnected (admin ended game)");
      break;
    }

    //
    // !URL
    //

    case HashCode("url"):
    case HashCode("link"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      const string TargetUrl = TrimString(target);

      if (TargetUrl.empty()) {
        if (targetGame->GetMapSiteURL().empty()) {
          SendAll("Download URL unknown.");
        } else {
          SendAll("Visit  <" + targetGame->GetMapSiteURL() + "> to download [" + targetGame->GetClientFileName() + "]");
        }
        break;
      }

      if (!targetGame->GetIsLobbyStrict())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      targetGame->SetMapSiteURL(TargetUrl);
      SendReply("Download URL set to [" + TargetUrl + "]", targetGame->GetIsLobbyStrict() || !targetGame->GetIsHiddenPlayerNames() ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !GPROXY
    //

    case HashCode("reconnect"):
    case HashCode("gproxy"): {
      SendReply("Protect against disconnections using GProxyDLL, a Warcraft III plugin. See: <" + m_Aura->m_Net.m_Config.m_AnnounceGProxySite + ">");
      break;
    }

    //
    // !MODE
    //

    
    case HashCode("hcl"):
    case HashCode("mode"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      /*if (targetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }*/

      if (!targetGame->GetMap()->GetHCLEnabled()) {
        SendReply("Game mode feature (HCL) is disabled.");
        break;
      }

      if (target.empty()) {
        SendReply("Game mode (HCL) is [" + targetGame->m_HCLCommandString + "]");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (targetGame->GetMap()->GetHCLRequired() && !GetIsSudo()) {
        ErrorReply("Game mode (HCL) cannot be reconfigured.");
        break;
      }

      if (target.size() > targetGame->m_Slots.size()) {
        ErrorReply("Unable to set mode (HCL) because it's too long - it must not exceed the amount of occupied game slots");
        break;
      }

      const string checkResult = targetGame->CheckIsValidHCL(target);
      if (!checkResult.empty()) {
        ErrorReply(checkResult);
        break;
      }

      targetGame->m_HCLCommandString = target;
      SendAll("Game mode (HCL) set to [" + targetGame->m_HCLCommandString + "]");
      break;
    }

    //
    // !HOLD (hold a slot for someone)
    //

    case HashCode("reserve"):
    case HashCode("hold"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, targetGame->GetMap()->GetVersionMaxSlots());

      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "hold <PLAYER1> , <PLAYER2>, ...");
        break;
      }

      vector<string> addedList;
      for (auto& targetName: Args) {
        if (targetName.empty())
          continue;
        const GameUser::CGameUser* targetUser = GetTargetUser(targetName);
        if (targetUser) {
          targetGame->AddToReserved(targetUser->GetName());
          addedList.push_back(targetUser->GetDisplayName());
        } else {
          targetGame->AddToReserved(targetName);
          addedList.push_back(targetName);
        }
      }

      SendAll("Added user(s) to the hold list: " + JoinVector(addedList, false));
      break;
    }

    //
    // !HOLDALL (hold slots for everyone in the lobby)
    //

    case HashCode("reserveall"):
    case HashCode("holdall"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (!targetGame->ReserveAll()) {
        SendAll("All users in the lobby are already reserved.");
        break;
      }
      SendAll("Added all users in the lobby to the hold list.");
      break;
    }

    //
    // !UNHOLD
    //

    case HashCode("unholdall"):
    case HashCode("unhold"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, targetGame->GetMap()->GetVersionMaxSlots());

      if (Args.empty()) {
        if (!targetGame->RemoveAllReserved()) {
          SendAll("Reservations list was already empty.");
          break;
        }
        SendAll("Cleared the reservations list.");
        break;
      }

      for (auto& PlayerName: Args) {
        if (PlayerName.empty())
          continue;
        targetGame->RemoveFromReserved(PlayerName);
      }
      SendAll("Removed user(s) from the reservations list: " + JoinVector(Args, false));
      break;
    }


#ifdef DEBUG
    //
    // !DISCONNECT (disconnect a user)
    //
    case HashCode("disconnect"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (!GetIsSudo()) {
        ErrorReply("You are not a sudoer, and therefore cannot disconnect a player.");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(target);
      if (!targetPlayer) {
        break;
      }

      targetPlayer->GetSocket()->m_HasError = true;
      SendReply("Forcibly disconnected user [" + targetPlayer->GetName() + "]");
      break;
    }
#endif

    //
    // !KICK (kick a user)
    //

    case HashCode("closekick"):
    case HashCode("ckick"):
    case HashCode("kick"):
    case HashCode("k"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror() || (targetGame->GetCountDownStarted() && !targetGame->GetGameLoaded()))
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "kick <PLAYERNAME>");
        break;
      }

      auto searchResult = RunParseController(target);
      if (!searchResult.GetSuccess()) {
        ErrorReply("Usage: " + cmdToken + "kick <PLAYERNAME>");
        break;
      }
      uint8_t SID = searchResult.SID;
      GameUser::CGameUser* targetPlayer = searchResult.user;

      if (!targetPlayer) {
        ErrorReply("Slot #" + to_string(SID + 1) + " is not occupied by a user.");
        break;
      }

      targetPlayer->CloseConnection();
      //targetPlayer->SetDeleteMe(true);
      targetPlayer->SetLeftReason("was kicked by [" + GetSender() + "]");

      if (targetGame->GetIsLobbyStrict())
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
      else
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOST);

      if (targetGame->GetIsLobbyStrict()) {
        bool KickAndClose = cmdHash == HashCode("ckick") || cmdHash == HashCode("closekick");
        if (KickAndClose && !targetGame->GetIsRestored()) {
          targetGame->CloseSlot(SID, false);
        }
      }

      break;
    }

    //
    // !LATENCY (set game latency)
    //

    case HashCode("latency"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (target.empty()) {
        SendReply("The game latency is " + to_string(targetGame->GetNextLatency()) + " ms");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot modify the game latency.");
        break;
      }

      string lower = target;
      transform(begin(lower), end(lower), begin(lower), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lower == "default" || lower == "reset") {
        targetGame->ResetLatency();
        SendReply("Latency settings reset to default.", targetGame->GetIsLobbyStrict() || !targetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
        break;
      } else if (lower == "ignore" || lower == "bypass" || lower == "normal") {
        if (!targetGame->GetGameLoaded()) {
          ErrorReply("This command must be used after the game has loaded.");
          break;
        }
        targetGame->NormalizeSyncCounters();
        SendReply("Ignoring lagging players. (They may not be able to control their units.)", targetGame->GetIsLobbyStrict() || !targetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(target, 1u, 2u);
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
        if (tolerance.value() <= 0) {
          ErrorReply("Spike tolerance must be a positive value in ms.");
          break;
        }
        if (tolerance.value() < targetGame->m_Config.m_SyncLimitSafeMinMilliSeconds && !GetIsSudo()) {
          ErrorReply("Minimum spike tolerance is " + to_string(targetGame->m_Config.m_SyncLimitSafeMinMilliSeconds) + " ms.");
          break;
        }
        if (tolerance.value() > targetGame->m_Config.m_SyncLimitMaxMilliSeconds && !GetIsSudo()) {
          ErrorReply("Maximum spike tolerance is " + to_string(targetGame->m_Config.m_SyncLimitMaxMilliSeconds) + " ms.");
          break;
        }
      }

      if (refreshTime < targetGame->m_Config.m_LatencyMin) {
        refreshTime = targetGame->m_Config.m_LatencyMin;
      } else if (refreshTime > targetGame->m_Config.m_LatencyMax) {
        refreshTime = targetGame->m_Config.m_LatencyMax;
      }

      const double oldRefresh = (uint16_t)targetGame->GetNextLatency();
      const double oldSyncLimit = targetGame->GetSyncLimit();
      const double oldSyncLimitSafe = targetGame->GetSyncLimitSafe();

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

      if (!targetGame->SetupLatency(refreshTime, (uint16_t)syncLimit, (uint16_t)syncLimitSafe)) {
        // Sudo abuse caused overflow or other aberrant behavior
        ErrorReply("Failed to reconfigure game latency");
        break;
      }

      const uint32_t finalToleranceMilliseconds = (
        static_cast<uint32_t>(targetGame->GetNextLatency()) *
        static_cast<uint32_t>(targetGame->GetSyncLimit())
      );

      if (refreshTime == targetGame->m_Config.m_LatencyMin) {
        SendReply("Game will be updated at the fastest rate (every " + to_string(targetGame->GetNextLatency()) + " ms)", targetGame->GetIsLobbyStrict() || !targetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
      } else if (refreshTime == targetGame->m_Config.m_LatencyMax) {
        SendReply("Game will be updated at the slowest rate (every " + to_string(targetGame->GetNextLatency()) + " ms)", targetGame->GetIsLobbyStrict() || !targetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
      } else {
        SendReply("Game will be updated with a delay of " + to_string(targetGame->GetNextLatency()) + "ms.", targetGame->GetIsLobbyStrict() || !targetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
      }
      SendReply("Spike tolerance set to " + to_string(finalToleranceMilliseconds) + "ms.", targetGame->GetIsLobbyStrict() || !targetGame->GetIsHiddenPlayerNames()  ? CHAT_SEND_TARGET_ALL : 0);
      break;
    }

    //
    // !EQUALIZER (set ping equalizer)
    //

    case HashCode("equalizer"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot toggle the latency equalizer.");
        break;
      }

      optional<bool> targetToggle = ParseBoolean(target);
      if (!targetToggle.has_value()) {
        ErrorReply("Unrecognized setting [" + target + "].");
        break;
      }

      if (targetGame->m_Config.m_LatencyEqualizerEnabled == targetToggle.value()) {
        if (targetToggle.value()) {
          ErrorReply("Latency equalizer is already enabled.");
        } else {
          ErrorReply("Latency equalizer is already disabled.");
        }
        break;
      }

      targetGame->m_Config.m_LatencyEqualizerEnabled = targetToggle.value();

      if (!targetToggle.value()) {
        if (targetGame->GetGameLoaded()) {
          auto nodes = targetGame->GetAllFrameNodes();
          targetGame->MergeFrameNodes(nodes);
          targetGame->ResetUserPingEqualizerDelays();
        }
        SendReply("Latency equalizer DISABLED.");
      } else {
        SendReply("Latency equalizer ENABLED.");
      }
      break;
    }

    //
    // !OPEN (open slot)
    //

    case HashCode("open"):
    case HashCode("o"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (target.empty()) {
        if (!targetGame->OpenSlot()) {
          ErrorReply("Cannot open further slots.");
        } else {
          SendReply("One slot opened.");
        }
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(target, 1u, targetGame->GetMap()->GetVersionMaxSlots());
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "o <SLOTNUM>");
        break;
      }

      vector<string> failedSlots;
      for (auto& elem : Args) {
        if (elem == 0 || elem > targetGame->GetMap()->GetVersionMaxSlots()) {
          ErrorReply("Usage: " + cmdToken + "o <SLOTNUM>");
          break;
        }
        const uint8_t SID = static_cast<uint8_t>(elem) - 1;
        const CGameSlot* slot = targetGame->GetSlot(SID);
        if (!slot || slot->GetSlotStatus() == SLOTSTATUS_OPEN) {
          failedSlots.push_back(to_string(elem));
          continue;
        }
        if (!targetGame->OpenSlot(SID, cmdHash == HashCode("open"))) {
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
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "pub <GAMENAME>");
        ErrorReply("Usage: " + cmdToken + "priv <GAMENAME>");
        break;
      }
      shared_ptr<CGame> targetGame = GetTargetGame();
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (targetGame) {
        if (!targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted()) {
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
          ErrorReply("A map must be loaded with " + (sourceRealm ? sourceRealm->GetCommandToken() : "!") + "map first.");
          break;
        }
      }

      if (target.length() > m_Aura->m_MaxGameNameSize) {
        ErrorReply("Unable to create game [" + target + "]. The game name is too long (the maximum is " + to_string(m_Aura->m_MaxGameNameSize) + " characters)");
        break;
      }

      if (targetGame) {
        SendReply("Trying to rehost with name [" + target + "].", CHAT_SEND_TARGET_ALL | CHAT_LOG_INCIDENT);
      }

      bool IsPrivate = cmdHash == HashCode("priv");
      if (targetGame) {
        targetGame->m_DisplayMode  = IsPrivate ? GAME_PRIVATE : GAME_PUBLIC;
        targetGame->m_GameName     = target;
        targetGame->m_HostCounter  = m_Aura->NextHostCounter();
        targetGame->UpdateGameDiscovery();

        for (auto& realm : m_Aura->m_Realms) {
          if (targetGame->m_IsMirror && realm->GetIsMirror()) {
            continue;
          }
          if (!realm->GetLoggedIn()) {
            continue;
          }
          if (targetGame->m_RealmsExcluded.find(realm->GetServer()) != targetGame->m_RealmsExcluded.end()) {
            continue;
          }
          realm->SetGameBroadcastPending(targetGame);
        }

        targetGame->m_CreationTime = targetGame->m_LastRefreshTime = GetTime();
      } else {
        if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
          ErrorReply("A map must be loaded with " + (sourceRealm ? sourceRealm->GetCommandToken() : "!") + "map first.");
          break;
        }
        if (!m_Aura->GetNewGameIsInQuotaConservative()) {
          ErrorReply("Already hosting a game.");
          break;
        }
        m_Aura->m_GameSetup->SetContext(shared_from_this());
        m_Aura->m_GameSetup->SetBaseName(target);
        m_Aura->m_GameSetup->SetDisplayMode(IsPrivate ? GAME_PRIVATE : GAME_PUBLIC);
        m_Aura->m_GameSetup->AcquireCreator();
        if (m_Aura->m_Config.m_AutomaticallySetGameOwner) {
          m_Aura->m_GameSetup->SetOwner(GetSender(), sourceRealm);
        }
        m_Aura->m_GameSetup->RunHost();
      }
      break;
    }

    //
    // !PUBBY (create public game by other user)
    // !PRIVBY (create private game by other user)
    //

    case HashCode("privby"):
    case HashCode("pubby"): {
      if (!CheckPermissions(m_Config->m_HostPermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to host games.");
        break;
      }
      shared_ptr<CGame> targetGame = GetTargetGame();
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      vector<string> Args = SplitArgs(target, 2u, 2u);
      string gameName;
      if (Args.empty() || (gameName = TrimString(Args[1])).empty()) {
        ErrorReply("Usage: " + cmdToken + "pubby <OWNER> , <GAMENAME>");
        ErrorReply("Usage: " + cmdToken + "privby <OWNER> , <GAMENAME>");
        break;
      }

      if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("A map must be loaded with " + (sourceRealm ? sourceRealm->GetCommandToken() : "!") + "map first.");
        break;
      }

      if (!m_Aura->GetNewGameIsInQuotaConservative()) {
        ErrorReply("Already hosting a game.");
        break;
      }

      bool IsPrivate = cmdHash == HashCode("privby");

      auto realmUserResult = GetParseTargetRealmUser(Args[0], true);
      if (!realmUserResult.GetSuccess()) {
        ErrorReply("Usage: " + cmdToken + "pubby <PLAYERNAME>@<REALM>");
        break;
      }
      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();
      m_Aura->m_GameSetup->SetContext(shared_from_this());
      m_Aura->m_GameSetup->SetBaseName(gameName);
      m_Aura->m_GameSetup->SetDisplayMode(IsPrivate ? GAME_PRIVATE : GAME_PUBLIC);
      m_Aura->m_GameSetup->AcquireCreator();
      m_Aura->m_GameSetup->SetOwner(targetName, targetRealm ? targetRealm : sourceRealm);
      m_Aura->m_GameSetup->RunHost();
      break;
    }

    //
    // !REMAKE
    //

    case HashCode("remake"):
    case HashCode("rmk"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame) {
        ErrorReply("No game is selected.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot remake the game.");
        break;
      }

      if (!targetGame->GetGameLoading() && !targetGame->GetGameLoaded()) {
        ErrorReply("This game has not started yet.");
        break;
      }

      if (!m_Aura->GetNewGameIsInQuotaConservative()) {
        ErrorReply("There is already a lobby being hosted.");
        break;
      }

      if (!targetGame->GetIsRemakeable()) {
        ErrorReply("This game cannot be remade.");
        break;
      }
      targetGame->SendAllChat("Please rejoin the remade game <<" + targetGame->GetGameName() + ">>.");
      targetGame->SendEveryoneElseLeftAndDisconnect("was disconnected (admin remade game)");
      targetGame->RemakeStart();
      break;
    }

    //
    // !START
    //

    case HashCode("start"):
    case HashCode("vs"):
    case HashCode("go"):
    case HashCode("g"):
    case HashCode("s"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_START_GAME)) {
        ErrorReply("You are not allowed to start this game.");
        break;
      }

      uint32_t ConnectionCount = targetGame->GetNumJoinedUsersOrFake();
      if (ConnectionCount == 0) {
        ErrorReply("Not enough players have joined.");
        break;
      }

      bool IsForce = target == "force" || target == "f";
      if (IsForce && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot forcibly start it.");
        break;
      }

      targetGame->StartCountDown(true, IsForce);
      break;
    }

    //
    // !QUICKSTART
    //

    case HashCode("sn"):
    case HashCode("startn"):
    case HashCode("quickstart"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot quickstart it.");
        break;
      }

      if (targetGame->m_LastPlayerLeaveTicks.has_value() && GetTicks() < targetGame->m_LastPlayerLeaveTicks.value() + 2000) {
        ErrorReply("Someone left the game less than two seconds ago.");
        break;
      }
      targetGame->StartCountDownFast(true);
      break;
    }

    //
    // !FREESTART
    //

    case HashCode("freestart"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit the game permissions.");
        break;
      }

      optional<bool> targetToggle = ParseBoolean(target);
      if (!targetToggle.has_value()) {
        ErrorReply("Unrecognized setting [" + target + "].");
        break;
      }
      if (!targetToggle.has_value()) {
        ErrorReply("Unrecognized setting [" + target + "].");
        break;
      }
      targetGame->m_PublicStart = targetToggle.value();
      if (targetGame->m_PublicStart) {
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (target.empty()) {
        targetGame->SendAllAutoStart();
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(target, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "autostart <slots> , <minutes>");
        break;
      }

      uint8_t minReadyControllers = static_cast<uint8_t>(Args[0]);
      uint32_t MinMinutes = 0;
      if (Args.size() >= 2) {
        MinMinutes = Args[1];
      }

      if (minReadyControllers > targetGame->GetMap()->GetMapNumControllers()) {
        ErrorReply("This map does not allow " + to_string(minReadyControllers) + " players.");
        break;
      }

      if (minReadyControllers <= targetGame->m_ControllersReadyCount) {
        // Misuse protection. Make sure the user understands AI players are added.
        ErrorReply("There are already " + to_string(targetGame->m_ControllersReadyCount) + " players ready. Use " + cmdToken + "start instead.");
        break;
      }

      int64_t time = GetTime();
      int64_t dueTime = time + static_cast<int64_t>(MinMinutes) * 60;
      if (dueTime < time) {
        ErrorReply("Failed to set timed start after " + to_string(MinMinutes) + " minutes.");
        break;
      }
      if (cmdHash != HashCode("addas")) {
        targetGame->m_AutoStartRequirements.clear();
      }
      targetGame->m_AutoStartRequirements.push_back(make_pair(minReadyControllers, dueTime));
      targetGame->SendAllAutoStart();
      break;
    }

    //
    // !CLEARAUTOSTART
    //

    case HashCode("clearas"):
    case HashCode("clearautostart"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (targetGame->m_AutoStartRequirements.empty()) {
        ErrorReply("There are no active autostart conditions.");
        break;
      }
      targetGame->m_AutoStartRequirements.clear();
      SendReply("Autostart removed.");
      break;
    }

    //
    // !SWAP (swap slots)
    //

    case HashCode("swap"):
    case HashCode("sw"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      bool onlyDraft = false;
      if (!GetIsSudo()) {
        if ((targetGame->GetMap()->GetMapOptions() & MAPOPT_CUSTOMFORCES) && (onlyDraft = targetGame->GetIsDraftMode())) {
          if (!GetIsGameUser() || !GetGameUser()->GetIsDraftCaptain()) {
            ErrorReply("Draft mode is enabled. Only draft captains may assign teams.");
            break;
          }
        } else if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "swap <PLAYER> , <PLAYER>");
        break;
      }

      vector<string> Args = SplitArgs(target, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "swap <PLAYER> , <PLAYER>" + HelpMissingComma(target));
        break;
      }

      auto searchResultOne = RunParseController(Args[0]);
      auto searchResultTwo = RunParseController(Args[1]);
      if (!searchResultOne.GetSuccess() || !searchResultTwo.GetSuccess()) {
        break;
      }

      uint8_t slotNumOne = searchResultOne.SID;
      uint8_t slotNumTwo = searchResultTwo.SID;
      GameUser::CGameUser* userOne = searchResultOne.user;
      GameUser::CGameUser* userTwo = searchResultTwo.user;

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
        //    ii. One slot is controlled by a user, and the other slot is empty. 
        //

        const CGameSlot* slotOne = targetGame->GetSlot(slotNumOne);
        const CGameSlot* slotTwo = targetGame->GetSlot(slotNumTwo);

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
          if (!GetIsGameUser() || (!GetGameUser()->GetIsDraftCaptainOf(slotOne->GetTeam()) && !GetGameUser()->GetIsDraftCaptainOf(slotTwo->GetTeam()))) {
            // Attempting to swap two slots of different unauthorized teams.
            ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
            break;
          }
          // One targetted slot is authorized. The other one belongs to another team.
          // Allow snatching or donating the player, but not trading it for another player.
          // Non-players cannot be transferred.
          if ((userOne == nullptr) == (userTwo == nullptr)) {
            ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
            break;
          }
          if (userOne == nullptr) {
            // slotOne is guaranteed to be occupied by a non-user
            // slotTwo is guaranteed to be occupied by a user
            if (slotOne->GetSlotStatus() != SLOTSTATUS_OPEN) {
              ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
              break;
            }
          } else {
            // slotTwo is guaranteed to be occupied by a non-user
            // slotOne is guaranteed to be occupied by a user
            if (slotTwo->GetSlotStatus() != SLOTSTATUS_OPEN) {
              ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
              break;
            }
          }
        } else if (!GetIsGameUser() || !GetGameUser()->GetIsDraftCaptainOf(slotOne->GetTeam())) {
          // Both targetted slots belong to the same team, but not to the authorized team.
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        } else {
          // OK. Attempting to swap two slots within own team.
          // They could do it themselves tbh. But let's think of the afks.
        }
      }

      if (!targetGame->SwapSlots(slotNumOne, slotNumTwo)) {
        ErrorReply("These slots cannot be swapped.");
        break;
      }
      targetGame->ResetLayoutIfNotMatching();
      if ((userOne != nullptr) && (userTwo != nullptr)) {
        SendReply("Swapped " + userOne->GetName() + " with " + userTwo->GetName() + ".");
      } else if (!userOne && !userTwo) {
        SendReply("Swapped slots " + ToDecString(slotNumOne + 1) + " and " + ToDecString(slotNumTwo + 1) + ".");
      } else if (userOne) {
        SendReply("Swapped user [" + userOne->GetName() + "] to slot " + ToDecString(slotNumTwo + 1) + ".");
      } else {
        SendReply("Swapped user [" + userTwo->GetName() + "] to slot " + ToDecString(slotNumOne + 1) + ".");
      }
      break;
    }

    //
    // !UNHOST
    //

    case HashCode("unhost"):
    case HashCode("uh"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetCountDownStarted()) {
        // Intentionally allows !unhost for fake (mirror) lobbies.
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      LogStream(*m_Output, targetGame->GetLogPrefix() + "is over (admin cancelled game) [" + GetSender() + "]");
      SendReply("Aborting " + targetGame->GetStatusDescription());
      targetGame->m_Exiting = true;
      targetGame->StopPlayers("was disconnected (admin cancelled game)");
      break;
    }

    //
    // !DOWNLOAD
    // !DL
    //

    case HashCode("download"):
    case HashCode("dl"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      if (
        m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_NEVER ||
        !targetGame->GetMap()->GetMapFileIsValid() ||
        m_Aura->m_StartedGames.size() >= m_Aura->m_Config.m_MaxStartedGames
      ) {
        if (targetGame->GetMapSiteURL().empty()) {
          ErrorAll("Cannot transfer the map.");
        } else {
          ErrorAll("Cannot transfer the map. Please download it from <" + targetGame->GetMapSiteURL() + ">");
        }
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(target);
      if (!targetPlayer) {
        break;
      }

      if (targetPlayer->GetMapTransfer().GetIsInProgress()) {
        ErrorReply("Player [" + targetPlayer->GetName() + "] is already downloading the map.");
        break;
      }

      const CGameSlot* slot = targetGame->InspectSlot(targetGame->GetSIDFromUID(targetPlayer->GetUID()));
      if (!slot || slot->GetDownloadStatus() == 100) {
        ErrorReply("Map transfer failed unexpectedly.", CHAT_LOG_INCIDENT);
        break;
      }

      SendReply("Map download started for [" + targetPlayer->GetName() + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_INCIDENT);
      targetGame->Send(targetPlayer, GameProtocol::SEND_W3GS_STARTDOWNLOAD(targetGame->GetHostUID()));
      targetPlayer->SetDownloadAllowed(true);
      targetPlayer->GetMapTransfer().Start();
      targetPlayer->RemoveKickReason(GameUser::KickReason::MAP_MISSING);
      targetPlayer->CheckStillKicked();
      break;
    }

    //
    // !DROP
    //

    case HashCode("drop"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetGameLoaded())
        break;

      if (!targetGame->GetIsLagging()) {
        ErrorReply("Nobody is currently lagging.");
        break;
      }

      bool hasPermissions = CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER);
      if (!hasPermissions) {
        GameUser::CGameUser* gameOwner = targetGame->GetOwner();
        if (gameOwner && !gameOwner->GetIsLagging()) {
          ErrorReply("You are not the game owner, and therefore cannot drop laggers.");
          break;
        }
        if (targetGame->GetLockedOwnerLess()) {
          ErrorReply("This command is not available for this game. Please wait for automatic drop.");
          break;
        }
        if (targetGame->GetStartedLaggingTime() + 20 >= GetTime()) {
          ErrorReply(cmdToken + "drop command is not yet available. Please allow some leeway for network conditions to improve.");
          break;
        }
      }

      if (GetIsGameUser() && GetGameUser()->GetIsOwner(nullopt)) {
        targetGame->StopLaggers("lagged out (dropped by owner)");
      } else if (hasPermissions) {
        targetGame->StopLaggers("lagged out (dropped by admin)");
      } else {
        targetGame->StopLaggers("lagged out (dropped by " + GetSender() + ")");
      }
      break;
    }

    //
    // !REFEREE
    //

    case HashCode("referee"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      // Don't allow during countdown for transparency purposes.
      if (!targetGame || targetGame->GetIsMirror() ||
        (targetGame->GetCountDownStarted() && !targetGame->GetGameLoaded()))
        break;

      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(target);
      if (!targetPlayer) {
        break;
      }
      if (!targetPlayer->GetIsObserver()) {
        ErrorReply("[" + targetPlayer->GetName() + "] is not an observer.");
        break;
      }

      if (targetGame->GetMap()->GetMapObservers() != MAPOBS_REFEREES) {
        ErrorReply("This game does not allow referees.");
        break;
      }

      targetGame->SetUsesCustomReferees(true);
      for (auto& otherPlayer: targetGame->m_Users) {
        if (otherPlayer->GetIsObserver())
          otherPlayer->SetPowerObserver(false);
      }
      targetPlayer->SetPowerObserver(true);
      SendAll("Player [" + targetPlayer->GetName() + "] was promoted to referee (Other observers may only use observer chat)");
      break;
    }

    //
    // !MUTE
    //

    case HashCode("mute"): {
      shared_ptr<CGame> targetGame = GetTargetGame();
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot mute people.");
        break;
      }

      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games. Use " + cmdToken + "muteall from the game lobby next time.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "mute [PLAYERNAME]");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetUser(target);
      if (!targetPlayer) {
        break;
      }
      if (!GetIsSudo()) {
        if (targetPlayer == GetGameUser()) {
          ErrorReply("Cannot mute yourself.");
          break;
        }
        if (sourceRealm && (
          sourceRealm->GetIsAdmin(targetPlayer->GetName()) || (
            sourceRealm->GetIsModerator(targetPlayer->GetName()) &&
            !CheckPermissions(m_Config->m_AdminBasePermissions, COMMAND_PERMISSIONS_ROOTADMIN)
          )
        )) {
          ErrorReply("User [" + targetPlayer->GetName() + "] is an admin on server [" + m_ServerName + "]");
          break;
        }
      }

      int64_t muteSeconds = targetGame->GetGameLoading() || targetGame->GetGameLoaded() ? 1200 : 420;
      if (!targetPlayer->Mute(muteSeconds)) {
        ErrorReply("User [" + targetPlayer->GetName() + "] is already muted.");
        break;
      }

      SendAll("[" + targetPlayer->GetName() + "] was muted by [" + GetSender() + "] for " + ToDurationString(muteSeconds));
      break;
    }

    //
    // !MUTEALL
    //

    case HashCode("muteall"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror() || !targetGame->GetGameLoaded())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot disable \"All\" chat channel.");
        break;
      }

      if (targetGame->m_MuteAll) {
        ErrorReply("Global and private chats are already muted.");
        break;
      }

      if (targetGame->GetIsHiddenPlayerNames() && targetGame->GetGameLoaded()) {
        ErrorReply("Chat can only be toggled from the game lobby for incognito mode games.");
        break;
      }

      SendAll("Global and private chats muted (allied chat is unaffected)");
      targetGame->m_MuteAll = true;
      break;
    }

    //
    // !ABORT (abort countdown)
    // !A
    //

    case HashCode("abort"):
    case HashCode("a"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot abort game start.");
        break;
      }

      if (targetGame->GetCountDownStarted()) {
        targetGame->StopCountDown();
        if (targetGame->GetIsAutoStartDue()) {
          targetGame->m_AutoStartRequirements.clear();
          SendAll("Countdown stopped by " + GetSender() + ". Autostart removed.");
        } else {
          SendAll("Countdown stopped by " + GetSender() + ".");
        }
      } else {
        targetGame->m_AutoStartRequirements.clear();
        SendAll("Autostart removed.");
      }
      break;
    }

    //
    // !CHECKNETWORK
    //
    case HashCode("checknetwork"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!targetGame || !targetGame->GetIsStageAcceptingJoins()) {
        ErrorReply("Use this command when you are hosting a game lobby.");
        break;
      }
      uint8_t checkMode = HEALTH_CHECK_ALL | HEALTH_CHECK_VERBOSE;
      if (!m_Aura->m_Net.m_SupportTCPOverIPv6) {
        checkMode &= ~HEALTH_CHECK_PUBLIC_IPV6;
        checkMode &= ~HEALTH_CHECK_LOOPBACK_IPV6;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        ErrorReply("Not allowed to check network status.");
        break;
      }
      const bool TargetAllRealms = target == "*" || (target.empty() && !sourceRealm);
      shared_ptr<CRealm> targetRealm = nullptr;
      if (!TargetAllRealms) {
        targetRealm = GetTargetRealmOrCurrent(target);
        if (!targetRealm) {
          ErrorReply("Usage: " + cmdToken + "checknetwork *");
          ErrorReply("Usage: " + cmdToken + "checknetwork <REALM>");
          break;
        }
      }

      if (!m_Aura->m_Net.QueryHealthCheck(shared_from_this(), checkMode, targetRealm, targetGame)) {
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      vector<uint32_t> Args = SplitNumericArgs(target, 1, 2);
      if (Args.size() == 1) {
        if (Args[0] == 0 || Args[0] > 0xFFFF) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
        Args.push_back(Args[0]);
      } else if (Args.empty()) {
        if (!target.empty() || !targetGame) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
        Args.push_back(targetGame->GetHostPort());
        Args.push_back(targetGame->GetHostPort());
      } else {
        if (Args[0] == 0 || Args[0] > 0xFFFF || Args[1] == 0 || Args[1] > 0xFFFF) {
          ErrorReply("Usage: " + cmdToken + "portforward <EXTPORT> , <INTPORT>");
          break;
        }
      }

      uint16_t extPort = static_cast<uint16_t>(Args[0]);
      uint16_t intPort = static_cast<uint16_t>(Args[1]);

      SendReply("Trying to forward external port " + to_string(extPort) + " to internal port " + to_string(intPort) + "...");
      uint8_t result = m_Aura->m_Net.RequestUPnP(NET_PROTOCOL_TCP, extPort, intPort, LOG_LEVEL_INFO, true);
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
      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("Not allowed to check ban status.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "checkban <PLAYERNAME>@<REALM>");
        break;
      }

      auto realmUserResult = GetParseTargetRealmUser(target, true);
      if (!realmUserResult.GetSuccess()) {
        if (!realmUserResult.hostName.empty()) {
          ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "checkban <PLAYERNAME>@<REALM>");
        }
        break;
      }
      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();
      if (targetRealm) {
        targetHostName = targetRealm->GetDataBaseID();
      }

      vector<string> CheckResult;
      for (auto& realm : m_Aura->m_Realms) {
        if (realm->GetIsMirror()) {
          continue;
        }

        CDBBan* Ban = m_Aura->m_DB->UserBanCheck(targetName, targetHostName, realm->GetDataBaseID());
        if (Ban) {
          CheckResult.push_back("[" + realm->GetServer() + "] - banned by [" + Ban->GetModerator() + "] because [" + Ban->GetReason() + "] (" + Ban->GetDate() + ")");
          delete Ban;
        }
      }

      {
        CDBBan* Ban = m_Aura->m_DB->UserBanCheck(targetName, targetHostName, string());
        if (Ban) {
          CheckResult.push_back("[LAN/VPN] - banned by [" + Ban->GetModerator() + "] because [" + Ban->GetReason() + "] (" + Ban->GetDate() + ")");
          delete Ban;
        }
      }

      if (CheckResult.empty()) {
        SendReply("[" + targetName + "@" + targetHostName + "] is not banned from any server.");
      } else {
        SendReply("[" + targetName + "@" + targetHostName + "] is banned from " + to_string(CheckResult.size()) + " server(s): " + JoinVector(CheckResult, false));
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
      shared_ptr<CRealm> targetRealm = GetTargetRealmOrCurrent(target);
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "listbans <REALM>");
        break;
      }
      if (targetRealm != sourceRealm && !GetIsSudo()) {
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
      if (target.empty()) {
        ErrorReply("Usage " + cmdToken + "import [DATA TYPE]. Supported data types are: aliases");
        break;
      }
      string lower = target;
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
      if (target.empty()) {
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [FILE NAME]");
        ErrorReply("Usage " + cmdToken + "alias [ALIAS], [MAP IDENTIFIER]");
        break;
      }
      vector<string> Args = SplitArgs(target, 2u, 2u);
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
    // !ALTS
    //

    case HashCode("alts"): {
      if (!CheckPermissions(m_Config->m_AliasPermissions, COMMAND_PERMISSIONS_SUDO)) {
        ErrorReply("Not allowed to check alternate accounts.");
        break;
      }
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "alts <PLAYERNAME>");
        ErrorReply("Usage: " + cmdToken + "alts <PLAYERNAME>@<REALM>");
        break;
      }
      shared_ptr<CGame> targetGame = GetTargetGame();

      auto realmUserResult = GetParseTargetRealmUser(target, true, true);
      if (!realmUserResult.GetSuccess()) {
        if (!realmUserResult.hostName.empty()) {
          ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "alts <PLAYERNAME>");
          ErrorReply("Usage: " + cmdToken + "alts <PLAYERNAME>@<REALM>");
        }
        break;
      }
      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();

      string targetIP;
      if (targetGame) {
        targetIP = targetGame->GetBannableIP(targetName, targetHostName);
      }
      vector<string> targetIPs = m_Aura->m_DB->GetIPs(
        targetName,
        targetRealm ? targetRealm->GetDataBaseID() : targetHostName
      );
      if (!targetIP.empty() && std::find(targetIPs.begin(), targetIPs.end(), targetIP) == targetIPs.end()) {
        targetIPs.push_back(targetIP);
      }

      set<string> allAlts;
      for (const auto& ip : targetIPs) {
        vector<string> alts = m_Aura->m_DB->GetAlts(ip);
        for (const auto& alt : alts) {
          allAlts.insert(alt);
        }
      }
      if (allAlts.empty()) {
        SendReply("No alternate accounts found.");
      } else {
        vector<string> allAltsVector = vector<string>(allAlts.begin(), allAlts.end());
        SendReply("Alternate accounts: " + JoinVector(allAltsVector, false));
      }
      break;
    }

    case HashCode("ban"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict()) {
        ErrorReply("This command may only be used in a game lobby, and will only affect it. For persistent bans, use " + cmdToken + "pban .");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot ban users.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>, <REASON>");
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>@<REALM>, <REASON>");
        break;
      }

      // Demand a reason to ensure transparency.
      vector<string> Args = SplitArgs(target, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>, <REASON>");
        ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>@<REALM>, <REASON>");
        break;
      }

      string inputTarget = Args[0];
      string reason = Args[1];

      auto realmUserResult = GetParseTargetRealmUser(inputTarget, true, true);
      if (!realmUserResult.GetSuccess()) {
        if (!realmUserResult.hostName.empty()) {
          ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>");
          ErrorReply("Usage: " + cmdToken + "ban <PLAYERNAME>@<REALM>");
        }
        break;
      }
      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();

      string targetIP = targetGame->GetBannableIP(targetName, targetHostName);
      if (targetIP.empty()) {
        string databaseHostName = targetHostName;
        if (targetRealm) databaseHostName = targetRealm->GetDataBaseID();
        targetIP = m_Aura->m_DB->GetLatestIP(targetName, databaseHostName);
      }

      string emptyAddress;
      if (targetGame->GetIsScopeBanned(targetName, targetHostName, emptyAddress)) {
        ErrorReply("[" + targetName + "@" + targetHostName + "] was already banned from this game.");
        break;
      }

      if (!targetGame->AddScopeBan(targetName, targetHostName, targetIP)) {
        ErrorReply("Failed to ban [" + targetName + "@" + targetHostName + "] from joining this game.");
        break;
      }

      GameUser::CGameUser* targetPlayer = targetGame->GetUserFromName(targetName, false);
      if (targetPlayer && targetPlayer->GetRealm(false) == targetRealm) {
        targetPlayer->CloseConnection();
        //targetPlayer->SetDeleteMe(true);
        targetPlayer->SetLeftReason("was banned by [" + GetSender() + "]");
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
      }

      SendReply("[" + targetName + "@" + targetHostName + "] banned from joining this game.");
      Print(targetGame->GetLogPrefix() + "[" + targetName + "@" + targetHostName + "|" + targetIP  + "] banned from joining game.");
      break;
    }

    case HashCode("unban"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict()) {
        ErrorReply("This command may only be used in a game lobby, and will only affect it. For persistent bans, use " + cmdToken + "punban .");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unban users.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "unban <PLAYERNAME>@<REALM>");
        break;
      }

      auto realmUserResult = GetParseTargetRealmUser(target, true, true);
      if (!realmUserResult.GetSuccess()) {
        if (!realmUserResult.hostName.empty()) {
          ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "unban <PLAYERNAME>@<REALM>");
        }
        break;
      }
      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();

      string emptyAddress;
      if (!targetGame->GetIsScopeBanned(targetName, targetHostName, emptyAddress)) {
        ErrorReply("[" + targetName + "@" + targetHostName + "] was not banned from this game.");
        break;
      }

      if (!targetGame->RemoveScopeBan(targetName, targetHostName)) {
        ErrorReply("Failed to unban user [" + targetName + "@" + targetHostName + "].");
        break;
      }

      SendReply("User [" + targetName + "@" + targetHostName + "] will now be allowed to join this game.");
      break;
    }

    //
    // !PBAN
    //

    case HashCode("pban"): {
      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to persistently ban players.");
        break;
      }
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>, <REASON>");
        ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>@<REALM>, <REASON>");
        break;
      }

      vector<string> Args = SplitArgs(target, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>, <REASON>");
        ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>@<REALM>, <REASON>");
        break;
      }
      shared_ptr<CGame> targetGame = GetTargetGame();
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      string inputTarget = Args[0];
      string reason = Args[1];

      auto realmUserResult = GetParseTargetRealmUser(inputTarget, true, true);
      if (!realmUserResult.GetSuccess()) {
        if (!realmUserResult.hostName.empty()) {
          ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>");
          ErrorReply("Usage: " + cmdToken + "pban <PLAYERNAME>@<REALM>");
        }
        break;
      }
      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();

      string authServer = m_ServerName;
      if (sourceRealm) {
        authServer = sourceRealm->GetDataBaseID();
      }
      if (targetRealm) {
        targetHostName = targetRealm->GetDataBaseID();
      }

      CDBBan* Ban = m_Aura->m_DB->UserBanCheck(targetName, targetHostName, authServer);
      if (Ban != nullptr) {
        ErrorReply("[" + Ban->GetName() + "@" + Ban->GetServer() + "] was already banned by [" + Ban->GetModerator() + "] because [" + Ban->GetReason() + "] (" + Ban->GetDate() + ")");
        delete Ban;
        break;
      }

      string ip = m_Aura->m_DB->GetLatestIP(targetName, targetHostName);

      if (sourceRealm && authServer == targetHostName) {
        if (sourceRealm->GetIsAdmin(targetName) || (sourceRealm->GetIsModerator(targetName) &&
          !CheckPermissions(m_Config->m_AdminBasePermissions, COMMAND_PERMISSIONS_ROOTADMIN))
        ) {
          ErrorReply("User [" + targetName + "] is an admin on server [" + m_ServerName + "]");
          break;
        }
      }

      if (!m_Aura->m_DB->BanAdd(targetName, targetHostName, authServer, ip, GetSender(), reason)) {
        ErrorReply("Failed to execute ban.");
        break;
      }

      GameUser::CGameUser* targetPlayer = targetGame->GetUserFromName(targetName, false);
      if (targetPlayer && targetPlayer->GetRealm(false) == targetRealm) {
        targetPlayer->CloseConnection();
        //targetPlayer->SetDeleteMe(true);
        targetPlayer->SetLeftReason("was persistently banned by [" + GetSender() + "]");
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
      }

      SendReply("[" + targetName + "@" + targetHostName + "] banned.");
      break;
    }

    //
    // !UNPBAN
    //

    case HashCode("unpban"):
    case HashCode("punban"): {
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to remove persistent bans.");
        break;
      }
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "punban <PLAYERNAME>@<REALM>");
        break;
      }

      auto realmUserResult = GetParseTargetRealmUser(target, true, true);
      if (!realmUserResult.GetSuccess()) {
        if (!realmUserResult.hostName.empty()) {
          ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
        } else {
          ErrorReply("Usage: " + cmdToken + "punban <PLAYERNAME>@<REALM>");
        }
        break;
      }
      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();

      if (targetRealm) {
        targetHostName = targetRealm->GetDataBaseID();
      }
      string authServer = m_ServerName;
      if (sourceRealm) {
        authServer = sourceRealm->GetDataBaseID();
      }
      CDBBan* Ban = m_Aura->m_DB->UserBanCheck(targetName, targetHostName, authServer);
      if (Ban) {
        delete Ban;
      } else {
        ErrorReply("User [" + targetName + "@" + targetHostName + "] is not banned @" + ToFormattedRealm(m_ServerName) + ".");
        break;
      }

      if (!m_Aura->m_DB->BanRemove(targetName, targetHostName, authServer)) {
        ErrorReply("Failed to unban user [" + targetName + "@" + targetHostName + "].");
        break;
      }

      SendReply("Unbanned user [" + targetName + "@" + targetHostName + "] on @" + ToFormattedRealm(m_ServerName) + ".");
      break;
    }

    //
    // !CLEARHCL
    //

    case HashCode("clearhcl"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's configuration.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (targetGame->m_HCLCommandString.empty()) {
        ErrorReply("There was no game mode set.");
        break;
      }

      if (targetGame->GetMap()->GetHCLRequired() && !GetIsSudo()) {
        ErrorReply("Game mode cannot be reconfigured.");
        break;
      }

      targetGame->m_HCLCommandString.clear();
      SendAll("Game mode reset");
      break;
    }

    //
    // !STATUS
    //

    case HashCode("status"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      string message = "Status: ";

      for (const auto& realm : m_Aura->m_Realms) {
        string statusFragment;
        if (!realm->GetLoggedIn()) {
          statusFragment = "offline";
        } else if (targetGame && (realm->GetGameBroadcast() != targetGame || realm->GetIsGameBroadcastErrored())) {
          statusFragment = "unlisted";
        } else {
          statusFragment = "online";
        }
        message += "[" + realm->GetUniqueDisplayName() + " - " + statusFragment + "] ";
      }

      if (m_Aura->m_IRC.GetIsEnabled()) {
        message += "[" + m_Aura->m_IRC.m_Config.m_HostName + (!m_Aura->m_IRC.m_WaitingToConnect ? " - online]" : " - offline] ");
      }

      if (m_Aura->m_Discord.GetIsEnabled()) {
        message += m_Aura->m_Discord.m_Config.m_HostName + (m_Aura->m_Discord.GetIsConnected() ? " [online]" : " [offline]");
      }

      SendReply(message);
      break;
    }

    //
    // !SENDLAN
    //

    case HashCode("sendlan"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsStageAcceptingJoins()) {
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (targetGame->GetIsMirror()) {
        // This is not obvious.
        ErrorReply("Mirrored games cannot be broadcast to LAN");
        break;
      }

      optional<bool> targetToggle = ParseBoolean(target);
      if (!targetToggle.has_value()) {
        ErrorReply("Unrecognized setting [" + target + "].");
        break;
      }

      if (targetToggle.has_value()) {
        // Turn ON/OFF
        targetGame->SetUDPEnabled(targetToggle.value());
        if (targetToggle.value()) {
          targetGame->SendGameDiscoveryCreate();
          targetGame->SendGameDiscoveryRefresh();
          if (!m_Aura->m_Net.m_UDPMainServerEnabled)
            targetGame->SendGameDiscoveryInfo(); // Since we won't be able to handle incoming GAME_SEARCH packets
        }
        if (targetGame->GetUDPEnabled()) {
          SendReply("This lobby will now be displayed in the Local Area Network game list");
        } else {
          SendReply("This lobby will no longer be displayed in the Local Area Network game list");
        }
      } else {
        string ip;
        uint16_t port;
        if (!SplitIPAddressAndPortOrDefault(target, GAME_DEFAULT_UDP_PORT, ip, port) || port == 0) {
          ErrorReply("Usage: " + cmdToken + "sendlan ON/OFF");
          ErrorReply("Usage: " + cmdToken + "sendlan <IP>");
          break;
        }
        optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(ip, ACCEPT_ANY);
        if (!maybeAddress.has_value()) {
          ErrorReply("Usage: " + cmdToken + "sendlan ON/OFF");
          ErrorReply("Usage: " + cmdToken + "sendlan <IP>");
          break;
        }
        SetAddressPort(&(maybeAddress.value()), port);
        sockaddr_storage* address = &(maybeAddress.value());
        if (GetInnerIPVersion(address) == AF_INET6 && !(m_Aura->m_Net.m_SupportUDPOverIPv6 && m_Aura->m_Net.m_SupportTCPOverIPv6)) {
          ErrorReply("IPv6 support hasn't been enabled. Set <net.ipv6.tcp.enabled = yes>, and <net.udp_ipv6.enabled = yes> if you want to enable it.");
          break;
        }
        if (((address->ss_family == AF_INET6 && isSpecialIPv6Address(reinterpret_cast<struct sockaddr_in6*>(address))) ||
          (address->ss_family == AF_INET && isSpecialIPv4Address(reinterpret_cast<struct sockaddr_in*>(address)))) && !GetIsSudo()) {
          ErrorReply("Special IP address rejected. Add it to <net.game_discovery.udp.extra_clients.ip_addresses> or use sudo if you are sure about this.");
          break;
        }
        if (targetGame->m_Config.m_ExtraDiscoveryAddresses.size() >= UDP_DISCOVERY_MAX_EXTRA_ADDRESSES) {
          ErrorReply("Max sendlan addresses reached.");
          break;
        }
        bool alreadySending = false;
        for (auto& existingAddress : targetGame->m_Config.m_ExtraDiscoveryAddresses) {
          if (GetSameAddressesAndPorts(&existingAddress, address)) {
            alreadySending = true;
            break;
          }
        }
        if (alreadySending && targetGame->GetUDPEnabled()) {
          ErrorReply("Already sending game info to " + target);
          break;
        }
        if (!targetGame->GetUDPEnabled()) {
          SendReply("This lobby will now be displayed in the Local Area Network game list");
        }
        targetGame->SetUDPEnabled(true);
        if (!alreadySending) {
          targetGame->m_Config.m_ExtraDiscoveryAddresses.push_back(std::move(maybeAddress.value()));
        }
        SendReply("This lobby will be displayed in the Local Area Network game list for IP " + target + ". Make sure your peer has done UDP hole-punching.");
      }

      break;
    }

    //
    // !SENDLANINFO
    //

    case HashCode("sendlaninfo"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsStageAcceptingJoins()) {
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (!target.empty()) {
        ErrorReply("Usage: " + cmdToken + "sendlaninfo");
        ErrorReply("You may want !sendlan <IP> or !sendlan <ON/OFF> instead");
        break;
      }

      if (targetGame->GetIsMirror()) {
        // This is not obvious.
        ErrorReply("Mirrored games cannot be broadcast to LAN");
        break;
      }

      targetGame->SetUDPEnabled(true);
      targetGame->SendGameDiscoveryInfo();
      SendReply("Sent game info to peers.");
      break;
    }

    //
    // !LANVERSION
    //

    case HashCode("lanversion"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict()) {
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (!target.empty()) {
        ErrorReply("Usage: " + cmdToken + "lanversion <PLAYERNAME>, <VERSION>");
        break;
      }

      vector<string> Args = SplitArgs(target, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "lanversion <PLAYERNAME>, <VERSION>");
        break;
      }

      string targetName = ToLowerCase(Args[0]);
      optional<Version> targetVersion = ParseGameVersion(Args[1]);
      if (targetName.empty() || targetName.size() > MAX_PLAYER_NAME_SIZE || !targetVersion.has_value()) {
        ErrorReply("Usage: " + cmdToken + "lanversion <PLAYERNAME>, <VERSION>");
        break;
      }

      if (!targetGame->GetIsSupportedGameVersion(targetVersion.value())) {
        ErrorReply("This lobby does not support crossplay with v" + ToVersionString(targetVersion.value()));
        break;
      }

      auto match = targetGame->m_Config.m_GameVersionsByLANPlayerNames.find(targetName);
      if (match != targetGame->m_Config.m_GameVersionsByLANPlayerNames.end()) {
        match->second = targetVersion.value();
      } else if (targetGame->m_Config.m_GameVersionsByLANPlayerNames.size() >= MAX_GAME_VERSION_OVERRIDES) {
        ErrorReply("Cannot customize additional game versions (limit is " + ToDecString(MAX_GAME_VERSION_OVERRIDES) + ".");
        break;
      } else {
        targetGame->m_Config.m_GameVersionsByLANPlayerNames[targetName] = targetVersion.value();
      }

      if (!targetGame->GetIsHiddenPlayerNames()) {
        GameUser::CGameUser* targetPlayer = targetGame->GetUserFromName(targetName, false);
        if (!targetPlayer->GetGameVersionIsExact() || targetPlayer->GetGameVersion() != targetVersion) {
          targetPlayer->CloseConnection();
          targetPlayer->SetLeftReason("was automatically kicked (game version mismatch)");
          targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
        }
      }

      SendReply("[" + targetName + "@@@LAN/VPN] will now be able to join using v" + ToVersionString(targetVersion.value()));
      break;
    }

    //
    // !OWNER (set game owner)
    //

    case HashCode("owner"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!targetGame) {
        ErrorReply("No game found.");
        break;
      }

      if ((!targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted() || targetGame->m_OwnerLess) && !GetIsSudo()) {
        ErrorReply("Cannot take ownership of this game.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_POTENTIAL_OWNER)) {
        if (target.empty() && targetGame->HasOwnerSet()) {
          SendReply("The owner is [" + targetGame->m_OwnerName + "@" + ToFormattedRealm(targetGame->m_OwnerRealm) + "]");
        }
        // These checks help with troubleshooting.
        if (!targetGame->MatchOwnerName(GetSender()) || !GetIsGameUser()) {
          ErrorReply("You are not allowed to change the owner of this game.");
        } else if (m_ServerName.empty() != targetGame->m_OwnerRealm.empty()) {
          if (targetGame->m_OwnerRealm.empty()) {
            ErrorReply("You must join from LAN/VPN to use your owner permissions.");
          } else {
            ErrorReply("You must join from [" + targetGame->m_OwnerRealm + "] to use your owner permissions.");
          }
        } else if (!targetGame->m_OwnerRealm.empty() && GetGameUser()->GetRealm(true) == nullptr) {
          ErrorReply("You have not verified your identity yet.");
        } else {
          ErrorReply("Permissions not granted.");
        }
        break;
      }

      string targetName, targetHostName;
      shared_ptr<CRealm> targetRealm = nullptr;
      if (target.empty()) {
        targetName = GetSender();
        if (sourceRealm) {
          targetRealm = sourceRealm;
          targetHostName = m_ServerName;
        }
      } else {
        auto realmUserResult = GetParseTargetRealmUser(target, true, true);
        if (!realmUserResult.GetSuccess()) {
          if (!realmUserResult.hostName.empty()) {
            ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
          } else {
            ErrorReply("Usage: " + cmdToken + "owner <PLAYERNAME>");
            ErrorReply("Usage: " + cmdToken + "owner <PLAYERNAME>@<REALM>");
          }
          break;
        }
        targetName = realmUserResult.userName;
        targetHostName = realmUserResult.hostName;
        targetRealm = realmUserResult.GetRealm();
      }

      GameUser::CGameUser* targetPlayer = targetGame->GetUserFromName(targetName, false);
      if (targetPlayer && targetPlayer->GetRealmHostName() != targetHostName) {
        ErrorReply("[" + targetPlayer->GetExtendedName() + "] is not connected from " + targetHostName);
        break;
      }

      if (!targetPlayer && !targetRealm) {
        // Prevent arbitrary LAN players from getting ownership before joining.
        ErrorReply("[" + targetName + "@" + ToFormattedRealm() + "] must join the game first.");
        break;
      }
      if (!targetPlayer && !GetIsSudo() && !CheckConfirmation(cmdToken, baseCommand, baseTarget, "Player [" + targetName + "] is not in this game lobby. ")) {
        break;
      }
      if ((targetPlayer && targetPlayer != GetGameUser() && !targetRealm && !targetPlayer->GetIsRealmVerified()) && !GetIsSudo() &&
        !CheckConfirmation(cmdToken, baseCommand, baseTarget, "Player [" + targetName + "] has not been verified by " + targetHostName + ". ")) {
        break;
      }
      const bool alreadyOwner = targetGame->m_OwnerName == targetName && targetGame->m_OwnerRealm == targetHostName;

      if (targetPlayer) {
        targetPlayer->SetWhoisShouldBeSent(true);
        targetGame->SendOwnerCommandsHelp(cmdToken, targetPlayer);
      }

      if (alreadyOwner) {
        SendAll("[" + targetName + "@" + ToFormattedRealm(targetHostName) + "] is already the owner of this game.");
      } else {
        targetGame->SetOwner(targetName, targetHostName);
        SendReply("Setting game owner to [" + targetName + "@" + ToFormattedRealm(targetHostName) + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_INCIDENT);
      }
      break;
    }

    //
    // !UNOWNER (removes a game owner)
    //

    case HashCode("unowner"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame) {
        ErrorReply("No game found.");
        break;
      }

      if ((!targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted() || targetGame->m_OwnerLess) && !GetIsSudo()) {
        ErrorReply("Cannot take ownership of this game.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not allowed to change the owner of this game.");
        break;
      }

      if (!targetGame->HasOwnerSet()) {
        ErrorReply("This game has no owner.");
        break;
      }

      targetGame->ReleaseOwner();
      SendReply("Owner removed.");
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

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "say <REALM> , <MESSAGE>");
        break;
      }

      string::size_type MessageStart = target.find(',');
      string RealmId = TrimString(target.substr(0, MessageStart));
      string Message = TrimString(target.substr(MessageStart + 1));
      if (Message.empty()) {
        ErrorReply("Usage: " + cmdToken + "say <REALM> , <MESSAGE>");
        break;
      }
      bool isBNETCommand = Message[0] == '/';
      bool willConflictWithWhisper = false;
      if (isBNETCommand && !GetIsSudo()) {
        ErrorReply("You are not allowed to send bnet commands.");
        break;
      }
      if (!isBNETCommand && m_Aura->GetIsAdvertisingGames()) {
        ErrorReply("Cannot send bnet chat messages while the bot is hosting a game lobby.");
        break;
      }
      if (isBNETCommand) {
        string bnetCommand;
        string::size_type spIndex = Message.find(' ');
        if (spIndex == string::npos) {
          bnetCommand = ToLowerCase(Message.substr(1));
        } else {
          bnetCommand = ToLowerCase(Message.substr(1, spIndex -1));
        }
        willConflictWithWhisper = BNETProtocol::GetIsCommandConflictsWithWhisper(HashCode(bnetCommand), spIndex != string::npos);
      }
      transform(begin(RealmId), end(RealmId), begin(RealmId), [](char c) { return static_cast<char>(std::tolower(c)); });

      const bool ToAllRealms = RealmId.length() == 1 && RealmId[0] == '*';
      bool Success = false;

      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;
        if (ToAllRealms || bnet->GetInputID() == RealmId) {
          if (isBNETCommand) {
            CQueuedChatMessage* queuedMessage = bnet->QueueCommand(Message);
            queuedMessage->SetEarlyFeedback("Command sent.");
            if (willConflictWithWhisper) queuedMessage->SetCallback(CHAT_CALLBACK_RESET, 0);
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (m_Aura->m_Realms.empty())
        break;

      if (!targetGame || !targetGame->GetIsLobbyStrict())
        break;

      if (targetGame->m_DisplayMode == GAME_PRIVATE) {
        ErrorReply("This game is private.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot advertise.");
        break;
      }

      string renameTarget;
      vector<string> Args = SplitArgs(target, 1u, 2u);
      if (Args.size() >= 2) {
        renameTarget = TrimString(Args[1]);
        if (renameTarget.length() > m_Aura->m_MaxGameNameSize) {
          ErrorReply("Unable to rename to [" + renameTarget + "]. The game name is too long (the maximum is " + to_string(m_Aura->m_MaxGameNameSize) + " characters)");
          break;
        }
      } else if (targetGame->GetHasPvPGNPlayers()) {
        // PvGPN servers keep references to all games to which any PvPGN users have joined.
        // Therefore, the bot leaving the game isn't enough for it to be considered unhosted.
        // QueueGameUncreate doesn't seem to work, either.
        ErrorReply("Usage: " + cmdToken + "announce <REALM>, <GAME NAME>");
        break;
      }

      bool toAllRealms = Args[0] == "*";
      shared_ptr<CRealm> targetRealm = nullptr;
      if (toAllRealms) {
        if (0 != (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
          ErrorReply("Announcing on all realms requires sudo permissions."); // But not really
          break;
        }
      } else {
        targetRealm = GetTargetRealmOrCurrent(Args[0]);
        if (!targetRealm) {
          ErrorReply("Usage: " + cmdToken + "announce <REALM>, <GAME NAME>");
          break;
        }
        if (!targetGame->GetIsSupportedGameVersion(targetRealm->GetGameVersion())) {
          ErrorReply("Crossplay is not enabled. [" + targetRealm->GetCanonicalDisplayName() + "] is running v" + ToVersionString(targetRealm->GetGameVersion()));
          break;
        }
        if (targetGame->GetIsExpansion() != targetRealm->GetGameIsExpansion()) {
          if (targetGame->GetIsExpansion()) {
            ErrorReply("Cannot announce TFT game in a Reign of Chaos realm.");
          } else {
            ErrorReply("Cannot announce ROC game in a Frozen Throne realm.");
          }
          break;
        }
      }

      targetGame->m_DisplayMode = GAME_PUBLIC;
      if (Args.size() >= 2) {
        targetGame->m_GameName = renameTarget;
      }
      targetGame->m_HostCounter  = m_Aura->NextHostCounter();
      targetGame->UpdateGameDiscovery();
      string earlyFeedback = "Announcement sent.";
      if (toAllRealms) {
        for (auto& bnet : m_Aura->m_Realms) {
          if (!targetGame->GetIsSupportedGameVersion(bnet->GetGameVersion())) continue;
          if (targetGame->GetIsExpansion() != bnet->GetGameIsExpansion()) continue;
          bnet->ResetGameBroadcastData();
          bnet->ResetGameBroadcastPending();
          bnet->QueueGameChatAnnouncement(targetGame, shared_from_this(), true)->SetEarlyFeedback(earlyFeedback);
        }
      } else {
        targetRealm->ResetGameBroadcastData();
        targetRealm->ResetGameBroadcastPending();
        targetRealm->QueueGameChatAnnouncement(targetGame, shared_from_this(), true)->SetEarlyFeedback(earlyFeedback);
      }
      break;
    }

    //
    // !CLOSEALL
    //

    case HashCode("closeall"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (targetGame->CloseAllSlots()) {
        // Also sent if there was nobody in the game, and so all slots except one were closed.
        SendReply("Closed all slots.");
      } else {
        ErrorReply("There are no open slots.");
      }
      
      break;
    }

    //
    // !COMP (computer slot)
    //

    case HashCode("bot"):
    case HashCode("comp"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (target.empty()) {
        // ignore layout, don't override computers
        if (!targetGame->ComputerNSlots(SLOTCOMP_HARD, targetGame->GetNumComputers() + 1, true, false)) {
          ErrorReply("No slots available.");
        } else {
          SendReply("Insane computer added.");
        }
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "comp <SLOT> , <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }

      optional<uint8_t> maybeSID = RunParseNonPlayerSlot(Args[0]);
      if (!maybeSID.has_value()) {
        ErrorReply("Usage: " + cmdToken + "comp <SLOT> , <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }
      uint8_t skill = SLOTCOMP_HARD;
      if (Args.size() >= 2) {
        skill = ParseComputerSkill(Args[1]);
      }
      if (!targetGame->ComputerSlot(maybeSID.value(), skill, false)) {
        ErrorReply("Cannot add computer on that slot.");
        break;
      }
      targetGame->ResetLayoutIfNotMatching();
      SendReply("Computer slot added.");
      break;
    }

    //
    // !DELETECOMP (remove computer slots)
    //

    case HashCode("deletecomp"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (!targetGame->ComputerNSlots(SLOTCOMP_HARD, 0)) {
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12");
        break;
      }

      vector<string> Args = SplitArgs(target, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12" + HelpMissingComma(target));
        break;
      }

      auto searchResult = RunParseController(target);
      if (!searchResult.GetSuccess()) {
        ErrorReply("Usage: " + cmdToken + "color <PLAYER> , <COLOR> - Color goes from 1 to 12");
        break;
      }
      uint8_t SID = searchResult.SID;
      GameUser::CGameUser* targetPlayer = searchResult.user;

      uint8_t color = ParseColor(Args[1]);
      if (color >= targetGame->GetMap()->GetVersionMaxSlots()) {
        color = ParseSID(Args[1]);

        if (color >= targetGame->GetMap()->GetVersionMaxSlots()) {
          ErrorReply("Color identifier \"" + Args[1] + "\" is not valid.");
          break;
        }
      }

      if (!targetGame->SetSlotColor(SID, color, true)) {
        ErrorReply("Cannot recolor slot #" + to_string(SID + 1) + " to " + GetColorName(color) + ".");
        break;
      }

      if (targetPlayer) {
        SendReply("[" + targetPlayer->GetDisplayName() + "]'s color changed to " + GetColorName(color) + ".");
      } else {
        SendReply("Color changed to " + GetColorName(color) + ".");
      }
      break;
    }

    //
    // !HANDICAP (handicap change)
    //

    case HashCode("handicap"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }
      
      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }

      vector<string> Args = SplitArgs(target, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100" + HelpMissingComma(target));
        break;
      }

      auto searchResult = RunParseController(target);
      if (!searchResult.GetSuccess()) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }
      uint8_t SID = searchResult.SID;
      GameUser::CGameUser* targetPlayer = searchResult.user;

      vector<uint32_t> handicap = SplitNumericArgs(Args[1], 1u, 1u);
      if (handicap.empty() || handicap[0] % 10 != 0 || !(50 <= handicap[0] && handicap[0] <= 100)) {
        ErrorReply("Usage: " + cmdToken + "handicap <PLAYER> , <HANDICAP> - Handicap is percent: 50/60/70/80/90/100");
        break;
      }

      if (targetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        // The WC3 client is incapable of modifying handicap when Fixed Player Settings is enabled.
        // However, the GUI misleads users into thinking that it can be modified.
        // While it's indeed editable for HCL purposes,
        // we don't intend to forcibly allow its edition outside of HCL context.
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      CGameSlot* slot = targetGame->GetSlot(SID);
      if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + Args[0] + " is empty.");
        break;
      }

      if (slot->GetHandicap() == static_cast<uint8_t>(handicap[0])) {
        ErrorReply("Handicap is already at " + Args[1] + "%");
        break;
      }
      slot->SetHandicap(static_cast<uint8_t>(handicap[0]));
      targetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (targetGame->GetGameLoaded()) {
        ErrorReply("Game already started. Did you mean to check " + cmdToken + "races instead?");
        break;
      }

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "race <RACE> - Race is human/orc/undead/elf/random/roll");
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll");
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll" + HelpMissingComma(target));
        break;
      }
      if (Args.size() == 1) {
        Args.push_back(Args[0]);
        Args[0] = GetSender();
      }

      auto searchResult = RunParseController(target);
      if (!searchResult.GetSuccess()) {
        ErrorReply("Usage: " + cmdToken + "race <PLAYER> , <RACE> - Race is human/orc/undead/elf/random/roll");
        break;
      }
      uint8_t SID = searchResult.SID;
      GameUser::CGameUser* targetPlayer = searchResult.user;

      if ((!GetIsGameUser() || GetGameUser() != targetPlayer) && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
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

      if (targetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      if (targetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
        ErrorReply("This game has Random Races enabled.");
        break;
      }

      CGameSlot* slot = targetGame->GetSlot(SID);
      if (!slot || slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED || slot->GetTeam() == targetGame->GetMap()->GetVersionMaxSlots()) {
        ErrorReply("Slot " + ToDecString(SID + 1) + " is not playable.");
        break;
      }
      if (Race == (slot->GetRace() & Race)) {
        ErrorReply("Slot " + ToDecString(SID + 1) + " is already [" + GetRaceName(Race) + "] race.");
        break;
      }
      slot->SetRace(Race | SLOTRACE_SELECTABLE);
      targetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      bool onlyDraft = false;
      if (!GetIsSudo()) {
        if ((onlyDraft = targetGame->GetIsDraftMode())) {
          if (!GetIsGameUser() || !GetGameUser()->GetIsDraftCaptain()) {
            ErrorReply("Draft mode is enabled. Only draft captains may assign teams.");
            break;
          }
        } else if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
          ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
          break;
        }
      }

      if (target.empty()) {
        if (GetIsGameUser()) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, 2u);
      if (Args.empty()) {
        if (GetIsGameUser()) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }

      auto searchResult = RunParseController(target);
      if (!searchResult.GetSuccess()) {
        if (GetIsGameUser()) ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }
      uint8_t SID = searchResult.SID;
      GameUser::CGameUser* targetPlayer = searchResult.user;

      uint8_t targetTeam = 0xFF;
      if (Args.size() >= 2) {
        targetTeam = ParseSID(Args[1]);
      } else {      
        if (!GetIsGameUser()) {
          ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
          break;
        }
        const CGameSlot* slot = targetGame->InspectSlot(targetGame->GetSIDFromUID(GetGameUser()->GetUID()));
        if (!slot) {
          ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
          break;
        }

        // If there are several admins in the game,
        // let them directly pick their team members with e.g. !team Arthas
        targetTeam = slot->GetTeam();
      }
      if (targetTeam > targetGame->GetMap()->GetVersionMaxSlots() + 1) { // accept 13/25 as observer
        ErrorReply("Usage: " + cmdToken + "team <PLAYER>");
        ErrorReply("Usage: " + cmdToken + "team <PLAYER> , <TEAM>");
        break;
      }
      if (onlyDraft && !GetGameUser()->GetIsDraftCaptainOf(targetTeam)) {
        ErrorReply("You are not the captain of team " + ToDecString(targetTeam + 1));
        break;
      }

      if (targetGame->GetSlot(SID)->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + Args[0] + " is empty.");
        break;
      }

      if (targetTeam == targetGame->GetMap()->GetVersionMaxSlots()) {
        if (targetGame->GetMap()->GetMapObservers() != MAPOBS_ALLOWED && targetGame->GetMap()->GetMapObservers() != MAPOBS_REFEREES) {
          ErrorReply("This game does not have observers enabled.");
          break;
        }
        if (targetGame->m_Slots[SID].GetIsComputer()) {
          ErrorReply("Computer slots cannot be moved to observers team.");
          break;
        }
      } else if (targetTeam > targetGame->GetMap()->GetMapNumTeams()) {
        ErrorReply("This map does not allow Team #" + ToDecString(targetTeam + 1) + ".");
        break;
      }

      if (!targetGame->SetSlotTeam(SID, targetTeam, true)) {
        if (targetPlayer) {
          ErrorReply("Cannot transfer [" + targetPlayer->GetName() + "] to team " + ToDecString(targetTeam + 1) + ".");
        } else {
          ErrorReply("Cannot transfer to team " + ToDecString(targetTeam + 1) + ".");
        }
      } else {
        targetGame->ResetLayoutIfNotMatching();
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "observer <PLAYER>");
        break;
      }

      auto searchResult = RunParseController(target);
      if (!searchResult.GetSuccess()) {
        ErrorReply("Usage: " + cmdToken + "observer <PLAYER>");
        break;
      }
      uint8_t SID = searchResult.SID;
      GameUser::CGameUser* targetPlayer = searchResult.user;

      if (!(targetGame->GetMap()->GetMapObservers() == MAPOBS_ALLOWED || targetGame->GetMap()->GetMapObservers() == MAPOBS_REFEREES)) {
        ErrorReply("This lobby does not allow observers.");
        break;
      }
      if (targetGame->m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + target + " is empty.");
        break;
      }
      if (targetGame->m_Slots[SID].GetIsComputer()) {
        ErrorReply("Computer slots cannot be moved to observers team.");
        break;
      }

      if (!targetGame->SetSlotTeam(SID, targetGame->GetMap()->GetVersionMaxSlots(), true)) {
        if (targetPlayer) {
          ErrorReply("Cannot turn [" + targetPlayer->GetName() + "] into an observer.");
        } else {
          ErrorReply("Cannot turn slot #" + to_string(SID + 1) + " into an observer slot.");
        }
      } else {
        targetGame->ResetLayoutIfNotMatching();
        if (targetPlayer) {
          SendReply("[" + targetPlayer->GetName() + "] is now an observer.");
        } else {
          SendReply("Moved to observers team.");
        }
      }
      break;
    }

    //
    // !TIMEHANDICAP (prevent players actions the first N seconds)
    //

    case HashCode("th"):
    case HashCode("timehandicap"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (targetGame->GetCountDownStarted() && !GetIsSudo()) {
        ErrorReply("Cannot set a time handicap now.");
        break;
      }

      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "timehandicap <SECONDS>");
        ErrorReply("Usage: " + cmdToken + "timehandicap <PLAYER>, <SECONDS>");
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "timehandicap <SECONDS>");
        ErrorReply("Usage: " + cmdToken + "timehandicap <PLAYER>, <SECONDS>");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(Args.size() >= 2 ? Args[0] : string());
      if (!targetPlayer) {
        break;
      }
      if (targetPlayer->GetIsObserver()) {
        ErrorReply("User [" + targetPlayer->GetDisplayName() + "] is an observer.");
        break;
      }

      if (targetPlayer != GetGameUser() && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot set a time handicap.");
        break;
      }

      optional<int64_t> handicapTicks;
      try {
        int64_t parsedSeconds = stol(Args.back());
        if (parsedSeconds < 0 || 300 < parsedSeconds) {
          ErrorReply("Time handicap cannot exceed 300 seconds (5 minutes)");
          break;
        }
        handicapTicks = 1000 * parsedSeconds;
      } catch (...) {
      }

      if (!handicapTicks.has_value()) {
        ErrorReply("Usage: " + cmdToken + "timehandicap <SECONDS>");
        ErrorReply("Usage: " + cmdToken + "timehandicap <PLAYER>, <SECONDS>");
        break;
      }

      if (targetPlayer->GetHandicapTicks() == handicapTicks.value()) {
        ErrorReply("Time handicap already set to " + ToDurationString(handicapTicks.value() / 1000));
        break;
      }

      targetPlayer->SetHandicapTicks(targetGame->GetEffectiveTicks() + handicapTicks.value());
      if (targetPlayer->GetHasAPMQuota()) {
        targetPlayer->GetAPMQuota().PauseRefillUntil(targetPlayer->GetHandicapTicks());
      }
      SendAll("Player [" + targetPlayer->GetDisplayName() + "] will start playing after " + ToDurationString(handicapTicks.value() / 1000));
      break;
    }

    //
    // !APMHANDICAP (limits a player below the specified APM)
    //

    case HashCode("ah"):
    case HashCode("apmhandicap"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (targetGame->GetCountDownStarted() && !GetIsSudo()) {
        ErrorReply("Cannot set an APM handicap now.");
        break;
      }

      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "apmhandicap <APM>");
        ErrorReply("Usage: " + cmdToken + "apmhandicap <PLAYER>, <APM>");
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "apmhandicap <APM>");
        ErrorReply("Usage: " + cmdToken + "apmhandicap <PLAYER>, <APM>");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(Args.size() >= 2 ? Args[0] : string());
      if (!targetPlayer) {
        break;
      }
      if (targetPlayer->GetIsObserver()) {
        ErrorReply("User [" + targetPlayer->GetDisplayName() + "] is an observer.");
        break;
      }

      if (targetPlayer != GetGameUser() && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot set an APM handicap.");
        break;
      }

      optional<size_t> maxAPM;
      try {
        int64_t parsedActions = stol(Args.back());
        if (0 <= parsedActions && parsedActions <= 0xFFFF) {
          maxAPM = static_cast<size_t>(parsedActions);
        }
      } catch (...) {
      }

      if (!maxAPM.has_value()) {
        ErrorReply("Usage: " + cmdToken + "apmhandicap <APM>");
        ErrorReply("Usage: " + cmdToken + "apmhandicap <PLAYER>, <APM>");
        break;
      }

      if (maxAPM.value() < APM_RATE_LIMITER_MIN && !GetIsSudo()) {
        maxAPM = APM_RATE_LIMITER_MIN;
      } else if (APM_RATE_LIMITER_MAX < maxAPM.value() && !GetIsSudo()) {
        maxAPM = APM_RATE_LIMITER_MAX;
      }

      targetPlayer->RestrictAPM(maxAPM.value(), targetGame->m_Config.m_MaxBurstAPM.value_or(APM_RATE_LIMITER_BURST_ACTIONS));
      targetPlayer->GetAPMQuota().PauseRefillUntil(targetPlayer->GetHandicapTicks());

      string limitText = to_string(maxAPM.value()) + " actions per minute (APM)";
      SendAll("Player [" + targetPlayer->GetDisplayName() + "] will be restricted to " + limitText);
      break;
    }

    //
    // !FILL (fill all open slots with computers)
    //

    case HashCode("compall"):
    case HashCode("fill"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      uint8_t targetSkill = target.empty() ? SLOTCOMP_HARD : ParseComputerSkill(target);
      if (targetSkill == SLOTCOMP_INVALID) {
        ErrorReply("Usage: " + cmdToken + "fill <SKILL> - Skill is any of: easy, normal, insane");
        break;
      }
      if (!targetGame->ComputerAllSlots(targetSkill)) {
        if (targetGame->GetCustomLayout() == CUSTOM_LAYOUT_HUMANS_VS_AI) {
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
        ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, targetGame->GetMap()->GetMapNumTeams());
      if (Args.empty()) {
        ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
        ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
        break;
      }

      if (Args.size() == 1) {
        optional<bool> targetToggle = ParseBoolean(target);
        if (!targetToggle.has_value()) {
          ErrorReply("Usage: " + cmdToken + "draft <ON|OFF>");
          ErrorReply("Usage: " + cmdToken + "draft <CAPTAIN1> , <CAPTAIN2>");
          break;
        }
        if (targetToggle.value()) {
          if (targetGame->GetIsDraftMode()) {
            ErrorReply("Draft mode is already enabled.");
            break;
          }
          targetGame->SetDraftMode(true);

          // Only has effect if observers are allowed.
          targetGame->ResetTeams(false);

          SendReply("Draft mode enabled. Only draft captains may assign teams.");
        } else {
          if (!targetGame->GetIsDraftMode()) {
            ErrorReply("Draft mode is already disabled.");
            break;
          }
          targetGame->ResetDraft();
          targetGame->SetDraftMode(false);
          SendReply("Draft mode disabled. Everyone may choose their own team.");
        }
        break;
      }

      targetGame->ResetDraft();
      vector<string> failPlayers;

      uint8_t team = static_cast<uint8_t>(Args.size());
      while (team--) {
        GameUser::CGameUser* user = GetTargetUser(Args[team]);
        if (user) {
          const uint8_t SID = targetGame->GetSIDFromUID(user->GetUID());
          if (targetGame->SetSlotTeam(SID, team, true) ||
            targetGame->InspectSlot(SID)->GetTeam() == team) {
            user->SetDraftCaptain(team + 1);
          } else {
            failPlayers.push_back(user->GetName());
          }
        } else {
          failPlayers.push_back(Args[team]);
        }
      }

      // Only has effect if observers are allowed.
      targetGame->ResetTeams(false);

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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetToggle = ParseBoolean(target);
      if (!targetToggle.has_value()) {
        ErrorReply("Usage: " + cmdToken + "ffa <ON|OFF>");
        break;
      }
      if (!targetToggle.value()) {
        targetGame->ResetLayout(true);
        SendReply("FFA mode disabled.");
        break;
      }
      if (targetGame->GetMap()->GetMapNumControllers() <= 2) {
        ErrorReply("This map does not support FFA mode.");
        break;
      }
      if (!targetGame->SetLayoutFFA()) {
        targetGame->ResetLayout(true);
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetToggle = ParseBoolean(target);
      if (targetToggle.has_value() && targetToggle.value() == false) {
        // Branching here means that you can't actually set a player named "Disable" against everyone else.
        targetGame->ResetLayout(true);
        SendReply("One-VS-All mode disabled.");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(target);
      if (!targetPlayer) {
        break;
      }

      const uint8_t othersCount = targetGame->GetNumPotentialControllers() - 1;
      if (othersCount < 2) {
        ErrorReply("There are too few players in the game.");
        break;
      }

      if (!targetGame->SetLayoutOneVsAll(targetPlayer)) {
        targetGame->ResetLayout(true);
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      optional<bool> targetToggle = ParseBoolean(target);
      if (!targetToggle.has_value()) {
        vector<uint32_t> Args = SplitNumericArgs(target, 1u, 1u);
        // Special-case max slots so that if someone careless enough types !terminator 12, it just works.
        if (Args.empty() || Args[0] <= 0 || (Args[0] >= targetGame->GetMap()->GetMapNumControllers() && Args[0] != targetGame->GetMap()->GetVersionMaxSlots())) {
          ErrorReply("Usage: " + cmdToken + "terminator <ON|OFF>");
          ErrorReply("Usage: " + cmdToken + "terminator <NUMBER>");
          break;
        }
        targetGame->ResetLayout(false);
        uint8_t computerCount = static_cast<uint8_t>(Args[0]);
        if (computerCount == targetGame->GetMap()->GetVersionMaxSlots()) --computerCount; // Fix 1v12 into 1v11
        // ignore layout, don't override computers
        if (!targetGame->ComputerNSlots(SLOTCOMP_HARD, computerCount, true, false)) {
          ErrorReply("Not enough open slots for " + ToDecString(computerCount) + " computers.");
          break;
        }
      }
      if (!targetToggle.value_or(true)) {
        targetGame->ResetLayout(true);
        SendReply("Humans-VS-AI mode disabled.");
        break;
      }
      const uint8_t computersCount = targetGame->GetNumComputers();
      if (computersCount == 0) {
        ErrorReply("No computer slots found. Use [" + cmdToken + "terminator NUMBER] to play against one or more insane computers.");
        break;
      }
      const uint8_t humansCount = static_cast<uint8_t>(targetGame->GetNumJoinedUsersOrFake());
      pair<uint8_t, uint8_t> matchedTeams;
      if (!targetGame->FindHumanVsAITeams(humansCount, computersCount, matchedTeams)) {
        ErrorReply("Not enough open slots to host " + ToDecString(humansCount) + " humans VS " + ToDecString(computersCount) + " computers game.");
        break;
      }

      if (!targetGame->SetLayoutHumansVsAI(matchedTeams.first, matchedTeams.second)) {
        targetGame->ResetLayout(true);
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }
      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }
      optional<bool> targetToggle = ParseBoolean(target);
      if (!targetToggle.has_value()) {
        ErrorReply("Usage: " + cmdToken + "teams <ON|OFF>");
        break;
      }
      if (!targetToggle.value_or(true)) {
        targetGame->ResetLayout(true);
        // This doesn't have any effect, since
        // both CUSTOM_LAYOUT_COMPACT nor CUSTOM_LAYOUT_ISOPLAYERS are
        // missing from CUSTOM_LAYOUT_LOCKTEAMS mask.
        SendReply("No longer enforcing teams.");
        break;
      }
      if (targetGame->GetMap()->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
        if (targetGame->GetMap()->GetMapNumTeams() == 2) {
          // This is common enough to warrant a special case.
          if (targetGame->SetLayoutTwoTeams()) {
            SendReply("Teams automatically arranged.");
            break;
          }
        }
      } else {
        if (targetGame->SetLayoutCompact()) {
          SendReply("Teams automatically arranged.");
          break;
        }
      }
      targetGame->ResetLayout(true);
      ErrorReply("Failed to automatically assign teams.");
      break;
    }

    //
    // !FAKEPLAYER
    //

    case HashCode("fakeplayer"):
    case HashCode("fp"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      string inputLower = target;
      transform(begin(inputLower), end(inputLower), begin(inputLower), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (inputLower == "fill") {
        targetGame->DeleteVirtualHost();
        targetGame->FakeAllSlots();
        break;
      }

      const optional<bool> targetToggle = ParseBoolean(inputLower);

      if (!targetToggle.has_value() && !target.empty()) {
        ErrorReply("Usage: " + cmdToken + "fp");
        ErrorReply("Usage: " + cmdToken + "fp <ON|OFF>");
        break;
      }

      if (!targetToggle.has_value() && targetGame->GetIsRestored()) {
        ErrorReply("Usage: " + cmdToken + "fp <ON|OFF>");
        break;
      }
      if (targetToggle.has_value() && !targetGame->GetIsRestored()) {
        ErrorReply("Usage: " + cmdToken + "fp");
        break;
      }

      targetGame->SetAutoVirtualPlayers(targetToggle.value_or(false));
      if (!targetToggle.has_value() && !targetGame->CreateFakeUser(nullopt)) {
        ErrorReply("Cannot add another virtual user");
      } else if (targetToggle.has_value()) {
        if (targetGame->GetIsAutoVirtualPlayers()) {
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
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        if (!GetIsSudo() || !targetGame->GetGameLoaded()) {
          ErrorReply("Cannot edit this game's slots.");
          break;
        }
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (targetGame->m_FakeUsers.empty()) {
        ErrorReply("No virtual players found.");
        break;
      }

      if (targetGame->GetIsLobbyStrict()) {
        targetGame->DeleteFakeUsersLobby();
      } else {
        targetGame->DeleteFakeUsersLoaded();
      }
      break;
    }

    //
    // !FILLFAKE
    //

    case HashCode("fillfake"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      bool Success = false;
      while (targetGame->CreateFakeUser(nullopt)) {
        Success = true;
      }

      if (!Success) {
        ErrorReply("Cannot add another virtual user");
      }
      break;
    }

    //
    // !PAUSE
    //

    case HashCode("pause"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetGameLoaded())
        break;

      if ((!GetIsGameUser() || targetGame->GetNumJoinedPlayers() >= 2) && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot pause the game.");
        break;
      }

      if (targetGame->GetIsPaused()) {
        ErrorReply("Game already paused.");
        break;
      }

      if (targetGame->m_FakeUsers.empty()) {
        ErrorReply("This game does not support the " + cmdToken + "pause command. Use the game menu instead.");
        break;
      }

      if (targetGame->GetIsLagging()) {
        ErrorReply("This command cannot be used while the game is lagging.");
        break;
      }

      if (!targetGame->Pause(GetGameUser(), false)) {
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
    case HashCode("save"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetGameLoaded())
        break;

      if ((!GetIsGameUser() || targetGame->GetNumJoinedPlayers() >= 2) && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot save the game.");
        break;
      }

      if (targetGame->m_FakeUsers.empty()) {
        ErrorReply("This game does not support the " + cmdToken + "save command. Use the game menu instead.");
        break;
      }

      if (target.empty()) {
        if (targetGame->GetIsLagging()) {
          ErrorReply("This command cannot be used while the game is lagging.");
          break;
        }

        if (!targetGame->Save(GetGameUser(), false)) {
          ErrorReply("Can only save once per player per game session.");
          break;
        }
        SendReply("Saving game...");
        break;
      }

      optional<bool> targetToggle = ParseBoolean(target);
      if (!targetToggle.has_value()) {
        ErrorReply("Usage: " + cmdToken + "save");
        ErrorReply("Usage: " + cmdToken + "save <ON|OFF>");
        break;
      }

      if (targetToggle.value()) {
        targetGame->SetSaveOnLeave(SAVE_ON_LEAVE_ALWAYS);
        SendReply("Autosave on disconnections enabled.");
      } else {
        targetGame->SetSaveOnLeave(SAVE_ON_LEAVE_NEVER);
        SendReply("Autosave on disconnections disabled.");
      }
      break;
    }

    //
    // !RESUME
    //

    case HashCode("resume"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetGameLoaded())
        break;

      if ((!GetIsGameUser() || targetGame->GetNumJoinedPlayers() >= 2) && !CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot resume the game.");
        break;
      }

      if (!targetGame->GetIsPaused()) {
        ErrorReply("Game is not paused.");
        break;
      }

      if (targetGame->GetIsLagging()) {
        ErrorReply("This command cannot be used while the game is lagging.");
        break;
      }

      if (targetGame->m_FakeUsers.empty() || !targetGame->Resume(GetGameUser(), false)) {
        ErrorReply("This game does not support the " + cmdToken + "resume command. Use the game menu instead.");
        break;
      }

      SendReply("Resuming game...");
      break;
    }

    //
    // !SP
    //

    case HashCode("sp"):
    case HashCode("shuffle"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      targetGame->ShuffleSlots();
      SendAll("Players shuffled");
      break;
    }

    //
    // !LOCK
    //

    case HashCode("lock"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot lock the game.");
        break;
      }

      if (targetGame->GetLocked()) {
        ErrorReply("Game already locked.");
        break;
      }

      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("Game can only be locked from the lobby in incognito mode games.");
        break;
      }

      if (target.empty()) {
        if (targetGame->GetCountDownStarted()) {
          SendReply("Game is now locked. Only the game owner may use commands.");
        } else {
          string Warning = targetGame->m_OwnerRealm.empty() ? " (Owner joined over LAN - will get removed if they leave.)" : "";
          SendReply("Game is now locked. Only the game owner may use commands, and edit players' races, teams, etc." + Warning);
        }
        targetGame->m_Locked = true;
      } else {
        GameUser::CGameUser* targetPlayer = RunTargetUser(target);
        if (!targetPlayer) {
          break;
        }
        if (targetPlayer->GetIsActionLocked()) {
          ErrorReply("Player [" + targetPlayer->GetDisplayName() + "] was already locked.");
          break;
        }
        targetPlayer->SetActionLocked(true);
        if (targetGame->GetCountDownStarted()) {
          SendReply("Player [" + targetPlayer->GetDisplayName() + "]  locked. They cannot use commands.");
        } else {
          SendReply("Player [" + targetPlayer->GetDisplayName() + "]  locked. They cannot use commands, nor choose their race, team, etc.");
        }
      }
      break;
    }

    //
    // !OPENALL
    //

    case HashCode("openall"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame)
        break;

      if (!targetGame->GetIsLobbyStrict() || targetGame->GetIsRestored() || targetGame->GetCountDownStarted()) {
        ErrorReply("Cannot edit this game's slots.");
        break;
      }

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      targetGame->OpenAllSlots();
      break;
    }

    //
    // !UNLOCK
    //

    case HashCode("unlock"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (USER_PERMISSIONS_GAME_OWNER | USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner nor a root admin, and therefore cannot unlock the game.");
        break;
      }

      if (!targetGame->GetLocked()) {
        ErrorReply("Game is not locked.");
        break;
      }

      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("Game can only be unlocked from the lobby in incognito mode games.");
        break;
      }

      if (target.empty()) {
        if (targetGame->GetCountDownStarted()) {
          SendReply("Game unlocked. Everyone may now use commands.");
        } else {
          SendReply("Game unlocked. Everyone may now use commands, and choose their races, teams, etc.");
        }

        targetGame->m_Locked = false;
      } else {
        GameUser::CGameUser* targetPlayer = RunTargetUser(target);
        if (!targetPlayer) {
          break;
        }
        if (!targetPlayer->GetIsActionLocked()) {
          ErrorReply("Player [" + targetPlayer->GetDisplayName() + "] was not locked.");
          break;
        }
        targetPlayer->SetActionLocked(false);
        if (targetGame->GetCountDownStarted()) {
          SendReply("Player [" + targetPlayer->GetDisplayName() + "]  unlocked. They may now use commands.");
        } else {
          SendReply("Player [" + targetPlayer->GetDisplayName() + "]  unlocked. They may now use commands, and choose their race, team, etc.");
        }
      }
      break;
    }

    //
    // !UNMUTE
    //

    case HashCode("unmute"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot unmute people.");
        break;
      }

      if (targetGame->GetIsHiddenPlayerNames()) {
        ErrorReply("This command is disabled in incognito mode games. Use " + cmdToken + "unmuteall from the game lobby next time.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "unmute <PLAYERNAME>");
        break;
      }
      GameUser::CGameUser* targetPlayer = RunTargetUser(target);
      if (!targetPlayer) {
        break;
      }
      if (!targetPlayer->GetIsMuted()) {
        // Let this be transparent info.
        // And, more crucially, don't show "cannot unmute yourself" to people who aren't muted.
        ErrorReply("Player [" + targetPlayer->GetName() + "] is not muted.");
        break;
      }
      if (targetPlayer == GetGameUser() && !GetIsSudo()) {
        ErrorReply("Cannot unmute yourself.");
        break;
      }

      if (!targetPlayer->UnMute()) {
        ErrorReply("Failed to unmute player [" + targetPlayer->GetName() + "]");
        break;
      }
      SendAll("[" + targetPlayer->GetName() + "] was unmuted by [" + GetSender() + "]");
      break;
    }

    //
    // !UNMUTEALL
    //

    case HashCode("unmuteall"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot enable \"All\" chat channel.");
        break;
      }

      if (targetGame->m_MuteAll) {
        ErrorReply("Global chat is not muted.");
        break;
      }

      if (targetGame->GetIsHiddenPlayerNames() && targetGame->GetGameLoaded()) {
        ErrorReply("Chat can only be toggled from the game lobby for incognito mode games.");
        break;
      }

      SendAll("Global chat unmuted");
      targetGame->m_MuteAll = false;
      break;
    }

    //
    // !VOTECANCEL
    //

    case HashCode("votecancel"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || targetGame->GetIsMirror())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot cancel a votekick.");
        break;
      }

      if (targetGame->m_KickVotePlayer.empty()) {
        ErrorReply("There is no active votekick.");
        break;
      }

      SendReply("A votekick against [" + targetGame->m_KickVotePlayer + "] has been cancelled by [" + GetSender() + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_INCIDENT);
      targetGame->m_KickVotePlayer.clear();
      targetGame->m_StartedKickVoteTime = 0;
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

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      string::size_type MessageStart = target.find(',');

      if (MessageStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
      }

      string inputName = TrimString(target.substr(0, MessageStart));
      string subMessage = TrimString(target.substr(MessageStart + 1));

      string nameFragment, locationFragment;
      void* location = nullptr;
      uint8_t targetType = GetParseTargetServiceUser(inputName, nameFragment, locationFragment, location);
      if (targetType == SERVICE_TYPE_REALM) {
        CRealm* matchingRealm = reinterpret_cast<CRealm*>(location);
        if (inputName == matchingRealm->GetLoginName()) {
          ErrorReply("Cannot PM myself.");
          break;
        }

        // Name of sender and receiver should be included in the message,
        // so that they can be checked in successful whisper acks from the server (BNETProtocol::IncomingChatEvent::WHISPERSENT)
        // Note that the server doesn't provide any way to recognize whisper targets if the whisper fails.
        if (m_ServerName.empty()) {
          m_ActionMessage = inputName + ", " + GetSender() + " tells you: <<" + subMessage + ">>";
        } else {
          m_ActionMessage = inputName + ", " + GetSender() + " at " + m_ServerName + " tells you: <<" + subMessage + ">>";
        }
        matchingRealm->QueueWhisper(m_ActionMessage, inputName, shared_from_this(), true);
      } else if (targetType == SERVICE_TYPE_GAME) {
        CGame* matchingGame = reinterpret_cast<CGame*>(location);
        if (matchingGame->GetGameLoaded() && !GetIsSudo()) {
          ErrorReply("Cannot send messages to a game that has already started.");
          break;
        }
        if (matchingGame->GetIsHiddenPlayerNames() && !GetIsSudo()) {
          ErrorReply("Cannot send messages to an incognito mode game.");
          break;
        }
        auto searchResult = matchingGame->GetUserFromNamePartial(inputName);
        if (!searchResult.GetSuccess()) {
          ErrorReply("Player [" + inputName + "] not found in <<" + matchingGame->GetGameName() + ">>.");
          break;
        }
        if (m_ServerName.empty()) {
          matchingGame->SendChat(searchResult.user, inputName + ", " + GetSender() + " tells you: <<" + subMessage + ">>");
        } else {
          matchingGame->SendChat(searchResult.user, inputName + ", " + GetSender() + " at " + m_ServerName + " tells you: <<" + subMessage + ">>");
        }
        SendReply("Message sent to " + searchResult.user->GetName() + ".");
      } else {
        ErrorReply("Usage: " + cmdToken + "w <PLAYERNAME>@<LOCATION> , <MESSAGE>");
        break;
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

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "whois <PLAYERNAME>@<REALM>");
        break;
      }

      GameUser::CGameUser* targetPlayer = RunTargetUserOrSelf(target);
      if (!targetPlayer) {
        break;
      }

      shared_ptr<CRealm> targetRealm = targetPlayer->GetRealm(false);
      if (!targetRealm) {
        SendReply("Player [" + targetPlayer->GetName() + "] joined from LAN/VPN.");
        break;
      }

      const string Message = "/whois " + targetPlayer->GetName();
      targetRealm->QueueCommand(Message);
      SendReply("Verification requested for [" + targetPlayer->GetName() + "@" + targetRealm->GetServer() + "]");
      break;
    }

    //
    // !WHOARE
    //

    case HashCode("whoare"): {
      UseImplicitHostedGame();

      if (!CheckPermissions(m_Config->m_WhoarePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot ask for /whoare.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "whoare <PLAYERNAME>@<REALM>");
        break;
      }

      auto realmUserResult = GetParseTargetRealmUser(target, true);
      if (!realmUserResult.GetSuccess()) {
        if (!realmUserResult.hostName.empty() && realmUserResult.hostName != "*") {
          ErrorReply(realmUserResult.hostName + " is not a valid PvPGN realm.");
          break;
        }
      }
      string targetName = realmUserResult.userName;
      string targetHostName = realmUserResult.hostName;
      shared_ptr<CRealm> targetRealm = realmUserResult.GetRealm();

      if (targetHostName.empty()) {
        targetName = target;
      }

      bool queryAllRealms = targetHostName.empty() || targetHostName == "*";
      const string Message = "/whois " + targetName;

      bool success = false;
      for (auto& realm : m_Aura->m_Realms) {
        if (realm->GetIsMirror())
          continue;
        if (queryAllRealms || realm->GetServer() == targetRealm->GetServer()) {
          realm->QueueCommand(Message);
          success = true;
        }
      }

      if (success) {
        SendReply("Query sent.");
      } else {
        ErrorReply("No such realm found.");
      }

      break;
    }

    //
    // !VIRTUALHOST
    //

    case HashCode("virtualhost"): {
      UseImplicitHostedGame();
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || targetGame->GetCountDownStarted())
        break;

      if (!CheckPermissions(m_Config->m_HostingBasePermissions, COMMAND_PERMISSIONS_OWNER)) {
        ErrorReply("You are not the game owner, and therefore cannot send whispers.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "virtualhost <PLAYERNAME>");
        break;
      }

      string targetName = TrimString(target);
      if (targetName.empty() || targetName.length() > 15) {
        ErrorReply("Usage: " + cmdToken + "virtualhost <PLAYERNAME>");
        break;
      }
      if (targetGame->m_Config.m_LobbyVirtualHostName == targetName) {
        ErrorReply("Virtual host [" + targetName + "] is already in the game.");
        break;
      }
      if (targetGame->GetUserFromName(targetName, false)) {
        ErrorReply("Someone is already using the name [" + targetName + "].");
        break;
      }

      targetGame->m_Config.m_LobbyVirtualHostName = targetName;
      if (targetGame->DeleteVirtualHost()) {
        targetGame->CreateVirtualHost();
      }
      break;
    }

  /*****************
   * SUDOER COMMANDS *
   ******************/

    // !GETCLAN
    case HashCode("getclan"): {
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!sourceRealm)
        break;

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      sourceRealm->SendGetClanList();
      SendReply("Fetching clan member list from " + m_ServerName + "...");
      break;
    }

    // !GETFRIENDS
    case HashCode("getfriends"): {
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!sourceRealm)
        break;

      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      sourceRealm->SendGetFriendsList();
      SendReply("Fetching friends list from " + m_ServerName + "...");
      break;
    }

    case HashCode("egame"):
    case HashCode("eg"): {
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();      
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      size_t CommandStart = target.find(',');
      if (CommandStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "eg <GAMEID> , <COMMAND> - See game ids with !listgames");
        break;
      }
      string GameId = TrimString(target.substr(0, CommandStart));
      string ExecString = TrimString(target.substr(CommandStart + 1));
      if (GameId.empty() || ExecString.empty()) {
        ErrorReply("Usage: " + cmdToken + "eg <GAMEID> , <COMMAND> - See game ids with !listgames");
        break;
      }
      shared_ptr<CGame> targetGame = GetTargetGame(GameId);
      if (!targetGame) {
        ErrorReply("Game [" + GameId + "] not found. - See game ids with !listgames");
        break;
      }
      size_t TargetStart = ExecString.find(' ');
      const string SubCmd = TargetStart == string::npos ? ExecString : ExecString.substr(0, TargetStart);
      const string SubTarget = TargetStart == string::npos ? string() : ExecString.substr(TargetStart + 1);
      uint8_t serviceSourceType = GetServiceSourceType();
      shared_ptr<CCommandContext> ctx = nullptr;
      try {
        if (serviceSourceType == SERVICE_TYPE_IRC) {
          ctx = make_shared<CCommandContext>(serviceSourceType, m_Aura, m_Config, targetGame, GetChannelName(), GetSender(), m_FromWhisper, m_ServerName, m_IsBroadcast, &std::cout);
#ifndef DISABLE_DPP
        } else if (serviceSourceType == SERVICE_TYPE_DISCORD) {
          ctx = make_shared<CCommandContext>(serviceSourceType, m_Aura, m_Config, targetGame, GetServiceSource().GetDiscordAPI(), &std::cout);
#endif
        } else if (serviceSourceType == SERVICE_TYPE_REALM) {
          if (sourceRealm) {
            ctx = make_shared<CCommandContext>(serviceSourceType, m_Aura, m_Config, targetGame, sourceRealm, GetSender(), m_FromWhisper, m_IsBroadcast, &std::cout);
          }
        } else {
          ctx = make_shared<CCommandContext>(SERVICE_TYPE_LAN, m_Aura, m_Config, targetGame, GetSender(), m_IsBroadcast, &std::cout);
        }
      } catch (...) {
      }
      // Without permission overrides, sudoer would need to type
      // !eg GAMEID, su COMMAND
      // !eg GAMEID, sudo TOKEN COMMAND
      //
      // Instead, we grant all permissions, since !eg requires sudo anyway.
      if (ctx) {
        ctx->SetPermissions(SET_USER_PERMISSIONS_ALL);
        ctx->UpdatePermissions();
        ctx->Run(cmdToken, SubCmd, SubTarget);
      }
      break;
    }

    case HashCode("cachemaps"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      vector<filesystem::path> AllMaps = FilesMatch(m_Aura->m_Config.m_MapPath, FILE_EXTENSIONS_MAP);
      int goodCounter = 0;
      int badCounter = 0;
      
      for (const auto& fileName : AllMaps) {
        string nameString = PathToString(fileName);
        if (nameString.empty()) continue;
        if (cmdHash != HashCode("synccfg") && m_Aura->m_CFGCacheNamesByMapNames.find(fileName) != m_Aura->m_CFGCacheNamesByMapNames.end()) {
          continue;
        }
        if (nameString.find(target) == string::npos) {
          continue;
        }

        CConfig MapCFG;
        MapCFG.SetBool("map.cfg.partial", true);
        MapCFG.SetUint8("map.cfg.schema_number", MAP_CONFIG_SCHEMA_NUMBER);
        MapCFG.Set("map.path", R"(Maps\Download\)" + nameString);
        MapCFG.Set("map.local_path", nameString);
        string mapType;
        if (nameString.find("_evrgrn3") != string::npos) {
          mapType = "evergreen";
        } else if (nameString.find("DotA") != string::npos) {
          mapType = "dota";
        } else if (ToLowerCase(nameString).find("microtrain") != string::npos) {
          mapType = "microtraining";
        }
        if (!mapType.empty()) {
          MapCFG.Set("map.type", mapType);
        }
        if (mapType == "evergreen") {
          MapCFG.Set("map.meta.site", "https://www.hiveworkshop.com/threads/351924/");
          MapCFG.Set("map.meta.short_desc", "This map uses Warcraft 3: Reforged game mechanics.");
        }

        MapCFG.Set("map.downloaded.by", GetSender());

        shared_ptr<CMap> parsedMap = nullptr;
        try {
          parsedMap = make_shared<CMap>(m_Aura, &MapCFG);
        } catch (...) {
          Print("[AURA] warning - map [" + nameString + "] is not valid.");
          badCounter++;
          continue;
        }
        const bool isValid = parsedMap->GetValid();
        if (!isValid) {
          Print("[AURA] warning - map [" + nameString + "] is not valid.");
          badCounter++;
          continue;
        }

        string CFGName = "local-" + nameString + ".ini";
        filesystem::path CFGPath = m_Aura->m_Config.m_MapCachePath / filesystem::path(CFGName);

        vector<uint8_t> OutputBytes = MapCFG.Export();
        FileWrite(CFGPath, OutputBytes.data(), OutputBytes.size());
        m_Aura->m_CFGCacheNamesByMapNames[fileName] = CFGName;
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

      const auto MapCount = FilesMatch(m_Aura->m_Config.m_MapPath, FILE_EXTENSIONS_MAP).size();
      const auto CFGCount = FilesMatch(m_Aura->m_Config.m_MapCFGPath, FILE_EXTENSIONS_CONFIG).size();

      SendReply(to_string(MapCount) + " maps on disk, " + to_string(CFGCount) + " presets on disk, " + to_string(m_Aura->m_CFGCacheNamesByMapNames.size()) + " preloaded.");
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
      if (target.empty())
        return;

      string DeletionType = cmdHash == HashCode("deletecfg") ? "ini" : "map";
      filesystem::path Folder = DeletionType == "ini" ? m_Aura->m_Config.m_MapCFGPath : m_Aura->m_Config.m_MapPath;

      if ((DeletionType == "ini" && !IsValidCFGName(target)) || (DeletionType == "map" &&!IsValidMapName(target))) {
        ErrorReply("Removal failed");
        break;
      }
      filesystem::path FileFragment = target;
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
      SendReply("Deleted [" + target + "]");
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

      string::size_type MessageStart = target.find(',');
      if (MessageStart == string::npos) {
        ErrorReply("Usage: " + cmdToken + "saygame <GAMEID> , <MESSAGE>");
        break;
      }
      bool IsRaw = cmdHash == HashCode("saygameraw");
      string GameId = TrimString(target.substr(0, MessageStart));
      string Message = TrimString(target.substr(MessageStart + 1));
      if (!IsRaw) Message = "[ADMIN] " + Message;
      if (GameId == "*") {
        bool success = false;
        for (const auto& targetGame : m_Aura->m_Lobbies) {
          success = targetGame->SendAllChat(Message) || success;
        }
        for (const auto& targetGame : m_Aura->m_StartedGames) {
          success = targetGame->SendAllChat(Message) || success;
        }
        if (!success) {
          ErrorReply("No games found.");
          break;
        }
        if (InspectGameSource().GetIsEmpty()) {
          SendReply("Sent chat message to all games.");
        }
      } else {
        shared_ptr<CGame> targetGame = GetTargetGame(GameId);
        shared_ptr<CGame> sourceGame = GetSourceGame();
        if (!targetGame) {
          ErrorReply("Game [" + GameId + "] not found.");
          break;
        }
        if (targetGame->GetIsMirror()) {
          ErrorReply("Game [" + GameId + "] is a mirror game.");
          break;
        }
        if (!targetGame->SendAllChat(Message)) {
          ErrorReply("Failed to send chat message to [" + targetGame->GetGameName() + "]");
          break;
        }
        if (targetGame != sourceGame) {
          SendReply("Sent chat message to [" + targetGame->GetGameName() + "]");
        }
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

      m_Aura->ClearAutoRehost();
      for (auto& game : m_Aura->m_Lobbies) {
        if (game->GetIsLobbyStrict()) {
          game->SetChatOnly();
          if (game->GetCountDownStarted()) {
            game->StopCountDown();
          }
        }
      }

      m_Aura->m_Config.m_Enabled = false;
      SendReply("Creation and start of games has been disabled.");
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
      m_Aura->m_Config.m_Enabled = true;
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
      shared_ptr<CRealm> targetRealm = GetTargetRealmOrCurrent(target);
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "disablepub <REALM>");
        break;
      }
      if (targetRealm != sourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to toggle game creation in arbitrary realms.");
        break;
      }
      if (!targetRealm->m_Config.m_CommandCFG->m_Enabled) {
        ErrorReply("All commands are already completely disabled in " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      SendReply("Creation of new games has been temporarily disabled for non-staff at " + targetRealm->GetCanonicalDisplayName() + ". (Active lobbies will not be unhosted.)");
      targetRealm->m_Config.m_CommandCFG->m_HostPermissions = COMMAND_PERMISSIONS_VERIFIED;
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
      shared_ptr<CRealm> targetRealm = GetTargetRealmOrCurrent(target);
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "enablepub <REALM>");
        break;
      }
      if (targetRealm != sourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to toggle game creation in arbitrary realms.");
        break;
      }
      if (!targetRealm->m_Config.m_CommandCFG->m_Enabled) {
        ErrorReply("All commands are completely disabled in " + targetRealm->GetCanonicalDisplayName());
        break;
      }
      SendReply("Creation of new games has been enabled for non-staff at " + targetRealm->GetCanonicalDisplayName());
      targetRealm->m_Config.m_CommandCFG->m_HostPermissions = COMMAND_PERMISSIONS_VERIFIED;
      break;
    }

    //
    // !MAPTRANSFERS
    //

    case HashCode("setdownloads"):
    case HashCode("maptransfers"): {
      if (target.empty()) {
        if (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_NEVER) {
          SendReply("Map transfers are disabled");
        } else if (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC) {
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
        TargetValue = stol(target);
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

      m_Aura->m_Net.m_Config.m_AllowTransfers = static_cast<uint8_t>(TargetValue.value());
      if (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_NEVER) {
        SendAll("Map transfers disabled.");
      } else if (m_Aura->m_Net.m_Config.m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC) {
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
      if (!m_Aura->QueueConfigReload(shared_from_this())) {
        ErrorReply("Reload already scheduled. See the console output.");
      } else {
        SendReply("Reloading configuration files...");
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

      if (m_Aura->GetHasGames()) {
        if (target != "force" && target != "f") {
          ErrorReply("At least one game is in progress, or a lobby is hosted. Use '"+ cmdToken + "exit force' to shutdown anyway");
          break;
        }
      }
      m_Aura->GracefulExit();
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

      if (m_Aura->GetHasGames()) {
        if (target != "force" && target != "f") {
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
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list staff.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "checkstaff <NAME>");
        break;
      }

      if (!sourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      bool IsRootAdmin = sourceRealm->GetIsAdmin(target);
      bool IsAdmin = IsRootAdmin || sourceRealm->GetIsModerator(target);
      if (!IsAdmin && !IsRootAdmin)
        SendReply("User [" + target + "] is not staff on server [" + m_ServerName + "]");
      else if (IsRootAdmin)
        SendReply("User [" + target + "] is a root admin on server [" + m_ServerName + "]");
      else
        SendReply("User [" + target + "] is a moderator on server [" + m_ServerName + "]");

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
      shared_ptr<CRealm> targetRealm = GetTargetRealmOrCurrent(target);
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!targetRealm) {
        ErrorReply("Usage: " + cmdToken + "liststaff <REALM>");
        break;
      }
      if (targetRealm != sourceRealm && !GetIsSudo()) {
        ErrorReply("Not allowed to list staff in arbitrary realms.");
        break;
      }
      vector<string> admins = vector<string>(targetRealm->m_Config.m_Admins.begin(), targetRealm->m_Config.m_Admins.end());
      vector<string> moderators = m_Aura->m_DB->ListModerators(sourceRealm->GetDataBaseID());
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
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        shared_ptr<CGame> sourceGame = GetSourceGame();
        if (sourceGame && !sourceGame->m_OwnerLess) {
          ErrorReply("Only root admins may add staff. Did you mean to acquire control of this game? Use " + cmdToken + "owner");
        } else {
          ErrorReply("Only root admins may add staff.");
        }
        break;
      }
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "staff <NAME>");
        break;
      }
      if (!sourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (sourceRealm->GetIsModerator(target)) {
        ErrorReply("User [" + target + "] is already staff on server [" + m_ServerName + "]");
        break;
      }
      if (!m_Aura->m_DB->ModeratorAdd(sourceRealm->GetDataBaseID(), target)) {
        ErrorReply("Failed to add user [" + target + "] as moderator [" + m_ServerName + "]");
        break;
      }
      SendReply("Added user [" + target + "] to the moderator database on server [" + m_ServerName + "]");
      break;
    }

    //
    // !DELSTAFF
    //

    case HashCode("delstaff"): {
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (0 == (m_Permissions & (USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may change staff.");
        break;
      }
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "delstaff <NAME>");
        break;
      }

      if (!sourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (sourceRealm->GetIsAdmin(target)) {
        ErrorReply("User [" + target + "] is a root admin on server [" + m_ServerName + "]");
        break;
      }
      if (!sourceRealm->GetIsModerator(target)) {
        ErrorReply("User [" + target + "] is not staff on server [" + m_ServerName + "]");
        break;
      }
      if (!m_Aura->m_DB->ModeratorRemove(sourceRealm->GetDataBaseID(), target)) {
        ErrorReply("Error deleting user [" + target + "] from the moderator database on server [" + m_ServerName + "]");
        break;
      }
      SendReply("Deleted user [" + target + "] from the moderator database on server [" + m_ServerName + "]");
      break;
    }

    /*********************
     * ADMIN COMMANDS *
     *********************/

    //
    // !CHECKGAME (check info of a game, either currently hosted, or stored in the database)
    //
  
    case HashCode("checkgame"): {
      if (target.empty()) {
        break;
      }
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions");
        break;
      }

      shared_ptr<CGame> targetGame = GetTargetGame(target);
      if (!targetGame) {
        targetGame = GetTargetGame("game#" + target);
      }
      if (targetGame) {
        string Players = ToNameListSentence(targetGame->GetPlayers());
        string Observers = ToNameListSentence(targetGame->GetObservers());
        if (Players.empty() && Observers.empty()) {
          SendReply("Game#" + to_string(targetGame->GetGameID()) + ". Nobody is in the game.");
          break;
        }
        string PlayersFragment = Players.empty() ? "No players. " : "Players: " + Players + ". ";
        string ObserversFragment = Observers.empty() ? "No observers" : "Observers: " + Observers + ".";
        SendReply("Game#" + to_string(targetGame->GetGameID()) + ". " + PlayersFragment + ObserversFragment);
        break;
      }

      uint64_t gameID = 0;
      try {
        long long value = stoll(target);
        gameID = static_cast<uint64_t>(value);
      } catch (...) {
        ErrorReply("Invalid game identifier.");
        break;
      }
      CDBGameSummary* gameSummary = m_Aura->m_DB->GameCheck(gameID);
      if (!gameSummary) {
        ErrorReply("Game #" + to_string(gameID) + " not found in database.");
        break;
      }
      SendReply("Game players: " + JoinVector(gameSummary->GetPlayerNames(), false));
      SendReply("Slot IDs: " + ByteArrayToDecString(gameSummary->GetSIDs()));
      SendReply("Player IDs: " + ByteArrayToDecString(gameSummary->GetUIDs()));
      SendReply("Colors: " + ByteArrayToDecString(gameSummary->GetColors()));
      break;
    }

    //
    // !SUMODE (enable a su mode session for 10 minutes)
    //

    case HashCode("sumode"): {
      string inputLower = ToLowerCase(target);

      const optional<bool> parsedToggle = ParseBoolean(target);
      if (!parsedToggle.has_value()) {
        ErrorReply("Usage: " + cmdToken + "sumode <ON|OFF>");
        break;
      }
      const bool targetValue = parsedToggle.value();

      if (!GetIsSudo()) {
        if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
          ErrorReply("Requires sudo permissions.");
        } else if (!GetHasCommandHistory()) {
          ErrorReply("Requires sudo permissions. Please join a game, and use " + cmdToken + " su sumode to start a superuser session");
        } else {
          ErrorReply("Requires sudo permissions. Please use " + cmdToken + " su sumode to start a superuser session");
        }
        break;
      }

      if (!GetHasCommandHistory()) {
        ErrorReply("SU mode can only be toggled in a game.");
        break;
      }

      if (targetValue == GetCommandHistory()->CheckSudoMode(m_Aura, GetSourceGame(), GetSender())) {
        if (targetValue) {
          ErrorReply("SU mode is already ENABLED.");
        } else {
          ErrorReply("SU mode is already DISABLED.");
        }
        break;
      }

      if (targetValue) {
        GetCommandHistory()->SudoModeStart(m_Aura, GetSourceGame(), GetSender());
        SendReply("Sudo session started. You will have unrestricted access to all commands for 10 minutes.");
        SendReply("Your session will be over as soon as you leave the game.");
        SendReply("WARN: Make sure NOT to enable sudo session over a wireless Internet connection.");
        SendReply("(Prefer using per-command sudo to avoid getting hacked.)");
      } else {
        GetCommandHistory()->SudoModeEnd(m_Aura, GetSourceGame(), GetSender());
        SendReply("Sudo session ended.");
      }
      break;
    }

    //
    // !LOADCFG (load config file)
    //

    case HashCode("loadcfg"): {
      if (target.empty()) {
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
      if (m_Aura->m_ExitingSoon) {
        ErrorReply("Aura is shutting down. No games may be hosted.");
        break;
      }

      if (!FileExists(m_Aura->m_Config.m_MapCFGPath)) {
        ErrorReply("Map config path doesn't exist", CHAT_LOG_INCIDENT);
        break;
      }

      shared_ptr<CGameSetup> gameSetup = nullptr;
      try {
        gameSetup = make_shared<CGameSetup>(m_Aura, shared_from_this(), target, SEARCH_TYPE_ONLY_CONFIG, SETUP_PROTECT_ARBITRARY_TRAVERSAL, SETUP_PROTECT_ARBITRARY_TRAVERSAL, true /* lucky mode */);
      } catch (...) {
      }
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
    // !FLUSHIP
    // !FLUSHNET
    //

    case HashCode("flushdns"):
    case HashCode("fluship"):
    case HashCode("flushnet"): {
      if (!GetIsSudo()) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      m_Aura->m_Net.FlushDNSCache();
      m_Aura->m_Net.FlushSelfIPCache();
      m_Aura->m_Net.FlushOutgoingThrottles();
      SendReply("Cleared network data (DNS, IP, throttle.)");
      break;
    }

    //
    // !QUERY
    //

    case HashCode("query"): {
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Requires sudo permissions."); // But not really
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "query <QUERY>");
        ErrorReply("Usage: " + cmdToken + "query <REALM>, <QUERY>");
        ErrorReply("Valid queries are: listgames, printgames, netinfo, quota");
        break;
      }

      vector<string> Args = SplitArgs(target, 1u, 2u);
      if (Args.empty() || (Args.size() < 2 && !sourceRealm)) {
        ErrorReply("Usage: " + cmdToken + "query <QUERY>");
        ErrorReply("Usage: " + cmdToken + "query <REALM>, <QUERY>");
        ErrorReply("Valid queries are: listgames, printgames, netinfo, quota");
        break;
      }

      shared_ptr<CRealm> targetRealm = sourceRealm;
      if (Args.size() > 1) {
        targetRealm = GetTargetRealmOrCurrent(Args[0]);
      }
      if (!targetRealm) {
        ErrorReply("Realm not found.");
        break;
      }

      string lowerQuery = Args[Args.size() - 1];
      transform(begin(lowerQuery), end(lowerQuery), begin(lowerQuery), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (lowerQuery == "netinfo" || lowerQuery == "quota" || lowerQuery == "games") {
        targetRealm->QueueCommand("/" + lowerQuery);
        SendReply("Query sent.");
      } else if (lowerQuery == "printgames") {
        targetRealm->QueueCommand("/games");
        SendReply("Query sent.");
      } else if (lowerQuery == "listgames" || lowerQuery == "gamelist" || lowerQuery == "gameslist") {
        int64_t Time = GetTime();
        if (Time - targetRealm->m_LastGameListTime >= 30) {
          targetRealm->SendGetGamesList();
          SendReply("Query sent.");
        } else {
          ErrorReply("Query ignored (antiflood.)");
        }
      }
      break;
    }

    // !CHANNEL (change channel)
    //

    case HashCode("channel"): {
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!CheckPermissions(m_Config->m_ModeratorBasePermissions, COMMAND_PERMISSIONS_ADMIN)) {
        ErrorReply("Not allowed to invite the bot to another channel.");
        break;
      }
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "channel <CHANNEL>");
        break;
      }
      if (!sourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (sourceRealm->GetGameBroadcast()) {
        ErrorReply("Cannot join a chat channel while hosting a lobby.");
        break;
      }
      if (!sourceRealm->QueueCommand("/join " + target)) {
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
      vector<string> currentGames;
      for (const auto& game : m_Aura->GetAllGames()) {
        if (game->GetIsLobbyStrict()) {
          currentGames.push_back(string("Lobby#")+ to_string(game->GetGameID()) + ": " + game->GetStatusDescription());
        } else {
          currentGames.push_back(string("Game#") + to_string(game->GetGameID()) + ": " + game->GetStatusDescription());
        }
      }
      if (currentGames.empty()) {
        SendReply("No games hosted.");
        break;
      }
      SendReply(JoinVector(currentGames, false), CHAT_LOG_INCIDENT);
      break;
    }

    //
    // !MAP (load map file)
    // !HOST (create game)
    //

    case HashCode("map"):
    case HashCode("load"):
    case HashCode("hostpriv"):
    case HashCode("host"): {
      UseImplicitReplaceable();
      shared_ptr<CGame> sourceGame = GetSourceGame();
      shared_ptr<CGame> targetGame = GetTargetGame();
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (!CheckPermissions(m_Config->m_HostPermissions, (
        targetGame && targetGame->GetIsLobbyStrict() && targetGame->GetIsReplaceable() ?
        COMMAND_PERMISSIONS_UNVERIFIED :
        COMMAND_PERMISSIONS_ADMIN
      ))) {
        ErrorReply("Not allowed to host games.");
        break;
      }
      bool isHostPublic = cmdHash == HashCode("host");
      bool isHostPrivate = cmdHash == HashCode("hostpriv");
      bool isHostCommand = isHostPublic || isHostPrivate;
      vector<string> Args = isHostCommand ? SplitArgs(target, 1, 6) : SplitArgs(target, 1, 5);

      if (Args.empty() || Args[0].empty() || (isHostCommand && Args[Args.size() - 1].empty())) {
        if (isHostCommand) {
          ErrorReply("Usage: " + cmdToken + "host <MAP NAME> , <GAME NAME>");
          if (GetIsGameUser() || !sourceRealm || sourceRealm->GetIsFloodImmune()) {
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
      if (m_Aura->m_GameSetup && m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("Another user is hosting a game.");
        break;
      }
      if (m_Aura->m_ExitingSoon) {
        ErrorReply("Aura is shutting down. No games may be hosted.");
        break;
      }

      string gameName;
      if (isHostCommand) {
        if (targetGame && sourceGame != targetGame && targetGame->GetIsReplaceable() && targetGame->GetHasAnyUser()) {
          // If there are users in the replaceable lobby (and we are not among them), do not replace it.
          targetGame.reset();
        }
        const bool isReplace = targetGame && !targetGame->GetCountDownStarted() && targetGame->GetIsReplaceable() && !targetGame->GetIsBeingReplaced();
        if (!(isReplace ? m_Aura->GetNewGameIsInQuotaReplace() : m_Aura->GetNewGameIsInQuota())) {
          ErrorReply("Other lobbies are already being hosted (maximum is " + to_string(m_Aura->m_Config.m_MaxLobbies) + ").");
          break;
        }

        if (Args.size() >= 2) {
          gameName = Args[Args.size() - 1];
          Args.pop_back();
        } else {
          gameName = GetSender() + "'s " + Args[0];
          if (gameName.length() > m_Aura->m_MaxGameNameSize) {
            gameName = GetSender() + "'s game";
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

      shared_ptr<CGameSetup> gameSetup = nullptr;
      try {
        gameSetup = make_shared<CGameSetup>(m_Aura, shared_from_this(), Args[0], SEARCH_TYPE_ANY, SETUP_PROTECT_ARBITRARY_TRAVERSAL, SETUP_PROTECT_ARBITRARY_TRAVERSAL, isHostCommand /* lucky mode */);
      } catch (...) {
      }
      if (!gameSetup) {
        delete options;
        ErrorReply("Unable to host game", CHAT_SEND_SOURCE_ALL);
        break;
      }
      if (isHostCommand) {
        gameSetup->SetDisplayMode(isHostPrivate ? GAME_PRIVATE : GAME_PUBLIC);
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
      shared_ptr<CRealm> sourceRealm = GetSourceRealm();

      if (0 == (m_Permissions & USER_PERMISSIONS_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Not allowed to mirror games.");
        break;
      }

      if (!m_Aura->m_GameSetup || m_Aura->m_GameSetup->GetIsDownloading()) {
        ErrorReply("A map must first be loaded with " + (sourceRealm ? sourceRealm->GetCommandToken() : "!") + "map.");
        break;
      }

      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "mirror <EXCLUDESERVER> , <IP> , <PORT> , <GAMEID> , <GAMEKEY> , <GAMENAME> - GAMEID, GAMEKEY expected hex.");
        break;
      }

      if (!m_Aura->GetNewGameIsInQuotaConservative()) {
        ErrorReply("Already hosting a game.");
        break;
      }

      vector<string> Args = SplitArgs(target, 6);

      uint16_t gamePort = 6112;
      uint32_t gameHostCounter = 1;

      shared_ptr<CRealm> excludedServer = m_Aura->GetRealmByInputId(Args[0]);

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
      m_Aura->m_GameSetup->SetContext(shared_from_this());
      if (!m_Aura->m_GameSetup->SetMirrorSource(maybeAddress.value(), gameHostCounter)) {
        ErrorReply("Cannot mirror game at the provided address.");
        break;
      }
      m_Aura->m_GameSetup->SetBaseName(gameName);
      if (excludedServer) m_Aura->m_GameSetup->AddIgnoredRealm(excludedServer);
      m_Aura->m_GameSetup->RunHost();
      for (auto& realm : m_Aura->m_Realms) {
        if (realm != excludedServer && !realm->GetIsMirror()) {
          realm->ResetConnection(false);
          realm->SetReconnectNextTick(true);
        }
      }
      break;
    }

    case HashCode("nick"): {
      if (0 == (m_Permissions & USER_PERMISSIONS_CHANNEL_ADMIN)) {
        ErrorReply("You are not allowed to change my nickname.");
        break;
      }
      if (GetServiceSourceType() != SERVICE_TYPE_IRC && !GetIsSudo()) {
        ErrorReply("This is an IRC-exclusive command.");
        break;
      }
      m_Aura->m_IRC.Send("NICK :" + target);
      m_Aura->m_IRC.m_NickName = target;
      break;
    }

    case HashCode("discord"): {
      if (!m_Aura->m_Discord.m_Client) {
        ErrorReply("Not connected to Discord.");
        break;
      }

      if (m_Aura->m_Discord.m_Config.m_InviteUrl.empty()) {
        ErrorReply("This bot is invite-only. Ask the owner for an invitation link.");
        break;
      }

      switch (m_Aura->m_Discord.m_Config.m_FilterJoinServersMode) {
        case FILTER_DENY_ALL:
          SendReply("Install me to your user (DM-only) at <" + m_Aura->m_Discord.m_Config.m_InviteUrl + ">");
          break;
        case FILTER_ALLOW_LIST:
          SendReply("Install me to your server (requires approval) at <" + m_Aura->m_Discord.m_Config.m_InviteUrl + ">");
          break;
        default:
          SendReply("Install me to your server at <" + m_Aura->m_Discord.m_Config.m_InviteUrl + ">");
      }
      break;
    }

    case HashCode("v"): {
      SendReply("v" + cmdToken);
      break;
    }

    case HashCode("afk"):
    case HashCode("unready"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || !GetIsGameUser())
        break;

      if (targetGame->GetCountDownStarted()/* && !targetGame->GetCountDownUserInitiated()*/) {
        // Stopping the countdown here MAY be sensible behavior,
        // but only if it's not a manually initiated countdown, and if ...
        break;
      }

      uint8_t readyMode = targetGame->GetPlayersReadyMode();
      bool isAlwaysReadyMode = readyMode == READY_MODE_FAST;
      if (readyMode == READY_MODE_EXPECT_RACE) {
        if (targetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
          isAlwaysReadyMode = true;
        } else if (targetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
          isAlwaysReadyMode = true;
        }
      }
      if (isAlwaysReadyMode) {
        ErrorReply("You are always assumed to be ready. Please don't go AFK.");
        break;
      }
      if (GetGameUser()->GetIsObserver()) {
        ErrorReply("Observers are always assumed to be ready.");
        break;
      }
      if (!GetGameUser()->GetIsReady()) {
        ErrorReply("You are already flagged as not ready.");
        break;
      }
      GetGameUser()->SetUserReady(false);
      if (GetGameUser()->UpdateReady()) {
         // Should never happen
        ErrorReply("Failed to set not ready.");
        GetGameUser()->ClearUserReady();
        break;
      }
      --targetGame->m_ControllersReadyCount;
      ++targetGame->m_ControllersNotReadyCount;
      SendAll("Player [" + GetSender() + "] no longer ready to start the game. When you are, use " + cmdToken + "ready");
      break;
    }

    case HashCode("ready"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict() || !GetIsGameUser()) {
        break;
      }

      if (targetGame->GetCountDownStarted()) {
        break;
      }

      uint8_t readyMode = targetGame->GetPlayersReadyMode();
      bool isAlwaysReadyMode = readyMode == READY_MODE_FAST;
      if (readyMode == READY_MODE_EXPECT_RACE) {
        if (targetGame->GetMap()->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
          isAlwaysReadyMode = true;
        } else if (targetGame->GetMap()->GetMapFlags() & MAPFLAG_RANDOMRACES) {
          isAlwaysReadyMode = true;
        }
      }
      if (isAlwaysReadyMode) {
        ErrorReply("You are always assumed to be ready. Please don't go AFK.");
        break;
      }
      if (GetGameUser()->GetIsObserver()) {
        ErrorReply("Observers are always assumed to be ready.");
        break;
      }
      if (GetGameUser()->GetIsReady()) {
        ErrorReply("You are already flagged as ready.");
        break;
      }
      if (!GetGameUser()->GetMapReady()) {
        ErrorReply("Map not downloaded yet.");
        break;
      }
      GetGameUser()->SetUserReady(true);
      if (!GetGameUser()->UpdateReady()) {
         // Should never happen
        ErrorReply("Failed to set ready.");
        GetGameUser()->ClearUserReady();
        break;
      }
      ++targetGame->m_ControllersReadyCount;
      --targetGame->m_ControllersNotReadyCount;
      SendAll("Player [" + GetSender() + "] ready to start the game. Please don't go AFK.");
      break;
    }

    case HashCode("checkready"):
    case HashCode("askready"):
    case HashCode("readystatus"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!targetGame || !targetGame->GetIsLobbyStrict()) {
        break;
      }
      if (targetGame->GetCountDownStarted()) {
        break;
      }
      SendReply(targetGame->GetReadyStatusText());

      vector<const GameUser::CGameUser*> unreadyPlayers = targetGame->GetUnreadyPlayers();
      if (!unreadyPlayers.empty()) {
        SendReply("Waiting for: " + ToNameListSentence(unreadyPlayers));
      }
      break;
    }

    case HashCode("pin"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!GetIsGameUser()) {
        break;
      }
      if (!targetGame || !targetGame->GetIsLobbyStrict()) {
        break;
      }
      if (targetGame->GetCountDownStarted()) {
        break;
      }
      if (target.empty()) {
        ErrorReply("Usage: " + cmdToken + "pin <MESSAGE>");
        break;
      }
      if (target.size() > 140) {
        ErrorReply("Message cannot exceed 140 characters.");
        break;
      }
      GetGameUser()->SetPinnedMessage(target);
      SendReply("Message pinned. It will be shown to every user that joins the game.");
      break;
    }

    case HashCode("unpin"): {
      shared_ptr<CGame> targetGame = GetTargetGame();

      if (!GetIsGameUser()) {
        break;
      }
      if (!targetGame || !targetGame->GetIsLobbyStrict()) {
        break;
      }
      if (targetGame->GetCountDownStarted()) {
        break;
      }
      
      if (!GetGameUser()->GetHasPinnedMessage()) {
        ErrorReply("You don't have a pinned message.");
        break;
      }
      GetGameUser()->ClearPinnedMessage();
      SendReply("Pinned message removed.");
      break;
    }

    case HashCode("ff"): {
      // 2x 4x 6x 8x 16x 32x 64x
      auto gameSource = GetGameSource();
      if (!gameSource.GetIsSpectator()) {
        ErrorReply("This command can only be used in spectator mode.");
        break;
      }
      CAsyncObserver* spectator = gameSource.GetSpectator();
      int64_t frameRate = spectator->GetFrameRate();
      if (frameRate >= 64) {
        spectator->SendChat("Playback rate is limited to 64x");
      } else {
        switch (frameRate) {
          // Smooth out acceleration around 8x, since
          // 1 - many computers cannot handle such a large speed
          // 2 - vanilla WC3 cannot actually handle speeds above 8x
          case 4: case 8:
            spectator->SetFrameRate(frameRate * 3 / 2);
            break;
          case 6: case 12:
            spectator->SetFrameRate(frameRate *  4 / 3);
            break;
          default:
            spectator->SetFrameRate(2 * frameRate);
        }
        spectator->SendProgressReport();
      }
      break;
    }

    case HashCode("sync"): {
      auto gameSource = GetGameSource();
      if (!gameSource.GetIsSpectator()) {
        ErrorReply("This command can only be used in spectator mode.");
        break;
      }
      CAsyncObserver* spectator = gameSource.GetSpectator();
      int64_t frameRate = spectator->GetFrameRate();
      if (frameRate == 1) {
        spectator->SendChat("Playback rate is already 1x.");
      } else {
        spectator->SetFrameRate(1);
        spectator->SendChat("Playback rate set to " + to_string(spectator->GetFrameRate()) + "x");
      }
      break;
    }

    default: {
      bool hasLetter = false;
      for (const auto& c : baseCommand) {
        if (97 <= c && c <= 122) {
          hasLetter = true;
          break;
        }
      }
      if (hasLetter) {
        ErrorReply("Unrecognized command [" + baseCommand + "].");
      }
      break;
    }
  }
}


uint8_t CCommandContext::TryDeferred(CAura* nAura, const LazyCommandContext& lazyCtx)
{
  string cmdToken;
  shared_ptr<CGame> targetGame = nullptr;
  CCommandConfig* commandCFG = nAura->m_Config.m_LANCommandCFG;
  shared_ptr<CCommandContext> ctx = nullptr;

  if (lazyCtx.identityLoc.empty()) {
    Print("[AURA] --exec service empty.");
    return APP_ACTION_ERROR;
  }

  void* servicePtr = nullptr;
  uint8_t serviceType = nAura->FindServiceFromHostName(lazyCtx.identityLoc, servicePtr);
  if (serviceType == SERVICE_TYPE_NONE) {
    Print("[AURA] --exec parsed user at service invalid.");
    return APP_ACTION_ERROR;
  }

  if (!lazyCtx.targetGame.empty()) {
    targetGame = nAura->GetGameByString(lazyCtx.targetGame);
    if (!targetGame) {
      return APP_ACTION_WAIT;
    }
  }

  switch (serviceType) {
    case SERVICE_TYPE_GAME:
    case SERVICE_TYPE_DISCORD:
      Print("[AURA] --exec-as: @service not supported [" + lazyCtx.identityLoc + "]");
      return APP_ACTION_ERROR;
    case SERVICE_TYPE_CLI:
      try {
        if (targetGame) {
          ctx = make_shared<CCommandContext>(
            serviceType, nAura, commandCFG, targetGame, lazyCtx.identityName, lazyCtx.broadcast, &std::cout
          );
        } else {
          ctx = make_shared<CCommandContext>(
            serviceType, nAura, lazyCtx.identityName, lazyCtx.broadcast, &std::cout
          );
        }
      } catch (...) {
      }
      break;
    case SERVICE_TYPE_IRC:
      if (!nAura->m_IRC.GetIsEnabled()) return APP_ACTION_ERROR;
      if (nAura->m_IRC.m_Config.m_Channels.empty()) return APP_ACTION_ERROR;
      if (!nAura->m_IRC.GetIsLoggedIn()) return APP_ACTION_WAIT;

      try {
         if (targetGame) {
          ctx = make_shared<CCommandContext>(
            serviceType, nAura, commandCFG,
            nAura->m_IRC.m_Config.m_Channels[0], lazyCtx.identityName,
            false, lazyCtx.identityName + nAura->m_IRC.m_Config.m_VerifiedDomain,
            lazyCtx.broadcast, &std::cout
          );
         } else {
           ctx = make_shared<CCommandContext>(
            serviceType, nAura, commandCFG, targetGame,
            nAura->m_IRC.m_Config.m_Channels[0], lazyCtx.identityName,
            false, lazyCtx.identityName + nAura->m_IRC.m_Config.m_VerifiedDomain,
            lazyCtx.broadcast, &std::cout
          );
         }
      } catch (...) {
      }
      break;
    case SERVICE_TYPE_REALM:
      Print("[AURA] --exec parsed user at service is realm.");
      CRealm* sourceRealm = reinterpret_cast<CRealm*>(servicePtr);
      Print("[AURA] --exec parsed user at service is realm " + sourceRealm->GetCanonicalDisplayName() + ".");
      if (!sourceRealm->GetLoggedIn()) {
        Print("[AURA] --exec realm not logged in yet...");
        return APP_ACTION_WAIT;
      }

      commandCFG = sourceRealm->GetCommandConfig();
      try {
         if (targetGame) {
          ctx = make_shared<CCommandContext>(
            serviceType, nAura, commandCFG, targetGame, sourceRealm->shared_from_this(),
            lazyCtx.identityName, true, lazyCtx.broadcast, &std::cout
          );
         } else {
           ctx = make_shared<CCommandContext>(
            serviceType, nAura, commandCFG, sourceRealm->shared_from_this(),
            lazyCtx.identityName, true, lazyCtx.broadcast, &std::cout
          );
         }
      } catch (...) {
      }
      break;
  }

  if (!ctx) {
    return APP_ACTION_ERROR;
  }

  switch (lazyCtx.auth) {
    case CommandAuth::kAuto:
      break;
    case CommandAuth::kSpoofed:
      ctx->SetPermissions(0u);
      break;
    case CommandAuth::kVerified:
      ctx->SetPermissions(USER_PERMISSIONS_CHANNEL_VERIFIED);
      break;
    case CommandAuth::kAdmin:
      ctx->SetPermissions(USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_CHANNEL_VERIFIED);
      break;
    case CommandAuth::kRootAdmin:
      ctx->SetPermissions(USER_PERMISSIONS_CHANNEL_ROOTADMIN | USER_PERMISSIONS_CHANNEL_ADMIN | USER_PERMISSIONS_CHANNEL_VERIFIED);
      break;
    case CommandAuth::kSudo:
      ctx->SetPermissions(SET_USER_PERMISSIONS_ALL);
      break;
  }
  ctx->Run(cmdToken, lazyCtx.command, lazyCtx.target);
  return APP_ACTION_DONE;
}

CCommandContext::~CCommandContext()
{
  m_Output = nullptr;
  m_Aura = nullptr;
  m_TargetRealm.reset();
  m_TargetGame.reset();
  ResetGameSource();
  ResetServiceSource();
}

