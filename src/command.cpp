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
#include "gameprotocol.h"
#include "hash.h"

using namespace std;

//
// CCommandContext
//

/* In-game command */
CCommandContext::CCommandContext(CAura* nAura, CGame* game, CGamePlayer* player, ostream* outputStream)
  : m_Aura(nAura),
    m_FromName(player->GetName()),
    m_FromWhisper(false),
    m_Game(game),
    m_Permissions(0),

    m_Player(player),

    m_HostName(player->GetRealmHostName()),
    m_Realm(player->GetRealm(false)),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(outputStream)
{
}

/* Command received from BNET but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CGame* targetGame, CBNET* fromRealm, string& fromName, bool& isWhisper, ostream* outputStream)
  : m_Aura(nAura),
    m_FromName(fromName),
    m_FromWhisper(isWhisper),
    m_Game(targetGame),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(fromRealm->GetServer()),
    m_Realm(fromRealm),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(outputStream)
{
}

/* Command received from IRC but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CGame* targetGame, CIRC* ircNetwork, string& channelName, string& userName, bool& isWhisper, ostream* outputStream)
  : m_Aura(nAura),
    m_FromName(userName),
    m_FromWhisper(isWhisper),
    m_Game(targetGame),
    m_Permissions(0),    

    m_Player(nullptr),

    m_HostName(string()),
    m_Realm(nullptr),

    m_IRC(ircNetwork),
    m_ChannelName(channelName),

    m_Output(outputStream)
{
}

/* Command received from elsewhere but targetting a game */
CCommandContext::CCommandContext(CAura* nAura, CGame* targetGame, ostream* outputStream)
  : m_Aura(nAura),
    m_FromName(string()),
    m_FromWhisper(false),
    m_Game(targetGame),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(string()),
    m_Realm(nullptr),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(outputStream)
{
}

/* BNET command */
CCommandContext::CCommandContext(CAura* nAura, CBNET* fromRealm, string& fromName, bool& isWhisper, ostream* outputStream)
  : m_Aura(nAura),
    m_FromName(fromName),
    m_FromWhisper(isWhisper),
    m_Game(nullptr),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(fromRealm->GetServer()),
    m_Realm(fromRealm),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(outputStream)
{
}

/* IRC command */
CCommandContext::CCommandContext(CAura* nAura, CIRC* ircNetwork, string& channelName, string& userName, bool& isWhisper, ostream* outputStream)
  : m_Aura(nAura),
    m_FromName(userName),
    m_FromWhisper(isWhisper),
    m_Game(nullptr),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(string()),
    m_Realm(nullptr),

    m_IRC(ircNetwork),
    m_ChannelName(channelName),

    m_Output(outputStream)
{
}

/* Generic command */
CCommandContext::CCommandContext(CAura* nAura, ostream* outputStream)
  : m_Aura(nAura),
    m_FromName(string()),
    m_FromWhisper(false),
    m_Game(nullptr),
    m_Permissions(0),

    m_Player(nullptr),

    m_HostName(string()),
    m_Realm(nullptr),

    m_IRC(nullptr),
    m_ChannelName(string()),

    m_Output(outputStream)
{
}

bool CCommandContext::SetIdentity(const string& userName, const string& realmId)
{
  m_Realm = m_Aura->GetRealmByInputId(realmId);
  if (m_Realm == nullptr)
    return false;
  m_FromName = userName;
  m_HostName = m_Realm->GetServer();
  return true;
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
    IsRealmVerified = m_Player ? m_Player->IsRealmVerified() : m_Realm != nullptr;
  }

  // Trust PvPGN servers on player identities for admin powers. Impersonation is not an issue.
  // However, do NOT trust them regarding sudo access, since those commands may cause data deletion or worse.
  // Note also that sudo permissions must be ephemeral, since neither WC3 nor PvPGN TCP connections are encrypted.
  bool IsOwner = m_Game && m_Game->MatchOwnerName(m_FromName) && (
    IsRealmVerified && m_HostName == m_Game->m_OwnerRealm ||
    m_HostName.empty() && m_Game->m_OwnerRealm.empty()
  );
  bool IsAdmin = IsRealmVerified && m_Realm != nullptr && m_Realm->GetIsAdmin(m_FromName);
  bool IsRootAdmin = IsRealmVerified && m_Realm != nullptr && m_Realm->GetIsRootAdmin(m_FromName);
  bool IsSudoSpoofable = IsRealmVerified && m_Realm != nullptr && m_Realm->GetIsSudoer(m_FromName);

  // Owners are always treated as players if the game hasn't started yet. Even if they haven't joined.
  if (m_Player || IsOwner && m_Game->GetIsLobby()) m_Permissions |= PERM_GAME_PLAYER;

  // Leaver or absent owners are automatically demoted.
  if (IsOwner && (m_Player || m_Game->GetIsLobby())) m_Permissions |= PERM_GAME_OWNER;
  if (IsAdmin || IsRootAdmin) m_Permissions |= PERM_BNET_ADMIN;
  if (IsRootAdmin) m_Permissions |= PERM_BNET_ROOTADMIN;

  // Sudo is a separate permission system.
  if (IsSudoSpoofable) m_Permissions |= PERM_BOT_SUDO_SPOOFABLE;
}

optional<pair<string, string>> CCommandContext::CheckSudo(const string& message)
{
  optional<pair<string, string>> Result;
  if (!(m_Permissions & PERM_BOT_SUDO_SPOOFABLE)) {
    return Result;
  }
  bool isValidCaller = false;
  if (m_FromName == m_Aura->m_SudoUser) {
    if (m_Aura->m_SudoGame) {
      isValidCaller = m_Player && m_Game == m_Aura->m_SudoGame;
    } else if (m_Aura->m_SudoRealm) {
      isValidCaller = m_Realm == m_Aura->m_SudoRealm;
    } else if (m_Aura->m_SudoIRC) {
      isValidCaller = m_IRC = m_Aura->m_SudoIRC;
    }
  }
  if (isValidCaller && message == m_Aura->m_SudoAuthCommand) {
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
    m_Permissions |= PERM_BOT_SUDO_OK;
  }
  m_Aura->m_SudoUser.clear();
  m_Aura->m_SudoAuthCommand.clear();
  m_Aura->m_SudoExecCommand.clear();
  m_Aura->m_SudoGame = nullptr;
  m_Aura->m_SudoRealm = nullptr;
  m_Aura->m_SudoIRC = nullptr;
  return Result;
}

void CCommandContext::SendReply(const string& message)
{
  if (message.empty())
    return;

  if (m_Player) {
    if (message.length() <= 100) {
      m_Game->SendChat(m_Player, message);
    } else {
      string leftMessage = message;
      do {
        m_Game->SendChat(m_Player, leftMessage.substr(0, 100));
        leftMessage = leftMessage.substr(100, leftMessage.length());
      } while (!leftMessage.empty());
    }
  } else if (m_Realm) {
    m_Realm->QueueChatCommand(message, m_FromName, m_FromWhisper);
  } else if (m_IRC) {
    if (m_FromWhisper) {
      // TODO(IceSandslash)
      m_IRC->SendMessageIRC(message, string());
    } else {
      m_IRC->SendMessageIRC(message, string());
    }
  } else {
    (*m_Output) << "[AURA] " + message << std::endl;
  }
}

