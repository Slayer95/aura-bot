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

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

 */

#include "game.h"
#include "command.h"
#include "aura.h"
#include "util.h"
#include "config.h"
#include "config_bot.h"
#include "config_game.h"
#include "socket.h"
#include "auradb.h"
#include "realm.h"
#include "map.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "stats.h"
#include "irc.h"
#include "fileutil.h"

#include <ctime>
#include <cmath>

using namespace std;

//
// CGame
//


CGame::CGame(CAura* nAura, CMap* nMap, uint8_t nGameDisplay, string& nGameName, vector<uint8_t> nPublicHostAddress, uint16_t nPublicHostPort, uint32_t nHostCounter, uint32_t nEntryKey, string nExcludedServer)
  : m_Aura(nAura),
    m_Socket(nullptr),
    m_DBBanLast(nullptr),
    m_Stats(nullptr),
    m_Slots(nMap->GetSlots()),
    m_Map(nMap),
    m_GameName(nGameName),
    m_LastGameName(nGameName),
    m_OwnerName(string()),
    m_OwnerRealm(string()),
    m_CreatorName(string()),
    m_CreatorServer(nullptr),
    m_ExcludedServer(nExcludedServer),
    m_HCLCommandString(nMap->GetMapDefaultHCL()),
    m_MapPath(nMap->GetMapPath()),
    m_GameTicks(0),
    m_CreationTime(GetTime()),
    m_LastPingTime(GetTime()),
    m_LastRefreshTime(GetTime()),
    m_LastDownloadTicks(GetTime()),
    m_LastDownloadCounterResetTicks(GetTicks()),
    m_LastCountDownTicks(0),
    m_StartedLoadingTicks(0),
    m_LastActionSentTicks(0),
    m_LastActionLateBy(0),
    m_StartedLaggingTime(0),
    m_LastLagScreenTime(0),
    m_LastOwnerSeen(GetTime()),
    m_StartedKickVoteTime(0),
    m_GameOverTime(0),
    m_LastPlayerLeaveTicks(0),
    m_LastLagScreenResetTime(0),
    m_RandomSeed(0),
    m_HostCounter(nHostCounter),
    m_EntryKey(nEntryKey),
    m_SyncCounter(0),
    m_DownloadCounter(0),
    m_CountDownCounter(0),
    m_StartPlayers(0),
    m_AutoStartMinTime(0),
    m_AutoStartMaxTime(0),
    m_AutoStartPlayers(0),
    m_PlayersWithMap(0),
    m_HostPort(0),
    m_LANHostPort(0),
    m_LANEnabled(false),
    m_PublicHostOverride(true),
    m_PublicHostAddress(nPublicHostAddress),
    m_PublicHostPort(nPublicHostPort),
    m_GameDisplay(nGameDisplay),
    m_VirtualHostPID(255),
    m_Exiting(false),
    m_Saving(false),
    m_SlotInfoChanged(0),
    m_PublicStart(false),
    m_Locked(false),
    m_RefreshError(false),
    m_MuteAll(false),
    m_MuteLobby(false),
    m_IsMirror(true),
    m_CountDownStarted(false),
    m_GameLoading(false),
    m_GameLoaded(false),
    m_Lagging(false),
    m_Desynced(false),
    m_HasMapLock(false),
    m_HadLeaver(false)
{
  m_IndexVirtualHostName = m_Aura->m_GameDefaultConfig->m_IndexVirtualHostName;
  m_LobbyVirtualHostName = m_Aura->m_GameDefaultConfig->m_LobbyVirtualHostName;
  m_Latency = m_Aura->m_GameDefaultConfig->m_Latency;
  m_SyncLimit = m_Aura->m_GameDefaultConfig->m_SyncLimit;
  m_SyncLimitSafe = m_Aura->m_GameDefaultConfig->m_SyncLimitSafe;
  m_AutoKickPing = m_Aura->m_GameDefaultConfig->m_AutoKickPing;
  m_WarnHighPing = m_Aura->m_GameDefaultConfig->m_WarnHighPing;

  m_VoteKickPercentage = m_Aura->m_GameDefaultConfig->m_VoteKickPercentage;
  m_CommandTrigger = m_Aura->m_GameDefaultConfig->m_CommandTrigger;
  m_LacksMapKickDelay = m_Aura->m_GameDefaultConfig->m_LacksMapKickDelay;
  m_NotifyJoins = m_Aura->m_GameDefaultConfig->m_NotifyJoins;
  m_PerfThreshold = m_Aura->m_GameDefaultConfig->m_PerfThreshold;
  m_LobbyNoOwnerTime = m_Aura->m_GameDefaultConfig->m_LobbyNoOwnerTime;
  m_LobbyTimeLimit = m_Aura->m_GameDefaultConfig->m_LobbyTimeLimit;
  m_NumPlayersToStartGameOver = m_Aura->m_GameDefaultConfig->m_NumPlayersToStartGameOver;
}

CGame::CGame(CAura* nAura, CMap* nMap, uint16_t nHostPort, uint16_t nLANHostPort, uint8_t nGameDisplay, string& nGameName, string& nOwnerName, string& nOwnerRealm, string& nCreatorName, CRealm* nCreatorServer)
  : m_Aura(nAura),
    m_Socket(nullptr),
    m_DBBanLast(nullptr),
    m_Stats(nullptr),
    m_Slots(nMap->GetSlots()),
    m_Map(nMap),
    m_GameName(nGameName),
    m_LastGameName(nGameName),
    m_OwnerName(nOwnerName),
    m_OwnerRealm(nOwnerRealm),
    m_CreatorName(nCreatorName),
    m_CreatorServer(nCreatorServer),
    m_ExcludedServer(string()),
    m_HCLCommandString(nMap->GetMapDefaultHCL()),
    m_MapPath(nMap->GetMapPath()),
    m_GameTicks(0),
    m_CreationTime(GetTime()),
    m_LastPingTime(GetTime()),
    m_LastRefreshTime(GetTime()),
    m_LastDownloadTicks(GetTime()),
    m_LastDownloadCounterResetTicks(GetTicks()),
    m_LastCountDownTicks(0),
    m_StartedLoadingTicks(0),
    m_LastActionSentTicks(0),
    m_LastActionLateBy(0),
    m_StartedLaggingTime(0),
    m_LastLagScreenTime(0),
    m_LastOwnerSeen(GetTime()),
    m_StartedKickVoteTime(0),
    m_GameOverTime(0),
    m_LastPlayerLeaveTicks(0),
    m_LastLagScreenResetTime(0),
    m_RandomSeed(static_cast<uint32_t>(GetTicks())),
    m_HostCounter(nAura->NextHostCounter()),
    m_EntryKey(rand()),
    m_SyncCounter(0),
    m_DownloadCounter(0),
    m_CountDownCounter(0),
    m_StartPlayers(0),
    m_AutoStartMinTime(0),
    m_AutoStartMaxTime(0),
    m_AutoStartPlayers(0),
    m_PlayersWithMap(0),
    m_HostPort(nHostPort),
    m_LANHostPort(nLANHostPort),
    m_LANEnabled(false),
    m_PublicHostOverride(false),
    m_GameDisplay(nGameDisplay),
    m_VirtualHostPID(255),
    m_Exiting(false),
    m_Saving(false),
    m_SlotInfoChanged(0),
    m_PublicStart(false),
    m_Locked(false),
    m_RefreshError(false),
    m_MuteAll(false),
    m_MuteLobby(false),
    m_IsMirror(false),
    m_CountDownStarted(false),
    m_GameLoading(false),
    m_GameLoaded(false),
    m_Lagging(false),
    m_Desynced(false),
    m_HasMapLock(false),
    m_HadLeaver(false)
{
  m_IndexVirtualHostName = m_Aura->m_GameDefaultConfig->m_IndexVirtualHostName;
  m_LobbyVirtualHostName = m_Aura->m_GameDefaultConfig->m_LobbyVirtualHostName;
  m_Latency = m_Aura->m_GameDefaultConfig->m_Latency;
  m_SyncLimit = m_Aura->m_GameDefaultConfig->m_SyncLimit;
  m_SyncLimitSafe = m_Aura->m_GameDefaultConfig->m_SyncLimitSafe;
  m_AutoKickPing = m_Aura->m_GameDefaultConfig->m_AutoKickPing;
  m_WarnHighPing = m_Aura->m_GameDefaultConfig->m_WarnHighPing;

  m_VoteKickPercentage = m_Aura->m_GameDefaultConfig->m_VoteKickPercentage;
  m_CommandTrigger = m_Aura->m_GameDefaultConfig->m_CommandTrigger;
  m_LacksMapKickDelay = m_Aura->m_GameDefaultConfig->m_LacksMapKickDelay;
  m_NotifyJoins = m_Aura->m_GameDefaultConfig->m_NotifyJoins;
  m_PerfThreshold = m_Aura->m_GameDefaultConfig->m_PerfThreshold;
  m_LobbyNoOwnerTime = m_Aura->m_GameDefaultConfig->m_LobbyNoOwnerTime;
  m_LobbyTimeLimit = m_Aura->m_GameDefaultConfig->m_LobbyTimeLimit;
  m_NumPlayersToStartGameOver = m_Aura->m_GameDefaultConfig->m_NumPlayersToStartGameOver;

  m_LANEnabled = nAura->m_GameDefaultConfig->m_LANEnabled;

  // wait time of 1 minute  = 0 empty actions required
  // wait time of 2 minutes = 1 empty action required...

  m_GProxyEmptyActions = static_cast<int64_t>(m_Aura->m_Config->m_ReconnectWaitTime) - 1;
  if (m_GProxyEmptyActions < 0)
    m_GProxyEmptyActions = 0;
  else if (m_GProxyEmptyActions > 9)
    m_GProxyEmptyActions = 9;

  // start listening for connections

  m_Socket = m_Aura->GetGameServer(m_HostPort, m_GameName);
  if (!m_Socket) {
    m_Exiting = true;
  }

  if (!m_Map->GetMapData()->empty()) {
    m_Aura->m_BusyMaps.insert(m_Map->GetMapLocalPath());
    m_HasMapLock = true;
  }
}

CGame::~CGame()
{
  if (m_HasMapLock) {
    m_Aura->m_BusyMaps.erase(m_Map->GetMapLocalPath());
    bool IsTooLarge = ByteArrayToUInt32(m_Map->GetMapSize(), false) > m_Aura->m_Config->m_MaxSavedMapSize * 1024;
    if (IsTooLarge && m_Aura->m_BusyMaps.find(m_Map->GetMapLocalPath()) == m_Aura->m_BusyMaps.end()) {
      m_Map->DeleteFile();
    }
    m_HasMapLock = false;
  }
  delete m_Map;

  for (auto& entry : m_SyncPlayers) {
    entry.second.clear();
  }
  m_SyncPlayers.clear();

  for (auto& player : m_Players)
    delete player;

  // store the CDBGamePlayers in the database
  // add non-dota stats

  for (auto& player : m_DBGamePlayers)
    m_Aura->m_DB->GamePlayerAdd(player->GetName(), player->GetLoadingTime(), m_GameTicks / 1000, player->GetLeft());

  // store the dota stats in the database

  if (m_Stats)
    m_Stats->Save(m_Aura, m_Aura->m_DB);

  while (!m_Actions.empty())
  {
    delete m_Actions.front();
    m_Actions.pop();
  }

  for (auto& player : m_DBGamePlayers)
    delete player;

  for (auto& ban : m_DBBans)
    delete ban;

  delete m_Stats;

  if (m_Aura->m_SudoGame == this) {
    m_Aura->m_SudoGame = nullptr;
  }
}

CGameProtocol* CGame::GetProtocol() const
{
  return m_Aura->m_GameProtocol;
}

int64_t CGame::GetNextTimedActionTicks() const
{
  // return the number of ticks (ms) until the next "timed action", which for our purposes is the next game update
  // the main Aura++ loop will make sure the next loop update happens at or before this value
  // note: there's no reason this function couldn't take into account the game's other timers too but they're far less critical
  // warning: this function must take into account when actions are not being sent (e.g. during loading or lagging)

  if (!m_GameLoaded || m_Lagging)
    return 50;

  const int64_t TicksSinceLastUpdate = GetTicks() - m_LastActionSentTicks;

  if (TicksSinceLastUpdate > m_Latency - m_LastActionLateBy)
    return 0;
  else
    return m_Latency - m_LastActionLateBy - TicksSinceLastUpdate;
}

uint32_t CGame::GetSlotsOccupied() const
{
  uint32_t NumSlotsOccupied = 0;

  for (const auto& slot : m_Slots)
  {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED)
      ++NumSlotsOccupied;
  }

  return NumSlotsOccupied;
}

uint32_t CGame::GetSlotsOpen() const
{
  uint32_t NumSlotsOpen = 0;

  for (const auto& slot : m_Slots)
  {
    if (slot.GetSlotStatus() == SLOTSTATUS_OPEN)
      ++NumSlotsOpen;
  }

  return NumSlotsOpen;
}

uint32_t CGame::GetNumConnectionsOrFake() const
{
  uint32_t NumHumanPlayers = m_FakePlayers.size();

  for (const auto& player : m_Players) {
    if (player->GetLeftMessageSent())
      continue;

    ++NumHumanPlayers;
  }

  return NumHumanPlayers;
}

uint32_t CGame::GetNumHumanPlayers() const
{
  uint32_t NumHumanPlayers = 0;

  for (const auto& player : m_Players) {
    if (player->GetLeftMessageSent())
      continue;
    if (player->GetObserver())
      continue;

    ++NumHumanPlayers;
  }

  return NumHumanPlayers;
}

string CGame::GetMapFileName() const
{
  size_t LastSlash = m_MapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_MapPath;
  }
  return m_MapPath.substr(LastSlash + 1);
}

string CGame::GetDescription() const
{
  if (m_IsMirror)
     return "[" + GetMapFileName() + "] (Mirror) \"" + m_GameName + "\"";

  string Description = "[" + GetMapFileName() + "] \"" + m_GameName + "\" - " + m_OwnerName + " - " + to_string(GetNumHumanPlayers()) + "/" + to_string(m_GameLoading || m_GameLoaded ? m_StartPlayers : m_Slots.size());

  if (m_GameLoading || m_GameLoaded)
    Description += " : " + to_string((m_GameTicks / 1000) / 60) + "min";
  else
    Description += " : " + to_string((GetTime() - m_CreationTime) / 60) + "min";

  return Description;
}

string CGame::GetCategory() const
{
  if (m_GameLoading || m_GameLoaded)
    return "GAME";

  return "LOBBY";
}

string CGame::GetLogPrefix() const
{
  string MinString = to_string((m_GameTicks / 1000) / 60);
  string SecString = to_string((m_GameTicks / 1000) % 60);

  if (MinString.size() == 1)
    MinString.insert(0, "0");

  if (SecString.size() == 1)
    SecString.insert(0, "0");

  return "[" + GetCategory() + ": " + GetGameName() + "] (" + MinString + ":" + SecString + ") ";
}

string CGame::GetPlayers() const
{
  string Players;

  for (const auto& player : m_Players)
  {
    const uint8_t SID = GetSIDFromPID(player->GetPID());

    if (player->GetLeftMessageSent() == false && m_Slots[SID].GetTeam() != m_Aura->m_MaxSlots)
      Players += player->GetName() + ", ";
  }

  const size_t size = Players.size();

  if (size > 2)
    Players = Players.substr(0, size - 2);

  return Players;
}

string CGame::GetObservers() const
{
  string Observers;

  for (const auto& player : m_Players)
  {
    const uint8_t SID = GetSIDFromPID(player->GetPID());

    if (player->GetLeftMessageSent() == false && m_Slots[SID].GetTeam() == m_Aura->m_MaxSlots)
      Observers += player->GetName() + ", ";
  }

  const size_t size = Observers.size();

  if (size > 2)
    Observers = Observers.substr(0, size - 2);

  return Observers;
}

uint32_t CGame::SetFD(void* fd, void* send_fd, int32_t* nfds)
{
  uint32_t NumFDs = 0;

  for (auto& player : m_Players)
  {
    player->GetSocket()->SetFD(static_cast<fd_set*>(fd), static_cast<fd_set*>(send_fd), nfds);
    ++NumFDs;
  }

  return NumFDs;
}

