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

using namespace std;

//
// CCommandContext
//

/* In-game command */
CCommandContext::CCommandContext(CAura* nAura, CGame* game, CGamePlayer* player, ostream* nOutputStream, char nToken)
  : m_Aura(nAura),
    m_FromName(player->GetName()),
    m_FromWhisper(false),
    m_FromType(FROM_GAME),
    m_Token(nToken),

    m_SourceGame(game),
    m_TargetGame(game),
    m_Permissions(0),

    m_Player(player),

    m_HostName(player->GetRealmHostName()),
    m_SourceRealm(player->GetRealm(false)),
    m_TargetRealm(nullptr),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* Command received from BNET but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CGame* targetGame, CRealm* fromRealm, string& fromName, bool& isWhisper, ostream* nOutputStream, char nToken)
  : m_Aura(nAura),
    m_FromName(fromName),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_BNET),
    m_Token(nToken),

    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(fromRealm->GetServer()),
    m_SourceRealm(fromRealm),
    m_TargetRealm(nullptr),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* Command received from IRC but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CGame* targetGame, CIRC* ircNetwork, string& channelName, string& userName, bool& isWhisper, ostream* nOutputStream, char nToken)
  : m_Aura(nAura),
    m_FromName(userName),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_IRC),
    m_Token(nToken),

    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_Permissions(0),    

    m_Player(nullptr),

    m_HostName(string()),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),

    m_IRC(ircNetwork),
    m_ChannelName(channelName),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* Command received from elsewhere but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CGame* targetGame, ostream* nOutputStream, char nToken)
  : m_Aura(nAura),
    m_FromName(string()),
    m_FromWhisper(false),
    m_FromType(FROM_OTHER),
    m_Token(nToken),

    m_SourceGame(nullptr),
    m_TargetGame(targetGame),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(string()),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* BNET command */
CCommandContext::CCommandContext(CAura* nAura, CRealm* fromRealm, string& fromName, bool& isWhisper, ostream* nOutputStream, char nToken)
  : m_Aura(nAura),
    m_FromName(fromName),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_BNET),
    m_Token(nToken),

    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(fromRealm->GetServer()),
    m_SourceRealm(fromRealm),
    m_TargetRealm(nullptr),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* IRC command */
CCommandContext::CCommandContext(CAura* nAura, CIRC* ircNetwork, string& channelName, string& userName, bool& isWhisper, ostream* nOutputStream, char nToken)
  : m_Aura(nAura),
    m_FromName(userName),
    m_FromWhisper(isWhisper),
    m_FromType(FROM_IRC),
    m_Token(nToken),

    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(string()),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),

    m_IRC(ircNetwork),
    m_ChannelName(channelName),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

/* Generic command */
CCommandContext::CCommandContext(CAura* nAura, ostream* nOutputStream, char nToken)
  : m_Aura(nAura),
    m_FromName(string()),
    m_FromWhisper(false),
    m_FromType(FROM_OTHER),
    m_Token(nToken),

    m_SourceGame(nullptr),
    m_TargetGame(nullptr),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(string()),
    m_SourceRealm(nullptr),
    m_TargetRealm(nullptr),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(nOutputStream),
    m_RefCount(1),
    m_PartiallyDestroyed(false)
{
}

bool CCommandContext::SetIdentity(const string& userName, const string& realmId)
{
  m_SourceRealm = m_Aura->GetRealmByInputId(realmId);
  if (m_SourceRealm == nullptr)
    return false;
  m_FromName = userName;
  m_HostName = m_SourceRealm->GetServer();
  return true;
}

string CCommandContext::GetUserAttribution()
{
  if (m_Player) {
    return m_FromName + "@" + (m_HostName.empty() ? "@@LAN/VPN" : m_HostName);
  } else if (m_SourceRealm) {
    return m_FromName + "@" + m_SourceRealm->GetServer();
  } else if (m_IRC) {
    return m_FromName + "@" + (m_IRC->m_Config->m_HostName);
  } else if (!m_FromName.empty()) {
    return m_FromName;
  } else {
    return "[Anonymous]";
  }
}

string CCommandContext::GetUserAttributionPreffix()
{
  if (m_Player) {
    return m_TargetGame->GetLogPrefix() + "Player [" + m_FromName + "@" + (m_HostName.empty() ? "@@LAN/VPN" : m_HostName) + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (m_SourceRealm) {
    return m_SourceRealm->GetLogPrefix() + "User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
  } else if (m_IRC) {
    return "[IRC] User [" + m_FromName + "] (Mode " + ToHexString(m_Permissions) + ") ";
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
  bool IsRealmVerified = false;
  if (m_OverrideVerified.has_value()) {
    IsRealmVerified = m_OverrideVerified.value();
  } else {
    IsRealmVerified = m_Player ? m_Player->IsRealmVerified() : m_SourceRealm != nullptr;
  }

  // Trust PvPGN servers on player identities for admin powers. Their impersonation is not a threat we worry about.
  // However, do NOT trust them regarding sudo access, since those commands may cause data deletion or worse.
  // Note also that sudo permissions must be ephemeral, since neither WC3 nor PvPGN TCP connections are secure.
  bool IsOwner = m_TargetGame && m_TargetGame->MatchOwnerName(m_FromName) && m_HostName == m_TargetGame->m_OwnerRealm && (
    IsRealmVerified || m_Player && m_HostName.empty()
  );
  bool IsRootAdmin = IsRealmVerified && m_SourceRealm != nullptr && m_SourceRealm->GetIsRootAdmin(m_FromName);
  bool IsAdmin = IsRootAdmin || IsRealmVerified && m_SourceRealm != nullptr && m_SourceRealm->GetIsAdmin(m_FromName);
  bool IsSudoSpoofable = IsRealmVerified && m_SourceRealm != nullptr && m_SourceRealm->GetIsSudoer(m_FromName);
  // bool IsIRCAdmin = m_IRC != nullptr && m_IRC->GetIsRootAdmin(m_FromName);

  // Owners are always treated as players if the game hasn't started yet. Even if they haven't joined.
  if (m_Player || IsOwner && m_TargetGame && m_TargetGame->GetIsLobby()) m_Permissions |= PERM_GAME_PLAYER;

  // Leaver or absent owners are automatically demoted.
  if (IsOwner && (m_Player || m_TargetGame && m_TargetGame->GetIsLobby())) m_Permissions |= PERM_GAME_OWNER;
  if (IsAdmin) m_Permissions |= PERM_BNET_ADMIN;
  if (IsRootAdmin) m_Permissions |= PERM_BNET_ROOTADMIN;
  // if (IsIRCAdmin) m_Permissions |= PERM_IRC_ADMIN;

  // Sudo is a separate permission system.
  if (IsSudoSpoofable) m_Permissions |= PERM_BOT_SUDO_SPOOFABLE;
}

void CCommandContext::SetIRCAdmin(const bool& isAdmin)
{
  if (isAdmin) {
    m_Permissions |= PERM_IRC_ADMIN;
  } else {
    m_Permissions &= ~PERM_IRC_ADMIN;
  }
}

optional<pair<string, string>> CCommandContext::CheckSudo(const string& message)
{
  optional<pair<string, string>> Result;
  if (!m_HostName.empty() && !(m_Permissions & PERM_BOT_SUDO_SPOOFABLE)) {
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
  );
  if (isValidCaller && message == m_Aura->m_SudoAuthPayload) {
    (*m_Output) << "[AURA] Confirmed " + m_FromName + " command \"" + m_Aura->m_SudoExecCommand + "\"" << std::endl;
    size_t PayloadStart = m_Aura->m_SudoExecCommand.find(' ');
    string Command, Payload;
    if (PayloadStart != string::npos) {
      Command = m_Aura->m_SudoExecCommand.substr(0, PayloadStart);
      Payload = m_Aura->m_SudoExecCommand.substr(PayloadStart + 1);
    } else {
      Command = m_Aura->m_SudoExecCommand;
    }
    transform(begin(Command), end(Command), begin(Command), ::tolower);
    Result = make_pair(Command, Payload);
    //m_Permissions |= PERM_BOT_SUDO_OK;
    m_Permissions = 0xFFFF;
  }
  m_Aura->UnholdContext(m_Aura->m_SudoContext);
  m_Aura->m_SudoContext = nullptr;
  m_Aura->m_SudoAuthPayload.clear();
  m_Aura->m_SudoExecCommand.clear();
  return Result;
}

void CCommandContext::SendReply(const string& message)
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
      if (!m_SourceRealm) break;
      m_SourceRealm->TrySendChat(message, m_FromName, true, m_Output);
      break;
    }

    case FROM_IRC: {
      if (!m_IRC) break;
      if (m_FromWhisper) {
        m_IRC->SendUser(message, m_FromName);
      } else {
        m_IRC->SendChannel(message, m_ChannelName);
      }
      break;
    }

    default: {
      (*m_Output) << "[AURA] " + message << std::endl;
    }
  }
}

void CCommandContext::SendReply(const string& message, const uint8_t ccFlags) {
  bool AllTarget = ccFlags & CHAT_SEND_TARGET_ALL;
  bool AllSource = ccFlags & CHAT_SEND_SOURCE_ALL;
  bool AllSourceSuccess = false;
  if (AllTarget) {
    if (m_TargetGame) {
      m_TargetGame->SendAllChat(message);
      if (m_TargetGame == m_SourceGame) {
        AllSourceSuccess = true;
      }
    }
    if (m_TargetRealm) {
      m_TargetRealm->TrySendChat(message, m_FromName, false, m_Output);
      if (m_TargetRealm == m_SourceRealm) {
        AllSourceSuccess = true;
      }
    }
    // IRC is not a valid target.
  }
  if (AllSource && !AllSourceSuccess && !m_FromWhisper) {
    if (m_SourceGame) {
      m_SourceGame->SendAllChat(message);
      AllSourceSuccess = true;
    }
    if (m_SourceRealm) {
      m_SourceRealm->TrySendChat(message, m_FromName, false, m_Output);
      AllSourceSuccess = true;
    }
    if (m_IRC) {
      m_IRC->SendChannel(message, m_ChannelName);
      AllSourceSuccess = true;
    }
  }
  if (!AllSourceSuccess) {
    SendReply(message);
  }

  if (m_FromType != FROM_OTHER && (ccFlags & CHAT_LOG_CONSOLE)) {
    if (m_TargetGame) {
      (*m_Output) << m_TargetGame->GetLogPrefix() + message << std::endl;
    } else if (m_SourceRealm) {
      (*m_Output) << m_SourceRealm->GetLogPrefix() + message << std::endl;
    } else if (m_IRC) {
      (*m_Output) << "[IRC] " + message << std::endl;
    } else {
      (*m_Output) << "[AURA] " + message << std::endl;
    }
  }
}

void CCommandContext::ErrorReply(const string& message)
{
  SendReply("Error: " + message);
}

void CCommandContext::ErrorReply(const string& message, const uint8_t ccFlags) {
  SendReply("Error: " + message, ccFlags);
}

void CCommandContext::SendAll(const string& message)
{
  SendReply(message, CHAT_SEND_TARGET_ALL);
}

void CCommandContext::ErrorAll(const string& message)
{
  ErrorReply(message, CHAT_SEND_TARGET_ALL);
}