void CCommandContext::SendReply(const string& message, const uint8_t ccFlags) {
  if (m_Player && (ccFlags & CHAT_SEND_ALL)) {
    m_Game->SendAllChat(message);
  } else {
    SendReply(message);
  }
  if ((m_Player || m_Realm || m_IRC) && (ccFlags & CHAT_LOG_CONSOLE)) {
    if (m_Game) {
      (*m_Output) << m_Game->GetLogPrefix() + message << std::endl;
    } else if (m_Realm) {
      (*m_Output) << m_Realm->GetLogPrefix() + message << std::endl;
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

void CCommandContext::SendAll(const string& message)
{
  SendReply(message, CHAT_SEND_ALL);
}

void CCommandContext::ErrorAll(const string& message)
{
  SendReply("Error: " + message, CHAT_SEND_ALL);
}

CGamePlayer* CCommandContext::GetTargetPlayer(const string& target)
{
  CGamePlayer* TargetPlayer = nullptr;
  if (target.empty() || !m_Game) {
    return TargetPlayer;
  }

  uint8_t Matches = m_Game->GetPlayerFromNamePartial(target, &TargetPlayer);
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
  if (!m_Game) return TargetPlayer;
  uint8_t Matches = m_Game->GetPlayerFromNamePartial(target, &TargetPlayer);
  if (Matches > 1) {
    TargetPlayer = nullptr;
  }
  return TargetPlayer;
}

void CCommandContext::UseImplicitHostedGame()
{
  if (!m_Game) return;
  m_Game = m_Aura->m_CurrentLobby;
  if (m_Game && 0 == (m_Permissions & PERM_BOT_SUDO_OK)) {
    UpdatePermissions();
  }
}

void CCommandContext::Run(const string& command, const string& payload)
{
  const std::string& Command = command;
  const std::string& Payload = payload;
  if (m_Game->m_Locked && 0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
    (*m_Output) << m_Game->GetLogPrefix() + "Command ignored, the game is locked" << std::endl;
    ErrorReply("Only the game owner and root admins can run game commands when the game is locked.");
    return;
  }

  const uint64_t CommandHash = HashCode(Command);

  /*********************
   * NON ADMIN COMMANDS *
   *********************/

  switch (CommandHash)
  {
    //
    // !VERSION
    //

    case HashCode("version"): {
      SendReply("Version: Aura " + m_Aura->m_Version);
      break;
    }

    //
    // !CHECKME
    // !CHECK <PLAYER>
    //

    case HashCode("checkme"):
    case HashCode("check"): {
      if (!m_Game || m_Game->GetIsMirror()) {
        break;
      }
      CGamePlayer* targetPlayer = GetTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }
      CBNET* targetPlayerRealm = targetPlayer->GetRealm(true);
      bool IsRealmVerified = targetPlayerRealm != nullptr;
      bool IsOwner = m_Game->MatchOwnerName(targetPlayer->GetName()) && m_Game->HasOwnerInGame();
      bool IsAdmin = IsRealmVerified && targetPlayerRealm->GetIsAdmin(m_FromName);
      bool IsRootAdmin = IsRealmVerified && targetPlayerRealm->GetIsRootAdmin(m_FromName);
      string SyncStatus;
      if (m_Game->GetGameLoaded()) {
        if (m_Game->m_SyncPlayers[targetPlayer].size() + 1 == m_Game->m_Players.size()) {
          SyncStatus = "Full";
        } else if (m_Game->m_SyncPlayers[targetPlayer].empty()) {
          SyncStatus = "Alone";
        } else {
          SyncStatus = "With: ";
          for (auto& otherPlayer: m_Game->m_SyncPlayers[targetPlayer]) {
            SyncStatus += otherPlayer->GetName() + ", ";
          }
          SyncStatus = SyncStatus.substr(0, SyncStatus.length() - 2);
        }
      }
      bool IsForwards = m_Game->m_SyncFactor * static_cast<float>(targetPlayer->GetSyncCounter()) >= static_cast<float>(m_Game->m_SyncCounter);
      float OffsetAbs = (
        IsForwards ? m_Game->m_SyncFactor * static_cast<float>(targetPlayer->GetSyncCounter()) - static_cast<float>(m_Game->m_SyncCounter) :
        static_cast<float>(m_Game->m_SyncCounter) - m_Game->m_SyncFactor * static_cast<float>(targetPlayer->GetSyncCounter())
      );
      string SyncOffsetText = IsForwards ? " +" + to_string(OffsetAbs) : " -" + to_string(OffsetAbs);
      string SlotFragment;
      if (m_Game->GetIsLobby()) {
        SlotFragment = "Slot #" + to_string(1 + m_Game->GetSIDFromPID(targetPlayer->GetPID())) + ". ";
      }
      SendReply("[" + targetPlayer->GetName() + "]. " + SlotFragment + "Ping: " + (targetPlayer->GetNumPings() > 0 ? to_string(targetPlayer->GetPing()) + "ms" : "N/A") + ", From: " + m_Aura->m_DB->FromCheck(ByteArrayToUInt32(targetPlayer->GetExternalIP(), true)) + (m_Game->GetGameLoaded() ? ", Sync: " + SyncStatus + SyncOffsetText : ""));
      SendReply("[" + targetPlayer->GetName() + "]. Realm: " + (targetPlayer->GetRealmHostName().empty() ? "LAN" : targetPlayer->GetRealmHostName()) + ", Verified: " + (IsRealmVerified ? "Yes" : "No") + ", Owner: " + (IsOwner ? "Yes" : "No")+ ", Reserved: " + (targetPlayer->GetReserved() ? "Yes" : "No") + ", Admin: " + (IsAdmin ? "Yes" : "No") + ", Root Admin: " + (IsRootAdmin ? "Yes" : "No"));
      break;
    }


    //
    // !PING
    //
    case HashCode("ping"):
    case HashCode("p"): {
      // kick players with ping higher than payload if payload isn't empty
      // we only do this if the game hasn't started since we don't want to kick players from a game in progress

      if (!m_Game || m_Game->GetIsMirror())
        break;

      uint32_t KickedCount = 0;
      optional<uint32_t> KickPing;
      if (!Payload.empty()) {
        if (!m_Game->GetIsLobby() || 0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_OK))) {
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
        m_Game->m_AutoKickPing = KickPing.value();

      // copy the m_Players vector so we can sort by descending ping so it's easier to find players with high pings

      vector<CGamePlayer*> SortedPlayers = m_Game->m_Players;
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

          if (m_Game->GetIsLobby() && !(*i)->GetReserved()) {
            if (KickPing.has_value() && (*i)->GetPing() > m_Game->m_AutoKickPing) {
              (*i)->SetKickByTime(GetTime() + 5);
              (*i)->SetLeftReason("was kicked for excessive ping " + to_string((*i)->GetPing()) + " > " + to_string(m_Game->m_AutoKickPing));
              (*i)->SetLeftCode(PLAYERLEAVE_LOBBY);
              ++KickedCount;
            } else if ((*i)->GetKickQueued() && ((*i)->GetHasMap() || (*i)->GetDownloadStarted())) {
              (*i)->SetKickByTime(0);
              (*i)->SetLeftReason(string());
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
        SendAll("Kicking " + to_string(KickedCount) + " players with pings greater than " + to_string(m_Game->m_AutoKickPing) + "...");

      break;
    }

    //
    // !VOTEKICK
    //

    case HashCode("votekick"):
    {
      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (m_Game->GetCountDownStarted() && !m_Game->GetGameLoaded())
        break;

      if (Payload.empty()){
        ErrorReply("Usage: !votekick [PLAYERNAME]");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }

      if (!m_Game->m_KickVotePlayer.empty()) {
        ErrorReply("Unable to start votekick. Another votekick is in progress");
        break;
      }
      if (m_Game->m_Players.size() <= 2) {
        ErrorReply("Unable to start votekick. There aren't enough players in the game for a votekick");
        break;
      }
      if (targetPlayer->GetReserved()) {
        ErrorAll("Unable to votekick player [" + targetPlayer->GetName() + "]. That player is reserved and cannot be votekicked");
        break;
      }

      m_Game->m_KickVotePlayer      = targetPlayer->GetName();
      m_Game->m_StartedKickVoteTime = GetTime();

      for (auto& it : m_Game->m_Players)
        it->SetKickVote(false);

      SendReply("Votekick against player [" + m_Game->m_KickVotePlayer + "] started by player [" + m_FromName + "]", CHAT_SEND_ALL | CHAT_LOG_CONSOLE);
      if (m_Player && m_Player != targetPlayer) {
        m_Player->SetKickVote(true);
        SendAll("Player [" + m_Player->GetName() + "] voted to kick player [" + m_Game->m_KickVotePlayer + "]. " + to_string(static_cast<uint32_t>(ceil((m_Game->GetNumHumanPlayers() - 1) * static_cast<float>(m_Aura->m_GameDefaultConfig->m_VoteKickPercentage) / 100)) - 1) + " more votes are needed to pass");
      }
      SendAll("Type " + string(1, m_Aura->m_GameDefaultConfig->m_CommandTrigger) + "yes or " + string(1, m_Aura->m_GameDefaultConfig->m_CommandTrigger) + "no to vote.");

      break;
    }

    //
    // !YES
    //

    case HashCode("yes"):
    {
      if (!m_Player || m_Game->m_KickVotePlayer.empty() || m_Player->GetKickVote().value_or(false))
        break;

      uint32_t VotesNeeded = static_cast<uint32_t>(ceil((m_Game->GetNumHumanPlayers() - 1) * static_cast<float>(m_Aura->m_GameDefaultConfig->m_VoteKickPercentage) / 100));
      m_Player->SetKickVote(true);
      m_Game->SendAllChat("Player [" + m_Player->GetName() + "] voted for kicking player [" + m_Game->m_KickVotePlayer + "]. " + to_string(VotesNeeded) + " affirmative votes required to pass");
      m_Game->CountKickVotes();
      break;
    }

    //
    // !NO
    //

    case HashCode("no"):
    {
      if (!m_Player || m_Game->m_KickVotePlayer.empty() || !m_Player->GetKickVote().value_or(true))
        break;

      uint32_t VotesNeeded = static_cast<uint32_t>(ceil((m_Game->GetNumHumanPlayers() - 1) * static_cast<float>(m_Aura->m_GameDefaultConfig->m_VoteKickPercentage) / 100));
      m_Player->SetKickVote(false);
      m_Game->SendAllChat("Player [" + m_Player->GetName() + "] voted against kicking player [" + m_Game->m_KickVotePlayer + "].");
      break;
    }

    //
    // !INVITE
    //

    case HashCode("invite"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetCountDownStarted()) {
        // Intentionally allows !invite to fake (mirror) lobbies.
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !invite [PLAYERNAME]");
        break;
      }

      const string MapPath = m_Game->m_Map->GetMapPath();
      string PassedMessage;
      size_t LastSlash = MapPath.rfind('\\');
      if (LastSlash != string::npos && LastSlash <= MapPath.length() - 6) {
        PassedMessage = m_FromName + " invites you to play [" + MapPath.substr(LastSlash + 1, MapPath.length()) + "]. Join game \"" + m_Game->m_GameName + "\"";
      } else {
        PassedMessage = m_FromName + " invites you to join game \"" + m_Game->m_GameName + "\"";
      }

      bool CanInviteGlobally = 0 != (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE));
      for (auto& bnet : m_Aura->m_BNETs) {
        if (bnet->GetIsMirror())
          continue;

        if (bnet == m_Realm || CanInviteGlobally) {
          bnet->QueueChatCommand(PassedMessage, Payload, true);
        }
      }

      break;
    }

    /*****************
     * ADMIN COMMANDS *
     ******************/
     
    //
    // !FROM
    //

    case HashCode("from"):
    case HashCode("f"):
    {
      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (0 == (m_Permissions & ((m_Game->HasOwnerSet() ? PERM_GAME_OWNER : PERM_GAME_PLAYER) | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to check players geolocalization.");
        break;
      }

      string Froms;

      for (auto i = begin(m_Game->m_Players); i != end(m_Game->m_Players); ++i) {
        // we reverse the byte order on the IP because it's stored in network byte order

        Froms += (*i)->GetName();
        Froms += ": (";
        Froms += m_Aura->m_DB->FromCheck(ByteArrayToUInt32((*i)->GetExternalIP(), true));
        Froms += ")";

        if (i != end(m_Game->m_Players) - 1)
          Froms += ", ";
      }

      SendAll(Froms);
      break;
    }

    //
    // !BANLAST
    //

    case HashCode("banlast"):
    case HashCode("bl"):
    {
      if (!m_Game || !m_Game->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (!m_Game->m_DBBanLast) {
        ErrorReply("No ban candidates stored.");
        break;
      }
      if (m_Aura->m_BNETs.empty()) {
        ErrorReply("No realms joined.");
        break;
      }

      m_Aura->m_DB->BanAdd(m_Game->m_DBBanLast->GetServer(), m_Game->m_DBBanLast->GetName(), m_FromName, Payload);
      SendAll("Player [" + m_Game->m_DBBanLast->GetName() + "] was banned by player [" + m_FromName + "] on server [" + m_Game->m_DBBanLast->GetServer() + "]");
      break;
    }

    //
    // !CLOSE (close slot)
    //

    case HashCode("close"):
    case HashCode("c"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "c [SLOTNUM]");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 12u);
      if (Args.empty()) {
        ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "c [SLOTNUM]");
        break;
      }

      for (auto& elem : Args) {
        if (elem == 0 || elem > 12) {
          ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "o [SLOTNUM]");
          break;
        }
        uint8_t SID = static_cast<uint8_t>(elem) - 1;
        m_Game->DeleteFakePlayer(SID);
        m_Game->CloseSlot(SID, CommandHash == HashCode("close"));
      }
      break;
    }

    //
    // !END
    //

    case HashCode("end"):
    {
      if (!m_Game || !m_Game->GetGameLoaded())
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

      (*m_Output) << m_Game->GetLogPrefix() + "is over (admin ended game) [" + m_FromName + "]" << std::endl;
      m_Game->StopPlayers("was disconnected (admin ended game)");
      break;
    }

    //
    // !URL
    //

    case HashCode("url"):
    case HashCode("link"):
    {
      if (!m_Game || !m_Game->GetIsLobby())
        break;

      const string TargetUrl = TrimString(Payload);

      if (TargetUrl.empty()) {
        if (m_Game->m_Map->GetMapSiteURL().empty()) {
          SendAll("Download URL unknown");
        } else {
          SendAll("Download map from <" + m_Game->m_Map->GetMapSiteURL() + ">");
        }
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      m_Game->m_Map->SetMapSiteURL(TargetUrl);
      SendAll("Download URL set to [" + TargetUrl + "]");
      break;
    }

    //
    // !HCL
    //

    case HashCode("hcl"):
    {
      if (!m_Game || m_Game->GetCountDownStarted())
        break;

      if (Payload.empty()) {
        SendReply("The HCL command string is [" + m_Game->m_HCLCommandString + "]");
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (Payload.size() > m_Game->m_Slots.size()) {
        ErrorReply("Unable to set HCL command string because it's too long - it must not exceed the amount of occupied game slots");
        break;
      }

      const string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";
      size_t IllegalIndex = Payload.find_first_not_of(HCLChars);

      if (IllegalIndex != string::npos) {
        ErrorReply("Unable to set HCL command string because it contains invalid character [" + string(1, static_cast<char>(Payload[IllegalIndex])) + "]");
        break;
      }

      m_Game->m_HCLCommandString = Payload;
      SendAll("Setting game mode to HCL [" + m_Game->m_HCLCommandString + "]");
      break;
    }

    //
    // !HOLD (hold a slot for someone)
    //

    case HashCode("hold"):
    {
      UseImplicitHostedGame();
      if (!m_Game || !m_Game->GetIsLobby())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 12u);

      if (Args.empty()) {
        ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "hold [PLAYER1], [PLAYER2], ...");
        break;
      }

      for (auto& PlayerName: Args) {
        if (PlayerName.empty())
          continue;
        Args.push_back("[" + PlayerName + "]");
        m_Game->AddToReserved(PlayerName);
      }

      SendAll("Added player(s) to the hold list: " + JoinVector(Args, false));
      break;
    }

    //
    // !UNHOLD
    //

    case HashCode("unhold"):
    {
      UseImplicitHostedGame();
      if (!m_Game || !m_Game->GetIsLobby())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 1u, 12u);

      if (Args.empty()) {
        ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "hold [PLAYER1], [PLAYER2], ...");
        break;
      }

      for (auto& PlayerName: Args) {
        if (PlayerName.empty())
          continue;
        m_Game->RemoveFromReserved(PlayerName);
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
    case HashCode("k"):
    {
      UseImplicitHostedGame();
      if (!m_Game || m_Game->GetIsMirror() || m_Game->GetCountDownStarted() && !m_Game->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "kick [PLAYERNAME]");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayer(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }

      targetPlayer->SetDeleteMe(true);
      targetPlayer->SetLeftReason("was kicked by player [" + m_FromName + "]");

      if (m_Game->GetIsLobby())
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
      else
        targetPlayer->SetLeftCode(PLAYERLEAVE_LOST);

      if (m_Game->GetIsLobby()) {
        bool KickAndClose = CommandHash == HashCode("ckick") || CommandHash == HashCode("closekick");
        if (KickAndClose) {
          m_Game->CloseSlot(m_Game->GetSIDFromPID(targetPlayer->GetPID()), false);
        } else {
          m_Game->OpenSlot(m_Game->GetSIDFromPID(targetPlayer->GetPID()), false);
        }
      }

      break;
    }

    //
    // !LATENCY (set game latency)
    //

    case HashCode("latency"):
    {
      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (Payload.empty()) {
        SendReply("The game latency is " + to_string(m_Game->m_Latency) + " ms");
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
        m_Game->m_Latency = static_cast<uint16_t>(TargetValue);
      } catch (...) {
        ErrorReply("Invalid game latency [" + Payload + "].");
        break;
      }

      if (m_Game->m_Latency <= 10) {
        m_Game->m_Latency = 10;
        SendAll("Setting game latency to the minimum of 10 ms");
      } else if (m_Game->m_Latency >= 500) {
        m_Game->m_Latency = 500;
        SendAll("Setting game latency to the maximum of 500 ms");
      } else {
        SendAll("Setting game latency to " + to_string(m_Game->m_Latency) + " ms");
      }

      break;
    }

    //
    // !OPEN (open slot)
    //

    case HashCode("open"):
    case HashCode("o"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "o [SLOTNUM]");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 12u);
      if (Args.empty()) {
        ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "o [SLOTNUM]");
        break;
      }

      for (auto& elem : Args) {
        if (elem == 0 || elem > 12) {
          ErrorReply("Usage: " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "o [SLOTNUM]");
          break;
        }
        uint8_t SID = static_cast<uint8_t>(elem) - 1;
        if (!m_Game->DeleteFakePlayer(SID)) {
          m_Game->OpenSlot(SID, CommandHash == HashCode("open"));
        }
      }
      break;
    }

    //
    // !PRIV (rehost as private game)
    //

    case HashCode("priv"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot rehost the game.");
        break;
      }

      if (Payload.length() >= 31) {
        ErrorReply("Unable to create game [" + Payload + "]. The game name is too long (the maximum is 31 characters)");
        break;
      }

      SendReply("Trying to rehost as private game [" + Payload + "]. (This will take a few seconds)", CHAT_SEND_ALL | CHAT_LOG_CONSOLE);
      m_Game->m_GameDisplay  = GAME_PRIVATE;
      m_Game->m_LastGameName = m_Game->m_GameName;
      m_Game->m_GameName     = Payload;
      m_Game->m_HostCounter  = m_Aura->NextHostCounter();
      m_Game->m_RefreshError = false;

      for (auto& bnet : m_Aura->m_BNETs) {
        if (m_Game->m_IsMirror && bnet->GetIsMirror())
          continue;

        // unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
        // this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
        // we assume this won't happen very often since the only downside is a potential false positive

        bnet->UnqueueGameRefreshes();
        bnet->QueueGameUncreate();
        bnet->QueueEnterChat();

        // we need to send the game creation message now because private games are not refreshed

        bnet->QueueGameCreate(m_Game->m_GameDisplay, m_Game->m_GameName, m_Game->m_Map, m_Game->m_HostCounter, m_Game->m_HostPort);

        if (!bnet->GetPvPGN())
          bnet->QueueEnterChat();
      }

      m_Game->m_CreationTime = m_Game->m_LastRefreshTime = GetTime();

      break;
    }

    //
    // !PUB (rehost as public game)
    //

    case HashCode("pub"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot rehost the game.");
        break;
      }

      if (Payload.length() >= 31) {
        ErrorReply("Unable to create game [" + Payload + "]. The game name is too long (the maximum is 31 characters)");
        break;
      }
      SendReply("Trying to rehost as public game [" + Payload + "]. (This will take a few seconds)", CHAT_SEND_ALL | CHAT_LOG_CONSOLE);
      m_Game->m_GameDisplay  = GAME_PUBLIC;
      m_Game->m_LastGameName = m_Game->m_GameName;
      m_Game->m_GameName     = Payload;
      m_Game->m_HostCounter  = m_Aura->NextHostCounter();
      m_Game->m_RefreshError = false;

      for (auto& bnet : m_Aura->m_BNETs) {
        if (m_Game->m_IsMirror && bnet->GetIsMirror())
          continue;

        // unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
        // this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
        // we assume this won't happen very often since the only downside is a potential false positive

        bnet->UnqueueGameRefreshes();
        bnet->QueueGameUncreate();
        bnet->QueueEnterChat();

        // the game creation message will be sent on the next refresh
      }

      m_Game->m_CreationTime = m_Game->m_LastRefreshTime = GetTime();

      break;
    }

    //
    // !START
    //

    case HashCode("start"):
    case HashCode("go"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & ((m_Game->HasOwnerSet() && !m_Game->m_PublicStart ? PERM_GAME_OWNER : PERM_GAME_PLAYER) | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not allowed to start this game.");
        break;
      }

      uint32_t ConnectionCount = m_Game->GetNumConnectionsOrFake();
      if (ConnectionCount == 0) {
        ErrorReply("Not enough players have joined.");
        break;
      }

      bool IsForce = Payload == "force";
      if (IsForce && (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE)))) {
        ErrorReply("You are not the game owner, and therefore cannot forcibly start it.");
        break;
      }

      m_Game->StartCountDown(IsForce);

      break;
    }

    //
    // !QUICKSTART
    //

    case HashCode("quickstart"):
    {
      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot quickstart it.");
        break;
      }

      if (GetTicks() - m_Game->m_LastPlayerLeaveTicks < 2000) {
        ErrorReply("Someone left the game less than two seconds ago.");
        break;
      }
      m_Game->StartCountDown(true);
      m_Game->m_CountDownCounter = 1;
      // 500 ms countdown
      break;
    }

    //
    // !FREESTART
    //

    case HashCode("freestart"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit the game permissions.");
        break;
      }

      optional<bool> TargetValue;
      if (Payload.empty() || Payload == "on") {
        TargetValue = true;
      } else if (Payload == "off") {
        TargetValue = false;
      }
      if (!TargetValue.has_value()) {
        ErrorReply("Unrecognized setting [" + Payload + "].");
        break;
      }
      m_Game->m_PublicStart = TargetValue.value();
      if (m_Game->m_PublicStart) {
        SendAll("Anybody may now use the " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "start command.");
      } else {
        SendAll("Only the game owner may now use the " + string(1, static_cast<char>(m_Aura->m_GameDefaultConfig->m_CommandTrigger)) + "start command.");
      }
      break;
    }

    //
    // !AUTOSTART
    //

    case HashCode("autostart"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (Payload.empty()) {
        m_Game->SendAllAutoStart();
        break;
      }

      uint32_t MinSlots = 2;
      uint32_t MinMinutes = 0;
      stringstream SS;
      SS << Payload;
      SS >> MinSlots;

      if (SS.fail()) {
        ErrorReply("Usage: !autostart <slots>, <minutes>");
        break;
      }

      if (MinSlots > m_Game->m_Slots.size()) {
        ErrorReply("This map does not allow " + to_string(MinSlots) + " slots.");
        break;
      }

      if (!SS.eof()) {
        SS >> MinMinutes;
      }

      int64_t Time = GetTime();
      m_Game->m_AutoStartPlayers = MinSlots;
      m_Game->m_AutoStartMinTime = Time + static_cast<int64_t>(MinMinutes) * 60;
      if (m_Game->m_AutoStartMinTime <= Time)
        m_Game->m_AutoStartMinTime = 0;
      if (m_Game->m_AutoStartMaxTime <= m_Game->m_AutoStartMinTime)
        m_Game->m_AutoStartMaxTime = 0;

      m_Game->SendAllAutoStart();
      break;
    }

    //
    // !FORCEAUTOSTART
    //


    case HashCode("forceautostart"):
    {
      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot use this command.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !forceautostart <minutes>");
        break;
      }

      uint32_t MaxMinutes = 0;
      stringstream SS;
      SS << Payload;
      SS >> MaxMinutes;

      if (SS.fail() || MaxMinutes == 0) {
        ErrorReply("Usage: !forceautostart <minutes>");
        break;
      }

      int64_t Time = GetTime();
      m_Game->m_AutoStartMaxTime = Time + static_cast<int64_t>(MaxMinutes) * 60;
      if (m_Game->m_AutoStartMaxTime <= Time)
        m_Game->m_AutoStartMaxTime = 0;
      if (m_Game->m_AutoStartMinTime >= m_Game->m_AutoStartMaxTime)
        m_Game->m_AutoStartMinTime = 0;

      m_Game->SendAllAutoStart();
      break;
    }

    //
    // !SWAP (swap slots)
    //

    case HashCode("swap"):
    case HashCode("sw"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<uint32_t> Slots = SplitNumericArgs(Payload, 2u, 2u);
      if (Slots.empty()) {
        ErrorReply("Usage: !swap <SLOTNUM>, <SLOTNUM>");
        break;
      }

      if (Slots[0] == 0 || Slots[1] == 0 || Slots[0] > 12 || Slots[1] > 12) {
        ErrorReply("Usage: !swap <SLOTNUM>, <SLOTNUM>");
        break;
      }

      uint8_t SID1 = static_cast<uint8_t>(Slots[0]) - 1;
      uint8_t SID2 = static_cast<uint8_t>(Slots[1]) - 1;
      m_Game->SwapSlots(SID1, SID2);
      break;
    }

    //
    // !SYNC
    //

    case HashCode("sync"):
    {
      if (!m_Game || !m_Game->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game settings.");
        break;
      }

      CGamePlayer* targetPlayer = GetTargetPlayerOrSelf(Payload);
      if (!targetPlayer) {
        ErrorReply("Player [" + Payload + "] not found.");
        break;
      }
      if (m_Game->m_SyncCounter == 0 || targetPlayer->GetSyncCounter() == 0)
        break;

      m_Game->m_SyncFactor = static_cast<float>(m_Game->m_SyncCounter) / static_cast<float>(targetPlayer->GetSyncCounter());
      SendReply("Sync rate updated to " + to_string(m_Game->m_SyncFactor) + ".", CHAT_SEND_ALL | CHAT_LOG_CONSOLE);

      uint32_t LeastSyncCounter = 0xFFFFFFFF;
      CGamePlayer* LeastSyncPlayer = nullptr;
      for (auto& eachPlayer: m_Game->m_Players) {
        if (eachPlayer->GetSyncCounter() < LeastSyncCounter) {
          LeastSyncCounter = eachPlayer->GetSyncCounter();
          LeastSyncPlayer = eachPlayer;
        }
      }
      if (LeastSyncPlayer && (static_cast<float>(m_Game->m_SyncCounter) > m_Game->m_SyncFactor * static_cast<float>(LeastSyncCounter))) {
        float FramesBehind = static_cast<float>(m_Game->m_SyncCounter) / m_Game->m_SyncFactor - static_cast<float>(LeastSyncCounter);
        SendReply("Player [" + targetPlayer->GetName() + "] is " + to_string(FramesBehind) + " frame(s) behind.", CHAT_LOG_CONSOLE);
      }

      break;
    }

    //
    // !SYNCRESET
    //

    case HashCode("syncreset"):
    case HashCode("sr"):
    {
      if (!m_Game || !m_Game->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game settings.");
        break;
      }

      if (m_Game->m_Lagging) {
        ErrorReply("Cannot reset sync counters while there are players lagging.");
        break;
      }

      m_Game->ResetSync();
      SendReply("Sync counters reset.", CHAT_LOG_CONSOLE);
      break;
    }

    //
    // !SYNCLIMIT
    //

    case HashCode("synclimit"):
    case HashCode("sl"):
    {
      if (Payload.empty()) {
        SendReply("Sync limit: " + to_string(m_Game->m_SyncLimit) + " packets. Rate: " + to_string(m_Game->m_SyncFactor));
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game settings.");
        break;
      }

      uint32_t     Packets;
      float        Rate;
      stringstream SS;
      SS << Payload;
      SS >> Packets;

      if (SS.fail()) {
        ErrorReply("bad input #1 to !sl command");
        break;
      }
      if (SS.eof()) {
        Rate = m_Game->m_SyncFactor;
      } else {
        SS >> Rate;
        if (SS.fail() || Rate < 0.1f)
          Rate = m_Game->m_SyncFactor;
      }

      if (Packets < 2)
        Packets = 2;
      if (Packets > 10000)
        Packets = 10000;

      m_Game->m_SyncLimit = static_cast<float>(Packets);
      m_Game->m_SyncFactor = Rate;

      SendAll("Sync limit updated: " + to_string(m_Game->m_SyncLimit) + " packets. Rate: " + to_string(m_Game->m_SyncFactor));
      break;
    }

    //
    // !UNHOST
    //

    case HashCode("unhost"):
    case HashCode("uh"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetCountDownStarted()) {
        // Intentionally allows !unhost for fake (mirror) lobbies.
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      m_Game->m_Exiting = true;
      break;
    }

    //
    // !DOWNLOAD
    // !DL
    //

    case HashCode("download"):
    case HashCode("dl"):
    {
      if (!m_Game || !m_Game->GetIsLobby())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot unhost this game lobby.");
        break;
      }

      bool IsMapAvailable = !m_Game->m_Map->GetMapData()->empty() && m_Game->m_Map->GetValidLinkedMap();
      if (m_Aura->m_Config->m_AllowUploads == 0 || !IsMapAvailable) {
        if (m_Game->m_Map->GetMapSiteURL().empty()) {
          ErrorAll("Cannot transfer the map.");
        } else {
          ErrorAll("Cannot transfer the map. Please download it from <" + m_Game->m_Map->GetMapSiteURL() + ">");
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

      const uint8_t SID = m_Game->GetSIDFromPID(targetPlayer->GetPID());

      if (SID >= m_Game->m_Slots.size() || m_Game->m_Slots[SID].GetDownloadStatus() == 100) {
        // TODO(IceSandslash): ErrorReply
        SendReply("Error: Map transfer failed unexpectedly.", CHAT_LOG_CONSOLE);
        break;
      }

      SendReply("Map download started for player [" + targetPlayer->GetName() + "]", CHAT_SEND_ALL | CHAT_LOG_CONSOLE);
      m_Game->Send(targetPlayer, m_Game->GetProtocol()->SEND_W3GS_STARTDOWNLOAD(m_Game->GetHostPID()));
      targetPlayer->SetDownloadAllowed(true);
      targetPlayer->SetDownloadStarted(true);
      targetPlayer->SetStartedDownloadingTicks(GetTicks());
      targetPlayer->SetKickByTime(0);
      break;
    }

    //
    // !DROP
    //

    case HashCode("drop"):
    {
      if (!m_Game || m_Game->GetGameLoaded())
        break;

      if (!m_Game->GetLagging()) {
        ErrorReply("No player is currently lagging.");
        break;
      }

      m_Game->StopLaggers("lagged out (dropped by admin)");
      break;
    }

    //
    // !REFEREE
    //

    case HashCode("referee"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetIsMirror() || m_Game->GetCountDownStarted() && !m_Game->GetGameLoaded())
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

      for (auto& otherPlayer: m_Game->m_Players) {
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

    case HashCode("mute"):
    {
      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot mute people.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !mute [PLAYERNAME]");
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

    case HashCode("muteall"):
    {
      if (!m_Game || m_Game->GetIsMirror() || !m_Game->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot disable \"All\" chat channel.");
        break;
      }

      if (m_Game->m_MuteAll) {
        ErrorReply("Global chat is already muted.");
        break;
      }

      SendAll("Global chat muted (allied and private chat is unaffected)");
      m_Game->m_MuteAll = true;
      break;
    }

    //
    // !ABORT (abort countdown)
    // !A
    //

    case HashCode("abort"):
    case HashCode("a"):
    {
      if (!m_Game || m_Game->GetIsMirror() || m_Game->GetGameLoading() || m_Game->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot abort game start.");
        break;
      }

      if (m_Game->GetCountDownStarted()) {
        SendAll("Countdown aborted!");
        m_Game->m_CountDownStarted = false;
      } else {
        m_Game->m_AutoStartMinTime = 0;
        m_Game->m_AutoStartMaxTime = 0;
        m_Game->m_AutoStartPlayers = 0;
      }
      break;
    }

    //
    // !ADDBAN
    // !BAN
    //

    case HashCode("addban"):
    case HashCode("ban"):
    {
      if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to ban players.");
        break;
      }
      if (m_Aura->m_BNETs.empty()) {
        ErrorReply("No realms joined.");
        break;
      }
      if (Payload.empty()) {
        ErrorReply("Usage: !ban [PLAYERNAME]");
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

      if (m_Game && m_Game->GetGameLoaded()) {
        string VictimLower = Victim;
        transform(begin(VictimLower), end(VictimLower), begin(VictimLower), ::tolower);
        uint32_t Matches   = 0;
        CDBBan*  LastMatch = nullptr;

        // try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")
        // we use the m_DBBans vector for this in case the player already left and thus isn't in the m_Players vector anymore

        for (auto& ban : m_Game->m_DBBans) {
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
      } else if (m_Game) {
        CGamePlayer* targetPlayer = GetTargetPlayer(Victim);
        if (!targetPlayer) {
          ErrorReply("Player [" + Victim + "] not found.");
          break;
        }
        m_Aura->m_DB->BanAdd(targetPlayer->GetRealmDataBaseID(false), targetPlayer->GetName(), m_FromName, Reason);
        SendAll("Player [" + targetPlayer->GetName() + "] was banned by player [" + m_FromName + "] on server [" + targetPlayer->GetRealmHostName() + "]");
      } else {
      }

      break;
    }

    //
    // !CHECKBAN
    //

    case HashCode("checkban"):
    {
      if (m_Aura->m_BNETs.empty())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("Not allowed to check ban status.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !checkban [PLAYERNAME]");
        break;
      }

      vector<string> CheckResult;
      for (auto& bnet : m_Aura->m_BNETs) {
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
    // !CLEARHCL
    //

    case HashCode("clearhcl"):
    {
      if (!m_Game || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore change the map settings.");
        break;
      }

      if (m_Game->m_HCLCommandString.empty()) {
        ErrorReply("There was no game mode set.");
        break;
      }

      m_Game->m_HCLCommandString.clear();
      SendAll("Game mode reset");
      break;
    }

    //
    // !STATUS
    //

    case HashCode("status"):
    {
      string message = "Status: ";

      for (const auto& bnet : m_Aura->m_BNETs)
        message += "[" + bnet->GetServer() + "]" + (bnet->GetLoggedIn() ? " - online " : " - offline ");

      if (m_Aura->m_IRC)
        message += m_Aura->m_IRC->m_Config->m_HostName + (!m_Aura->m_IRC->m_WaitingToConnect ? " [online]" : " [offline]");

      SendReply(message);
      break;
    }

    //
    // !SENDLAN
    //

    case HashCode("sendlan"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetCountDownStarted()) {
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (m_Game->GetIsMirror()) {
        // This is not obvious.
        ErrorReply("Mirrored games cannot be broadcast to LAN");
        break;
      }

      optional<bool> TargetValue;
      if (Payload.empty() || Payload == "on") {
        TargetValue = true;
      } else if (Payload == "off") {
        TargetValue = false;
      }

      if (!TargetValue.has_value()) {
        ErrorReply("Unrecognized setting [" + Payload + "].");
        break;
      }

      m_Game->m_LANEnabled = TargetValue.value();
      if (TargetValue) {
        m_Game->LANBroadcastGameCreate();
        m_Game->LANBroadcastGameRefresh();
        if (!m_Aura->m_UDPServerEnabled)
          m_Game->LANBroadcastGameInfo();
      }
      break;
    }

    //
    // !SENDLAN
    //

    case HashCode("sendlaninfo"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetCountDownStarted()) {
        break;
      }

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore change the game settings.");
        break;
      }

      if (m_Game->GetIsMirror()) {
        // This is not obvious.
        ErrorReply("Mirrored games cannot be broadcast to LAN");
        break;
      }

      m_Game->m_LANEnabled = true;
      m_Game->LANBroadcastGameInfo();
      break;
    }

    //
    // !OWNER (set game owner)
    //

    case HashCode("owner"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & ((m_Game->HasOwnerSet() ? PERM_GAME_OWNER : PERM_GAME_PLAYER) | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        if (Payload.empty() && m_Game->HasOwnerSet()) {
          SendReply("The owner is [" + m_Game->m_OwnerName + "@" + (m_Game->m_OwnerRealm.empty() ? "@@LAN/VPN" : m_Game->m_OwnerRealm) + "]");
        }
        ErrorReply("You are not allowed to change the owner of this game.");
        break;
      }

      CGamePlayer* TargetPlayer = GetTargetPlayerOrSelf(Payload);
      string TargetName = TargetPlayer ? TargetPlayer->GetName() : Payload;
      if (TargetName.empty()) {
        ErrorReply("Usage : !owner [PLAYERNAME]");
        break;
      }
      string TargetRealm = TargetPlayer ? TargetPlayer->GetRealmHostName() : m_HostName;
      if (m_Game->m_OwnerName == TargetName && m_Game->m_OwnerRealm == TargetRealm) {
        SendAll(TargetName + "@" + (TargetRealm.empty() ? "@@LAN/VPN" : TargetRealm) + " is already the owner of this game.");
      } else {
        m_Game->SetOwner(TargetName, TargetRealm);
        SendReply("Setting game owner to [" + TargetName + "@" + (TargetRealm.empty() ? "@@LAN/VPN" : TargetRealm) + "]", CHAT_SEND_ALL | CHAT_LOG_CONSOLE);
      }
      if (TargetPlayer) TargetPlayer->SetWhoisShouldBeSent(true);
      break;
    }

    //
    // !SAY
    //

    case HashCode("say"):
    {
      if (0 == (m_Permissions & (PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not allowed to advertise.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage : !say [MESSAGE]");
        break;
      }

      if (m_Aura->m_CurrentLobby) {
        ErrorReply("Cannot send chat messages while hosting a game lobby.");
        break;
      }

      for (auto& bnet : m_Aura->m_BNETs) {
        if (bnet->GetIsMirror())
          continue;
        bnet->QueueChatCommand(Payload);
      }

      break;
    }

    //
    // !CLOSEALL
    //

    case HashCode("closeall"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      m_Game->CloseAllSlots();
      break;
    }

    //
    // !COMP (computer slot)
    //

    case HashCode("comp"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !comp [SLOT], [SKILL] - Skill is any of: 0, 1, 2");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: !comp [SLOT], [SKILL] - Skill is any of: 0, 1, 2");
        break;
      }

      if (Args[0] == 0 || Args[0] > m_Game->m_Slots.size() || Args[1] > 2) {
        ErrorReply("Usage: !comp [SLOT], [SKILL] - Skill is any of: 0, 1, 2");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Args[0]) - 1;
      uint8_t Skill = static_cast<uint8_t>(Args[1]);

      m_Game->DeleteFakePlayer(SID);
      m_Game->ComputerSlot(SID, Skill, false);
      break;
    }

    //
    // !COMPCOLOUR (computer colour change)
    //

    case HashCode("compcolor"):
    case HashCode("compcolour"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !compcolor [SLOT], [COLOR] - Color goes from 0 to 12");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: !compcolor [SLOT], [COLOR] - Color goes from 0 to 12");
        break;
      }

      if (Args[0] == 0 || Args[0] > m_Game->m_Slots.size() || Args[1] >= 12) {
        ErrorReply("Usage: !compcolor [SLOT], [COLOR] - Color goes from 0 to 12");
        break;
      }

      if (m_Game->m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Args[0]) - 1;
      uint8_t Color = static_cast<uint8_t>(Args[1]);

      if (m_Game->m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED || m_Game->m_Slots[SID].GetComputer() != 1) {
        ErrorReply("There is no computer in slot " + to_string(Args[0]));
        break;
      }

      m_Game->ColorSlot(SID, Color);
      break;
    }

    //
    // !CHANDICAP (handicap change)
    //

    case HashCode("handicap"):
    case HashCode("sethandicap"):
    case HashCode("comphandicap"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !comphandicap [SLOT], [HANDICAP] - Handicap is percent 50/60/70/80/90/100");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: !comphandicap [SLOT], [HANDICAP] - Handicap is percent 50/60/70/80/90/100");
        break;
      }

      if (Args[0] == 0 || Args[0] > m_Game->m_Slots.size() || Args[1] % 10 != 0 || !(50 <= Args[1] && Args[1] <= 100)) {
        ErrorReply("Usage: !comphandicap [SLOT], [HANDICAP] - Handicap is percent 50/60/70/80/90/100");
        break;
      }

      if (m_Game->m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Args[0]) - 1;
      uint8_t Handicap = static_cast<uint8_t>(Args[1]);

      if (m_Game->m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        ErrorReply("Slot " + to_string(Args[0]) + " is empty.");
        break;
      }

      m_Game->m_Slots[SID].SetHandicap(Handicap);
      m_Game->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      break;
    }

    //
    // !COMPRACE (computer race change)
    //

    case HashCode("comprace"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
        break;
      }

      vector<string> Args = SplitArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: !comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
        break;
      }

      uint32_t Slot = 0;
      try {
        Slot = stoul(Args[0]);
        if (Slot == 0 || Slot > m_Game->m_Slots.size()) {
          ErrorReply("Usage: !comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
          break;
        }
      } catch (...) {
        ErrorReply("Usage: !comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
        break;
      }

      if (Args[1] != "random" && Args[1] != "human" && Args[1] != "orc" && Args[1] != "undead" && Args[1] != "elf" && Args[1] != "nightelf" && Args[1] != "night elf") {
        ErrorReply("Usage: !comprace [SLOT], [RACE] - Race is human/orc/undead/elf/random");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Slot) - 1;
      string Race = Args[1];

      if (m_Game->m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      if (m_Game->m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES) {
        ErrorReply("This game has Random Races enabled.");
        break;
      }

      if (Race == "human") {
        m_Game->m_Slots[SID].SetRace(SLOTRACE_HUMAN | SLOTRACE_SELECTABLE);
      } else if (Race == "orc") {
        m_Game->m_Slots[SID].SetRace(SLOTRACE_ORC | SLOTRACE_SELECTABLE);
      } else if (Race == "elf" || Race == "nightelf" || Race == "night elf") {
        m_Game->m_Slots[SID].SetRace(SLOTRACE_NIGHTELF | SLOTRACE_SELECTABLE);
      } else if (Race == "undead") {
        m_Game->m_Slots[SID].SetRace(SLOTRACE_UNDEAD | SLOTRACE_SELECTABLE);
      } else if (Race == "random") {
        m_Game->m_Slots[SID].SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
        
      }
      m_Game->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      break;
    }

    //
    // !CHANGETEAM (forced team change)
    //

    case HashCode("setteam"):
    case HashCode("compteam"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !compteam [SLOT], [TEAM]");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 2u, 2u);
      if (Args.empty()) {
        ErrorReply("Usage: !compteam [SLOT], [TEAM]");
        break;
      }

      if (Args[0] == 0 || Args[0] > m_Game->m_Slots.size() || Args[1] >= 14) {
        ErrorReply("Usage: !compteam [SLOT], [TEAM]");
        break;
      }

      uint8_t SID = static_cast<uint8_t>(Args[0]) - 1;
      uint8_t Team = static_cast<uint8_t>(Args[1]) - 1;

      if (m_Game->m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
        ErrorReply("This map has Fixed Player Settings enabled.");
        break;
      }

      if (m_Game->m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
        ErrorReply("This map has Custom Forces enabled.");
        break;
      }

      m_Game->m_Slots[SID].SetTeam(Team);
      m_Game->m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      break;
    }

    //
    // !FILL (fill all open slots with computers)
    //

    case HashCode("fill"):
    case HashCode("compall"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      vector<uint32_t> Args = SplitNumericArgs(Payload, 1u, 1u);
      if (Payload.empty()) Args.push_back(2);

      if (Args.empty() || Args[0] > 2) {
        ErrorReply("Usage: !fill [SKILL] - Skill is any of: 0, 1, 2");
        break;
      }

      uint8_t Skill = static_cast<uint8_t>(Args[0]);
      m_Game->ComputerAllSlots(Skill);
      break;
    }

    //
    // !FAKEPLAYER
    //

    case HashCode("fakeplayer"):
    case HashCode("fp"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (!m_Game->CreateFakePlayer()) {
        ErrorReply("Cannot add another fake player");
        break;
      }

      break;
    }

    //
    // !DELETEFAKE

    case HashCode("deletefake"):
    case HashCode("deletefakes"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      if (m_Game->m_FakePlayers.empty()) {
        ErrorReply("No fake players found.");
        break;
      }

      m_Game->DeleteFakePlayers();
      break;
    }

    //
    // !FILLFAKE
    //

    case HashCode("fillfake"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      bool Success = false;
      while (m_Game->CreateFakePlayer()) {
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
    case HashCode("fppause"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot pause the game.");
        break;
      }

      if (m_Game->m_FakePlayers.empty()) {
        ErrorReply("Fake player not found.");
        break;
      }

      std::vector<uint8_t> CRC, Action;
      Action.push_back(1);
      m_Game->m_Actions.push(new CIncomingAction(m_Game->m_FakePlayers[rand() % m_Game->m_FakePlayers.size()], CRC, Action));
      break;
    }

    //
    // !RESUME
    //

    case HashCode("resume"):
    case HashCode("fpresume"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetGameLoaded())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot resume the game.");
        break;
      }

      if (m_Game->m_FakePlayers.empty()) {
        ErrorReply("Fake player not found.");
        break;
      }

      std::vector<uint8_t> CRC, Action;
      Action.push_back(2);
      m_Game->m_Actions.push(new CIncomingAction(m_Game->m_FakePlayers[0], CRC, Action));
      break;
    }

    //
    // !SP
    //

    case HashCode("sp"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      SendAll("Shuffling players");
      m_Game->ShuffleSlots();
      break;
    }

    //
    // !LOCK
    //

    case HashCode("lock"):
    {
      UseImplicitHostedGame();

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot lock the game.");
        break;
      }

      string Warning = m_Game->m_OwnerRealm.empty() ? " (Owner joined over LAN - will get removed if they leave.)" : "";
      SendAll("Game locked. Only the game owner and root admins may run game commands." + Warning);
      m_Game->m_Locked = true;
      break;
    }

    //
    // !OPENALL
    //

    case HashCode("openall"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot edit game slots.");
        break;
      }

      m_Game->OpenAllSlots();
      break;
    }

    //
    // !UNLOCK
    //

    case HashCode("unlock"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ROOTADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner nor a root admin, and therefore cannot unlock the game.");
        break;
      }

      SendAll("Game unlocked. All admins may now run game commands");
      m_Game->m_Locked = false;
      break;
    }

    //
    // !UNMUTE
    //

    case HashCode("unmute"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot unmute people.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !unmute [PLAYERNAME]");
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

    case HashCode("unmuteall"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot enable \"All\" chat channel.");
        break;
      }

      if (m_Game->m_MuteAll) {
        ErrorReply("Global chat is not muted.");
        break;
      }

      SendAll("Global chat unmuted");
      m_Game->m_MuteAll = false;
      break;
    }

    //
    // !VOTECANCEL
    //

    case HashCode("votecancel"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot cancel a votekick.");
        break;
      }

      if (m_Game->m_KickVotePlayer.empty()) {
        ErrorReply("There is no active votekick.");
        break;
      }

      SendReply("A votekick against player [" + m_Game->m_KickVotePlayer + "] has been cancelled by [" + m_FromName + "]", CHAT_SEND_ALL | CHAT_LOG_CONSOLE);
      m_Game->m_KickVotePlayer.clear();
      m_Game->m_StartedKickVoteTime = 0;
      break;
    }

    //
    // !W
    //

    case HashCode("tell"):
    case HashCode("w"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot send whispers.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !w [PLAYERNAME], [MESSAGE]");
        break;
      }

      string::size_type MessageStart = Payload.find(',');

      if (MessageStart == string::npos) {
        ErrorReply("Usage: !w [PLAYERNAME], [MESSAGE]");
        break;
      }

      string Name = TrimString(Payload.substr(0, MessageStart));
      string SubMessage = TrimString(Payload.substr(MessageStart + 1));
      if (Name.empty() || SubMessage.empty()) {
        ErrorReply("Usage: !w [PLAYERNAME], [MESSAGE]");
        break;
      }

      string PassedMessage = m_FromName + " at " + (m_HostName.empty() ? "LAN/VPN" : m_HostName) + " says to you: \"" + SubMessage + "\"";
      for (auto& bnet : m_Aura->m_BNETs) {
        if (bnet->GetIsMirror())
          continue;
        bnet->QueueChatCommand(PassedMessage, Name, true);
      }

      break;
    }
    //
    // !WHOIS
    //

    case HashCode("whois"):
    {
      UseImplicitHostedGame();

      if (!m_Game || m_Game->GetIsMirror())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot requires /whois check.");
        break;
      }

      if (Payload.empty()) {
        ErrorReply("Usage: !whois [PLAYERNAME]");
        break;
      }

      for (auto& bnet : m_Aura->m_BNETs) {
        if (bnet->GetIsMirror())
          continue;
        bnet->QueueChatCommand("/whois " + Payload);
      }

      break;
    }

    //
    // !VIRTUALHOST
    //

    case HashCode("virtualhost"):
    {
      UseImplicitHostedGame();

      if (!m_Game || !m_Game->GetIsLobby() || m_Game->GetCountDownStarted())
        break;

      if (0 == (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
        ErrorReply("You are not the game owner, and therefore cannot send whispers.");
        break;
      }

      if (Payload.empty() || Payload.length() > 15) {
        ErrorReply("Usage: !virtualhost [PLAYERNAME]");
        break;
      }

      m_Game->m_LobbyVirtualHostName = Payload;
      if (m_Game->DeleteVirtualHost()) {
        m_Game->CreateVirtualHost();
      }
      break;
    }
  }

  // TODO(IceSandslash): CLear this shit.
  if (m_Game) {
    if (0 != (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
      (*m_Output) << m_Game->GetLogPrefix() + "authorized [" + m_FromName + "] sent command [" + Command + "] with payload [" + Payload + "]" << std::endl;
    } else if (m_Game && m_Game->MatchOwnerName(m_FromName) && CommandHash != HashCode("owner")) {
      (*m_Output) << m_Game->GetLogPrefix() + "non-verified maybe-owner [" + m_FromName + "] sent command [" + Command + "] with payload [" + Payload + "]" << std::endl;
      //SendReply("You have not been verified as the owner yet. Type the following to verify your identity: sc");
    } else if (m_Player && m_Player->IsRealmVerified()) {
      (*m_Output) << m_Game->GetLogPrefix() + "verified [" + m_FromName + "] sent command [" + Command + "] with payload [" + Payload + "]" << std::endl;
    } else {
      (*m_Output) << m_Game->GetLogPrefix() + "non-verified [" + m_FromName + "] sent command [" + Command + "] with payload [" + Payload + "]" << std::endl;
    }
  } else if (m_Realm) {
    if (0 != (m_Permissions & (PERM_GAME_OWNER | PERM_BNET_ADMIN | PERM_BOT_SUDO_SPOOFABLE))) {
      (*m_Output) << m_Realm->GetLogPrefix() + "authorized [" + m_FromName + "] sent command [" + Command + "] with payload [" + Payload + "]" << std::endl;
    } else if (m_Game && m_Game->MatchOwnerName(m_FromName)) {
      (*m_Output) << m_Realm->GetLogPrefix() + "non-verified maybe-owner [" + m_FromName + "] sent command [" + Command + "] with payload [" + Payload + "]" << std::endl;
      SendReply("You have not been verified as the owner yet. Type the following to verify your identity: sc");
    } else if (m_Player && m_Player->IsRealmVerified()) {
      (*m_Output) << m_Realm->GetLogPrefix() + "verified [" + m_FromName + "] sent command [" + Command + "] with payload [" + Payload + "]" << std::endl;
    } else {
      (*m_Output) << m_Realm->GetLogPrefix() + "non-verified [" + m_FromName + "] sent command [" + Command + "] with payload [" + Payload + "]" << std::endl;
    }
  }
}


CCommandContext::~CCommandContext()
{
  m_Aura = nullptr;
  m_Game = nullptr;
  m_Player = nullptr;
  m_Realm = nullptr;
  m_IRC = nullptr;
  m_Output = nullptr;
}