bool CGame::Update(void* fd, void* send_fd)
{
  const int64_t Time = GetTime(), Ticks = GetTicks();

  // ping every 5 seconds
  // changed this to ping during game loading as well to hopefully fix some problems with people disconnecting during loading
  // changed this to ping during the game as well

  if (Time - m_LastPingTime >= 5)
  {
    // note: we must send pings to players who are downloading the map because Warcraft III disconnects from the lobby if it doesn't receive a ping every ~90 seconds
    // so if the player takes longer than 90 seconds to download the map they would be disconnected unless we keep sending pings

    SendAll(GetProtocol()->SEND_W3GS_PING_FROM_HOST());

    // we also broadcast the game to the local network every 5 seconds so we hijack this timer for our nefarious purposes
    // however we only want to broadcast if the countdown hasn't started

    if (!m_CountDownStarted && m_LANEnabled)
    {
      // construct a fixed host counter which will be used to identify players from this realm
      // the fixed host counter's highest-order byte will contain a 8 bit ID (0-255)
      // the rest of the fixed host counter will contain the 24 least significant bits of the actual host counter
      // since we're destroying 8 bits of information here the actual host counter should not be greater than 2^24 which is a reasonable assumption
      // when a player joins a game we can obtain the ID from the received host counter
      // note: LAN broadcasts use an ID of 0, battle.net refreshes use IDs of 16-255, the rest are reserved

      if (m_Aura->m_Net->m_UDPServerEnabled && m_Aura->m_Config->m_UDPInfoStrictMode) {
        LANBroadcastGameRefresh();
      } else {
        LANBroadcastGameInfo();
      }
    }

    m_LastPingTime = Time;
  }

  // update players

  for (auto i = begin(m_Players); i != end(m_Players);)
  {
    if ((*i)->Update(fd))
    {
      EventPlayerDeleted(*i);
      delete *i;
      i = m_Players.erase(i);
    }
    else
      ++i;
  }

  // keep track of the largest sync counter (the number of keepalive packets received by each player)
  // if anyone falls behind by more than m_SyncLimit keepalives we start the lag screen

  if (m_GameLoaded)
  {
    // check if anyone has started lagging
    // we consider a player to have started lagging if they're more than m_SyncLimit keepalives behind

    if (!m_Lagging) {
      string LaggingString;
      float MaxAbsFramesBehind = 0.0;

      for (auto& player : m_Players) {
        if (player->GetObserver())
          continue;
        uint32_t FramesBehind = m_SyncCounter - player->GetSyncCounter();
        if (m_SyncCounter > player->GetSyncCounter() && FramesBehind > m_SyncLimit) {
          player->SetLagging(true);
          player->SetStartedLaggingTicks(Ticks);
          m_Lagging            = true;
          m_StartedLaggingTime = Time;

          if (LaggingString.empty())
            LaggingString = player->GetName();
          else
            LaggingString += ", " + player->GetName();

          if (FramesBehind > MaxAbsFramesBehind)
            MaxAbsFramesBehind = FramesBehind;
        }
      }

      if (m_Lagging) {
        // start the lag screen

        Print(GetLogPrefix() + "started lagging on [" + LaggingString + "] (" + to_string(static_cast<uint32_t>(MaxAbsFramesBehind)) + " frames)");
        SendAll(GetProtocol()->SEND_W3GS_START_LAG(m_Players));

        // reset everyone's drop vote

        for (auto& player : m_Players)
          player->SetDropVote(false);

        m_LastLagScreenResetTime = Time;
      }
    }

    if (m_Lagging) {
      bool UsingGProxy = false;
      for (auto& player : m_Players) {
        if (player->GetGProxy()) {
          UsingGProxy = true;
          break;
        }
      }

      int64_t WaitTime = 60;

      if (UsingGProxy)
        WaitTime = (m_GProxyEmptyActions + 1) * 60;

      if (Time - m_StartedLaggingTime >= WaitTime)
        StopLaggers("was automatically dropped after " + to_string(WaitTime) + " seconds");

      // we cannot allow the lag screen to stay up for more than ~65 seconds because Warcraft III disconnects if it doesn't receive an action packet at least this often
      // one (easy) solution is to simply drop all the laggers if they lag for more than 60 seconds
      // another solution is to reset the lag screen the same way we reset it when using load-in-game

      if (Time - m_LastLagScreenResetTime >= 60) {
        for (auto& _i : m_Players) {
          // stop the lag screen

          for (auto& player : m_Players) {
            if (player->GetLagging())
              Send(_i, GetProtocol()->SEND_W3GS_STOP_LAG(player));
          }

          // send an empty update
          // this resets the lag screen timer

          if (UsingGProxy && !(_i)->GetGProxy()) {
            // we must send additional empty actions to non-GProxy++ players
            // GProxy++ will insert these itself so we don't need to send them to GProxy++ players
            // empty actions are used to extend the time a player can use when reconnecting

            for (uint8_t j = 0; j < m_GProxyEmptyActions; ++j)
              Send(_i, GetProtocol()->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
          }

          Send(_i, GetProtocol()->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));

          // start the lag screen

          Send(_i, GetProtocol()->SEND_W3GS_START_LAG(m_Players));
        }

        // Warcraft III doesn't seem to respond to empty actions

        m_LastLagScreenResetTime = Time;
      }

      // check if anyone has stopped lagging normally
      // we consider a player to have stopped lagging if they're less than m_SyncLimitSafe keepalives behind

      uint32_t PlayersStillLagging = 0;
      for (auto& player : m_Players) {
        if (!player->GetLagging()) {
          continue;
        }

        if (player->GetGProxyDisconnectNoticeSent()) {
          ++PlayersStillLagging;
          continue;
        }

        uint32_t FramesBehind = m_SyncCounter - player->GetSyncCounter();
        if (m_SyncCounter > player->GetSyncCounter() && FramesBehind >= m_SyncLimitSafe) {
          ++PlayersStillLagging;
        } else {
          Print(GetLogPrefix() + "stopped lagging on [" + player->GetName() + "] (" + to_string(player->GetSyncCounter()) + "/" + to_string(m_SyncCounter) + ")");
          SendAll(GetProtocol()->SEND_W3GS_STOP_LAG(player));
          player->SetLagging(false);
          player->SetStartedLaggingTicks(0);
        }
      }

      if (PlayersStillLagging == 0) {
        m_Lagging = false;
      }

      // reset m_LastActionSentTicks because we want the game to stop running while the lag screen is up
      m_LastActionSentTicks = Ticks;

      // keep track of the last lag screen time so we can avoid timing out players
      m_LastLagScreenTime = Time;
    }
  }

  // send actions every m_Latency milliseconds
  // actions are at the heart of every Warcraft 3 game but luckily we don't need to know their contents to relay them
  // we queue player actions in EventPlayerAction then just resend them in batches to all players here

  if (m_GameLoaded && !m_Lagging && Ticks - m_LastActionSentTicks >= m_Latency - m_LastActionLateBy)
    SendAllActions();

  // end the game if there aren't any players left

  if (m_Players.empty() && (m_GameLoading || m_GameLoaded))
  {
    Print(GetLogPrefix() + "is over (no players left)");
    Print(GetLogPrefix() + "saving game data to database");

    return true;
  }

  // check if the game is loaded

  if (m_GameLoading) {
    bool FinishedLoading = true;
    for (auto& player : m_Players) {
      if (!player->GetFinishedLoading()) {
        FinishedLoading = false;
        break;
      }
    }

    if (FinishedLoading) {
      m_LastActionSentTicks = Ticks;
      m_GameLoading         = false;
      m_GameLoaded          = true;
      EventGameLoaded();
    }
  }

  if ((!m_GameLoading && !m_GameLoaded) && (m_SlotInfoChanged & SLOTS_ALIGNMENT_CHANGED)) {
    SendAllSlotInfo(); // Updates m_PlayersWithMap
    m_SlotInfoChanged &= ~SLOTS_ALIGNMENT_CHANGED;
  }

  // start countdown timer if autostart is due
  if (!m_CountDownStarted && (m_AutoStartMaxTime != 0 || m_AutoStartPlayers != 0) && m_PlayersWithMap >= 2) {
    bool IsTriggerTime = 0 < m_AutoStartMinTime && m_AutoStartMinTime <= Time;
    bool IsTriggerPlayers = 0 < m_AutoStartPlayers && m_AutoStartPlayers <= m_PlayersWithMap;
    bool ShouldStart = false;
    if (IsTriggerTime || IsTriggerPlayers) {
      ShouldStart = IsTriggerTime && IsTriggerPlayers || 0 < m_AutoStartMaxTime && m_AutoStartMaxTime <= Time;
      if (!ShouldStart) {
        ShouldStart = 0 == (IsTriggerTime ? m_AutoStartPlayers : m_AutoStartMinTime);
      }
    }

    if (ShouldStart) {
      StartCountDown(true);
    }
  }

  // start the gameover timer if there's only a configured number of players left
  uint32_t RemainingPlayers = GetNumHumanPlayers() + m_FakePlayers.size();
  if (RemainingPlayers != m_StartPlayers && m_GameOverTime == 0 && (m_GameLoading || m_GameLoaded)) {
    if (RemainingPlayers == 0 || RemainingPlayers <= m_NumPlayersToStartGameOver) {
      m_GameOverTime = Time;
      Print(GetLogPrefix() + "gameover timer started (" + std::to_string(RemainingPlayers) + " player(s) left)");
      if (GetNumHumanPlayers() > 0) {
        SendAllChat("Gameover timer started");
      }
    }
  }

  // finish the gameover timer

  if (m_GameOverTime != 0 && Time - m_GameOverTime >= 60) {
    for (auto& player : m_Players) {
      if (!player->GetDeleteMe()) {
        Print(GetLogPrefix() + "is over (gameover timer finished)");
        StopPlayers("was disconnected (gameover timer finished)");
        break;
      }
    }
  }

  // expire the votekick

  if (!m_KickVotePlayer.empty() && Time - m_StartedKickVoteTime >= 60) {
    Print(GetLogPrefix() + "votekick against player [" + m_KickVotePlayer + "] expired");
    SendAllChat("A votekick against player [" + m_KickVotePlayer + "] has expired");
    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }

  if (m_GameLoaded)
    return m_Exiting;

  // refresh every 3 seconds

  if (!m_RefreshError && !m_CountDownStarted && m_GameDisplay == GAME_PUBLIC && GetSlotsOpen() > 0 && Time - m_LastRefreshTime >= 3) {
    // send a game refresh packet to each battle.net connection

    for (auto& bnet : m_Aura->m_Realms) {
      if (bnet->GetInputID() == m_ExcludedServer)
        continue;

      if (m_IsMirror && bnet->GetIsMirror())
        continue;

      // don't queue a game refresh message if the queue contains more than 1 packet because they're very low priority
      if (bnet->GetOutPacketsQueued() > 1)
        continue;

      bnet->QueueGameRefresh(m_GameDisplay, m_GameName, m_Map, m_HostCounter, !m_IsMirror);
    }

    m_LastRefreshTime = Time;
  }

  // send more map data

  if (!m_GameLoading && !m_GameLoaded && Ticks - m_LastDownloadCounterResetTicks >= 1000)
  {
    // hackhack: another timer hijack is in progress here
    // since the download counter is reset once per second it's a great place to update the slot info if necessary

    if (m_SlotInfoChanged & SLOTS_DOWNLOAD_PROGRESS_CHANGED) {
      SendAllSlotInfo();
      m_SlotInfoChanged &= ~SLOTS_DOWNLOAD_PROGRESS_CHANGED;
    }

    m_DownloadCounter               = 0;
    m_LastDownloadCounterResetTicks = Ticks;
  }

  if (!m_GameLoading && Ticks - m_LastDownloadTicks >= 100)
  {
    uint32_t Downloaders = 0;

    for (auto& player : m_Players)
    {
      if (player->GetDownloadStarted() && !player->GetDownloadFinished())
      {
        ++Downloaders;

        if (m_Aura->m_Config->m_MaxDownloaders > 0 && Downloaders > m_Aura->m_Config->m_MaxDownloaders)
          break;

        // send up to 100 pieces of the map at once so that the download goes faster
        // if we wait for each MAPPART packet to be acknowledged by the client it'll take a long time to download
        // this is because we would have to wait the round trip time (the ping time) between sending every 1442 bytes of map data
        // doing it this way allows us to send at least 1.4 MB in each round trip interval which is much more reasonable
        // the theoretical throughput is [1.4 MB * 1000 / ping] in KB/sec so someone with 100 ping (round trip ping, not LC ping) could download at 1400 KB/sec
        // note: this creates a queue of map data which clogs up the connection when the client is on a slower connection (e.g. dialup)
        // in this case any changes to the lobby are delayed by the amount of time it takes to send the queued data (i.e. 140 KB, which could be 30 seconds or more)
        // for example, players joining and leaving, slot changes, chat messages would all appear to happen much later for the low bandwidth player
        // note: the throughput is also limited by the number of times this code is executed each second
        // e.g. if we send the maximum amount (1.4 MB) 10 times per second the theoretical throughput is 1400 KB/sec
        // therefore the maximum throughput is 14 MB/sec, and this value slowly diminishes as the player's ping increases
        // in addition to this, the throughput is limited by the configuration value bot_maxdownloadspeed
        // in summary: the actual throughput is MIN( 1.4 * 1000 / ping, 1400, bot_maxdownloadspeed ) in KB/sec assuming only one player is downloading the map

        const uint32_t MapSize = ByteArrayToUInt32(m_Map->GetMapSize(), false);

        while (player->GetLastMapPartSent() < player->GetLastMapPartAcked() + 1442 * m_Aura->m_Config->m_MaxParallelMapPackets && player->GetLastMapPartSent() < MapSize)
        {
          if (player->GetLastMapPartSent() == 0)
          {
            // overwrite the "started download ticks" since this is the first time we've sent any map data to the player
            // prior to this we've only determined if the player needs to download the map but it's possible we could have delayed sending any data due to download limits

            player->SetStartedDownloadingTicks(Ticks);
          }

          // limit the download speed if we're sending too much data
          // the download counter is the # of map bytes downloaded in the last second (it's reset once per second)

          if (m_Aura->m_Config->m_MaxUploadSpeed > 0 && m_DownloadCounter > m_Aura->m_Config->m_MaxUploadSpeed * 1024)
            break;

          Send(player, GetProtocol()->SEND_W3GS_MAPPART(GetHostPID(), player->GetPID(), player->GetLastMapPartSent(), m_Map->GetMapData()));
          player->SetLastMapPartSent(player->GetLastMapPartSent() + 1442);
          m_DownloadCounter += 1442;
        }
      }
    }

    m_LastDownloadTicks = Ticks;
  }

  // countdown every 500 ms

  if (m_CountDownStarted && !m_GameLoading && !m_GameLoaded && Ticks - m_LastCountDownTicks >= 500) {
    if (m_CountDownCounter > 0) {
      // we use a countdown counter rather than a "finish countdown time" here because it might alternately round up or down the count
      // this sometimes resulted in a countdown of e.g. "6 5 3 2 1" during my testing which looks pretty dumb
      // doing it this way ensures it's always "5 4 3 2 1" but each int32_terval might not be *exactly* the same length

      SendAllChat(to_string(m_CountDownCounter--) + ". . .");
    } else {
      if (GetNumConnectionsOrFake() >= 1) {
        EventGameStarted();
      } else {
        // Some operations may remove fake players during countdown.
        // Ensure that the game doesn't start if there are neither real nor fake players.
        // (If a player leaves or joins, the countdown is aborted elsewhere.)
        Print(GetLogPrefix() + "countdown aborted - lobby is empty.");
        m_CountDownStarted = false;
        m_CountDownCounter = 0;
      }
    }

    m_LastCountDownTicks = Ticks;
  }

  // release abandoned lobbies, so other players can take ownership
  if (!m_GameLoading && !m_GameLoaded && !m_IsMirror && (m_LobbyTimeLimit > 0 || m_LobbyNoOwnerTime > 0)) {
    // check if there is an owner in the game
    if (HasOwnerInGame()) {
      m_LastOwnerSeen = Time;
    } else {
      // check if we've hit the time limit
      int64_t TimeSinceSeenOwner = Time - m_LastOwnerSeen;
      if (TimeSinceSeenOwner > 60 * static_cast<int64_t>(m_LobbyNoOwnerTime)) {
        if (!m_OwnerName.empty()) {
          ReleaseOwner();
        }
      }
      if (TimeSinceSeenOwner > 60 * static_cast<int64_t>(m_LobbyTimeLimit)) {
        Print(GetLogPrefix() + "is over (lobby time limit hit)");
        return true;
      }
    }
  }

  // create the virtual host player

  if (!m_GameLoading && !m_GameLoaded && GetSlotsOpen() > 0)
    CreateVirtualHost();

  return m_Exiting;
}

void CGame::UpdatePost(void* send_fd)
{
  // we need to manually call DoSend on each player now because CGamePlayer :: Update doesn't do it
  // this is in case player 2 generates a packet for player 1 during the update but it doesn't get sent because player 1 already finished updating
  // in reality since we're queueing actions it might not make a big difference but oh well

  for (auto& player : m_Players)
    player->GetSocket()->DoSend(static_cast<fd_set*>(send_fd));
}