CGamePlayer* CCommandContext::GetTargetPlayer(const string& target)
{
  CGamePlayer* TargetPlayer = nullptr;
  if (target.empty() || !m_TargetGame) {
    return TargetPlayer;
  }

  uint8_t Matches = m_TargetGame->GetPlayerFromNamePartial(target, &TargetPlayer);
  if (Matches > 1) {
    TargetPlayer = nullptr;
  }
  return TargetPlayer;
}

CGamePlayer* CCommandContext::GetTargetPlayerOrSelf(const string& target)
{
  if (target.empty()) {
    return m_Player;
  }

  CGamePlayer* TargetPlayer = nullptr;
  if (!m_TargetGame) return TargetPlayer;
  uint8_t Matches = m_TargetGame->GetPlayerFromNamePartial(target, &TargetPlayer);
  if (Matches > 1) {
    TargetPlayer = nullptr;
  }
  return TargetPlayer;
}

CRealm* CCommandContext::GetTargetRealmOrCurrent(const string& target)
{
  if (target.empty()) {
    return m_SourceRealm;
  }
  string realmId = target;
  transform(begin(realmId), end(realmId), begin(realmId), ::tolower);
  return m_Aura->GetRealmByInputId(realmId);
}

CGame* CCommandContext::GetTargetGame(const string& target)
{
  if (target.empty()) {
    return nullptr;
  }
  string gameId = target;
  transform(begin(gameId), end(gameId), begin(gameId), ::tolower);
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
  if (m_TargetGame && 0 == (m_Permissions & PERM_BOT_SUDO_OK)) {
    UpdatePermissions();
  }
}