void CGame::Send(CPotentialPlayer* player, const std::vector<uint8_t>& data) const
{
  if (player)
    player->Send(data);
}

void CGame::Send(CGamePlayer* player, const std::vector<uint8_t>& data) const
{
  if (player)
    player->Send(data);
}

void CGame::Send(uint8_t PID, const std::vector<uint8_t>& data) const
{
  Send(GetPlayerFromPID(PID), data);
}

void CGame::Send(const std::vector<uint8_t>& PIDs, const std::vector<uint8_t>& data) const
{
  for (auto& PID : PIDs)
    Send(PID, data);
}

void CGame::SendAll(const std::vector<uint8_t>& data) const
{
  for (auto& player : m_Players)
    player->Send(data);
}

void CGame::SendChat(uint8_t fromPID, CGamePlayer* player, const string& message) const
{
  // send a private message to one player - it'll be marked [Private] in Warcraft 3
  if (message.empty())
    return;

  if (player)
  {
    if (!m_GameLoading && !m_GameLoaded)
    {
      if (message.size() > 254)
        Send(player, GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, CreateByteArray(player->GetPID()), 16, std::vector<uint8_t>(), message.substr(0, 254)));
      else
        Send(player, GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, CreateByteArray(player->GetPID()), 16, std::vector<uint8_t>(), message));
    }
    else
    {
      uint8_t ExtraFlags[] = {3, 0, 0, 0};

      // based on my limited testing it seems that the extra flags' first byte contains 3 plus the recipient's colour to denote a private message

      uint8_t SID = GetSIDFromPID(player->GetPID());

      if (SID < m_Slots.size())
        ExtraFlags[0] = 3 + m_Slots[SID].GetColour();

      if (message.size() > 127)
        Send(player, GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, CreateByteArray(player->GetPID()), 32, CreateByteArray(ExtraFlags, 4), message.substr(0, 127)));
      else
        Send(player, GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, CreateByteArray(player->GetPID()), 32, CreateByteArray(ExtraFlags, 4), message));
    }
  }
}

void CGame::SendChat(uint8_t fromPID, uint8_t toPID, const string& message) const
{
  SendChat(fromPID, GetPlayerFromPID(toPID), message);
}

void CGame::SendChat(CGamePlayer* player, const string& message) const
{
  SendChat(GetHostPID(), player, message);
}

void CGame::SendChat(uint8_t toPID, const string& message) const
{
  SendChat(GetHostPID(), toPID, message);
}

void CGame::SendAllChat(uint8_t fromPID, const string& message) const
{
  if (message.empty())
    return;

  // send a public message to all players - it'll be marked [All] in Warcraft 3

  if (GetNumHumanPlayers() > 0) {
    if (!m_GameLoading && !m_GameLoaded)
    {
      if (message.size() > 254)
        SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, GetPIDs(), 16, std::vector<uint8_t>(), message.substr(0, 254)));
      else
        SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, GetPIDs(), 16, std::vector<uint8_t>(), message));
    } else {
      if (message.size() > 127)
        SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, GetPIDs(), 32, CreateByteArray(static_cast<uint32_t>(0), false), message.substr(0, 127)));
      else
        SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, GetPIDs(), 32, CreateByteArray(static_cast<uint32_t>(0), false), message));
    }
  }
}

void CGame::SendAllChat(const string& message) const
{
  SendAllChat(GetHostPID(), message);
}

void CGame::SendAllSlotInfo()
{
  if (m_GameLoading || m_GameLoaded)
    return;

  SendAll(GetProtocol()->SEND_W3GS_SLOTINFO(m_Slots, m_RandomSeed, m_Map->GetMapLayoutStyle(), m_Map->GetMapNumPlayers()));

  m_PlayersWithMap = 0;
  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      CGamePlayer* Player = GetPlayerFromSID(i);
      if (!Player || Player->GetHasMap() && !Player->GetObserver())
        ++m_PlayersWithMap;
    }
    m_SlotInfoChanged = 0;
  }
}

string CGame::GetAutoStartText() const
{
  if (m_AutoStartMaxTime == 0 && m_AutoStartPlayers == 0)
    return "Autostart is not set.";

  int64_t Time = GetTime();
  string MinTimeString;
  string MaxTimeString;
  if (m_AutoStartMinTime != 0) {
    int64_t RemainingSeconds = m_AutoStartMinTime - Time;
    if (RemainingSeconds < 0)
      RemainingSeconds = 0;
    int64_t RemainingMinutes = RemainingSeconds / 60;
    RemainingSeconds = RemainingSeconds % 60;
    if (RemainingMinutes == 0) {
      MinTimeString = to_string(RemainingSeconds) + " seconds";
    } else if (RemainingSeconds == 0) {
      MinTimeString = to_string(RemainingMinutes) + " minutes";
    } else {
      MinTimeString = to_string(RemainingMinutes) + " min " + to_string(RemainingSeconds) + "s.";
    }
  }
  if (m_AutoStartMaxTime != 0) {
    int64_t RemainingSeconds = m_AutoStartMaxTime - Time;
    if (RemainingSeconds < 0)
      RemainingSeconds = 0;
    int64_t RemainingMinutes = RemainingSeconds / 60;
    RemainingSeconds = RemainingSeconds % 60;
    if (RemainingMinutes == 0) {
      MaxTimeString = to_string(RemainingSeconds) + " seconds.";
    } else if (RemainingSeconds == 0) {
      MaxTimeString = to_string(RemainingMinutes) + " minutes";
    } else {
      MaxTimeString = to_string(RemainingMinutes) + " min " + to_string(RemainingSeconds) + "s.";
    }
  }

  if (m_AutoStartPlayers != 0 && m_AutoStartMaxTime != 0) {
    if (m_AutoStartMinTime != 0) {
      return "Autostarts at " + to_string(m_AutoStartPlayers) + "/" + to_string(m_Slots.size()) + " after " + MinTimeString + " Max " + MaxTimeString;
    } else {
      return "Autostarts at " + to_string(m_AutoStartPlayers) + "/" + to_string(m_Slots.size()) + ". Max " + MaxTimeString;
    }
  } else if (m_AutoStartPlayers != 0) {
    if (m_AutoStartMinTime != 0) {
      return "Autostarts at " + to_string(m_AutoStartPlayers) + "/" + to_string(m_Slots.size()) + " after " + MinTimeString;
    } else {
      return "Autostarts at " + to_string(m_AutoStartPlayers) + "/" + to_string(m_Slots.size()) + " (AI are counted, but observers are not).";
    }
  } else {
    return "Autostarts in " + MaxTimeString;
  }
}

void CGame::SendAllAutoStart() const
{
  SendAllChat(GetAutoStartText());
}

void CGame::SendVirtualHostPlayerInfo(CPotentialPlayer* player) const
{
  if (m_VirtualHostPID == 255)
    return;

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  Send(player, GetProtocol()->SEND_W3GS_PLAYERINFO(m_VirtualHostPID, m_LobbyVirtualHostName, IP, IP));
}

void CGame::SendVirtualHostPlayerInfo(CGamePlayer* player) const
{
  if (m_VirtualHostPID == 255)
    return;

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  Send(player, GetProtocol()->SEND_W3GS_PLAYERINFO(m_VirtualHostPID, m_LobbyVirtualHostName, IP, IP));
}

void CGame::SendFakePlayersInfo(CPotentialPlayer* player) const
{
  if (m_FakePlayers.empty())
    return;

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  for (auto& fakeplayer : m_FakePlayers)
    Send(player, GetProtocol()->SEND_W3GS_PLAYERINFO(fakeplayer, "Placeholder[" + to_string(fakeplayer) + "]", IP, IP));
}

void CGame::SendFakePlayersInfo(CGamePlayer* player) const
{
  if (m_FakePlayers.empty())
    return;

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  for (auto& fakeplayer : m_FakePlayers)
    Send(player, GetProtocol()->SEND_W3GS_PLAYERINFO(fakeplayer, "Placeholder[" + to_string(fakeplayer) + "]", IP, IP));
}

void CGame::SendJoinedPlayersInfo(CPotentialPlayer* potential) const
{
  for (auto& otherPlayer : m_Players) {
    if (otherPlayer->GetLeftMessageSent())
      continue;
    Send(potential,
      GetProtocol()->SEND_W3GS_PLAYERINFO(otherPlayer->GetPID(), otherPlayer->GetName(), otherPlayer->GetExternalIP(), otherPlayer->GetInternalIP())
    );
  }
}

void CGame::SendJoinedPlayersInfo(CGamePlayer* player) const
{
  for (auto& otherPlayer : m_Players) {
    if (otherPlayer == player)
      continue;
    if (otherPlayer->GetLeftMessageSent())
      continue;
    Send(player,
      GetProtocol()->SEND_W3GS_PLAYERINFO(otherPlayer->GetPID(), otherPlayer->GetName(), otherPlayer->GetExternalIP(), otherPlayer->GetInternalIP())
    );
  }
}

void CGame::SendIncomingPlayerInfo(CGamePlayer* player) const
{
  for (auto& otherPlayer : m_Players) {
    if (otherPlayer == player)
      continue;
    if (otherPlayer->GetLeftMessageSent())
      break;
    otherPlayer->Send(GetProtocol()->SEND_W3GS_PLAYERINFO(player->GetPID(), player->GetName(), player->GetExternalIP(), player->GetInternalIP()));
  }
}

void CGame::SendWelcomeMessage(CGamePlayer *player) const
{
  for (size_t i = 0; i < m_Aura->m_Config->m_Greeting.size(); i++) {
    int matchIndex;
    string Line = m_Aura->m_Config->m_Greeting[i];
    if (Line.substr(0, 12) == "{SHORTDESC?}") {
      if (m_Map->GetMapShortDesc().empty()) {
        continue;
      }
      Line = Line.substr(12);
    }
    if (Line.substr(0, 12) == "{SHORTDESC!}") {
      if (!m_Map->GetMapShortDesc().empty()) {
        continue;
      }
      Line = Line.substr(12);
    }
    if (Line.substr(0, 6) == "{URL?}") {
      if (m_Map->GetMapSiteURL().empty()) {
        continue;
      }
      Line = Line.substr(6);
    }
    if (Line.substr(0, 6) == "{URL!}") {
      if (!m_Map->GetMapSiteURL().empty()) {
        continue;
      }
      Line = Line.substr(6);
    }
    if (Line.substr(0, 11) == "{FILENAME?}") {
      size_t LastSlash = m_MapPath.rfind('\\');
      if (LastSlash == string::npos || LastSlash > m_MapPath.length() - 6) {
        continue;
      }
      Line = Line.substr(11);
    }
    if (Line.substr(0, 12) == "{AUTOSTART?}") {
      if (m_AutoStartMinTime == 0 && m_AutoStartPlayers == 0) {
        continue;
      }
      Line = Line.substr(12);
    }
    if (Line.substr(0, 11) == "{FILENAME!}") {
      size_t LastSlash = m_MapPath.rfind('\\');
      if (!(LastSlash == string::npos || LastSlash > m_MapPath.length() - 6)) {
        continue;
      }
      Line = Line.substr(11);
    }
    if (Line.substr(0, 10) == "{CREATOR?}") {
      if (m_CreatorName.empty()) {
        continue;
      }
      Line = Line.substr(10);
    }
    if (Line.substr(0, 10) == "{CREATOR!}") {
      if (!m_CreatorName.empty()) {
        continue;
      }
      Line = Line.substr(10);
    }
    if (Line.substr(0, 8) == "{OWNER?}") {
      if (m_OwnerName.empty()) {
        continue;
      }
      Line = Line.substr(8);
    }
    if (Line.substr(0, 8) == "{OWNER!}") {
      if (!m_OwnerName.empty()) {
        continue;
      }
      Line = Line.substr(8);
    }
    if (Line.substr(0, 16) == "{CREATOR==OWNER}" || Line.substr(0, 16) == "{OWNER==CREATOR}") {
      if (m_OwnerName != m_CreatorName) {
        continue;
      }
      Line = Line.substr(16);
    }
    if (Line.substr(0, 16) == "{CREATOR!=OWNER}" || Line.substr(0, 16) == "{OWNER!=CREATOR}") {
      if (m_OwnerName == m_CreatorName) {
        continue;
      }
      Line = Line.substr(16);
    }
    while ((matchIndex = Line.find("{CREATOR}")) != string::npos) {
      Line.replace(matchIndex, 9, m_CreatorName);
    }
    while ((matchIndex = Line.find("{OWNER}")) != string::npos) {
      Line.replace(matchIndex, 7, m_OwnerName);
    }
    while ((matchIndex = Line.find("{OWNERREALM}")) != string::npos) {
      Line.replace(matchIndex, 12, m_OwnerRealm.empty() ? "@@@LAN/VPN" : ("@" + m_OwnerRealm));
    }
    while ((matchIndex = Line.find("{HOSTREALM}")) != string::npos) {
      Line.replace(matchIndex, 11, !m_CreatorServer ? "@@@LAN/VPN" : ("@" + m_CreatorServer->GetCanonicalDisplayName()));
    }
    while ((matchIndex = Line.find("{TRIGGER}")) != string::npos) {
      Line.replace(matchIndex, 9, string(1, m_CommandTrigger));
    }
    while ((matchIndex = Line.find("{URL}")) != string::npos) {
      Line.replace(matchIndex, 5, m_Map->GetMapSiteURL());
    }
    while ((matchIndex = Line.find("{FILENAME}")) != string::npos) {
      Line.replace(matchIndex, 10, m_Map->GetMapFileName());
    }
    while ((matchIndex = Line.find("{SHORTDESC}")) != string::npos) {
      Line.replace(matchIndex, 11, m_Map->GetMapShortDesc());
    }
    while ((matchIndex = Line.find("{AUTOSTART}")) != string::npos) {
      Line.replace(matchIndex, 11, GetAutoStartText());
    }
    SendChat(player, Line);
  }
}

void CGame::SendAllActions()
{
  bool UsingGProxy = false;
  for (auto& player : m_Players) {
    if (player->GetGProxy()) {
      UsingGProxy = true;
      break;
    }
  }

  m_GameTicks += m_Latency;

  if (UsingGProxy) {
    // we must send empty actions to non-GProxy++ players
    // GProxy++ will insert these itself so we don't need to send them to GProxy++ players
    // empty actions are used to extend the time a player can use when reconnecting

    for (auto& player : m_Players) {
      if (!player->GetGProxy()) {
        for (uint8_t j = 0; j < m_GProxyEmptyActions; ++j)
          Send(player, GetProtocol()->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
      }
    }
  }

  if (!m_Lagging)
    ++m_SyncCounter;

  // we aren't allowed to send more than 1460 bytes in a single packet but it's possible we might have more than that many bytes waiting in the queue

  if (!m_Actions.empty())
  {
    // we use a "sub actions queue" which we keep adding actions to until we reach the size limit
    // start by adding one action to the sub actions queue

    queue<CIncomingAction*> SubActions;
    CIncomingAction*        Action = m_Actions.front();
    m_Actions.pop();
    SubActions.push(Action);
    uint32_t SubActionsLength = Action->GetLength();

    while (!m_Actions.empty())
    {
      Action = m_Actions.front();
      m_Actions.pop();

      // check if adding the next action to the sub actions queue would put us over the limit (1452 because the INCOMING_ACTION and INCOMING_ACTION2 packets use an extra 8 bytes)

      if (SubActionsLength + Action->GetLength() > 1452)
      {
        // we'd be over the limit if we added the next action to the sub actions queue
        // so send everything already in the queue and then clear it out
        // the W3GS_INCOMING_ACTION2 packet handles the overflow but it must be sent *before* the corresponding W3GS_INCOMING_ACTION packet

        SendAll(GetProtocol()->SEND_W3GS_INCOMING_ACTION2(SubActions));

        while (!SubActions.empty())
        {
          delete SubActions.front();
          SubActions.pop();
        }

        SubActionsLength = 0;
      }

      SubActions.push(Action);
      SubActionsLength += Action->GetLength();
    }

    SendAll(GetProtocol()->SEND_W3GS_INCOMING_ACTION(SubActions, m_Latency));

    while (!SubActions.empty())
    {
      delete SubActions.front();
      SubActions.pop();
    }
  }
  else
    SendAll(GetProtocol()->SEND_W3GS_INCOMING_ACTION(m_Actions, m_Latency));

  const int64_t Ticks                = GetTicks();
  const int64_t ActualSendInterval   = Ticks - m_LastActionSentTicks;
  const int64_t ExpectedSendInterval = m_Latency - m_LastActionLateBy;
  m_LastActionLateBy                 = ActualSendInterval - ExpectedSendInterval;

  if (m_LastActionLateBy > m_PerfThreshold)
  {
    // something is going terribly wrong - Aura is probably starved of resources
    // print a message because even though this will take more resources it should provide some information to the administrator for future reference
    // other solutions - dynamically modify the latency, request higher priority, terminate other games, ???
    Print(GetLogPrefix() + "warning - last update was late by " + to_string(m_LastActionLateBy) + "ms");
  }

  m_LastActionSentTicks = Ticks;
}

void CGame::AnnounceToAddress(string IP, uint16_t port) const
{
  m_Aura->m_Net->m_UDPSocket->SendTo(
    IP, port,
    GetProtocol()->SEND_W3GS_GAMEINFO(
      m_Aura->m_GameVersion,
      CreateByteArray(static_cast<uint32_t>(MAPGAMETYPE_UNKNOWN0), false),
      m_Map->GetMapGameFlags(),
      m_Aura->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : m_Map->GetMapWidth(),
      m_Aura->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : m_Map->GetMapHeight(),
      m_GameName,
      m_IndexVirtualHostName,
      0,
      m_MapPath,
      m_Map->GetMapCRC(),
      m_Slots.size(), // Total Slots
      m_Slots.size() == GetSlotsOpen() ? m_Slots.size() : GetSlotsOpen() + 1, // "Available" Slots
      m_LANHostPort,
      m_HostCounter,
      m_EntryKey
    )
  );
}

void CGame::AnnounceToAddressForGameRanger(string tunnelLocalIP, uint16_t tunnelLocalPort, const std::vector<uint8_t>& remoteIP, const uint16_t remotePort, const uint8_t extraBit) const
{
  m_Aura->m_Net->m_UDPSocket->SendTo(
    tunnelLocalIP, tunnelLocalPort,
    GetProtocol()->SEND_W3GR_GAMEINFO(
      m_Aura->m_GameVersion,
      CreateByteArray(static_cast<uint32_t>(MAPGAMETYPE_UNKNOWN0), false),
      m_Map->GetMapGameFlags(),
      m_Aura->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : m_Map->GetMapWidth(),
      m_Aura->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : m_Map->GetMapHeight(),
      m_GameName,
      m_IndexVirtualHostName,
      0,
      m_MapPath,
      m_Map->GetMapCRC(),
      m_Slots.size(), // Total Slots
      m_Slots.size() == GetSlotsOpen() ? m_Slots.size() : GetSlotsOpen() + 1, // "Available" Slots
      m_LANHostPort,
      m_HostCounter | (1 << 24),
      m_EntryKey,
      remoteIP,
      remotePort,
      extraBit
    )
  );
}

void CGame::LANBroadcastGameCreate() const
{
  m_Aura->m_Net->SendBroadcast(6112, GetProtocol()->SEND_W3GS_CREATEGAME(m_Aura->m_GameVersion, m_HostCounter));
  if (m_Aura->m_Config->m_UDPSupportGameRanger && m_Aura->m_Net->m_GameRangerLocalPort != 0) {
    m_Aura->m_Net->m_UDPSocket->SendTo(
      m_Aura->m_Net->m_GameRangerLocalAddress, m_Aura->m_Net->m_GameRangerLocalPort,
      // Hardcoded remote 255.255.255.255:6112
      GetProtocol()->SEND_W3GR_CREATEGAME(m_Aura->m_GameVersion, m_HostCounter)
    );
  }
}

void CGame::LANBroadcastGameDecreate() const
{
  m_Aura->m_Net->SendBroadcast(6112, GetProtocol()->SEND_W3GS_DECREATEGAME(m_HostCounter));
  if (m_Aura->m_Config->m_UDPSupportGameRanger) {
    m_Aura->m_Net->m_UDPSocket->SendTo(
      m_Aura->m_Net->m_GameRangerLocalAddress, m_Aura->m_Net->m_GameRangerLocalPort,
      // Hardcoded remote 255.255.255.255:6112
      GetProtocol()->SEND_W3GR_DECREATEGAME(m_HostCounter)
    );
  }
}

void CGame::LANBroadcastGameRefresh() const
{
  m_Aura->m_Net->SendBroadcast(6112, GetProtocol()->SEND_W3GS_REFRESHGAME(
    m_HostCounter,
    m_Slots.size() == GetSlotsOpen() ? 1 : m_Slots.size() - GetSlotsOpen(),
    m_Slots.size()
  ));
  if (m_Aura->m_Config->m_UDPSupportGameRanger) {
    m_Aura->m_Net->m_UDPSocket->SendTo(
      m_Aura->m_Net->m_GameRangerLocalAddress, m_Aura->m_Net->m_GameRangerLocalPort,
      GetProtocol()->SEND_W3GR_REFRESHGAME(
        m_HostCounter,
        m_Slots.size() == GetSlotsOpen() ? 1 : m_Slots.size() - GetSlotsOpen(),
        m_Slots.size()
      )
    );
  }
}

void CGame::LANBroadcastGameInfo() const
{
  // we send 12 for SlotsTotal because this determines how many PID's Warcraft 3 allocates
  // we need to make sure Warcraft 3 allocates at least SlotsTotal + 1 but at most 12 PID's
  // this is because we need an extra PID for the virtual host player (but we always delete the virtual host player when the 12th person joins)
  // however, we can't send 13 for SlotsTotal because this causes Warcraft 3 to crash when sharing control of units
  // nor can we send SlotsTotal because then Warcraft 3 crashes when playing maps with less than 12 PID's (because of the virtual host player taking an extra PID)
  // we also send 12 for SlotsOpen because Warcraft 3 assumes there's always at least one player in the game (the host)
  // so if we try to send accurate numbers it'll always be off by one and results in Warcraft 3 assuming the game is full when it still needs one more player
  // the easiest solution is to simply send 12 for both so the game will always show up as (1/12) players

  // note: the PrivateGame flag is not set when broadcasting to LAN (as you might expect)
  // note: we do not use m_Map->GetMapGameType because none of the filters are set when broadcasting to LAN (also as you might expect)

	m_Aura->m_Net->SendBroadcast(
  6112,
    GetProtocol()->SEND_W3GS_GAMEINFO(
			m_Aura->m_GameVersion,
			CreateByteArray(static_cast<uint32_t>(MAPGAMETYPE_UNKNOWN0), false),
			m_Map->GetMapGameFlags(),
      m_Aura->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : m_Map->GetMapWidth(),
      m_Aura->m_Config->m_ProxyReconnectEnabled ? m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS() : m_Map->GetMapHeight(),
			m_GameName,
			m_IndexVirtualHostName,
			0,
			m_MapPath,
			m_Map->GetMapCRC(),
      m_Slots.size(), // Total Slots
      m_Slots.size() == GetSlotsOpen() ? m_Slots.size() : GetSlotsOpen() + 1, // "Available" Slots
			m_LANHostPort,
			m_HostCounter,
			m_EntryKey
		)
	);
}

void CGame::EventPlayerDeleted(CGamePlayer* player)
{
  Print(GetLogPrefix() + "deleting player [" + player->GetName() + "]: " + player->GetLeftReason());

  m_LastPlayerLeaveTicks = GetTicks();

  if (m_GameLoading || m_GameLoaded) {
    for (auto& otherPlayer : m_SyncPlayers[player]) {
      std::vector<CGamePlayer*>& BackList = m_SyncPlayers[otherPlayer];
      auto BackIterator = std::find(BackList.begin(), BackList.end(), player);
      if (BackIterator == BackList.end()) {
        Print("[ERROR] Player " + player->GetName() + " not found in back iterator from " + otherPlayer->GetName() + "@ EventPlayerDeleted");
      } else {
        *BackIterator = std::move(BackList.back());
        BackList.pop_back();
      }
    }
    m_SyncPlayers.erase(player);
    m_HadLeaver = true;
  } else {
    if (MatchOwnerName(player->GetName()) && m_OwnerRealm == player->GetRealmHostName() && player->GetRealmHostName().empty()) {
      ReleaseOwner();
    }
  }

  // in some cases we're forced to send the left message early so don't send it again
  if (!player->GetLeftMessageSent()) {
    if (m_GameLoaded)
      SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");

    if (player->GetLagging())
      SendAll(GetProtocol()->SEND_W3GS_STOP_LAG(player));

    // tell everyone about the player leaving
    SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(player->GetPID(), player->GetLeftCode()));
  }

  // abort the countdown if there was one in progress
  if (m_CountDownStarted && !m_GameLoading && !m_GameLoaded) {
    SendAllChat("Countdown aborted because [" + player->GetName() + "] left!");
    m_CountDownStarted = false;
  }

  // abort the votekick

  if (!m_KickVotePlayer.empty()) {
    SendAllChat("A votekick against player [" + m_KickVotePlayer + "] has been cancelled");
    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }

  // record everything we need to know about the player for storing in the database later
  // since we haven't stored the game yet (it's not over yet!) we can't link the gameplayer to the game
  // see the destructor for where these CDBGamePlayers are stored in the database
  // we could have inserted an incomplete record on creation and updated it later but this makes for a cleaner int32_terface

  if (m_GameLoading || m_GameLoaded) {
    // TODO: since we store players that crash during loading it's possible that the stats classes could have no information on them

    uint8_t SID = GetSIDFromPID(player->GetPID());

    m_DBGamePlayers.push_back(new CDBGamePlayer(player->GetName(), player->GetFinishedLoading() ? player->GetFinishedLoadingTicks() - m_StartedLoadingTicks : 0, m_GameTicks / 1000, m_Slots[SID].GetColour()));

    // also keep track of the last player to leave for the !banlast command

    for (auto& ban : m_DBBans) {
      if (ban->GetName() == player->GetName())
        m_DBBanLast = ban;
    }
  }
}

void CGame::EventPlayerDisconnectTimedOut(CGamePlayer* player)
{
  if (player->GetGProxy() && m_GameLoaded)
  {
    if (!player->GetGProxyDisconnectNoticeSent())
    {
      SendAllChat(player->GetName() + " " + "has lost the connection (timed out) but is using GProxy++ and may reconnect");
      player->SetGProxyDisconnectNoticeSent(true);
    }

    if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20) {
      int64_t Time = GetTime();
      if (!player->GetLagging()) {
        player->SetLagging(true);
        player->SetStartedLaggingTicks(GetTicks());
        if (!GetLagging()) {
          m_Lagging = true;
          m_StartedLaggingTime = Time;
          m_LastLagScreenResetTime = Time;
        }
        for (auto& eachPlayer : m_Players)
          eachPlayer->SetDropVote(false);
        SendAll(GetProtocol()->SEND_W3GS_START_LAG(m_Players));
      }
      int64_t TimeRemaining = (m_GProxyEmptyActions + 1) * 60 - (Time - m_StartedLaggingTime);

      if (TimeRemaining > (m_GProxyEmptyActions + 1) * 60)
        TimeRemaining = (m_GProxyEmptyActions + 1) * 60;
      else if (TimeRemaining < 0)
        TimeRemaining = 0;

      SendAllChat(player->GetPID(), "Please wait for me to reconnect (" + to_string(TimeRemaining) + " seconds remain)");
      player->SetLastGProxyWaitNoticeSentTime(GetTime());
    }

    return;
  }

  // not only do we not do any timeouts if the game is lagging, we allow for an additional grace period of 10 seconds
  // this is because Warcraft 3 stops sending packets during the lag screen
  // so when the lag screen finishes we would immediately disconnect everyone if we didn't give them some extra time

  if (GetTime() - m_LastLagScreenTime >= 10) {
    player->SetDeleteMe(true);
    player->SetLeftReason("has lost the connection (timed out)");
    player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

    if (!m_GameLoading && !m_GameLoaded)
      OpenSlot(GetSIDFromPID(player->GetPID()), false);
  }
}

void CGame::EventPlayerDisconnectSocketError(CGamePlayer* player)
{
  if (player->GetGProxy() && m_GameLoaded)
  {
    if (!player->GetGProxyDisconnectNoticeSent())
    {
      SendAllChat(player->GetName() + " " + "has lost the connection (connection error - " + player->GetSocket()->GetErrorString() + ") but is using GProxy++ and may reconnect");
      player->SetGProxyDisconnectNoticeSent(true);
    }

    if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20) {
      int64_t Time = GetTime();
      if (!player->GetLagging()) {
        player->SetLagging(true);
        player->SetStartedLaggingTicks(GetTicks());
        if (!GetLagging()) {
          m_Lagging = true;
          m_StartedLaggingTime = Time;
          m_LastLagScreenResetTime = Time;
        }
        for (auto& eachPlayer : m_Players)
          eachPlayer->SetDropVote(false);
        SendAll(GetProtocol()->SEND_W3GS_START_LAG(m_Players));
      }
      int64_t TimeRemaining = (m_GProxyEmptyActions + 1) * 60 - (Time - m_StartedLaggingTime);

      if (TimeRemaining > (m_GProxyEmptyActions + 1) * 60)
        TimeRemaining = (m_GProxyEmptyActions + 1) * 60;
      else if (TimeRemaining < 0)
        TimeRemaining = 0;

      SendAllChat(player->GetPID(), "Please wait for me to reconnect (" + to_string(TimeRemaining) + " seconds remain)");
      player->SetLastGProxyWaitNoticeSentTime(GetTime());
    }

    return;
  }

  player->SetDeleteMe(true);
  player->SetLeftReason("has lost the connection (connection error - " + player->GetSocket()->GetErrorString() + ")");
  player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

  if (!m_GameLoading && !m_GameLoaded)
    OpenSlot(GetSIDFromPID(player->GetPID()), false);
}

void CGame::EventPlayerDisconnectConnectionClosed(CGamePlayer* player)
{
  if (player->GetGProxy() && m_GameLoaded)
  {
    if (!player->GetGProxyDisconnectNoticeSent())
    {
      SendAllChat(player->GetName() + " " + "has lost the connection (connection closed by remote host) but is using GProxy++ and may reconnect");
      player->SetGProxyDisconnectNoticeSent(true);
    }

    if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20) {
      int64_t Time = GetTime();
      if (!player->GetLagging()) {
        player->SetLagging(true);
        player->SetStartedLaggingTicks(GetTicks());
        if (!GetLagging()) {
          m_Lagging = true;
          m_StartedLaggingTime = Time;
          m_LastLagScreenResetTime = Time;
        }
        for (auto& eachPlayer : m_Players)
          eachPlayer->SetDropVote(false);
        SendAll(GetProtocol()->SEND_W3GS_START_LAG(m_Players));
      }
      int64_t TimeRemaining = (m_GProxyEmptyActions + 1) * 60 - (Time - m_StartedLaggingTime);

      if (TimeRemaining > (m_GProxyEmptyActions + 1) * 60)
        TimeRemaining = (m_GProxyEmptyActions + 1) * 60;
      else if (TimeRemaining < 0)
        TimeRemaining = 0;

      SendAllChat(player->GetPID(), "Please wait for me to reconnect (" + to_string(TimeRemaining) + " seconds remain)");
      player->SetLastGProxyWaitNoticeSentTime(GetTime());
    }

    return;
  }

  player->SetDeleteMe(true);
  player->SetLeftReason("has lost the connection (connection closed by remote host)");
  player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

  if (!m_GameLoading && !m_GameLoaded)
    OpenSlot(GetSIDFromPID(player->GetPID()), false);
}

void CGame::EventPlayerKickHandleQueued(CGamePlayer* player)
{
  if (player->GetDeleteMe())
    return;

  if (m_CountDownStarted)
    return;

  player->SetDeleteMe(true);
  // left reason, left code already assigned when queued
  OpenSlot(GetSIDFromPID(player->GetPID()), false);
}