void CCommandContext::Run(const string& command, const string& payload)
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
    if (!m_HostName.empty() && 0 == (m_Permissions & PERM_BOT_SUDO_SPOOFABLE)) {
      ErrorReply("Forbidden");
      return;
    }
    m_Aura->HoldContext(this);
    m_Aura->m_SudoContext = this;
    // TODO(IceSandslash): GetSudoAuthPayload should only be executed after some local input.
    m_Aura->m_SudoAuthPayload = m_Aura->GetSudoAuthPayload(Payload);
    m_Aura->m_SudoExecCommand = Payload;
    Print("[AURA] Sudoer " + m_FromName + " requests command \"" + Payload + "\"");
    if (m_SourceRealm && m_FromWhisper) {
      Print("[AURA] Confirm from [" + m_HostName + "] with: \"/w " + m_SourceRealm->GetLoginName() + " " + GetToken() + "sudo " + m_Aura->m_SudoAuthPayload + "\"");
    } else if (m_IRC) {
      Print("[AURA] Confirm from [" + m_IRC->m_Config->m_HostName + "] with: \"" + GetToken() + "aura sudo " + m_Aura->m_SudoAuthPayload + "\"");
    } else {
      Print("[AURA] Confirm from the game client with: \"" + GetToken() + "sudo " + m_Aura->m_SudoAuthPayload + "\"");
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
      ErrorReply("Sudo check failure.");
      Print(m_FromName + "@" + m_HostName + " failed sudo authentication.");
      return;
    }
  }

  if (m_TargetGame && m_TargetGame->m_Locked && 0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
    (*m_Output) << m_TargetGame->GetLogPrefix() + "Command ignored, the game is locked" << std::endl;
    ErrorReply("Only the game owner and root admins can run game commands when the game is locked.");
    return;
  }

  if (Payload.empty()) {
    (*m_Output) << GetUserAttributionPreffix() + "sent command [" + GetToken() + Command + "]" << std::endl;
  } else {
    (*m_Output) << GetUserAttributionPreffix() + "sent command [" + GetToken() + Command + "] with payload [" + Payload + "]" << std::endl;
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
      bool IsOwner = m_TargetGame->MatchOwnerName(targetPlayer->GetName()) && m_TargetGame->HasOwnerInGame();
      bool IsRootAdmin = IsRealmVerified && targetPlayerRealm->GetIsRootAdmin(m_FromName);
      bool IsAdmin = IsRootAdmin || IsRealmVerified && targetPlayerRealm->GetIsAdmin(m_FromName);
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
      bool IsForwards = targetPlayer->GetSyncCounter() >= m_TargetGame->m_SyncCounter;
      uint32_t OffsetAbs = (
        IsForwards ? targetPlayer->GetSyncCounter() - m_TargetGame->m_SyncCounter :
        m_TargetGame->m_SyncCounter - targetPlayer->GetSyncCounter()
      );
      string SyncOffsetText = IsForwards ? " +" + to_string(OffsetAbs) : " -" + to_string(OffsetAbs);
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
      SendReply("[" + targetPlayer->GetName() + "]. " + SlotFragment + "Ping: " + (targetPlayer->GetNumPings() > 0 ? to_string(targetPlayer->GetPing()) + "ms" : "N/A") + IPVersionFragment + ", Reconnection: " + GProxyFragment + ", From: " + m_Aura->m_DB->FromCheck(ByteArrayToUInt32(targetPlayer->GetIPv4(), true)) + (m_TargetGame->GetGameLoaded() ? ", Sync: " + SyncStatus + SyncOffsetText : ""));
      SendReply("[" + targetPlayer->GetName() + "]. Realm: " + (targetPlayer->GetRealmHostName().empty() ? "LAN" : targetPlayer->GetRealmHostName()) + ", Verified: " + (IsRealmVerified ? "Yes" : "No") + ", Reserved: " + (targetPlayer->GetReserved() ? "Yes" : "No"));
      SendReply("[" + targetPlayer->GetName() + "]. Owner: " + (IsOwner ? "Yes" : "No") + ", Admin: " + (IsAdmin ? "Yes" : "No") + ", Root Admin: " + (IsRootAdmin ? "Yes" : "No"));
      break;
    }


    //
    // !PING
    //
    case HashCode("ping"):
    case HashCode("p"): {
      // kick players with ping higher than payload if payload isn't empty
      // we only do this if the game hasn't started since we don't want to kick players from a game in progress

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      uint32_t KickedCount = 0;
      optional<uint32_t> KickPing;
      if (!Payload.empty()) {
        if (!m_TargetGame->GetIsLobby() || 0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_OK))) {
          ErrorReply("Not allowed to set maximum ping.");
          break;
        }
        try {
          KickPing = stoul(Payload);
        } catch (...) {
          ErrorReply("Invalid maximum ping [" + Payload + "].");
          break;
        }
        if (KickPing <= 0) {
          ErrorReply("Invalid maximum ping [" + Payload + "].");
          break;
        }
      }

      if (KickPing.has_value())
        m_TargetGame->m_AutoKickPing = KickPing.value();

      // copy the m_Players vector so we can sort by descending ping so it's easier to find players with high pings

      vector<CGamePlayer*> SortedPlayers = m_TargetGame->m_Players;
      sort(begin(SortedPlayers), end(SortedPlayers), [](const CGamePlayer* a, const CGamePlayer* b) {
        return a->GetPing() < b->GetPing();
      });
      string PingsText;

      for (auto i = begin(SortedPlayers); i != end(SortedPlayers); ++i) {
        PingsText += (*i)->GetName();
        PingsText += ": ";
        uint32_t NumPings = (*i)->GetNumPings();

        if (0 < NumPings && NumPings < 3)
          PingsText += "*";
        
        if (NumPings > 0) {
          PingsText += to_string((*i)->GetPing());

          if (m_TargetGame->GetIsLobby() && !(*i)->GetReserved()) {
            if (KickPing.has_value() && (*i)->GetPing() > m_TargetGame->m_AutoKickPing) {
              (*i)->SetKickByTime(GetTime() + 5);
              (*i)->SetLeftReason("was kicked for excessive ping " + to_string((*i)->GetPing()) + " > " + to_string(m_TargetGame->m_AutoKickPing));
              (*i)->SetLeftCode(PLAYERLEAVE_LOBBY);
              ++KickedCount;
            } else if ((*i)->GetKickQueued() && ((*i)->GetHasMap() || (*i)->GetDownloadStarted())) {
              (*i)->SetKickByTime(0);
              (*i)->SetLeftReason(emptyString);
            }
          }

          PingsText += "ms";
        } else {
          PingsText += "N/A";
        }

        if (i != end(SortedPlayers) - 1)
          PingsText += ", ";
      }

      SendAll(PingsText);

      if (KickedCount > 0)
        SendAll("Kicking " + to_string(KickedCount) + " players with pings greater than " + to_string(m_TargetGame->m_AutoKickPing) + "...");

      break;
    }

    //
    // !GETPLAYERS
    // !GETOBSERVERS
    //

    case HashCode("getplayers"):
    case HashCode("getobservers"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (m_TargetGame->m_GameDisplay == GAME_PRIVATE && !m_Player) {
        if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
          ErrorReply("This game is private.");
          break;
        }
      }
      string Players = m_TargetGame->GetPlayers();
      string Observers = m_TargetGame->GetObservers();
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
        ErrorReply("Usage: " + GetToken() + "votekick [PLAYERNAME]");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
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
      if (targetPlayer->GetReserved()) {
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
        SendAll("Player [" + m_Player->GetName() + "] voted to kick player [" + m_TargetGame->m_KickVotePlayer + "]. " + to_string(static_cast<uint32_t>(ceil((m_TargetGame->GetNumHumanPlayers() - 1) * static_cast<float>(m_TargetGame->m_VoteKickPercentage) / 100)) - 1) + " more votes are needed to pass");
      }
      SendAll("Type " + GetToken() + "yes or " + GetToken() + "no to vote.");

      break;
    }

    //
    // !YES
    //

    case HashCode("yes"): {
      if (!m_Player || m_TargetGame->m_KickVotePlayer.empty() || m_Player->GetKickVote().value_or(false))
        break;

      uint32_t VotesNeeded = static_cast<uint32_t>(ceil((m_TargetGame->GetNumHumanPlayers() - 1) * static_cast<float>(m_TargetGame->m_VoteKickPercentage) / 100));
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
        ErrorReply("Usage: " + GetToken() + "invite [PLAYERNAME]");
        break;
      }

      const string MapPath = m_TargetGame->m_Map->GetMapPath();
      string PassedMessage;
      size_t LastSlash = MapPath.rfind('\\');
      if (LastSlash != string::npos && LastSlash <= MapPath.length() - 6) {
        PassedMessage = m_FromName + " invites you to play [" + MapPath.substr(LastSlash + 1) + "]. Join game \"" + m_TargetGame->m_GameName + "\"";
      } else {
        PassedMessage = m_FromName + " invites you to join game \"" + m_TargetGame->m_GameName + "\"";
      }

      bool CanInviteGlobally = 0 != (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE));
      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;

        if (bnet == m_SourceRealm || CanInviteGlobally) {
          bnet->SendWhisper(PassedMessage, Payload);
        }
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
      if (!Payload.empty()) {
        double chancePercent;
        try {
          chancePercent = stod(Payload);
        } catch (...) {
          ErrorReply("Usage: " + GetToken() + "flip [CHANCE%]");
          break;
        }
        if (chancePercent < 0. || chancePercent > 100.) {
          ErrorReply("Usage: " + GetToken() + "flip [CHANCE%]");
          break;
        }
        chance = chancePercent / 100.;
      }

      // Max 5pm

      std::random_device rd;
      std::mt19937 gen(rd());
      std::bernoulli_distribution bernoulliDist(chance);
      bool result = bernoulliDist(gen);

      SendAll(m_FromName + " flipped a coin and got " + (result ? "heads" : "tails") + ".");
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
        ErrorReply("Usage: " + GetToken() + "roll [FACES]");
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

      SendAll(m_FromName + " rolled " + to_string(rollCount) + "d" + to_string(rollFaces) + ". Got: " + JoinVector(gotRolls, false));
      break;
    }

    //
    // !PICK
    //

    case HashCode("pick"): {
      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "pick [OPTION], [OPTION], [OPTION], ...");
        break;
      }

      vector<string> options = SplitArgs(Payload, 1, 24);
      if (options.empty()) {
        ErrorReply("Empty options list.");
        break;
      }
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> distribution(1, options.size());

      string randomPick = options[distribution(gen) - 1];
      SendAll("Randomly picked: " + randomPick);
      break;
    }

    /*****************
     * ADMIN COMMANDS *
     ******************/
     
    //
    // !FROM
    //

    case HashCode("from"):
    case HashCode("f"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (0 == (m_Permissions & ((m_TargetGame->HasOwnerSet() ? PERM_GAME_OWNER : PERM_GAME_PLAYER) | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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

      SendAll(Froms);
      break;
    }

    //
    // !BANLAST
    //

    case HashCode("banlast"):
    case HashCode("bl"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "c [SLOTNUM]");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 12u);
      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "c [SLOTNUM]");
        break;
      }

      for (auto& elem : Args) {
        if (elem == 0 || elem > 12) {
          ErrorReply("Usage: " + GetToken() + "c [SLOTNUM]");
          break;
        }
        uint8_t SID = static_cast<uint8_t>(elem) - 1;
        m_TargetGame->DeleteFakePlayer(SID);
        m_TargetGame->CloseSlot(SID, CommandHash == HashCode("close"));
      }
      break;
    }

    //
    // !END
    //

    case HashCode("end"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot end the game.");
        break;
      }

      if (0 == (m_Permissions & PERM_BOT_SUDO_OK) && false) {
        // TODO(IceSandslash): Add a time limit.
        ErrorReply("The game has been running for too many minute(s) and can no longer be ended without sudo access.");
        break;
      }

      (*m_Output) << m_TargetGame->GetLogPrefix() + "is over (admin ended game) [" + m_FromName + "]" << std::endl;
      m_TargetGame->StopPlayers("was disconnected (admin ended game)");
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
          SendAll("Download [" + m_TargetGame->GetMapFileName() + "] from <" + m_TargetGame->GetMapSiteURL() + ">");
        }
        break;
      }

      if (!m_TargetGame->GetIsLobby())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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
      if (!m_TargetGame)
        break;

      bool isOwner = (0 != (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE)));
      SendReply(
        "Protect against disconnections using GProxyDLL, a Warcraft III plugin. See: <" + m_Aura->m_Net->m_Config->m_AnnounceGProxySite + ">",
        isOwner ? CHAT_SEND_TARGET_ALL : 0
      );
      break;
    }

    //
    // !HCL
    //

    case HashCode("hcl"): {
      if (!m_TargetGame || m_TargetGame->GetCountDownStarted())
        break;

      if (Payload.empty()) {
        SendReply("The HCL command string is [" + m_TargetGame->m_HCLCommandString + "]");
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (Payload.size() > m_TargetGame->m_Slots.size()) {
        ErrorReply("Unable to set HCL command string because it's too long - it must not exceed the amount of occupied game slots");
        break;
      }

      const string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";
      size_t IllegalIndex = Payload.find_first_not_of(HCLChars);

      if (IllegalIndex != string::npos) {
        ErrorReply("Unable to set HCL command string because it contains invalid character [" + GetToken() + "]");
        break;
      }

      m_TargetGame->m_HCLCommandString = Payload;
      SendAll("Setting game mode to HCL [" + m_TargetGame->m_HCLCommandString + "]");
      break;
    }

    //
    // !HOLD (hold a slot for someone)
    //

    case HashCode("hold"): {
      UseImplicitHostedGame();
      if (!m_TargetGame || !m_TargetGame->GetIsLobby())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 12u);

      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "hold [PLAYER1], [PLAYER2], ...");
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
      if (!m_TargetGame || !m_TargetGame->GetIsLobby())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 12u);

      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "unhold [PLAYER1], [PLAYER2], ...");
        break;
      }

      for (auto& PlayerName: Args) {
        if (PlayerName.empty())
          continue;
        m_TargetGame->RemoveFromReserved(PlayerName);
      }
      SendAll("Removed player(s) from the hold list: " + JoinVector(Args, false));
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
      if (!m_TargetGame || m_TargetGame->GetIsMirror() || m_TargetGame->GetCountDownStarted() && !m_TargetGame->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "kick [PLAYERNAME]");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
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
        if (KickAndClose) {
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
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (Payload.empty()) {
        SendReply("The game latency is " + to_string(m_TargetGame->m_Latency) + " ms");
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      try {
        uint32_t TargetValue = stoul(Payload);
        if (TargetValue <= 0 || TargetValue > 60000) {
          // WC3 clients disconnect after a minute without network activity.
          ErrorReply("Invalid game latency [" + to_string(TargetValue) + "ms].");
          break;
        }
        m_TargetGame->m_Latency = static_cast<uint16_t>(TargetValue);
      } catch (...) {
        ErrorReply("Invalid game latency [" + Payload + "].");
        break;
      }

      if (m_TargetGame->m_Latency <= 10) {
        m_TargetGame->m_Latency = 10;
        SendAll("Setting game latency to the minimum of 10 ms");
      } else if (m_TargetGame->m_Latency >= 500) {
        m_TargetGame->m_Latency = 500;
        SendAll("Setting game latency to the maximum of 500 ms");
      } else {
        SendAll("Setting game latency to " + to_string(m_TargetGame->m_Latency) + " ms");
      }

      break;
    }

    //
    // !OPEN (open slot)
    //

    case HashCode("open"):
    case HashCode("o"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "o [SLOTNUM]");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 12u);
      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "o [SLOTNUM]");
        break;
      }

      for (auto& elem : Args) {
        if (elem == 0 || elem > 12) {
          ErrorReply("Usage: " + GetToken() + "o [SLOTNUM]");
          break;
        }
        uint8_t SID = static_cast<uint8_t>(elem) - 1;
        if (!m_TargetGame->DeleteFakePlayer(SID)) {
          m_TargetGame->OpenSlot(SID, CommandHash == HashCode("open"));
        }
      }
      break;
    }

    //
    // !PUB (create or recreate as public game)
    // !PRIV (create or recreate as private game)
    //

    case HashCode("pub"):
    case HashCode("priv"): {
      if (m_TargetGame) {
        if (!m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted()) {
          break;
        }
        if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
          ErrorReply("You are not the game owner, and therefore cannot rehost the game.");
          break;
        }
      } else {
        if (!(m_SourceRealm && m_SourceRealm->m_Config->m_EnablePublicCreate) && !(m_IRC && m_IRC->m_Config->m_EnablePublicCreate)) {
          if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_IRC_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
            ErrorReply("Not allowed to host games.");
            break;
          }
        }
        if (!m_Aura->m_GameSetup) {
          ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
          break;
        }
        if (Payload.empty()) {
          ErrorReply("Usage: " + GetToken() + "pub [GAMENAME]");
          ErrorReply("Usage: " + GetToken() + "priv [GAMENAME]");
          break;
        }
      }

      if (Payload.length() >= 31) {
        ErrorReply("Unable to create game [" + Payload + "]. The game name is too long (the maximum is 31 characters)");
        break;
      }

      SendReply("Trying to rehost with name [" + Payload + "]. (This will take a few seconds)", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);

      bool IsPrivate = CommandHash == HashCode("priv");
      if (m_TargetGame) {
        m_TargetGame->m_GameDisplay  = IsPrivate ? GAME_PRIVATE : GAME_PUBLIC;
        m_TargetGame->m_LastGameName = m_TargetGame->m_GameName;
        m_TargetGame->m_GameName     = Payload;
        m_TargetGame->m_HostCounter  = m_Aura->NextHostCounter();
        m_TargetGame->m_RefreshError = false;

        for (auto& bnet : m_Aura->m_Realms) {
          if (m_TargetGame->m_IsMirror && bnet->GetIsMirror())
            continue;

          // unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
          // this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
          // we assume this won't happen very often since the only downside is a potential false positive

          bnet->QueueGameUncreate();
          bnet->QueueEnterChat();

          // we need to send the game creation message now because private games are not refreshed

          bnet->QueueGameCreate(m_TargetGame->m_GameDisplay, m_TargetGame->m_GameName, m_TargetGame->m_Map, m_TargetGame->m_HostCounter, m_TargetGame->m_HostPort);

          if (!bnet->GetPvPGN())
            bnet->QueueEnterChat();
        }

        m_TargetGame->m_CreationTime = m_TargetGame->m_LastRefreshTime = GetTime();
      } else {
        if (!m_Aura->m_GameSetup) {
          ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
          break;
        }
        if (m_Aura->m_CurrentLobby) {
          ErrorReply("Already hosting a game.");
          break;
        }
        m_Aura->m_GameSetup->SetContext(this);
        m_Aura->m_GameSetup->SetName(Payload);
        m_Aura->m_GameSetup->SetDisplayMode(IsPrivate ? GAME_PRIVATE : GAME_PUBLIC);
        m_Aura->m_GameSetup->SetCreator(m_FromName, m_SourceRealm);
        m_Aura->m_GameSetup->SetOwner(m_FromName, CommandHash == HashCode("hostlan") ? nullptr : m_SourceRealm);
        m_Aura->m_GameSetup->RunHost();
        delete m_Aura->m_GameSetup;
        m_Aura->m_GameSetup = nullptr;
      }
      break;
    }

    //
    // !PUBBY (create public game by other player)
    // !PRIVBY (create private game by other player)
    //

    case HashCode("pubby"):
    case HashCode("privby"): {
      if (!(m_SourceRealm && m_SourceRealm->m_Config->m_EnablePublicCreate) && !(m_IRC && m_IRC->m_Config->m_EnablePublicCreate)) {
        if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_IRC_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
          ErrorReply("Not allowed to host games.");
          break;
        }
      }
      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      string gameName;
      if (Args.empty() || (gameName = TrimString(Args[1])).empty()) {
        ErrorReply("Usage: " + GetToken() + "pubby [OWNER], [GAMENAME]");
        ErrorReply("Usage: " + GetToken() + "privby [OWNER], [GAMENAME]");
        break;
      }

      if (!m_Aura->m_GameSetup) {
        ErrorReply("A map must be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map first.");
        break;
      }
      if (m_Aura->m_CurrentLobby) {
        ErrorReply("Already hosting a game.");
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
      m_Aura->m_GameSetup->SetName(gameName);
      m_Aura->m_GameSetup->SetDisplayMode(IsPrivate ? GAME_PRIVATE : GAME_PUBLIC);
      m_Aura->m_GameSetup->SetCreator(m_FromName, m_SourceRealm);
      m_Aura->m_GameSetup->SetOwner(ownerName, ownerRealmName.empty() ? m_SourceRealm : m_Aura->GetRealmByInputId(ownerRealmName));
      m_Aura->m_GameSetup->RunHost();
      delete m_Aura->m_GameSetup;
      m_Aura->m_GameSetup = nullptr;
      break;
    }

    //
    // !START
    //

    case HashCode("start"):
    case HashCode("go"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & ((m_TargetGame->HasOwnerSet() && !m_TargetGame->m_PublicStart ? PERM_GAME_OWNER : PERM_GAME_PLAYER) | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not allowed to start this game.");
        break;
      }

      uint32_t ConnectionCount = m_TargetGame->GetNumConnectionsOrFake();
      if (ConnectionCount == 0) {
        ErrorReply("Not enough players have joined.");
        break;
      }

      bool IsForce = Payload == "force";
      if (IsForce && (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE)))) {
        ErrorReply("You are not the game owner, and therefore cannot forcibly start it.");
        break;
      }

      m_TargetGame->StartCountDown(IsForce);

      break;
    }

    //
    // !QUICKSTART
    //

    case HashCode("quickstart"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot quickstart it.");
        break;
      }

      if (GetTicks() - m_TargetGame->m_LastPlayerLeaveTicks < 2000) {
        ErrorReply("Someone left the game less than two seconds ago.");
        break;
      }
      m_TargetGame->StartCountDown(true);
      m_TargetGame->m_CountDownCounter = 1;
      // 500 ms countdown
      break;
    }

    //
    // !FREESTART
    //

    case HashCode("freestart"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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
        SendAll("Anybody may now use the " + GetToken() + "start command.");
      } else {
        SendAll("Only the game owner may now use the " + GetToken() + "start command.");
      }
      break;
    }

    //
    // !AUTOSTART
    //

    case HashCode("autostart"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (Payload.empty()) {
        m_TargetGame->SendAllAutoStart();
        break;
      }

      uint32_t MinSlots = 2;
      uint32_t MinMinutes = 0;
      stringstream SS;
      SS << Payload;
      SS >> MinSlots;

      if (SS.fail()) {
        ErrorReply("Usage: " + GetToken() + "autostart <slots>, <minutes>");
        break;
      }

      if (MinSlots > m_TargetGame->m_Slots.size()) {
        ErrorReply("This map does not allow " + to_string(MinSlots) + " slots.");
        break;
      }

      if (!SS.eof()) {
        SS >> MinMinutes;
      }

      if (MinSlots <= m_TargetGame->m_PlayersWithMap) {
        // Misuse protection. Make sure the user understands AI players are added.
        ErrorReply("There are already " + to_string(m_TargetGame->m_PlayersWithMap) + " players. Use " + GetToken() + "start instead.");
        break;
      }

      int64_t Time = GetTime();
      m_TargetGame->m_AutoStartPlayers = static_cast<uint8_t>(MinSlots);
      m_TargetGame->m_AutoStartMinTime = Time + static_cast<int64_t>(MinMinutes) * 60;
      if (m_TargetGame->m_AutoStartMinTime <= Time)
        m_TargetGame->m_AutoStartMinTime = 0;
      if (m_TargetGame->m_AutoStartMaxTime <= m_TargetGame->m_AutoStartMinTime)
        m_TargetGame->m_AutoStartMaxTime = 0;

      m_TargetGame->SendAllAutoStart();
      break;
    }

    //
    // !FORCEAUTOSTART
    //


    case HashCode("forceautostart"): {
      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "forceautostart <minutes>");
        break;
      }

      uint32_t MaxMinutes = 0;
      stringstream SS;
      SS << Payload;
      SS >> MaxMinutes;

      if (SS.fail() || MaxMinutes == 0) {
        ErrorReply("Usage: " + GetToken() + "forceautostart <minutes>");
        break;
      }

      int64_t Time = GetTime();
      m_TargetGame->m_AutoStartMaxTime = Time + static_cast<int64_t>(MaxMinutes) * 60;
      if (m_TargetGame->m_AutoStartMaxTime <= Time)
        m_TargetGame->m_AutoStartMaxTime = 0;
      if (m_TargetGame->m_AutoStartMinTime >= m_TargetGame->m_AutoStartMaxTime)
        m_TargetGame->m_AutoStartMinTime = 0;

      m_TargetGame->SendAllAutoStart();
      break;
    }

    //
    // !SWAP (swap slots)
    //

    case HashCode("swap"):
    case HashCode("sw"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<uint32_t> Slots = SplitNumericArgs(Payload, 2u, 2u);
      if (Slots.empty()) {
        ErrorReply("Usage: " + GetToken() + "swap <SLOTNUM>, <SLOTNUM>");
        break;
      }

      if (Slots[0] == 0 || Slots[1] == 0 || Slots[0] > 12 || Slots[1] > 12) {
        ErrorReply("Usage: " + GetToken() + "swap <SLOTNUM>, <SLOTNUM>");
        break;
      }

      uint8_t SID1 = static_cast<uint8_t>(Slots[0]) - 1;
      uint8_t SID2 = static_cast<uint8_t>(Slots[1]) - 1;
      m_TargetGame->SwapSlots(SID1, SID2);
      break;
    }

    //
    // !SYNCRESET
    //

    case HashCode("syncreset"):
    case HashCode("sr"): {
      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game settings.");
        break;
      }

      if (m_TargetGame->m_Lagging) {
        ErrorReply("Cannot reset sync counters while there are players lagging.");
        break;
      }

      m_TargetGame->ResetSync();
      SendReply("Sync counters reset.", CHAT_LOG_CONSOLE);
      break;
    }

    //
    // !SYNCLIMIT
    //

    case HashCode("synclimit"):
    case HashCode("sl"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (Payload.empty()) {
        SendReply("Sync limit: " + to_string(m_TargetGame->m_SyncLimit) + " packets.");
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game settings.");
        break;
      }

      uint32_t     Packets;
      stringstream SS;
      SS << Payload;
      SS >> Packets;

      if (SS.fail()) {
        ErrorReply("bad input #1 to !sl command");
        break;
      }

      if (Packets < 2)
        Packets = 2;
      if (Packets > 10000)
        Packets = 10000;

      m_TargetGame->m_SyncLimit = Packets;

      SendAll("Sync limit updated: " + to_string(m_TargetGame->m_SyncLimit) + " packets.");
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | (m_TargetGame->HasOwnerInGame() ? PERM_BNET_ROOTADMIN : PERM_BNET_ADMIN) | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      m_TargetGame->m_Exiting = true;
      SendReply("Aborting " + m_TargetGame->GetDescription());
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      bool IsMapAvailable = !m_TargetGame->m_Map->GetMapData()->empty() && m_TargetGame->m_Map->GetValidLinkedMap();
      if (m_Aura->m_Net->m_Config->m_AllowTransfers == MAP_TRANSFERS_NEVER || !IsMapAvailable) {
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

      const uint8_t SID = m_TargetGame->GetSIDFromPID(targetPlayer->GetPID());

      if (SID >= m_TargetGame->m_Slots.size() || m_TargetGame->m_Slots[SID].GetDownloadStatus() == 100) {
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
      if (!m_TargetGame || m_TargetGame->GetGameLoaded())
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

      if (!m_TargetGame || m_TargetGame->GetIsMirror() || m_TargetGame->GetCountDownStarted() && !m_TargetGame->GetGameLoaded())
        break;

      CGamePlayer* targetPlayer = GetTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }
      if (!targetPlayer->GetObserver()) {
        ErrorReply("[" + targetPlayer->GetName() + "] is not in the observer team.");
        break;
      }

      for (auto& otherPlayer: m_TargetGame->m_Players) {
        if (otherPlayer->GetObserver())
          otherPlayer->SetPowerObserver(false);
      }
      targetPlayer->SetPowerObserver(true);
      SendAll("Player [" + targetPlayer->GetName() + "] was promoted to referee by player [" + m_FromName + "] (Other observers may only use observer chat)");
      break;
    }

    //
    // !MUTE
    //

    case HashCode("mute"): {
      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot mute people.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "mute [PLAYERNAME]");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }
      if (targetPlayer == m_Player) {
        ErrorReply("Cannot mute yourself.");
        break;
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot abort game start.");
        break;
      }

      if (m_TargetGame->GetCountDownStarted()) {
        SendAll("Countdown aborted!");
        m_TargetGame->m_CountDownStarted = false;
      } else {
        m_TargetGame->m_AutoStartMinTime = 0;
        m_TargetGame->m_AutoStartMaxTime = 0;
        m_TargetGame->m_AutoStartPlayers = 0;
      }
      break;
    }

    //
    // !CHECKNETWORK
    //
    case HashCode("checknetwork"): {
      UseImplicitHostedGame();

      uint16_t gamePort = m_TargetGame ? m_TargetGame->GetHostPort() : m_Aura->m_Net->NextHostPort();

      if (0 == (m_Permissions & ((m_TargetGame && m_TargetGame->HasOwnerSet() ? PERM_GAME_OWNER : PERM_GAME_PLAYER) | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to check network status.");
        break;
      }
      const bool TargetAllRealms = Payload == "*" || Payload.empty() && !m_SourceRealm;
      CRealm* targetRealm = nullptr;
      if (!TargetAllRealms) {
        targetRealm = GetTargetRealmOrCurrent(Payload);
        if (!targetRealm) {
          ErrorReply("Usage: " + GetToken() + "checknetwork *");
          ErrorReply("Usage: " + GetToken() + "checknetwork [REALM]");
          break;
        }
      }
      if (m_Aura->m_Net->m_HealthCheckInProgress) {
        ErrorReply("Already testing the network configuration.");
        break;
      }

      vector<tuple<string, uint8_t, sockaddr_storage>> testServers;
      if (TargetAllRealms) {
        sockaddr_storage loopBackAddress;
        memset(&loopBackAddress, 0, sizeof(sockaddr_storage));
        loopBackAddress.ss_family = AF_INET;
        sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(&loopBackAddress);
        addr4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr4->sin_port = htons(gamePort);
        tuple<string, uint8_t, sockaddr_storage> testHost("[Loopback]", CONNECTION_TYPE_LOOPBACK, loopBackAddress);
        testServers.push_back(testHost);
      }
      sockaddr_storage* publicIPv4 = m_Aura->m_Net->GetPublicIPv4();
      sockaddr_storage* publicIPv6 = m_Aura->m_Net->GetPublicIPv6();

      bool anySendsPublicIp = false;
      for (auto& realm : m_Aura->m_Realms) {
        if (!TargetAllRealms && realm != targetRealm) {
          continue;
        }
        if (!realm->GetLoggedIn()) {
          SendReply("[Network] Not connected to " + realm->GetUniqueDisplayName(), CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
          continue;
        }
        sockaddr_storage* selfIPInThisRealm = realm->GetUsesCustomIPAddress() ? realm->GetPublicHostAddress() : publicIPv4;
        if (publicIPv4 == nullptr) {
          continue;
        }
        uint8_t connectionType = CONNECTION_TYPE_DEFAULT;
        uint16_t port = realm->GetUsesCustomPort() ? realm->GetPublicHostPort() : gamePort;
        string NameSuffix;
        if (realm->GetIsVPN()) {
          NameSuffix = " [VPN]";
          connectionType = connectionType | CONNECTION_TYPE_VPN;
        }
        if (realm->GetUsesCustomIPAddress()) {
          if (NameSuffix.empty()) NameSuffix = " [Tunnel]";
          connectionType = connectionType | CONNECTION_TYPE_CUSTOM_IP_ADDRESS;
        }
        if (realm->GetUsesCustomPort()) {
          if (NameSuffix.empty()) NameSuffix = " [Tunnel]";
          connectionType = connectionType | CONNECTION_TYPE_CUSTOM_PORT;
        }

        sockaddr_storage targetHost;
        memcpy(&targetHost, selfIPInThisRealm, sizeof(sockaddr_storage));
        SetAddressPort(&targetHost, port);
        tuple<string, uint8_t, sockaddr_storage> testHost(realm->GetUniqueDisplayName() + NameSuffix, connectionType, targetHost);
        testServers.push_back(testHost);

        if (reinterpret_cast<sockaddr_in*>(selfIPInThisRealm)->sin_addr.s_addr == reinterpret_cast<sockaddr_in*>(publicIPv4)->sin_addr.s_addr) {
          anySendsPublicIp = true;
        }
      }
      if (!anySendsPublicIp && publicIPv4 != nullptr) {
        sockaddr_storage targetHost;
        memcpy(&targetHost, publicIPv4, sizeof(sockaddr_storage));
        SetAddressPort(&targetHost, gamePort);
        tuple<string, uint8_t, sockaddr_storage> testHost("[Public IPv4]", 0, targetHost);
        testServers.push_back(testHost);
      }
      if (publicIPv6 != nullptr && !IN6_IS_ADDR_UNSPECIFIED(&(reinterpret_cast<sockaddr_in6*>(publicIPv6)->sin6_addr))) {
        sockaddr_storage targetHost;
        memcpy(&targetHost, publicIPv6, sizeof(sockaddr_storage));
        SetAddressPort(&targetHost, gamePort);
        tuple<string, uint8_t, sockaddr_storage> testHost("[Public IPv6]", CONNECTION_TYPE_IPV6, targetHost);
        testServers.push_back(testHost);
      }
      if (!m_TargetGame) {
        SendReply("Network check started... Check output in the console.");
      }
      m_Aura->m_Net->StartHealthCheck(testServers, this);
      break;
    }

    //
    // !UPNP
    //

#ifndef DISABLE_MINIUPNP
    case HashCode("upnp"): {
      UseImplicitHostedGame();

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to trigger UPnP.");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1, 2);
      if (Args.size() == 1) {
        if (Args[0] == 0 || Args[0] > 0xFFFF) {
          ErrorReply("Usage: " + GetToken() + "upnp [EXTPORT] [INTPORT]");
          break;
        }
        Args.push_back(Args[0]);
      } else if (Args.empty()) {
        if (!Payload.empty() || !m_TargetGame) {
          ErrorReply("Usage: " + GetToken() + "upnp [EXTPORT] [INTPORT]");
          break;
        }
        Args.push_back(m_TargetGame->GetHostPort());
        Args.push_back(m_TargetGame->GetHostPort());
      } else {
        if (Args[0] == 0 || Args[0] > 0xFFFF || Args[1] == 0 || Args[1] > 0xFFFF) {
          ErrorReply("Usage: " + GetToken() + "upnp [EXTPORT] [INTPORT]");
          break;
        }
      }

      uint16_t extPort = static_cast<uint16_t>(Args[0]);
      uint16_t intPort = static_cast<uint16_t>(Args[1]);

      SendReply("Trying to forward external port " + to_string(extPort) + " to internal port " + to_string(intPort) + "...");
      uint8_t result = m_Aura->m_Net->EnableUPnP(extPort, intPort);
      if (result == 0) {
        ErrorReply("Universal Plug and Play is not supported by the host router.");
      } else if (result == 1) {
        SendReply("Opened port " + to_string(extPort) + " with Universal Plug and Play");
      } else {
        SendReply("Unknown results. Try " + GetToken() + "checknetwork *");
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to check ban status.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "checkban [PLAYERNAME]");
        break;
      }

      vector<string> CheckResult;
      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;

        CDBBan* Ban = m_Aura->m_DB->BanCheck(bnet->GetDataBaseID(), Payload);
        if (Ban) {
          CheckResult.push_back("[" + bnet->GetServer() + "] - banned by [" + Ban->GetAdmin() + "] because [" + Ban->GetReason() + "] (" + Ban->GetDate() + ")");
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
      if (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list bans.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + GetToken() + "listbans [REALM]");
        break;
      }
      if (targetRealm != m_SourceRealm && (0 == (m_Permissions & PERM_BOT_SUDO_OK))) {
        ErrorReply("Not allowed to list bans in arbitrary realms.");
        break;
      }
      // TODO(IceSandslash): Implement !listbans
      ErrorReply("Unimplemented.");
      break;
    }

    //
    // !ADDBAN
    // !BAN
    //

    case HashCode("addban"):
    case HashCode("ban"): {
      if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (m_Aura->m_Realms.empty()) {
        ErrorReply("No realms joined.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "ban [PLAYERNAME]");
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
        transform(begin(VictimLower), end(VictimLower), begin(VictimLower), ::tolower);
        uint32_t Matches   = 0;
        CDBBan*  LastMatch = nullptr;

        // try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")
        // we use the m_DBBans vector for this in case the player already left and thus isn't in the m_Players vector anymore

        for (auto& ban : m_TargetGame->m_DBBans) {
          string TestName = ban->GetName();
          transform(begin(TestName), end(TestName), begin(TestName), ::tolower);

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
          ErrorReply("Player [" + Victim + "] not found.");
          break;
        }
        m_Aura->m_DB->BanAdd(targetPlayer->GetRealmDataBaseID(false), targetPlayer->GetName(), m_FromName, Reason);
        SendAll("Player [" + targetPlayer->GetName() + "] was banned by player [" + m_FromName + "] on server [" + targetPlayer->GetRealmHostName() + "]");
        break;
      } else if (m_SourceRealm) {
        if (m_SourceRealm->GetIsRootAdmin(Victim) || m_SourceRealm->GetIsAdmin(Victim) && (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE)))) {
          ErrorReply("User [" + Victim + "] is an admin on server [" + m_HostName + "]");
          break;
        }

        if (m_SourceRealm->IsBannedName(Victim)) {
          ErrorReply("User [" + Victim + "] is already banned on server [" + m_HostName + "]");
          break;
        }
        if (m_SourceRealm->GetIsAdmin(Victim)) {
          if (!m_Aura->m_DB->AdminRemove(m_SourceRealm->GetDataBaseID(), Victim)) {
            ErrorReply("Failed to ban user [" + Victim + "] on server [" + m_HostName + "]");
            break;
          }
        }
        if (!m_Aura->m_DB->BanAdd(m_SourceRealm->GetDataBaseID(), Victim, m_FromName, Reason)) {
          ErrorReply("Failed to ban user [" + Victim + "] on server [" + m_HostName + "]");
          break;
        }
        SendAll("User [" + Victim + "] banned on server [" + m_HostName + "]");
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
      if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (m_Aura->m_Realms.empty()) {
        ErrorReply("No realms joined.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "unban [PLAYERNAME]");
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
      if (!m_TargetGame || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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
        message += "[" + bnet->GetServer() + "]" + (bnet->GetLoggedIn() ? " - online " : " - offline ");

      if (m_Aura->m_IRC)
        message += m_Aura->m_IRC->m_Config->m_HostName + (!m_Aura->m_IRC->m_WaitingToConnect ? " [online]" : " [offline]");

      SendReply(message);
      break;
    }

    //
    // !SENDLAN
    //

    case HashCode("sendudp"):
    case HashCode("sendlan"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetCountDownStarted()) {
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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
          ErrorReply("Usage: " + GetToken() + "sendlan ON/OFF");
          ErrorReply("Usage: " + GetToken() + "sendlan [IP]");
          break;
        }
        sockaddr_storage* address = &(maybeAddress.value());
        if (GetInnerIPVersion(address) == AF_INET6 && !(m_Aura->m_Net->m_SupportUDPOverIPv6 && m_Aura->m_Net->m_SupportTCPOverIPv6)) {
          ErrorReply("IPv6 support hasn't been enabled. Set <net.ipv6.tcp.enabled = yes>, and <net.udp_ipv6.enabled = yes> if you want to enable it.");
          break;
        }
        if (address->ss_family == AF_INET6 && isSpecialIPv6Address(reinterpret_cast<struct sockaddr_in6*>(address)) ||
          address->ss_family == AF_INET && isSpecialIPv4Address(reinterpret_cast<struct sockaddr_in*>(address))) {
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

    case HashCode("sendudpinfo"):
    case HashCode("sendlaninfo"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetCountDownStarted()) {
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (!Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "sendlaninfo");
        ErrorReply("You may want !sendlan [IP] or !sendlan on/off instead");
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

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & ((m_TargetGame->HasOwnerSet() ? PERM_GAME_OWNER : PERM_GAME_PLAYER) | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        if (Payload.empty() && m_TargetGame->HasOwnerSet()) {
          SendReply("The owner is [" + m_TargetGame->m_OwnerName + "@" + (m_TargetGame->m_OwnerRealm.empty() ? "@@LAN/VPN" : m_TargetGame->m_OwnerRealm) + "]");
        }
        ErrorReply("You are not allowed to change the owner of this game.");
        break;
      }

      CGamePlayer* TargetPlayer = GetTargetPlayerOrSelf(Payload);
      string TargetName = TargetPlayer ? TargetPlayer->GetName() : Payload;
      if (TargetName.empty()) {
        ErrorReply("Usage: " + GetToken() + "owner [PLAYERNAME]");
        break;
      }
      string TargetRealm = TargetPlayer ? TargetPlayer->GetRealmHostName() : m_HostName;
      if (m_TargetGame->m_OwnerName == TargetName && m_TargetGame->m_OwnerRealm == TargetRealm) {
        SendAll(TargetName + "@" + (TargetRealm.empty() ? "@@LAN/VPN" : TargetRealm) + " is already the owner of this game.");
      } else {
        m_TargetGame->SetOwner(TargetName, TargetRealm);
        SendReply("Setting game owner to [" + TargetName + "@" + (TargetRealm.empty() ? "@@LAN/VPN" : TargetRealm) + "]", CHAT_SEND_TARGET_ALL | CHAT_LOG_CONSOLE);
      }
      if (TargetPlayer) TargetPlayer->SetWhoisShouldBeSent(true);
      break;
    }

    //
    // !SAY
    //

    case HashCode("say"): {
      if (m_Aura->m_Realms.empty())
        break;

      if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not allowed to advertise.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "say [REALM], [MESSAGE]");
        break;
      }

      if (m_Aura->m_CurrentLobby) {
        ErrorReply("Cannot send bnet chat messages while the bot is hosting a game lobby.");
        break;
      }

      string::size_type MessageStart = Payload.find(',');
      string RealmId = TrimString(Payload.substr(0, MessageStart));
      string Message = TrimString(Payload.substr(MessageStart + 1));
      if (Message.empty()) {
        ErrorReply("Usage: " + GetToken() + "say [REALM], [MESSAGE]");
        break;
      }
      bool IsCommand = Message[0] == '/';
      if (IsCommand && (0 == (m_Permissions & PERM_BOT_SUDO_OK))) {
        ErrorReply("You are not allowed to send bnet commands.");
        break;
      }
      transform(begin(RealmId), end(RealmId), begin(RealmId), ::tolower);

      const bool ToAllRealms = RealmId.length() == 1 && RealmId[0] == '*';
      bool Success = false;

      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;
        if (ToAllRealms || bnet->GetInputID() == RealmId) {
          if (IsCommand) {
            bnet->SendCommand(Message);
          } else {
            bnet->SendChatChannel(Message);
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

      if (m_TargetGame->m_GameDisplay == GAME_PRIVATE) {
        ErrorReply("This game is private.");
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot advertise.");
        break;
      }

      bool ToAllRealms = Payload == "*";
      CRealm* targetRealm = nullptr;
      if (ToAllRealms) {
        if (0 != (m_Permissions & PERM_BOT_SUDO_SPOOFABLE)) {
          ErrorReply("Announcing on all realms requires sudo permissions."); // But not really
          break;
        }
      } else {
        targetRealm = GetTargetRealmOrCurrent(Payload);
        if (!targetRealm) {
          ErrorReply("Usage: " + GetToken() + "announce [REALM]");
          break;
        }
      }

      if (ToAllRealms) {
        for (auto& bnet : m_Aura->m_Realms) {
          string AnnounceText = (
            string("[1.") + to_string(m_Aura->m_GameVersion) + ".x] Hosting public game of " + m_TargetGame->GetMapFileName() +
          ". (Started by " + m_TargetGame->m_OwnerName + ": \"" + bnet->GetPrefixedGameName(m_TargetGame->m_GameName) + "\")"
          );
          bnet->QueueGameUncreate();
          bnet->QueueEnterChat();
          bnet->SendChatChannel(AnnounceText);
        }
      } else {
        string AnnounceText = (
            string("[1.") + to_string(m_Aura->m_GameVersion) + ".x] Hosting public game of " + m_TargetGame->GetMapFileName() +
          ". (Started by " + m_TargetGame->m_OwnerName + ": \"" + targetRealm->GetPrefixedGameName(m_TargetGame->m_GameName) + "\")"
        );
        targetRealm->QueueGameUncreate();
        targetRealm->QueueEnterChat();
        targetRealm->SendChatChannel(AnnounceText);
      }

      SendReply("Announcement sent.");
      break;
    }

    //
    // !CLOSEALL
    //

    case HashCode("closeall"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      m_TargetGame->CloseAllSlots();
      break;
    }

    //
    // !COMP (computer slot)
    //

    case HashCode("comp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "comp [SLOT], [SKILL] - Skill is any of: 0, 1, 2");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "comp [SLOT], [SKILL] - Skill is any of: 0, 1, 2");
        break;
      }

      if (Args[0] == 0 || Args[0] > m_TargetGame->m_Slots.size() || Args[1] > 2) {
        ErrorReply("Usage: " + GetToken() + "comp [SLOT], [SKILL] - Skill is any of: 0, 1, 2");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Args[0]) - 1;
      uint8_t Skill = static_cast<uint8_t>(Args[1]);

      m_TargetGame->DeleteFakePlayer(SID);
      m_TargetGame->ComputerSlot(SID, Skill, false);
      break;
    }

    //
    // !COMPCOLOR (computer colour change)
    //

    case HashCode("compcolour"):
    case HashCode("compcolor"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "compcolor [SLOT], [COLOR] - Color goes from 0 to 12");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "compcolor [SLOT], [COLOR] - Color goes from 0 to 12");
        break;
      }

      if (Args[0] == 0 || Args[0] > m_TargetGame->m_Slots.size() || Args[1] >= 12) {
        ErrorReply("Usage: " + GetToken() + "compcolor [SLOT], [COLOR] - Color goes from 0 to 12");
        break;
      }

      if (m_TargetGame->m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Args[0]) - 1;
      uint8_t Color = static_cast<uint8_t>(Args[1]);

      if (m_TargetGame->m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED || m_TargetGame->m_Slots[SID].GetComputer() != 1) {
        ErrorReply("There is no computer in slot " + to_string(Args[0]));
        break;
      }

      m_TargetGame->ColorSlot(SID, Color);
      break;
    }

    //
    // !CHANDICAP (handicap change)
    //

    case HashCode("handicap"):
    case HashCode("sethandicap"):
    case HashCode("comphandicap"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "comphandicap [SLOT], [HANDICAP] - Handicap is percent 50/60/70/80/90/100");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "comphandicap [SLOT], [HANDICAP] - Handicap is percent 50/60/70/80/90/100");
        break;
      }

      if (Args[0] == 0 || Args[0] > m_TargetGame->m_Slots.size() || Args[1] % 10 != 0 || !(50 <= Args[1] && Args[1] <= 100)) {
        ErrorReply("Usage: " + GetToken() + "comphandicap [SLOT], [HANDICAP] - Handicap is percent 50/60/70/80/90/100");
        break;
      }

      if (m_TargetGame->m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Args[0]) - 1;
      uint8_t Handicap = static_cast<uint8_t>(Args[1]);

      if (m_TargetGame->m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + to_string(Args[0]) + " is empty.");
        break;
      }

      m_TargetGame->m_Slots[SID].SetHandicap(Handicap);
      m_TargetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      break;
    }

    //
    // !COMPRACE (computer race change)
    //

    case HashCode("comprace"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
        break;
      }

      uint32_t Slot = 0;
      try {
        Slot = stoul(Args[0]);
        if (Slot == 0 || Slot > m_TargetGame->m_Slots.size()) {
          ErrorReply("Usage: " + GetToken() + "comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
          break;
        }
      } catch (...) {
        ErrorReply("Usage: " + GetToken() + "comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
        break;
      }

      if (Args[1] != "random" && Args[1] != "human" && Args[1] != "orc" && Args[1] != "undead" && Args[1] != "elf" && Args[1] != "nightelf" && Args[1] != "night elf") {
        ErrorReply("Usage: " + GetToken() + "comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Slot) - 1;
      string Race = Args[1];

      if (m_TargetGame->m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      if (m_TargetGame->m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES) {
        ErrorReply("This game has Random Races enabled.");
        break;
      }

      if (Race == "human") {
        m_TargetGame->m_Slots[SID].SetRace(SLOTRACE_HUMAN | SLOTRACE_SELECTABLE);
      } else if (Race == "orc") {
        m_TargetGame->m_Slots[SID].SetRace(SLOTRACE_ORC | SLOTRACE_SELECTABLE);
      } else if (Race == "elf" || Race == "nightelf" || Race == "night elf") {
        m_TargetGame->m_Slots[SID].SetRace(SLOTRACE_NIGHTELF | SLOTRACE_SELECTABLE);
      } else if (Race == "undead") {
        m_TargetGame->m_Slots[SID].SetRace(SLOTRACE_UNDEAD | SLOTRACE_SELECTABLE);
      } else if (Race == "random") {
        m_TargetGame->m_Slots[SID].SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
        
      }
      m_TargetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      break;
    }

    //
    // !CHANGETEAM (forced team change)
    //

    case HashCode("setteam"):
    case HashCode("compteam"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "compteam [SLOT], [TEAM]");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: " + GetToken() + "compteam [SLOT], [TEAM]");
        break;
      }

      if (Args[0] == 0 || Args[0] > m_TargetGame->m_Slots.size() || Args[1] >= 14) {
        ErrorReply("Usage: " + GetToken() + "compteam [SLOT], [TEAM]");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Args[0]) - 1;
      uint8_t Team = static_cast<uint8_t>(Args[1]) - 1;

      if (m_TargetGame->m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      if (m_TargetGame->m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
        ErrorReply("This map has Custom Forces enabled.");
        break;
      }

      m_TargetGame->m_Slots[SID].SetTeam(Team);
      m_TargetGame->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      break;
    }

    //
    // !FILL (fill all open slots with computers)
    //

    case HashCode("compall"):
    case HashCode("fill"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 1u);
      if (Payload.empty()) Args.push_back(2);

      if (Args.empty() || Args[0] > 2) {
        ErrorReply("Usage: " + GetToken() + "fill [SKILL] - Skill is any of: 0, 1, 2");
        break;
      }

      uint8_t Skill = static_cast<uint8_t>(Args[0]);
      m_TargetGame->ComputerAllSlots(Skill);
      break;
    }

    //
    // !FAKEPLAYER
    //

    case HashCode("fakeplayer"):
    case HashCode("fp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (!m_TargetGame->CreateFakePlayer(false)) {
        ErrorReply("Cannot add another fake player");
        break;
      }

      break;
    }

    //
    // !DELETEFAKE

    case HashCode("deletefake"):
    case HashCode("deletefakes"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (m_TargetGame->m_FakePlayers.empty()) {
        ErrorReply("No fake players found.");
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

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      bool Success = false;
      while (m_TargetGame->CreateFakePlayer(false)) {
        Success = true;
      }

      if (!Success) {
        ErrorReply("Cannot add another fake player");
      }
      break;
    }

    //
    // !PAUSE
    //

    case HashCode("pause"):
    case HashCode("fppause"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot pause the game.");
        break;
      }

      if (m_TargetGame->m_FakePlayers.empty()) {
        ErrorReply("Fake player not found.");
        break;
      }

      vector<uint8_t> CRC, Action;
      Action.push_back(1);
      // Randomize because each fake player has up to 3 chances to pause the game.
      m_TargetGame->m_Actions.push(new CIncomingAction(m_TargetGame->m_FakePlayers[rand() % m_TargetGame->m_FakePlayers.size()], CRC, Action));
      break;
    }

    //
    // !RESUME
    //

    case HashCode("resume"):
    case HashCode("fpresume"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot resume the game.");
        break;
      }

      if (m_TargetGame->m_FakePlayers.empty()) {
        ErrorReply("Fake player not found.");
        break;
      }

      vector<uint8_t> CRC, Action;
      Action.push_back(2);
      m_TargetGame->m_Actions.push(new CIncomingAction(m_TargetGame->m_FakePlayers[m_TargetGame->m_FakePlayers.size() - 1], CRC, Action));
      break;
    }

    //
    // !SP
    //

    case HashCode("sp"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      SendAll("Shuffling players");
      m_TargetGame->ShuffleSlots();
      break;
    }

    //
    // !LOCK
    //

    case HashCode("lock"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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

      if (!m_TargetGame || !m_TargetGame->GetIsLobby() || m_TargetGame->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot unmute people.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "unmute [PLAYERNAME]");
        break;
      }
      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }
      if (targetPlayer == m_Player) {
        ErrorReply("Cannot unmute yourself.");
        break;
      }
      if (!targetPlayer->GetMuted()) {
        ErrorReply("Player [" + targetPlayer->GetName() + "] is not muted.");
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot send whispers.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "w [PLAYERNAME]@[REALM], [MESSAGE]");
        break;
      }

      string::size_type MessageStart = Payload.find(',');

      if (MessageStart == string::npos) {
        ErrorReply("Usage: " + GetToken() + "w [PLAYERNAME]@[REALM], [MESSAGE]");
        break;
      }

      string Name = TrimString(Payload.substr(0, MessageStart));
      string SubMessage = TrimString(Payload.substr(MessageStart + 1));
      string TargetRealm;
      if (Name.empty() || SubMessage.empty()) {
        ErrorReply("Usage: " + GetToken() + "w [PLAYERNAME]@[REALM], [MESSAGE]");
        break;
      }

      string::size_type RealmStart = Name.find('@');
      if (RealmStart != string::npos) {
        TargetRealm = TrimString(Name.substr(RealmStart + 1));
        Name = TrimString(Name.substr(0, RealmStart));
      }

      if (Name.empty()) {
        ErrorReply("Usage: " + GetToken() + "w [PLAYERNAME]@[REALM], [MESSAGE]");
        break;
      }

      bool ToAllRealms = TargetRealm.empty() || TargetRealm.length() == 1 && TargetRealm[0] == '*';

      string Message = m_FromName + " at " + (m_HostName.empty() ? "LAN/VPN" : m_HostName) + " says to you: \"" + SubMessage + "\"";
      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;
        if (ToAllRealms || bnet->GetInputID() == TargetRealm) {
          bnet->SendWhisper(Message, Name);
        }
      }

      break;
    }
    //
    // !WHOIS
    //

    case HashCode("whois"): {
      UseImplicitHostedGame();

      if (!m_TargetGame || m_TargetGame->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot requires /whois check.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "whois [PLAYERNAME]");
        break;
      }

      string Name = Payload;
      string TargetRealm;

      string::size_type RealmStart = Name.find('@');
      if (RealmStart != string::npos) {
        TargetRealm = TrimString(Name.substr(RealmStart + 1));
        Name = TrimString(Name.substr(0, RealmStart));
      }

      if (Name.empty()) {
        ErrorReply("Usage: " + GetToken() + "whois [PLAYERNAME]");
        break;
      }

      bool ToAllRealms = TargetRealm.empty() || TargetRealm.length() == 1 && TargetRealm[0] == '*';
      const string Message = "/whois " + Name;

      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet->GetIsMirror())
          continue;
        if (ToAllRealms || bnet->GetInputID() == TargetRealm) {
          bnet->SendCommand(Message);
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

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot send whispers.");
        break;
      }

      if (Payload.empty() || Payload.length() > 15) {
        ErrorReply("Usage: " + GetToken() + "virtualhost [PLAYERNAME]");
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

      if (0 == (m_Permissions & (PERM_BOT_SUDO_OK))) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      m_SourceRealm->SendGetClanList();
      SendReply("Fetching clan member list from " + m_HostName + "...");
      break;
    }

    // !GETFRIENDS
    case HashCode("getfriends"): {
      if (!m_SourceRealm)
        break;

      if (0 == (m_Permissions & (PERM_BOT_SUDO_OK))) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      m_SourceRealm->SendGetFriendsList();
      SendReply("Fetching friends list from " + m_HostName + "...");
      break;
    }

    case HashCode("game"): {
      if (0 == (m_Permissions & PERM_BOT_SUDO_OK)) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      size_t CommandStart = Payload.find(',');
      if (CommandStart == string::npos) {
        ErrorReply("Usage: " + GetToken() + "game [GAMEID], [COMMAND] - See game ids with !listgames");
        break;
      }
      string GameId = TrimString(Payload.substr(0, CommandStart));
      string ExecString = TrimString(Payload.substr(CommandStart + 1));
      if (GameId.empty() || ExecString.empty()) {
        ErrorReply("Usage: " + GetToken() + "game [GAMEID], [COMMAND] - See game ids with !listgames");
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
        ctx = new CCommandContext(m_Aura, targetGame, m_IRC, m_ChannelName, m_FromName, m_FromWhisper, &std::cout, m_Token);
      } else if (m_SourceRealm) {
        ctx = new CCommandContext(m_Aura, targetGame, m_SourceRealm, m_FromName, m_FromWhisper, &std::cout, m_Token);
      } else {
        ctx = new CCommandContext(m_Aura, targetGame, &std::cout, m_Token);
      }
      ctx->Run(SubCmd, SubPayload);
      m_Aura->UnholdContext(ctx);
      break;
    }

    case HashCode("synccfg"):
    case HashCode("initcfg"): {
      if (0 == (m_Permissions & (PERM_BOT_SUDO_OK))) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      vector<filesystem::path> AllMaps = FilesMatch(m_Aura->m_Config->m_MapPath, FILE_EXTENSIONS_MAP);
      int counter = 0;
      
      for (const auto& fileName : AllMaps) {
        string nameString = PathToString(fileName);
        if (nameString.empty()) continue;
        if (CommandHash != HashCode("synccfg") && m_Aura->m_CachedMaps.find(nameString) != m_Aura->m_CachedMaps.end()) {
          continue;
        }
        if (nameString.find(Payload) == string::npos) {
          continue;
        }
        if (!FileExists(m_Aura->m_Config->m_MapPath / fileName)) {
          continue;
        }

        CConfig MapCFG;
        MapCFG.Set("cfg_partial", "1");
        MapCFG.Set("map_path", R"(Maps\Download\)" + nameString);
        MapCFG.Set("map_localpath", nameString);
        if (nameString.find("_evrgrn32") != string::npos) {
          MapCFG.Set("map_site", "https://www.hiveworkshop.com/threads/351924/");
        } else {
          MapCFG.Set("map_site", "");
        }
        MapCFG.Set("map_url", "");
        if (nameString.find("_evrgrn32") != string::npos) {
          MapCFG.Set("map_shortdesc", "This map uses Warcraft 3: Reforged game mechanics.");
        } else {
          MapCFG.Set("map_shortdesc", "");
        }
        MapCFG.Set("downloaded_by", m_FromName);

        if (nameString.find("DotA") != string::npos)
          MapCFG.Set("map_type", "dota");

        CMap* ParsedMap = new CMap(m_Aura, &MapCFG);
        delete ParsedMap;

        string CFGName = "local-" + nameString + ".cfg";
        filesystem::path CFGPath = m_Aura->m_Config->m_MapCFGPath / filesystem::path(CFGName);

        vector<uint8_t> OutputBytes = MapCFG.Export();
        FileWrite(CFGPath, OutputBytes.data(), OutputBytes.size());
        m_Aura->m_CachedMaps[nameString] = CFGName;
        counter++;
      }
      SendReply("Initialized " + to_string(counter) + " map config files.");
      return;
    }

    //
    // !COUNTMAPS
    // !COUNTCFGS
    //

    case HashCode("countmaps"):
    case HashCode("countcfgs"): {
      if (0 == (m_Permissions & (PERM_BOT_SUDO_OK))) {
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
      if (0 == (m_Permissions & (PERM_BOT_SUDO_OK))) {
        ErrorReply("Requires sudo permissions.");
        break;
      }
      if (Payload.empty())
        return;

      string DeletionType = CommandHash == HashCode("deletecfg") ? "cfg" : "map";
      filesystem::path Folder = DeletionType == "cfg" ? m_Aura->m_Config->m_MapCFGPath : m_Aura->m_Config->m_MapPath;

      if (DeletionType == "cfg" && !IsValidCFGName(Payload) || DeletionType == "map" &&!IsValidMapName(Payload)) {
        ErrorReply("Removal failed");
        break;
      }
      filesystem::path FileFragment = Payload;
      if (FileFragment.is_absolute()) {
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
      if (0 == (m_Permissions & (PERM_BOT_SUDO_OK))) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      string::size_type MessageStart = Payload.find(',');
      if (MessageStart == string::npos) {
        ErrorReply("Usage: " + GetToken() + "saygame [GAMEID], [MESSAGE]");
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
      if (0 == (m_Permissions & (PERM_BOT_SUDO_OK))) {
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
      if (0 == (m_Permissions & (PERM_BOT_SUDO_OK))) {
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
      if (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may toggle public game creation.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + GetToken() + "disablepub [REALM]");
        break;
      }
      if (targetRealm != m_SourceRealm && (0 == (m_Permissions & PERM_BOT_SUDO_OK))) {
        ErrorReply("Not allowed to toggle game creation in arbitrary realms.");
        break;
      }
      SendReply("Creation of new games has been temporarily disabled for non-admins at " + targetRealm->GetCanonicalDisplayName() + ". (Active lobbies will not be unhosted.)");
      targetRealm->m_Config->m_EnablePublicCreate = false;
      break;
    }

    //
    // !ENABLEPUB
    //

    case HashCode("enablepub"): {
      if (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may toggle public game creation.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + GetToken() + "enablepub [REALM]");
        break;
      }
      if (targetRealm != m_SourceRealm && (0 == (m_Permissions & PERM_BOT_SUDO_OK))) {
        ErrorReply("Not allowed to toggle game creation in arbitrary realms.");
        break;
      }
      SendReply("Creation of new games has been enabled for non-admins at " + targetRealm->GetCanonicalDisplayName());
      targetRealm->m_Config->m_EnablePublicCreate = true;
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

      if (0 == (m_Permissions & PERM_BOT_SUDO_OK)) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      optional<uint32_t> TargetValue;
      try {
        TargetValue = stoul(Payload);
      } catch (...) {
      }

      if (!TargetValue.has_value()) {
        ErrorReply("Usage: " + GetToken() + "maptransfers [MODE]: Mode is 0/1/2.");
        break;
      }

      if (TargetValue.value() != MAP_TRANSFERS_NEVER && TargetValue.value() != MAP_TRANSFERS_AUTOMATIC && TargetValue.value() != MAP_TRANSFERS_MANUAL) {
        ErrorReply("Usage: " + GetToken() + "maptransfers [MODE]: Mode is 0/1/2.");
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
      if (0 == (m_Permissions & PERM_BOT_SUDO_OK)) {
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
      if (0 == (m_Permissions & PERM_BOT_SUDO_OK)) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      if (m_Aura->m_CurrentLobby || !m_Aura->m_Games.empty()) {
        if (Payload != "force") {
          ErrorReply("At least one game is in the lobby or in progress. Use '!exit force' to shutdown anyway");
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
      if (0 == (m_Permissions & PERM_BOT_SUDO_OK)) {
        ErrorReply("Requires sudo permissions.");
        break;
      }

      if (m_Aura->m_CurrentLobby || !m_Aura->m_Games.empty()) {
        if (Payload != "force") {
          ErrorReply("At least one game is in the lobby or in progress. Use '!restart force' to restart anyway");
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
    // !CHECKADMIN
    //

    case HashCode("checkadmin"): {
      if (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list admins.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "checkadmin [NAME]");
        break;
      }

      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      bool IsRootAdmin = m_SourceRealm->GetIsRootAdmin(Payload);
      bool IsAdmin = IsRootAdmin || m_SourceRealm->GetIsAdmin(Payload);
      if (!IsAdmin && !IsRootAdmin)
        SendReply("User [" + Payload + "] is not an admin on server [" + m_HostName + "]");
      else if (IsRootAdmin)
        SendReply("User [" + Payload + "] is a root admin on server [" + m_HostName + "]");
      else
        SendReply("User [" + Payload + "] is an admin on server [" + m_HostName + "]");

      break;
    }

    //
    // !LISTADMINS
    //

    case HashCode("listadmins"): {
      if (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list admins.");
        break;
      }
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + GetToken() + "listadmins [REALM]");
        break;
      }
      if (targetRealm != m_SourceRealm && (0 == (m_Permissions & PERM_BOT_SUDO_OK))) {
        ErrorReply("Not allowed to list admins in arbitrary realms.");
        break;
      }
      // TODO(IceSandslash): Implement !listadmins
      ErrorReply("Unimplemented.");
      break;
    }

    //
    // !ADDADMIN
    //

    case HashCode("addadmin"): {
      if (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may add admins.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "addadmin [NAME]");
        break;
      }
      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (m_SourceRealm->GetIsAdmin(Payload)) {
        ErrorReply("User [" + Payload + "] is already an admin on server [" + m_HostName + "]");
        break;
      }
      if (!m_Aura->m_DB->AdminAdd(m_SourceRealm->GetDataBaseID(), Payload)) {
        ErrorReply("Failed to add user [" + Payload + "] as admin [" + m_HostName + "]");
        break;
      }
      SendReply("Added user [" + Payload + "] to the admin database on server [" + m_HostName + "]");
      break;
    }

    //
    // !DELADMIN
    //

    case HashCode("deladmin"): {
      if (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Only root admins may list admins.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "deladmin [NAME]");
        break;
      }

      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      if (!m_SourceRealm->GetIsAdmin(Payload)) {
        ErrorReply("User [" + Payload + "] is not an admin on server [" + m_HostName + "]");
        break;
      }
      if (!m_Aura->m_DB->AdminRemove(m_SourceRealm->GetDataBaseID(), Payload)) {
        ErrorReply("Error deleting user [" + Payload + "] from the admin database on server [" + m_HostName + "]");
        break;
      }
      SendReply("Deleted user [" + Payload + "] from the admin database on server [" + m_HostName + "]");
      break;
    }

    /*********************
     * ADMIN COMMANDS *
     *********************/

    //
    // !LOAD (load config file)
    //

    case HashCode("load"): {
      if (Payload.empty()) {
        if (!m_Aura->m_GameSetup) {
          SendReply("There is no map/config file loaded.");
          break;
        }
        SendReply("The currently loaded map/config file is: [" + m_Aura->m_GameSetup->GetInspectName() + "]");
        break;
      }

      if (!FileExists(m_Aura->m_Config->m_MapCFGPath)) {
        ErrorReply("Map config path doesn't exist", CHAT_LOG_CONSOLE);
        break;
      }

      CGameSetup* gameSetup = new CGameSetup(m_Aura, this, Payload, SEARCH_TYPE_ONLY_CONFIG, SETUP_PROTECT_ARBITRARY_TRAVERSAL, false);
      if (!gameSetup) {
        ErrorReply("Unable to host game");
        break;
      }
      if (!gameSetup->LoadMap()) {
        delete gameSetup;
        ErrorReply("No map configs found with that name.");
        break;
      }
      gameSetup->SetActive();
      break;
    }

    //
    // !FLUSHDNS
    //

    case HashCode("flushdns"): {
      if (0 == (m_Permissions & PERM_BOT_SUDO_OK)) {
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
        ErrorReply("Usage: " + GetToken() + "netinfo [REALM]");
        break;
      }
      if (0 == (m_Permissions & PERM_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Requires sudo permissions."); // But not really
        break;
      }

      targetRealm->SendCommand("/netinfo");
      break;
    }

    //
    // !PRINTGAMES
    //

    case HashCode("printgames"): {
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + GetToken() + "printgames [REALM]");
        break;
      }
      if (0 == (m_Permissions & PERM_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Requires sudo permissions."); // But not really
        break;
      }

      targetRealm->SendCommand("/games");
      break;
    }

    //
    // !QUERYGAMES
    //

    case HashCode("querygames"): {
      CRealm* targetRealm = GetTargetRealmOrCurrent(Payload);
      if (!targetRealm) {
        ErrorReply("Usage: " + GetToken() + "querygames [REALM]");
        break;
      }
      if (0 == (m_Permissions & PERM_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Requires sudo permissions."); // But not really
        break;
      }

      int64_t Time = GetTime();
      if (Time - targetRealm->m_LastGameListTime >= 30) {
        targetRealm->QueuePacket(targetRealm->m_Protocol->SEND_SID_GETADVLISTEX());
      }
      break;
    }

    // !CHANNEL (change channel)
    //

    case HashCode("channel"): {
      if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to invite the bot to another channel.");
        break;
      }
      if (m_Aura->m_CurrentLobby)  {
        ErrorReply("Cannot join a chat channel while hosting a lobby.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "channel [CHANNEL]");
        break;
      }
      if (!m_SourceRealm) {
        ErrorReply("Realm not found.");
        break;
      }
      m_SourceRealm->SendCommand("/join " + Payload);
      break;
    }

    //
    // !LISTGAMES
    //

    case HashCode("listgames"):
    case HashCode("getgames"): {
      if (0 == (m_Permissions & (PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
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
    case HashCode("hostlan"):
    case HashCode("host"): {
      if (!(m_SourceRealm && m_SourceRealm->m_Config->m_EnablePublicCreate) && !(m_IRC && m_IRC->m_Config->m_EnablePublicCreate)) {
        if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_IRC_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
          ErrorReply("Not allowed to host games.");
          break;
        }
      }

      bool isHostCommand = CommandHash == HashCode("host") || CommandHash == HashCode("hostlan");
      vector<string> Args = SplitArgs(Payload, 1 + isHostCommand, 5 + isHostCommand);

      if (Args.empty() || Args[0].empty() || isHostCommand && Args[Args.size() - 1].empty()) {
        if (isHostCommand) {
          ErrorReply("Usage: " + GetToken() + "host [MAP NAME], [GAME NAME]");
          ErrorReply("Usage: " + GetToken() + "host [MAP NAME], [OBSERVERS], [GAME NAME]");
          ErrorReply("Usage: " + GetToken() + "host [MAP NAME], [OBSERVERS], [VISIBILITY], [GAME NAME]");
          ErrorReply("Usage: " + GetToken() + "host [MAP NAME], [OBSERVERS], [VISIBILITY], [RANDOM RACES], [GAME NAME]");
          ErrorReply("Usage: " + GetToken() + "host [MAP NAME], [OBSERVERS], [VISIBILITY], [RANDOM RACES], [RANDOM HEROES], [GAME NAME]");
          break;
        }
        if (!m_Aura->m_GameSetup) {
          SendReply("There is no map/config file loaded.", CHAT_SEND_SOURCE_ALL);
          break;
        }
        SendReply("The currently loaded map/config file is: [" + m_Aura->m_GameSetup->GetInspectName() + "]", CHAT_SEND_SOURCE_ALL);
        break;
      }

      CGameExtraOptions options;
      if (Args.size() >= 2 + isHostCommand) options.ParseMapObservers(Args[1]);
      if (Args.size() >= 3 + isHostCommand) options.ParseMapVisibility(Args[2]);
      if (Args.size() >= 4 + isHostCommand) options.ParseMapRandomRaces(Args[3]);
      if (Args.size() >= 5 + isHostCommand) options.ParseMapRandomHeroes(Args[4]);

      CGameSetup* gameSetup = new CGameSetup(m_Aura, this, Args[0], SEARCH_TYPE_ANY, SETUP_PROTECT_ARBITRARY_TRAVERSAL, isHostCommand /* lucky mode */);
      if (!gameSetup) {
        ErrorReply("Unable to host game");
        break;
      }
      if (!gameSetup->LoadMap()) {
        ErrorReply("Map not found");
        delete gameSetup;
        break;
      }
      if (!gameSetup->ApplyMapModifiers(&options)) {
        ErrorReply("Invalid map options. Map has fixed player settings.");
        delete gameSetup;
        break;
      }
      gameSetup->SetActive();
      if (isHostCommand) {
        if (m_Aura->m_CurrentLobby)  {
          ErrorReply("Already hosting a game.");
          delete gameSetup;
          break;
        }
        m_Aura->m_GameSetup->SetName(Args[Args.size() - 1]);
        m_Aura->m_GameSetup->SetCreator(m_FromName, m_SourceRealm);
        m_Aura->m_GameSetup->SetOwner(m_FromName, CommandHash == HashCode("hostlan") ? nullptr : m_SourceRealm);
        m_Aura->m_GameSetup->RunHost();
        delete m_Aura->m_GameSetup;
        m_Aura->m_GameSetup = nullptr;
      }
      break;
    }

    //
    // !MIRROR (mirror game from another server)
    //

    case HashCode("mirror"): {
      if (0 == (m_Permissions & PERM_BOT_SUDO_SPOOFABLE)) {
        ErrorReply("Not allowed to mirror games.");
        break;
      }

      if (!m_Aura->m_GameSetup) {
        ErrorReply("A map must first be loaded with " + (m_SourceRealm ? m_SourceRealm->GetCommandToken() : "!") + "map.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + GetToken() + "mirror [EXCLUDESERVER], [IP], [PORT], [GAMEID], [GAMEKEY], [GAMENAME] - GAMEID, GAMEKEY expected hex.");
        break;
      }

      if (m_Aura->m_CurrentLobby) {
        ErrorReply("Already hosting a game.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 6);

      uint16_t gamePort = 6112;
      uint32_t gameHostCounter = 1;
      uint32_t gameEntryKey = 0;

      CRealm* excludedServer = m_Aura->GetRealmByInputId(Args[0]);

      optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(Args[1], ACCEPT_IPV4);
      if (!maybeAddress.has_value()) {
        ErrorReply("Not a IPv4 address.");
        break;
      }

      try {
        gamePort = static_cast<uint16_t>(stoul(Args[2]));
        size_t posId, posKey;
        gameHostCounter = stoul(Args[3], &posId, 16);
        if (posId != Args[3].length()) {
          ErrorReply("Usage: " + GetToken() + "mirror [EXCLUDESERVER], [IP], [PORT], [GAMEID], [GAMEKEY], [GAMENAME] - GAMEID, GAMEKEY expected hex.");
          break;
        }
        gameEntryKey = stoul(Args[4], &posKey, 16);
        if (posKey != Args[4].length()) {
          ErrorReply("Usage: " + GetToken() + "mirror [EXCLUDESERVER], [IP], [PORT], [GAMEID], [GAMEKEY], [GAMENAME] - GAMEID, GAMEKEY expected hex.");
          break;
        }
      } catch (...) {
        ErrorReply("Usage: " + GetToken() + "mirror [EXCLUDESERVER], [IP], [PORT], [GAMEID], [GAMEKEY], [GAMENAME] - GAMEID, GAMEKEY expected hex.");
        break;
      }

      string gameName = Args[5];
      SetAddressPort(&(maybeAddress.value()), gamePort);
      m_Aura->m_GameSetup->SetContext(this);
      m_Aura->m_GameSetup->SetMirrorSource(maybeAddress.value(), gameHostCounter);
      m_Aura->m_GameSetup->SetName(gameName);
      if (excludedServer) m_Aura->m_GameSetup->AddIgnoredRealm(excludedServer);
      m_Aura->m_GameSetup->RunHost();
      for (auto& bnet : m_Aura->m_Realms) {
        if (bnet != excludedServer && !bnet->GetIsMirror()) {
          bnet->ResetConnection(false);
          bnet->SetReconnectNextTick(true);
        }
      }
      delete m_Aura->m_GameSetup;
      m_Aura->m_GameSetup = nullptr;
      break;
    }

    default: {
      ErrorReply("Unrecognized command [" + Command + "].");
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
  m_Output = nullptr;
}