CGamePlayer* CGame::JoinPlayer(CPotentialPlayer* potential, CIncomingJoinRequest* joinRequest, const uint8_t SID, const uint8_t HostCounterID, const string JoinedRealm, const bool IsReserved, const bool IsUnverifiedAdmin)
{
  // Transfer socket from CPotentialPlayer to CGamePlayer
  CGamePlayer* Player = new CGamePlayer(this, potential, GetNewPID(), HostCounterID, JoinedRealm, joinRequest->GetName(), joinRequest->GetInternalIP(), IsReserved);
  m_Players.push_back(Player);
  potential->SetSocket(nullptr);
  potential->SetDeleteMe(true);

  Player->SetWhoisShouldBeSent(IsUnverifiedAdmin || MatchOwnerName(Player->GetName()));

  if (m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
    m_Slots[SID] = CGameSlot(Player->GetPID(), 255, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam(), m_Slots[SID].GetColour(), m_Map->GetLobbyRace(&m_Slots[SID]));
  } else {
    m_Slots[SID] = CGameSlot(Player->GetPID(), 255, SLOTSTATUS_OCCUPIED, 0, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, m_Map->GetLobbyRace(&m_Slots[SID]));

    // try to pick a team and colour
    // make sure there aren't too many other players already

    uint8_t NumOtherPlayers = 0;

    for (auto& slot : m_Slots) {
      if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetTeam() != m_Aura->m_MaxSlots)
        ++NumOtherPlayers;
    }

    if (NumOtherPlayers < m_Map->GetMapNumPlayers()) {
      if (SID < m_Map->GetMapNumPlayers())
        m_Slots[SID].SetTeam(SID);
      else
        m_Slots[SID].SetTeam(0);

      m_Slots[SID].SetColour(GetNewColour());
    }
  }

  // send slot info to the new player
  // the SLOTINFOJOIN packet also tells the client their assigned PID and that the join was successful

  Player->Send(GetProtocol()->SEND_W3GS_SLOTINFOJOIN(Player->GetPID(), Player->GetSocket()->GetPort(), Player->GetExternalIP(), m_Slots, m_RandomSeed, m_Map->GetMapLayoutStyle(), m_Map->GetMapNumPlayers()));

  SendIncomingPlayerInfo(Player);

  // send virtual host info and fake players info (if present) to the new player

  SendVirtualHostPlayerInfo(Player);
  SendFakePlayersInfo(Player);
  SendJoinedPlayersInfo(Player);

  // send a map check packet to the new player

  Player->Send(GetProtocol()->SEND_W3GS_MAPCHECK(m_MapPath, m_Map->GetMapSize(), m_Map->GetMapInfo(), m_Map->GetMapCRC(), m_Map->GetMapSHA1()));

  // send slot info to everyone, so the new player gets this info twice but everyone else still needs to know the new slot layout
  SendAllSlotInfo();

  // send a welcome message

	SendWelcomeMessage(Player);

  // check for multiple IP usage

  string Others;

  bool IsLoopBack = Player->GetExternalIPString() == "127.0.0.1";
  for (auto& player : m_Players){
    if (Player == player)
      continue;
    if (Player->GetExternalIPString() != player->GetExternalIPString())
      continue;

    if (!IsLoopBack) {
      if (Others.empty())
        Others = player->GetName();
      else
        Others += ", " + player->GetName();
    }
  }

  if (!Others.empty()) {
    SendAllChat("Player [" + joinRequest->GetName() + "] has the same IP address as: " + Others);
  }

  // abort the countdown if there was one in progress

  if (m_CountDownStarted && !m_GameLoading && !m_GameLoaded)
  {
    SendAllChat("Countdown aborted!");
    m_CountDownStarted = false;
  }

  string notifyString = "";
  if (m_NotifyJoins && !m_Aura->IsIgnoredNotifyPlayer(joinRequest->GetName())) {
    notifyString = "\x07";
  }
  Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "@" + JoinedRealm + "] joined - [" + Player->GetSocket()->GetName() + "] (" + Player->GetExternalIPString() + ")" + notifyString);

  return Player;
}

bool CGame::EventRequestJoin(CPotentialPlayer* potential, CIncomingJoinRequest* joinRequest)
{
  // check the new player's name

  if (joinRequest->GetName().empty() || joinRequest->GetName().size() > 15 || GetPlayerFromName(joinRequest->GetName(), false))
  {
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "] invalid name (taken or too long) - [" + potential->GetSocket()->GetName() + "] (" + potential->GetExternalIPString() + ")");
    potential->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  } else if (joinRequest->GetName() == m_LobbyVirtualHostName) {
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "] spoofer (matches host name) - [" + potential->GetSocket()->GetName() + "] (" + potential->GetExternalIPString() + ")");
    potential->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  // identify their joined realm
  // this is only possible because when we send a game refresh via LAN or battle.net we encode an ID value in the 4 most significant bits of the host counter
  // the client sends the host counter when it joins so we can extract the ID value here
  // note: this is not a replacement for spoof checking since it doesn't verify the player's name and it can be spoofed anyway

  string         JoinedRealm;
  uint8_t HostCounterID = joinRequest->GetHostCounter() >> 24;
  bool IsUnverifiedAdmin = false;

  if (HostCounterID >= 0x10) {
    CRealm* matchingRealm = m_Aura->GetRealmByHostCounter(HostCounterID);
    if (matchingRealm) {
      JoinedRealm = matchingRealm->GetServer();
      IsUnverifiedAdmin = matchingRealm->GetIsAdmin(joinRequest->GetName()) || matchingRealm->GetIsRootAdmin(joinRequest->GetName());
    } else {
      // Trying to join from an unknown realm.
      HostCounterID = 0xF;
    }
    matchingRealm = nullptr;
  }

  if (joinRequest->GetName() == m_OwnerName && !m_OwnerRealm.empty() && !JoinedRealm.empty() && m_OwnerRealm != JoinedRealm) {
    // Prevent owner homonyms from other realms from joining. This doesn't affect LAN.
    // But LAN has its own rules, e.g. a LAN owner that leaves the game is immediately demoted.
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "@" + JoinedRealm + "] spoofer (matches owner name, but realm mismatch, expected " + m_OwnerRealm + ") - [" + potential->GetSocket()->GetName() + "] (" + potential->GetExternalIPString() + ")");
    potential->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  if (HostCounterID < 0x10 && joinRequest->GetEntryKey() != m_EntryKey) {
    // check if the player joining via LAN knows the entry key
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "@" + JoinedRealm + "] used a wrong LAN key (" + to_string(joinRequest->GetEntryKey()) + ") - [" + potential->GetSocket()->GetName() + "] (" + potential->GetExternalIPString() + ")");
    potential->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_WRONGPASSWORD));
    return false;
  }

  if (HostCounterID < 0x10 && HostCounterID != 0) {
    // 0x1: Information
    // 0x2: GameRanger
    // others: undefined
    if (HostCounterID == 0x1) {
      potential->Send(GetProtocol()->SEND_W3GS_SLOTINFOJOIN(GetNewPID(), potential->GetSocket()->GetPort(), potential->GetExternalIP(), m_Slots, m_RandomSeed, m_Map->GetMapLayoutStyle(), m_Map->GetMapNumPlayers()));
      SendVirtualHostPlayerInfo(potential);
      SendFakePlayersInfo(potential);
      SendJoinedPlayersInfo(potential);
      return false;
    }
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "@" + JoinedRealm + "] is trying to join over reserved realm " + to_string(HostCounterID) + " - [" + potential->GetSocket()->GetName() + "] (" + potential->GetExternalIPString() + ")");
    if (HostCounterID > 0x2) {
      potential->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_WRONGPASSWORD));
      return false;
    }
  }

  // check if the new player's name is banned
  // don't allow the player to spam the chat by attempting to join the game multiple times in a row

  CRealm* SourceRealm = m_Aura->GetRealmByHostName(JoinedRealm);
  if (SourceRealm) {
    CDBBan* Ban = SourceRealm->IsBannedName(joinRequest->GetName());
    if (Ban){
      Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "|" + potential->GetExternalIPString() + "] is banned");

      if (m_ReportedJoinFailNames.find(joinRequest->GetName()) == end(m_ReportedJoinFailNames)) {
        SendAllChat("[" + joinRequest->GetName() + "@" + JoinedRealm + "] is trying to join the game, but is banned");
        m_ReportedJoinFailNames.insert(joinRequest->GetName());
      }

      // let banned players "join" the game with an arbitrary PID then immediately close the connection
      // this causes them to be kicked back to the chat channel on battle.net

      vector<CGameSlot> Slots = m_Map->GetSlots();
      potential->Send(GetProtocol()->SEND_W3GS_SLOTINFOJOIN(1, potential->GetSocket()->GetPort(), potential->GetExternalIP(), Slots, 0, m_Map->GetMapLayoutStyle(), m_Map->GetMapNumPlayers()));
      delete Ban;
      return false;
    }
    SourceRealm = nullptr;
  }

  // Check if the player is an admin or root admin on any connected realm for determining reserved status
  // Note: vulnerable to spoofing
  const bool IsReserved = GetIsReserved(joinRequest->GetName()) || MatchOwnerName(joinRequest->GetName()) && JoinedRealm == m_OwnerRealm;

  // try to find an empty slot

  uint8_t SID = GetEmptySlot(false);

  if (SID == 255 && IsReserved) {
    // a reserved player is trying to join the game but it's full, try to find a reserved slot

    SID = GetEmptySlot(true);
    if (SID != 255) {
      CGamePlayer* KickedPlayer = GetPlayerFromSID(SID);

      if (KickedPlayer)
      {
        KickedPlayer->SetDeleteMe(true);
        KickedPlayer->SetLeftReason("was kicked to make room for a reserved player [" + joinRequest->GetName() + "]");
        KickedPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);

        // send a playerleave message immediately since it won't normally get sent until the player is deleted which is after we send a playerjoin message
        // we don't need to call OpenSlot here because we're about to overwrite the slot data anyway

        SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(KickedPlayer->GetPID(), KickedPlayer->GetLeftCode()));
        KickedPlayer->SetLeftMessageSent(true);
      }
    }
  }

  if (SID == 255 && MatchOwnerName(joinRequest->GetName()) && JoinedRealm == m_OwnerRealm)
  {
    // the owner player is trying to join the game but it's full and we couldn't even find a reserved slot, kick the player in the lowest numbered slot
    // updated this to try to find a player slot so that we don't end up kicking a computer

    SID = 0;

    for (uint8_t i = 0; i < m_Slots.size(); ++i)
    {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[i].GetComputer() == 0)
      {
        SID = i;
        break;
      }
    }

    CGamePlayer* KickedPlayer = GetPlayerFromSID(SID);

    if (KickedPlayer)
    {
      KickedPlayer->SetDeleteMe(true);
      KickedPlayer->SetLeftReason("was kicked to make room for the owner player [" + joinRequest->GetName() + "]");
      KickedPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);

      // send a playerleave message immediately since it won't normally get sent until the player is deleted which is after we send a playerjoin message
      // we don't need to call OpenSlot here because we're about to overwrite the slot data anyway

      SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(KickedPlayer->GetPID(), KickedPlayer->GetLeftCode()));
      KickedPlayer->SetLeftMessageSent(true);
    }
  }

  if (SID >= m_Slots.size())
  {
    potential->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  // we have a slot for the new player
  // make room for them by deleting the virtual host player if we have to

  if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN && GetSlotsOpen() == 1 && GetNumConnectionsOrFake() > 1)
    DeleteVirtualHost();

  JoinPlayer(potential, joinRequest, SID, HostCounterID, JoinedRealm, IsReserved, IsUnverifiedAdmin);
  return true;
}

void CGame::EventPlayerLeft(CGamePlayer* player, uint32_t reason)
{
  // this function is only called when a player leave packet is received, not when there's a socket error, kick, etc...

  player->SetDeleteMe(true);
  player->SetLeftReason("has left the game voluntarily");
  player->SetLeftCode(PLAYERLEAVE_LOST);

  if (!m_GameLoading && !m_GameLoaded) {
    OpenSlot(GetSIDFromPID(player->GetPID()), false);
  }
}

void CGame::EventPlayerLoaded(CGamePlayer* player)
{
  Print(GetLogPrefix() + "player [" + player->GetName() + "] finished loading in " + to_string(static_cast<double>(player->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000.f) + " seconds");

  SendAll(GetProtocol()->SEND_W3GS_GAMELOADED_OTHERS(player->GetPID()));
}

void CGame::EventPlayerAction(CGamePlayer* player, CIncomingAction* action)
{
  m_Actions.push(action);

  // check for players saving the game and notify everyone

  if (!action->GetAction()->empty() && (*action->GetAction())[0] == 6)
  {
    Print(GetLogPrefix() + "player [" + player->GetName() + "] is saving the game");
    SendAllChat("Player [" + player->GetName() + "] is saving the game");
  }

  // give the stats class a chance to process the action

  if (m_Stats && action->GetAction()->size() >= 6 && m_Stats->ProcessAction(action) && m_GameOverTime == 0)
  {
    Print(GetLogPrefix() + "gameover timer started (stats class reported game over)");
    m_GameOverTime = GetTime();
  }
}

void CGame::EventPlayerKeepAlive(CGamePlayer* player)
{
  if (!m_GameLoading && !m_GameLoaded)
    return;

  if (player->GetCheckSums()->empty())
    return;

  bool CanConsumeFrame = true;
  std::vector<CGamePlayer*>& OtherPlayers = m_SyncPlayers[player];
  for (auto& otherPlayer: OtherPlayers) {
    if (otherPlayer == player) {
      Print("Same player found in other players vector");
      CanConsumeFrame = false;;
      break;
    }

    if (otherPlayer->GetCheckSums()->empty()) {
      CanConsumeFrame = false;
      break;
    }
  }

  if (!CanConsumeFrame)
    return;

  const uint32_t MyCheckSum = player->GetCheckSums()->front();
  player->GetCheckSums()->pop();

  bool DesyncDetected = false;
  string DesyncedPlayers;
  typename std::vector<CGamePlayer*>::iterator it = OtherPlayers.begin();
  while (it != OtherPlayers.end()) {
    if ((*it)->GetCheckSums()->front() == MyCheckSum) {
      (*it)->GetCheckSums()->pop();
      ++it;
    } else {
      DesyncDetected = true;
      std::vector<CGamePlayer*>& BackList = m_SyncPlayers[*it];
      auto BackIterator = std::find(BackList.begin(), BackList.end(), player);
      if (BackIterator == BackList.end()) {
        Print("[ERROR] Player " + player->GetName() + " not found in back iterator from " + (*it)->GetName() + "@ EventPlayerKeepAlive");
      } else {
        *BackIterator = std::move(BackList.back());
        BackList.pop_back();
      }

      DesyncedPlayers += (*it)->GetName() + ", ";
      std::iter_swap(it, OtherPlayers.end() - 1);
      OtherPlayers.pop_back();
    }
  }
  if (DesyncDetected) {
    string SyncedPlayers;
    for (auto& sp : m_SyncPlayers[player]) {
      SyncedPlayers += sp->GetName() + ", ";
    }
    m_Desynced = true;
    Print(GetLogPrefix() + player->GetName() + " no longer synchronized with " + DesyncedPlayers.substr(0, DesyncedPlayers.length() - 2));
    Print(GetLogPrefix() + player->GetName() + " still synchronized with " + SyncedPlayers.substr(0, SyncedPlayers.length() - 2));
    SendAllChat("Warning! Desync detected (" + player->GetName() + " is not in the same game as " + DesyncedPlayers.substr(0, DesyncedPlayers.length() - 2) + ")");
  }
}

void CGame::EventPlayerChatToHost(CGamePlayer* player, CIncomingChatPlayer* chatPlayer)
{
  if (chatPlayer->GetFromPID() == player->GetPID())
  {
    if (chatPlayer->GetType() == CIncomingChatPlayer::CTH_MESSAGE || chatPlayer->GetType() == CIncomingChatPlayer::CTH_MESSAGEEXTRA)
    {
      // relay the chat message to other players

      bool                       Relay           = !player->GetMuted();
      bool                       OnlyToObservers = player->GetObserver() && (m_GameLoading || m_GameLoaded) && !player->GetPowerObserver();
      const std::vector<uint8_t> ExtraFlags      = chatPlayer->GetExtraFlags();

      // calculate timestamp

      if (!ExtraFlags.empty())
      {
        if (ExtraFlags[0] == 0) {
          // this is an ingame [All] message, print it to the console

          Print(GetLogPrefix() + "[All] [" + player->GetName() + "] " + chatPlayer->GetMessage());

          // don't relay ingame messages targeted for all players if we're currently muting all
          // note that commands will still be processed even when muting all because we only stop relaying the messages, the rest of the function is unaffected

          if (m_MuteAll)
            Relay = false;
        } else if (ExtraFlags[0] == 2) {
          // this is an ingame [Obs/Ref] message, print it to the console
					Print(GetLogPrefix() + "[Obs/Ref] [" + player->GetName() + "] " + chatPlayer->GetMessage());
        }
      }
      else
      {
        // this is a lobby message, print it to the console

        Print(GetLogPrefix() + "[" + player->GetName() + "] " + chatPlayer->GetMessage());

        if (m_MuteLobby)
          Relay = false;
      }

      // handle bot commands

      const string Message = chatPlayer->GetMessage();

      if (!Message.empty() && Message[0] == m_CommandTrigger) {
        char CommandToken = m_CommandTrigger;
        string            Command, Payload;
        string::size_type PayloadStart = Message.find(' ');

        if (PayloadStart != string::npos) {
          Command = Message.substr(1, PayloadStart - 1);
          Payload = Message.substr(PayloadStart + 1);
        } else {
          Command = Message.substr(1);
        }

        transform(begin(Command), end(Command), begin(Command), ::tolower);

        EventPlayerBotCommand(player, CommandToken, Command, Payload);
      }

      if (Relay) {
        if (OnlyToObservers) {
          std::vector<uint8_t> TargetPIDs = chatPlayer->GetToPIDs();
          std::vector<uint8_t> ObserversPIDs;
          for (uint8_t i = 0; i < TargetPIDs.size(); ++i) {
            CGamePlayer* TargetPlayer = GetPlayerFromPID(TargetPIDs[i]);
            if (TargetPlayer && TargetPlayer->GetObserver()) {
              ObserversPIDs.push_back(TargetPIDs[i]);
            }
          }
          if (!ObserversPIDs.empty())
            Send(ObserversPIDs, GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromPID(), ObserversPIDs, chatPlayer->GetFlag(), chatPlayer->GetExtraFlags(), chatPlayer->GetMessage()));
          else
            Print(GetLogPrefix() + "[Obs/Ref] --nobody listening--");
        } else {
          Send(chatPlayer->GetToPIDs(), GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromPID(), chatPlayer->GetToPIDs(), chatPlayer->GetFlag(), chatPlayer->GetExtraFlags(), chatPlayer->GetMessage()));
        }
      }
    }
    else
    {
      if (!m_CountDownStarted)
      {
        if (chatPlayer->GetType() == CIncomingChatPlayer::CTH_TEAMCHANGE)
          EventPlayerChangeTeam(player, chatPlayer->GetByte());
        else if (chatPlayer->GetType() == CIncomingChatPlayer::CTH_COLOURCHANGE)
          EventPlayerChangeColour(player, chatPlayer->GetByte());
        else if (chatPlayer->GetType() == CIncomingChatPlayer::CTH_RACECHANGE)
          EventPlayerChangeRace(player, chatPlayer->GetByte());
        else if (chatPlayer->GetType() == CIncomingChatPlayer::CTH_HANDICAPCHANGE)
          EventPlayerChangeHandicap(player, chatPlayer->GetByte());
      }
    }
  }
}

void CGame::EventPlayerBotCommand(CGamePlayer* player, char token, std::string& command, string& payload)
{
  CCommandContext* ctx = new CCommandContext(m_Aura, this, player, &std::cout, token);
  ctx->Run(command, payload);
  delete ctx;
}

void CGame::EventPlayerChangeTeam(CGamePlayer* player, uint8_t team)
{
  // player is requesting a team change

  if (m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES)
  {
    uint8_t oldSID = GetSIDFromPID(player->GetPID());
    uint8_t newSID = GetEmptySlot(team, player->GetPID());
    SwapSlots(oldSID, newSID);
  }
  else
  {
    if (team > m_Aura->m_MaxSlots)
      return;

    if (team == m_Aura->m_MaxSlots)
    {
      if (m_Map->GetMapObservers() != MAPOBS_ALLOWED && m_Map->GetMapObservers() != MAPOBS_REFEREES)
        return;
    }
    else
    {
      if (team >= m_Map->GetMapNumPlayers())
        return;

      // make sure there aren't too many other players already

      uint8_t NumOtherPlayers = 0;

      for (auto& slot : m_Slots)
      {
        if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetTeam() != m_Aura->m_MaxSlots && slot.GetPID() != player->GetPID())
          ++NumOtherPlayers;
      }

      if (NumOtherPlayers >= m_Map->GetMapNumPlayers())
        return;
    }

    uint8_t SID = GetSIDFromPID(player->GetPID());

    if (SID < m_Slots.size())
    {
      m_Slots[SID].SetTeam(team);
      player->SetObserver(team == m_Aura->m_MaxSlots);
      if (player->GetObserver())
        player->SetPowerObserver(player->GetObserver() && m_Map->GetMapObservers() == MAPOBS_REFEREES);
      if (team == m_Aura->m_MaxSlots) {
        // if they're joining the observer team give them the observer colour

        m_Slots[SID].SetColour(m_Aura->m_MaxSlots);
      } else if (m_Slots[SID].GetColour() == m_Aura->m_MaxSlots) {
        // if they're joining a regular team give them an unused colour

        m_Slots[SID].SetColour(GetNewColour());
      }

      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
    }
  }
}

void CGame::EventPlayerChangeColour(CGamePlayer* player, uint8_t colour)
{
  // player is requesting a colour change

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
    return;

  if (colour > m_Aura->m_MaxSlots-1)
    return;

  uint8_t SID = GetSIDFromPID(player->GetPID());

  if (SID < m_Slots.size())
  {
    // make sure the player isn't an observer

    if (m_Slots[SID].GetTeam() == m_Aura->m_MaxSlots)
      return;

    ColorSlot(SID, colour);
  }
}

void CGame::EventPlayerChangeRace(CGamePlayer* player, uint8_t race)
{
  // player is requesting a race change

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
    return;

  if (m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES)
    return;

  if (race != SLOTRACE_HUMAN && race != SLOTRACE_ORC && race != SLOTRACE_NIGHTELF && race != SLOTRACE_UNDEAD && race != SLOTRACE_RANDOM)
    return;

  uint8_t SID = GetSIDFromPID(player->GetPID());

  if (SID < m_Slots.size())
  {
    m_Slots[SID].SetRace(race | SLOTRACE_SELECTABLE);
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
}

void CGame::EventPlayerChangeHandicap(CGamePlayer* player, uint8_t handicap)
{
  // player is requesting a handicap change

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
    return;

  if (handicap != 50 && handicap != 60 && handicap != 70 && handicap != 80 && handicap != 90 && handicap != 100)
    return;

  uint8_t SID = GetSIDFromPID(player->GetPID());

  if (SID < m_Slots.size())
  {
    m_Slots[SID].SetHandicap(handicap);
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
}

void CGame::EventPlayerDropRequest(CGamePlayer* player)
{
  // TODO: check that we've waited the full 45 seconds

  if (m_Lagging)
  {
    Print(GetLogPrefix() + "player [" + player->GetName() + "] voted to drop laggers");
    SendAllChat("Player [" + player->GetName() + "] voted to drop laggers");

    // check if at least half the players voted to drop

    int32_t Votes = 0;

    for (auto& eachPlayer : m_Players) {
      if (eachPlayer->GetDropVote())
        ++Votes;
    }

    if (static_cast<double>(Votes) / m_Players.size() > 0.50f)
      StopLaggers("lagged out (dropped by vote)");
  }
}

void CGame::EventPlayerMapSize(CGamePlayer* player, CIncomingMapSize* mapSize)
{
  if (m_GameLoading || m_GameLoaded)
    return;

  int64_t Time = GetTime();
  uint32_t MapSize = ByteArrayToUInt32(m_Map->GetMapSize(), false);

  CRealm* JoinedRealm = player->GetRealm(false);
  uint32_t MaxUploadSize = m_Aura->m_Config->m_MaxUploadSize;
  if (JoinedRealm)
    MaxUploadSize = JoinedRealm->GetMaxUploadSize();

  if (mapSize->GetSizeFlag() != 1 || mapSize->GetMapSize() != MapSize) {
    // the player doesn't have the map

    string* MapData = m_Map->GetMapData();
    bool IsMapAvailable = !MapData->empty() && m_Map->GetValidLinkedMap();
    bool IsMapTooLarge = MapSize > MaxUploadSize * 1024;
    if (IsMapAvailable && m_Aura->m_Config->m_AllowTransfers != MAP_TRANSFERS_NEVER && (player->GetDownloadAllowed() || m_Aura->m_Config->m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC && !IsMapTooLarge)) {
      if (!player->GetDownloadStarted() && mapSize->GetSizeFlag() == 1) {
        // inform the client that we are willing to send the map

        Print(GetLogPrefix() + "map download started for player [" + player->GetName() + "]");
        Send(player, GetProtocol()->SEND_W3GS_STARTDOWNLOAD(GetHostPID()));
        player->SetDownloadStarted(true);
        player->SetStartedDownloadingTicks(GetTicks());
      } else {
        player->SetLastMapPartAcked(mapSize->GetMapSize());
      }
    } else if (!player->GetKickQueued()) {
        player->SetKickByTime(Time + m_LacksMapKickDelay);
        if (m_Aura->m_Config->m_AllowTransfers != MAP_TRANSFERS_AUTOMATIC) {
          // Even if manual, claim they are disabled.
          player->SetLeftReason("doesn't have the map and uploads are disabled");
        } else if (IsMapTooLarge) {
          player->SetLeftReason("doesn't have the map and the map is too large to send");
        } else if (MapData->empty()) {
          player->SetLeftReason("doesn't have the map and there is no local copy of the map to send");
        } else {
          player->SetLeftReason("doesn't have the map and the local copy of the map is invalid");
        }
        player->SetLeftCode(PLAYERLEAVE_LOBBY);
        if (m_Map->GetMapSiteURL().empty()) {
          SendChat(player, "" + player->GetName() + ", please download the map before joining. (Kick in " + to_string(m_LacksMapKickDelay) + " seconds...)");
        } else {
          SendChat(player, "" + player->GetName() + ", please download the map from <" + m_Map->GetMapSiteURL() + "> before joining. (Kick in " + to_string(m_LacksMapKickDelay) + " seconds...)");
        }
    }
  } else if (player->GetDownloadStarted()) {
    // calculate download rate
    const double Seconds = static_cast<double>(GetTicks() - player->GetStartedDownloadingTicks()) / 1000.f;
    const double Rate    = static_cast<double>(MapSize) / 1024.f / Seconds;
    Print(GetLogPrefix() + "map download finished for player [" + player->GetName() + "] in " + ToFormattedString(Seconds) + " seconds");
    SendAllChat("Player [" + player->GetName() + "] downloaded the map in " + ToFormattedString(Seconds) + " seconds (" + ToFormattedString(Rate) + " KB/sec)");
    player->SetDownloadFinished(true);
    player->SetFinishedDownloadingTime(GetTime());
    if (!player->GetHasMap()) {
      player->SetHasMap(true);
      if (!player->GetObserver())
        ++m_PlayersWithMap;
    }
  } else if (!player->GetHasMap()) {
    player->SetHasMap(true);
    if (!player->GetObserver())
      ++m_PlayersWithMap;
  }

  uint8_t       NewDownloadStatus = static_cast<uint8_t>(static_cast<float>(mapSize->GetMapSize()) / MapSize * 100.f);
  const uint8_t SID               = GetSIDFromPID(player->GetPID());

  if (NewDownloadStatus > 100)
    NewDownloadStatus = 100;

  if (SID < m_Slots.size())
  {
    // only send the slot info if the download status changed

    if (m_Slots[SID].GetDownloadStatus() != NewDownloadStatus)
    {
      m_Slots[SID].SetDownloadStatus(NewDownloadStatus);

      // we don't actually send the new slot info here
      // this is an optimization because it's possible for a player to download a map very quickly
      // if we send a new slot update for every percentage change in their download status it adds up to a lot of data
      // instead, we mark the slot info as "out of date" and update it only once in awhile (once per second when this comment was made)

      m_SlotInfoChanged |= SLOTS_DOWNLOAD_PROGRESS_CHANGED;
    }
  }
}

void CGame::EventPlayerPongToHost(CGamePlayer* player)
{
  if (m_GameLoading || m_GameLoaded || player->GetDeleteMe() || player->GetReserved()) {
    return;
  }
  // autokick players with excessive pings but only if they're not reserved and we've received at least 3 pings from them
  // see the Update function for where we send pings

  uint32_t LatencyMilliseconds = player->GetPing();
  if (LatencyMilliseconds >= m_AutoKickPing) {
    // send a chat message because we don't normally do so when a player leaves the lobby

    if (player->GetNumPings() == 2) {
      SendAllChat("Autokicking player [" + player->GetName() + "] for excessive ping of " + to_string(LatencyMilliseconds) + "ms");
    }
    if (player->GetNumPings() >= 3) {
      player->SetDeleteMe(true);
      player->SetLeftReason("was autokicked for excessive ping of " + to_string(LatencyMilliseconds) + "ms");
      player->SetLeftCode(PLAYERLEAVE_LOBBY);
      OpenSlot(GetSIDFromPID(player->GetPID()), false);
    }
  } else {
    bool HasHighPing = LatencyMilliseconds >= m_WarnHighPing;
    if (HasHighPing != player->GetHasHighPing()) {
      player->SetHasHighPing(HasHighPing);
      if (HasHighPing) {
        SendAllChat("Player [" + player->GetName() + "] has a high ping of " + to_string(LatencyMilliseconds) + "ms");
      } else {
        SendAllChat("Player [" + player->GetName() + "] ping went down to " + to_string(LatencyMilliseconds) + "ms");
      }
    }
  }
}

void CGame::EventGameStarted()
{
  Print(GetLogPrefix() + "started loading with " + to_string(GetNumHumanPlayers()) + " players");

  if (m_LANEnabled)
    LANBroadcastGameDecreate();

  // encode the HCL command string in the slot handicaps
  // here's how it works:
  //  the user inputs a command string to be sent to the map
  //  it is almost impossible to send a message from the bot to the map so we encode the command string in the slot handicaps
  //  this works because there are only 6 valid handicaps but Warcraft III allows the bot to set up to 256 handicaps
  //  we encode the original (unmodified) handicaps in the new handicaps and use the remaining space to store a short message
  //  only occupied slots deliver their handicaps to the map and we can send one character (from a list) per handicap
  //  when the map finishes loading, assuming it's designed to use the HCL system, it checks if anyone has an invalid handicap
  //  if so, it decodes the message from the handicaps and restores the original handicaps using the encoded values
  //  the meaning of the message is specific to each map and the bot doesn't need to understand it
  //  e.g. you could send game modes, # of rounds, level to start on, anything you want as long as it fits in the limited space available
  //  note: if you attempt to use the HCL system on a map that does not support HCL the bot will drastically modify the handicaps
  //  since the map won't automatically restore the original handicaps in this case your game will be ruined

  if (!m_HCLCommandString.empty())
  {
    if (m_HCLCommandString.size() <= GetSlotsOccupied())
    {
      string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

      if (m_HCLCommandString.find_first_not_of(HCLChars) == string::npos)
      {
        uint8_t EncodingMap[256];
        uint8_t j = 0;

        for (auto& encode : EncodingMap)
        {
          // the following 7 handicap values are forbidden

          if (j == 0 || j == 50 || j == 60 || j == 70 || j == 80 || j == 90 || j == 100)
            ++j;

          encode = j++;
        }

        uint8_t CurrentSlot = 0;

        for (auto& character : m_HCLCommandString)
        {
          while (m_Slots[CurrentSlot].GetSlotStatus() != SLOTSTATUS_OCCUPIED)
            ++CurrentSlot;

          uint8_t HandicapIndex = (m_Slots[CurrentSlot].GetHandicap() - 50) / 10;
          uint8_t CharIndex     = static_cast<uint8_t>(HCLChars.find(character));
          m_Slots[CurrentSlot++].SetHandicap(EncodingMap[HandicapIndex + CharIndex * 6]);
        }

        m_SlotInfoChanged |= SLOTS_HCL_INJECTED;
        Print(GetLogPrefix() + "successfully encoded HCL command string [" + m_HCLCommandString + "]");
      }
      else
        Print(GetLogPrefix() + "encoding HCL command string [" + m_HCLCommandString + "] failed because it contains invalid characters");
    }
    else
      Print(GetLogPrefix() + "encoding HCL command string [" + m_HCLCommandString + "] failed because there aren't enough occupied slots");
  }

  m_StartedLoadingTicks    = GetTicks();
  m_LastLagScreenResetTime = GetTime();

  if (GetNumConnectionsOrFake() >= 2) {
    // Remove the virtual host player to ensure consistent game state and networking.
    DeleteVirtualHost();
  } else if (GetSlotsOpen() > 0) {
    // Assign an available slot to our virtual host.
    // That makes it a fake player.
    DeleteVirtualHost();
    if (m_Map->GetMapObservers() == MAPOBS_REFEREES) {
      CreateFakeObserver();
    } else {
      CreateFakePlayer();
    }
  } else {
    // This is a single-player game. Neither chat events nor bot commands will work.
    // Keeping the virtual host does no good - The game client then refuses to remain in the game.
    DeleteVirtualHost();
  }

  // send a final slot info update for HCL, or in case there are pending updates
  if (m_SlotInfoChanged != 0)
    SendAllSlotInfo();

  m_GameLoading            = true;

  // since we use a fake countdown to deal with leavers during countdown the COUNTDOWN_START and COUNTDOWN_END packets are sent in quick succession
  // send a start countdown packet

  SendAll(GetProtocol()->SEND_W3GS_COUNTDOWN_START());

  // send an end countdown packet

  SendAll(GetProtocol()->SEND_W3GS_COUNTDOWN_END());

  // send a game loaded packet for the fake player (if present)

  for (auto& fakeplayer : m_FakePlayers)
    SendAll(GetProtocol()->SEND_W3GS_GAMELOADED_OTHERS(fakeplayer));

  // record the number of starting players

  m_StartPlayers = GetNumHumanPlayers() + m_FakePlayers.size();

  // enable stats

  if (m_Map->GetMapType() == "dota")
  {
    if (m_StartPlayers < 6 || !m_FakePlayers.empty())
      Print("[STATS] not using dotastats due to too few players");
    else
      m_Stats = new CStats(this);
  }

  for (auto& currentPlayer : m_Players) {
    std::vector<CGamePlayer*> otherPlayers;
    for (auto& otherPlayer : m_Players) {
      if (otherPlayer != currentPlayer) {
          otherPlayers.push_back(otherPlayer);
      }
    }
    m_SyncPlayers[currentPlayer] = otherPlayers;
  }

  // delete any potential players that are still hanging around
  // only one lobby at a time is supported, so we can just do it from here

  for (auto& pair : m_Aura->m_Net->m_IncomingConnections) {
    for (auto& potential : pair.second)
      delete potential;

    pair.second.clear();
  }

  // delete the map data

  if (m_HasMapLock) {
    m_Aura->m_BusyMaps.erase(m_Map->GetMapLocalPath());
    bool IsTooLarge = ByteArrayToUInt32(m_Map->GetMapSize(), false) > m_Aura->m_Config->m_MaxSavedMapSize * 1024;
    if (IsTooLarge && m_Aura->m_BusyMaps.find(m_Map->GetMapLocalPath()) == m_Aura->m_BusyMaps.end()) {
      m_Map->DeleteFile();
    }
    m_HasMapLock = false;
  }

  delete m_Map;
  m_Map = nullptr;

  // move the game to the games in progress vector

  m_Aura->m_CurrentLobby = nullptr;
  m_Aura->m_Games.push_back(this);

  // and finally reenter battle.net chat

  for (auto& bnet : m_Aura->m_Realms) {
    if (m_IsMirror && bnet->GetIsMirror())
      continue;

    bnet->QueueGameUncreate();
    bnet->QueueEnterChat();
  }

  // record everything we need to ban each player in case we decide to do so later
  // this is because when a player leaves the game an admin might want to ban that player
  // but since the player has already left the game we don't have access to their information anymore
  // so we create a "potential ban" for each player and only store it in the database if requested to by an admin

  for (auto& player : m_Players) {
    m_DBBans.push_back(new CDBBan(player->GetRealmDataBaseID(false), player->GetName(), string(), string(), string()));
  }
}

void CGame::EventGameLoaded()
{
  Print(GetLogPrefix() + "finished loading with " + to_string(GetNumHumanPlayers()) + " players");

  // send shortest, longest, and personal load times to each player

  const CGamePlayer* Shortest = nullptr;
  const CGamePlayer* Longest  = nullptr;

  for (const auto& player : m_Players)
  {
    if (!Shortest || player->GetFinishedLoadingTicks() < Shortest->GetFinishedLoadingTicks())
      Shortest = player;

    if (!Longest || player->GetFinishedLoadingTicks() > Longest->GetFinishedLoadingTicks())
      Longest = player;
  }

  if (Shortest && Longest)
  {
    SendAllChat("Shortest load by player [" + Shortest->GetName() + "] was " + ToFormattedString(static_cast<double>(Shortest->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000.f) + " seconds");
    SendAllChat("Longest load by player [" + Longest->GetName() + "] was " + ToFormattedString(static_cast<double>(Longest->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000.f) + " seconds");
  }

  for (auto& player : m_Players)
    SendChat(player, "Your load time was " + ToFormattedString(static_cast<double>(player->GetFinishedLoadingTicks() - m_StartedLoadingTicks) / 1000.f) + " seconds");

  if (GetNumConnectionsOrFake() < 2) {
    // This creates a lag spike client-side.
    StopPlayers("single-player game untracked");
  }
}

uint8_t CGame::GetSIDFromPID(uint8_t PID) const
{
  if (m_Slots.size() > 255)
    return 255;

  for (uint8_t i = 0; i < m_Slots.size(); ++i)
  {
    if (m_Slots[i].GetPID() == PID)
      return i;
  }

  return 255;
}

CGamePlayer* CGame::GetPlayerFromPID(uint8_t PID) const
{
  for (auto& player : m_Players)
  {
    if (!player->GetLeftMessageSent() && player->GetPID() == PID)
      return player;
  }

  return nullptr;
}

CGamePlayer* CGame::GetPlayerFromSID(uint8_t SID) const
{
  if (SID >= m_Slots.size())
    return nullptr;

  const uint8_t PID = m_Slots[SID].GetPID();

  for (auto& player : m_Players)
  {
    if (!player->GetLeftMessageSent() && player->GetPID() == PID)
      return player;
  }

  return nullptr;
}

bool CGame::HasOwnerSet() const
{
  return !m_OwnerName.empty();
}

bool CGame::HasOwnerInGame() const
{
  CGamePlayer* MaybeOwner = GetPlayerFromName(m_OwnerName, false);
  if (!MaybeOwner) return false;
  if (MaybeOwner->IsRealmVerified() && MaybeOwner->GetRealmHostName() == m_OwnerRealm) return true;
  if (MaybeOwner->GetRealmHostName().empty() && m_OwnerRealm.empty()) return true;
  return false;
}

CGamePlayer* CGame::GetPlayerFromName(string name, bool sensitive) const
{
  if (!sensitive)
    transform(begin(name), end(name), begin(name), ::tolower);

  for (auto& player : m_Players)
  {
    if (!player->GetLeftMessageSent())
    {
      string TestName = player->GetName();

      if (!sensitive)
        transform(begin(TestName), end(TestName), begin(TestName), ::tolower);

      if (TestName == name)
        return player;
    }
  }

  return nullptr;
}

uint8_t CGame::GetPlayerFromNamePartial(string name, CGamePlayer** player) const
{
  transform(begin(name), end(name), begin(name), ::tolower);
  uint8_t Matches = 0;
  *player          = nullptr;

  // try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")

  for (auto& realplayer : m_Players)
  {
    if (!realplayer->GetLeftMessageSent())
    {
      string TestName = realplayer->GetName();
      transform(begin(TestName), end(TestName), begin(TestName), ::tolower);

      if (TestName.find(name) != string::npos)
      {
        ++Matches;
        *player = realplayer;

        // if the name matches exactly stop any further matching

        if (TestName == name)
        {
          Matches = 1;
          break;
        }
      }
    }
  }

  return Matches;
}

string CGame::GetDBPlayerNameFromColour(uint8_t colour) const
{
  for (const auto& player : m_DBGamePlayers)
  {
    if (player->GetColour() == colour)
      return player->GetName();
  }

  return string();
}

CGamePlayer* CGame::GetPlayerFromColour(uint8_t colour) const
{
  for (uint8_t i = 0; i < m_Slots.size(); ++i)
  {
    if (m_Slots[i].GetColour() == colour)
      return GetPlayerFromSID(i);
  }

  return nullptr;
}

uint8_t CGame::GetNewPID() const
{
  // find an unused PID for a new player to use

  for (uint8_t TestPID = 1; TestPID < 255; ++TestPID)
  {
    if (TestPID == m_VirtualHostPID)
      continue;

    bool InUse = false;

    for (auto& fakeplayer : m_FakePlayers)
    {
      if (fakeplayer == TestPID)
      {
        InUse = true;
        break;
      }
    }

    if (InUse)
      continue;

    for (auto& player : m_Players)
    {
      if (!player->GetLeftMessageSent() && player->GetPID() == TestPID)
      {
        InUse = true;
        break;
      }
    }

    if (!InUse)
      return TestPID;
  }

  // this should never happen

  return 255;
}

uint8_t CGame::GetNewColour() const
{
  // find an unused colour for a player to use

  for (uint8_t TestColour = 0; TestColour < m_Aura->m_MaxSlots; ++TestColour)
  {
    bool InUse = false;

    for (auto& slot : m_Slots)
    {
      if (slot.GetColour() == TestColour)
      {
        InUse = true;
        break;
      }
    }

    if (!InUse)
      return TestColour;
  }

  // this should never happen

  return m_Aura->m_MaxSlots;
}

std::vector<uint8_t> CGame::GetPIDs() const
{
  std::vector<uint8_t> result;

  for (auto& player : m_Players)
  {
    if (!player->GetLeftMessageSent())
      result.push_back(player->GetPID());
  }

  return result;
}

std::vector<uint8_t> CGame::GetPIDs(uint8_t excludePID) const
{
  std::vector<uint8_t> result;

  for (auto& player : m_Players)
  {
    if (!player->GetLeftMessageSent() && player->GetPID() != excludePID)
      result.push_back(player->GetPID());
  }

  return result;
}

uint8_t CGame::GetHostPID() const
{
  // return the player to be considered the host (it can be any player) - mainly used for sending text messages from the bot
  // try to find the virtual host player first

  if (m_VirtualHostPID != 255)
    return m_VirtualHostPID;

  // try to find the fakeplayer next

  if (!m_FakePlayers.empty())
    return m_FakePlayers[0];

  // try to find the owner player next

  for (auto& player : m_Players)
  {
    if (player->GetLeftMessageSent())
      continue;

    if (MatchOwnerName(player->GetName())) {
      if (player->IsRealmVerified() && player->GetRealmHostName() == m_OwnerRealm) {
        return player->GetPID();
      }
      if (player->GetRealmHostName().empty() && m_OwnerRealm.empty()) {
        return player->GetPID();
      }
      break;
    }
  }

  // okay then, just use the first available player

  for (auto& player : m_Players)
  {
    if (!player->GetLeftMessageSent())
      return player->GetPID();
  }

  return 255;
}

uint8_t CGame::GetEmptySlot(bool reserved) const
{
  if (m_Slots.size() > 255)
    return 255;

  // look for an empty slot for a new player to occupy
  // if reserved is true then we're willing to use closed or occupied slots as long as it wouldn't displace a player with a reserved slot

  for (uint8_t i = 0; i < m_Slots.size(); ++i)
  {
    if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN)
      return i;
  }

  if (reserved)
  {
    // no empty slots, but since player is reserved give them a closed slot

    for (uint8_t i = 0; i < m_Slots.size(); ++i)
    {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_CLOSED)
        return i;
    }

    // no closed slots either, give them an occupied slot but not one occupied by another reserved player
    // first look for a player who is downloading the map and has the least amount downloaded so far

    uint8_t LeastDownloaded = 100;
    uint8_t LeastSID        = 255;

    for (uint8_t i = 0; i < m_Slots.size(); ++i)
    {
      CGamePlayer* Player = GetPlayerFromSID(i);

      if (Player && !Player->GetReserved() && m_Slots[i].GetDownloadStatus() < LeastDownloaded)
      {
        LeastDownloaded = m_Slots[i].GetDownloadStatus();
        LeastSID        = i;
      }
    }

    if (LeastSID != 255)
      return LeastSID;

    // nobody who isn't reserved is downloading the map, just choose the first player who isn't reserved

    for (uint8_t i = 0; i < m_Slots.size(); ++i)
    {
      CGamePlayer* Player = GetPlayerFromSID(i);

      if (Player && !Player->GetReserved())
        return i;
    }
  }

  return 255;
}

uint8_t CGame::GetEmptySlot(uint8_t team, uint8_t PID) const
{
  if (m_Slots.size() > 255)
    return 255;

  // find an empty slot based on player's current slot

  uint8_t StartSlot = GetSIDFromPID(PID);

  if (StartSlot < m_Slots.size())
  {
    if (m_Slots[StartSlot].GetTeam() != team)
    {
      // player is trying to move to another team so start looking from the first slot on that team
      // we actually just start looking from the very first slot since the next few loops will check the team for us

      StartSlot = 0;
    }

    // find an empty slot on the correct team starting from StartSlot

    for (uint8_t i = StartSlot; i < m_Slots.size(); ++i)
    {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team)
        return i;
    }

    // didn't find an empty slot, but we could have missed one with SID < StartSlot
    // e.g. in the DotA case where I am in slot 4 (yellow), slot 5 (orange) is occupied, and slot 1 (blue) is open and I am trying to move to another slot

    for (uint8_t i = 0; i < StartSlot; ++i)
    {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team)
        return i;
    }
  }

  return 255;
}

void CGame::SwapSlots(uint8_t SID1, uint8_t SID2)
{
  if (SID1 < m_Slots.size() && SID2 < m_Slots.size() && SID1 != SID2)
  {
    CGameSlot Slot1 = m_Slots[SID1];
    CGameSlot Slot2 = m_Slots[SID2];

    if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
    {
      // don't swap the team, colour, race, or handicap
      m_Slots[SID1] = CGameSlot(Slot2.GetPID(), Slot2.GetDownloadStatus(), Slot2.GetSlotStatus(), Slot2.GetComputer(), Slot1.GetTeam(), Slot1.GetColour(), Slot1.GetRace(), Slot2.GetComputerType(), Slot1.GetHandicap());
      m_Slots[SID2] = CGameSlot(Slot1.GetPID(), Slot1.GetDownloadStatus(), Slot1.GetSlotStatus(), Slot1.GetComputer(), Slot2.GetTeam(), Slot2.GetColour(), Slot2.GetRace(), Slot1.GetComputerType(), Slot2.GetHandicap());
    } else {
      // swap everything

      if (m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES) {
        // except if custom forces is set, then we don't swap teams...
        Slot1.SetTeam(m_Slots[SID2].GetTeam());
        Slot2.SetTeam(m_Slots[SID1].GetTeam());
      }

      m_Slots[SID1] = Slot2;
      m_Slots[SID2] = Slot1;
      CGamePlayer* PlayerOne = GetPlayerFromSID(SID1);
      CGamePlayer* PlayerTwo = GetPlayerFromSID(SID2);
      if (PlayerOne) {
        PlayerOne->SetObserver(Slot2.GetTeam() == m_Aura->m_MaxSlots);
        if (PlayerOne->GetObserver())
          PlayerOne->SetPowerObserver(PlayerOne->GetObserver() && m_Map->GetMapObservers() == MAPOBS_REFEREES);
      }
      if (PlayerTwo) {
        PlayerTwo->SetObserver(Slot1.GetTeam() == m_Aura->m_MaxSlots);
        if (PlayerTwo->GetObserver())
          PlayerTwo->SetPowerObserver(PlayerTwo->GetObserver() && m_Map->GetMapObservers() == MAPOBS_REFEREES);
      }
    }

    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
}

bool CGame::OpenSlot(uint8_t SID, bool kick)
{
  if (SID < m_Slots.size())
  {
    CGamePlayer* Player = GetPlayerFromSID(SID);
    if (Player && !Player->GetDeleteMe()) {
      if (!kick) return false;
      Player->SetDeleteMe(true);
      Player->SetLeftReason("was kicked when opening a slot");
      Player->SetLeftCode(PLAYERLEAVE_LOBBY);
    }

    CGameSlot Slot = m_Slots[SID];
    m_Slots[SID]   = CGameSlot(0, 255, SLOTSTATUS_OPEN, 0, Slot.GetTeam(), Slot.GetColour(), m_Map->GetLobbyRace(&Slot));
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }

  return true;
}

bool CGame::CloseSlot(uint8_t SID, bool kick)
{
  if (SID < m_Slots.size())
  {
    CGamePlayer* Player = GetPlayerFromSID(SID);
    if (Player && !Player->GetDeleteMe()) {
      if (!kick) return false;
      Player->SetDeleteMe(true);
      Player->SetLeftReason("was kicked when closing a slot");
      Player->SetLeftCode(PLAYERLEAVE_LOBBY);
    }
    CGameSlot Slot = m_Slots[SID];
    if (Slot.GetSlotStatus() == SLOTSTATUS_OPEN && GetSlotsOpen() == 1 && GetNumConnectionsOrFake() > 1)
      DeleteVirtualHost();
    m_Slots[SID]   = CGameSlot(0, 255, SLOTSTATUS_CLOSED, 0, Slot.GetTeam(), Slot.GetColour(), m_Map->GetLobbyRace(&Slot));
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
  return true;
}

bool CGame::ComputerSlot(uint8_t SID, uint8_t skill, bool kick)
{
  if (SID < m_Slots.size() && skill < 3) {
    CGamePlayer* Player = GetPlayerFromSID(SID);
    if (Player && !Player->GetDeleteMe()) {
      if (!kick) return false;
      Player->SetDeleteMe(true);
      Player->SetLeftReason("was kicked when creating a computer in a slot");
      Player->SetLeftCode(PLAYERLEAVE_LOBBY);
    }

    CGameSlot Slot = m_Slots[SID];
    if (Slot.GetSlotStatus() == SLOTSTATUS_OPEN && GetSlotsOpen() == 1 && GetNumConnectionsOrFake() > 1)
      DeleteVirtualHost();
    m_Slots[SID]   = CGameSlot(0, 100, SLOTSTATUS_OCCUPIED, 1, Slot.GetTeam(), Slot.GetColour(), m_Map->GetLobbyRace(&Slot), skill);
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
  return true;
}

void CGame::ColorSlot(uint8_t SID, uint8_t colour)
{
  if (SID < m_Slots.size() && colour < m_Aura->m_MaxSlots)
  {
    // make sure the requested colour isn't already taken

    optional<uint8_t> TakenSID;
    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      if (m_Slots[i].GetColour() == colour) {
        TakenSID = i;
      }
    }

    if (TakenSID.has_value()) {
      m_Slots[TakenSID.value()].SetColour(m_Slots[SID].GetColour());
    }
    m_Slots[SID].SetColour(colour);
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
}

void CGame::OpenAllSlots()
{
  bool Changed = false;

  for (auto& slot : m_Slots)
  {
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED)
    {
      slot.SetSlotStatus(SLOTSTATUS_OPEN);
      Changed = true;
    }
  }

  if (Changed) {
    CreateVirtualHost();
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
}

void CGame::CloseAllSlots()
{
  bool Changed = false;

  for (auto& slot : m_Slots)
  {
    if (slot.GetSlotStatus() == SLOTSTATUS_OPEN)
    {
      slot.SetSlotStatus(SLOTSTATUS_CLOSED);
      Changed = true;
    }
  }

  if (Changed) {
    if (GetNumConnectionsOrFake() > 1)
      DeleteVirtualHost();
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
}

void CGame::ComputerAllSlots(uint8_t skill)
{
  bool Changed = false;

  uint8_t SID = 0;

  while (SID < m_Slots.size()) {
    CGameSlot Slot = m_Slots[SID];
    if (Slot.GetSlotStatus() == SLOTSTATUS_OPEN) {
      m_Slots[SID]   = CGameSlot(0, 100, SLOTSTATUS_OCCUPIED, 1, Slot.GetTeam(), Slot.GetColour(), m_Map->GetLobbyRace(&Slot), skill);
      Changed = true;
    }
    ++SID;
  }

  if (Changed) {
    if (GetNumConnectionsOrFake() > 1)
      DeleteVirtualHost();
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }
}

void CGame::ShuffleSlots()
{
  // we only want to shuffle the player slots (exclude observers)
  // that means we need to prevent this function from shuffling the open/closed/computer slots too
  // so we start by copying the player slots to a temporary vector

  vector<CGameSlot> PlayerSlots;

  for (auto& slot : m_Slots)
  {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetComputer() == 0 && slot.GetTeam() != m_Aura->m_MaxSlots)
      PlayerSlots.push_back(slot);
  }

  // now we shuffle PlayerSlots

  if (m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES)
  {
    // rather than rolling our own probably broken shuffle algorithm we use random_shuffle because it's guaranteed to do it properly
    // so in order to let random_shuffle do all the work we need a vector to operate on
    // unfortunately we can't just use PlayerSlots because the team/colour/race shouldn't be modified
    // so make a vector we can use

    vector<uint8_t> SIDs;

    for (uint8_t i = 0; i < PlayerSlots.size(); ++i)
      SIDs.push_back(i);

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(begin(SIDs), end(SIDs), g);

    // now put the PlayerSlots vector in the same order as the SIDs vector

    vector<CGameSlot> Slots;

    // as usual don't modify the team/colour/race

    for (uint8_t i = 0; i < SIDs.size(); ++i)
      Slots.emplace_back(PlayerSlots[SIDs[i]].GetPID(), PlayerSlots[SIDs[i]].GetDownloadStatus(), PlayerSlots[SIDs[i]].GetSlotStatus(), PlayerSlots[SIDs[i]].GetComputer(), PlayerSlots[i].GetTeam(), PlayerSlots[i].GetColour(), PlayerSlots[i].GetRace());

    PlayerSlots = Slots;
  }
  else
  {
    // regular game
    // it's easy when we're allowed to swap the team/colour/race!

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(begin(PlayerSlots), end(PlayerSlots), g);
  }

  // now we put m_Slots back together again

  auto              CurrentPlayer = begin(PlayerSlots);
  vector<CGameSlot> Slots;

  for (auto& slot : m_Slots)
  {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetComputer() == 0 && slot.GetTeam() != m_Aura->m_MaxSlots)
    {
      Slots.push_back(*CurrentPlayer);
      ++CurrentPlayer;
    }
    else
      Slots.push_back(slot);
  }

  m_Slots = Slots;
  m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
}

void CGame::ReportSpoofed(const string& server, CGamePlayer* Player)
{
  SendAllChat("Name spoof detected. The real [" + Player->GetName() + "@" + server + "] is not in this game.");
  if (GetIsLobby() && MatchOwnerName(Player->GetName())) {
    Player->SetDeleteMe(true);
    Player->SetLeftReason("was kicked for spoofing the game owner");
    Player->SetLeftCode(PLAYERLEAVE_LOBBY);
    OpenSlot(GetSIDFromPID(Player->GetPID()), false);
  }
}

void CGame::AddToRealmVerified(const string& server, CGamePlayer* Player, bool sendMessage)
{
  Player->SetRealmVerified(true);
  if (sendMessage) {
    if (MatchOwnerName(Player->GetName()) && m_OwnerRealm == Player->GetRealmHostName()) {
      SendAllChat("Identity accepted for game owner [" + Player->GetName() + "@" + server + "]");
    } else {
      SendChat(Player, "Identity accepted for [" + Player->GetName() + "@" + server + "]");
    }
  }
}

void CGame::AddToReserved(string name)
{
  transform(begin(name), end(name), begin(name), ::tolower);

  // check that the user is not already reserved

  for (auto& player : m_Reserved)
  {
    if (player == name)
      return;
  }

  m_Reserved.push_back(name);

  // upgrade the user if they're already in the game

  for (auto& player : m_Players)
  {
    string NameLower = player->GetName();
    transform(begin(NameLower), end(NameLower), begin(NameLower), ::tolower);

    if (NameLower == name)
      player->SetReserved(true);
  }
}

void CGame::RemoveFromReserved(string name)
{
  transform(begin(name), end(name), begin(name), ::tolower);

  auto it = find(begin(m_Reserved), end(m_Reserved), name);

  if (it != end(m_Reserved))
  {
    m_Reserved.erase(it);

    // demote the user if they're already in the game

    for (auto& player : m_Players)
    {
      string NameLower = player->GetName();
      transform(begin(NameLower), end(NameLower), begin(NameLower), ::tolower);

      if (NameLower == name)
        player->SetReserved(false);
    }
  }
}

bool CGame::MatchOwnerName(string name) const
{
  string OwnerLower = m_OwnerName;
  transform(begin(name), end(name), begin(name), ::tolower);
  transform(begin(OwnerLower), end(OwnerLower), begin(OwnerLower), ::tolower);

  return name == OwnerLower;
}

bool CGame::GetIsReserved(string name) const
{
  transform(begin(name), end(name), begin(name), ::tolower);

  for (auto& player : m_Reserved)
  {
    if (player == name)
      return true;
  }

  return false;
}

bool CGame::IsDownloading() const
{
  // returns true if at least one player is downloading the map

  for (auto& player : m_Players)
  {
    if (player->GetDownloadStarted() && !player->GetDownloadFinished())
      return true;
  }

  return false;
}

void CGame::SetOwner(std::string name, std::string realm)
{
  m_OwnerName = name;
  m_OwnerRealm = realm;
}

void CGame::ReleaseOwner()
{
  Print("[LOBBY: "  + m_GameName + "] Owner \"" + m_OwnerName + "@" + (m_OwnerRealm.empty() ? "@@LAN/VPN" : m_OwnerRealm) + "\" removed.");
  m_OwnerName = "";
  m_OwnerRealm = "";
  m_Locked = false;
  SendAllChat("This game is now ownerless. Type " + string(1, m_CommandTrigger) + "owner to take ownership of this game.");
}

void CGame::ResetSync()
{
  m_SyncCounter = 0;
  for (auto& TargetPlayer: m_Players) {
    TargetPlayer->SetSyncCounter(0);
  }
}

void CGame::CountKickVotes()
{
  uint32_t Votes = 0, VotesNeeded = static_cast<uint32_t>(ceil((GetNumHumanPlayers() - 1) * static_cast<float>(m_VoteKickPercentage) / 100));
  for (auto& eachPlayer : m_Players) {
    if (eachPlayer->GetKickVote().value_or(false))
      ++Votes;
  }

  if (Votes >= VotesNeeded) {
    CGamePlayer* Victim = GetPlayerFromName(m_KickVotePlayer, true);

    if (Victim) {
      Victim->SetDeleteMe(true);
      Victim->SetLeftReason("was kicked by vote");

      if (GetIsLobby())
        Victim->SetLeftCode(PLAYERLEAVE_LOBBY);
      else
        Victim->SetLeftCode(PLAYERLEAVE_LOST);

      if (GetIsLobby())
        OpenSlot(GetSIDFromPID(Victim->GetPID()), false);

      Print(GetLogPrefix() + "votekick against player [" + m_KickVotePlayer + "] passed with " + to_string(Votes) + "/" + to_string(GetNumHumanPlayers()) + " votes");
      SendAllChat("A votekick against player [" + m_KickVotePlayer + "] has passed");
    } else {
      Print(GetLogPrefix() + "votekick against player [" + m_KickVotePlayer + "] errored");
    }

    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }
}

void CGame::StartCountDown(bool force)
{
  if (m_CountDownStarted)
    return;

  // if the player sent "!start force" skip the checks and start the countdown
  // otherwise check that the game is ready to start

  if (force) {
    for (auto& player : m_Players) {
      if (!player->GetHasMap()) {
        player->SetDeleteMe(true);
        player->SetLeftReason("kicked when starting the game");
        player->SetLeftCode(PLAYERLEAVE_LOBBY);
        CloseSlot(GetSIDFromPID(player->GetPID()), false);
      }
    }
  } else {
    bool ChecksPassed = true;

    // check if the HCL command string is short enough
    if (m_HCLCommandString.size() > GetSlotsOccupied()) {
      SendAllChat("The HCL command string is too long. Use 'force' to start anyway");
      ChecksPassed = false;
    }

    // check if everyone has the map
    string StillDownloading;
    for (auto& slot : m_Slots) {
      if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetComputer() == 0 && slot.GetDownloadStatus() != 100) {
        CGamePlayer* Player = GetPlayerFromPID(slot.GetPID());
        if (Player) {
          if (StillDownloading.empty())
            StillDownloading = Player->GetName();
          else
            StillDownloading += ", " + Player->GetName();
        }
      }
    }
    if (!StillDownloading.empty()) {
      SendAllChat("Players still downloading the map: " + StillDownloading);
      ChecksPassed = false;
    } else if (m_PlayersWithMap < 2) {
      SendAllChat("Only " + to_string(m_PlayersWithMap) + " player has the map.");
      ChecksPassed = false;
    }

    // check if everyone has been pinged enough (3 times) that the autokicker would have kicked them by now
    // see function EventPlayerPongToHost for the autokicker code
    string NotPinged;
    for (auto& player : m_Players) {
      if (!player->GetReserved() && player->GetNumPings() < 3) {
        if (NotPinged.empty())
          NotPinged = player->GetName();
        else
          NotPinged += ", " + player->GetName();
      }
    }
    if (!NotPinged.empty()) {
      SendAllChat("Players not yet pinged 3 times: " + NotPinged);
      ChecksPassed = false;
    }
    if (GetTicks() < m_LastPlayerLeaveTicks + 2000) {
      SendAllChat("Someone left the game less than two seconds ago!");
      ChecksPassed = false;
    }
    if (GetNumConnectionsOrFake() == 1 && m_Map->GetMapObservers() != MAPOBS_REFEREES) {
      SendAllChat("Single-player game requires map observers set to referees. Or add a fake player instead (type " + string(1, m_CommandTrigger) + "fp)");
      ChecksPassed = false;
    }

    if (!ChecksPassed)
      return;
  }

  m_CountDownStarted = true;
  m_CountDownCounter = 5;

  if (!m_KickVotePlayer.empty()) {
    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }

  for (auto& player : m_Players) {
    if (player->GetKickQueued())
      player->SetKickByTime(0);
  }
}

void CGame::StopPlayers(const string& reason)
{
  // disconnect every player and set their left reason to the passed string
  // we use this function when we want the code in the Update function to run before the destructor (e.g. saving players to the database)
  // therefore calling this function when m_GameLoading || m_GameLoaded is roughly equivalent to setting m_Exiting = true
  // the only difference is whether the code in the Update function is executed or not

  for (auto& player : m_Players) {
    player->SetDeleteMe(true);
    player->SetLeftReason(reason);
    player->SetLeftCode(PLAYERLEAVE_LOST);
  }
}

void CGame::StopLaggers(const string& reason)
{
  for (auto& player : m_Players) {
    if (player->GetLagging()) {
      player->SetDeleteMe(true);
      player->SetLeftReason(reason);
      player->SetLeftCode(PLAYERLEAVE_DISCONNECT);
    }
  }
}

// Virtual host is needed to generate network traffic when only one player is in the game or lobby.
// Fake players may also accomplish the same purpose.
bool CGame::CreateVirtualHost()
{
  if (m_VirtualHostPID != 255)
    return false;

  m_VirtualHostPID = GetNewPID();

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  SendAll(GetProtocol()->SEND_W3GS_PLAYERINFO(m_VirtualHostPID, m_LobbyVirtualHostName, IP, IP));
  return true;
}

bool CGame::DeleteVirtualHost()
{
  if (m_VirtualHostPID == 255)
    return false;

  SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(m_VirtualHostPID, PLAYERLEAVE_LOBBY));
  m_VirtualHostPID = 255;
  return true;
}

bool CGame::CreateFakePlayer()
{
  if (m_FakePlayers.size() + 1 == m_Slots.size())
    return false;

  uint8_t SID = GetEmptySlot(false);

  if (SID < m_Slots.size()) {
    if (GetSlotsOpen() == 1)
      DeleteVirtualHost();

    const uint8_t              FakePlayerPID = GetNewPID();
    const std::vector<uint8_t> IP            = {0, 0, 0, 0};

    SendAll(GetProtocol()->SEND_W3GS_PLAYERINFO(FakePlayerPID, "Placeholder[" + to_string(FakePlayerPID) + "]", IP, IP));
    m_Slots[SID] = CGameSlot(FakePlayerPID, 100, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam(), m_Slots[SID].GetColour(), m_Map->GetLobbyRace(&m_Slots[SID]));
    m_FakePlayers.push_back(FakePlayerPID);
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
    return true;
  }

  return false;
}

bool CGame::CreateFakeObserver()
{
  if (m_FakePlayers.size() + 1 == m_Slots.size())
    return false;

  uint8_t SID = GetEmptySlot(false);

  if (SID < m_Slots.size()) {
    if (GetSlotsOpen() == 1)
      DeleteVirtualHost();

    const uint8_t              FakePlayerPID = GetNewPID();
    const std::vector<uint8_t> IP            = {0, 0, 0, 0};

    SendAll(GetProtocol()->SEND_W3GS_PLAYERINFO(FakePlayerPID, "Placeholder[" + to_string(FakePlayerPID) + "]", IP, IP));
    m_Slots[SID] = CGameSlot(FakePlayerPID, 100, SLOTSTATUS_OCCUPIED, 0, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, m_Map->GetLobbyRace(&m_Slots[SID]));
    m_FakePlayers.push_back(FakePlayerPID);
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
    return true;
  }

  return false;
}

bool CGame::DeleteFakePlayer(uint8_t SID)
{
  if (SID < m_Slots.size()) {
    for (auto i = begin(m_FakePlayers); i != end(m_FakePlayers); ++i) {
      if (m_Slots[SID].GetPID() == (*i)) {
        m_Slots[SID] = CGameSlot(0, 255, SLOTSTATUS_OPEN, 0, m_Slots[SID].GetTeam(), m_Slots[SID].GetColour(), m_Map->GetLobbyRace(&m_Slots[SID]));
        SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(*i, PLAYERLEAVE_LOBBY));
        m_FakePlayers.erase(i);
        CreateVirtualHost();
        m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
        return true;
      }
    }
  }
  return false;
}

void CGame::DeleteFakePlayers()
{
  if (m_FakePlayers.empty())
    return;

  for (auto& fakeplayer : m_FakePlayers) {
    for (auto& slot : m_Slots) {
      if (slot.GetPID() == fakeplayer) {
        slot = CGameSlot(0, 255, SLOTSTATUS_OPEN, 0, slot.GetTeam(), slot.GetColour(), m_Map->GetLobbyRace(&slot));
        SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(fakeplayer, PLAYERLEAVE_LOBBY));
        break;
      }
    }
  }

  m_FakePlayers.clear();
  CreateVirtualHost();
  m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
}
