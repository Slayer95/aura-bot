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

#include "game.h"
#include "command.h"
#include "aura.h"
#include "util.h"
#include "config.h"
#include "config_bot.h"
#include "config_game.h"
#include "config_commands.h"
#include "irc.h"
#include "socket.h"
#include "net.h"
#include "auradb.h"
#include "realm.h"
#include "map.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "stats.h"
#include "irc.h"
#include "fileutil.h"

#include <bitset>
#include <ctime>
#include <cmath>

using namespace std;

//
// CGame
//

CGame::CGame(CAura* nAura, CGameSetup* nGameSetup)
  : m_Aura(nAura),
    m_Verbose(nGameSetup->m_Verbose),
    m_Socket(nullptr),
    m_DBBanLast(nullptr),
    m_Stats(nullptr),
    m_RestoredGame(nGameSetup->m_RestoredGame),
    m_Map(nGameSetup->m_Map),
    m_GameName(nGameSetup->m_GameName),
    m_OwnerName(nGameSetup->m_GameOwner.first),
    m_OwnerRealm(nGameSetup->m_GameOwner.second),
    m_CreatorText(nGameSetup->m_Attribution),
    m_CreatedBy(nGameSetup->m_CreatedBy),
    m_CreatedFrom(nGameSetup->m_CreatedFrom),
    m_CreatedFromType(nGameSetup->m_CreatedFromType),
    m_RealmsExcluded(nGameSetup->m_RealmsExcluded),
    m_HCLCommandString(nGameSetup->m_Map->GetMapDefaultHCL()),
    m_MapPath(nGameSetup->m_Map->GetClientPath()),
    m_MapSiteURL(nGameSetup->m_Map->GetMapSiteURL()),
    m_GameTicks(0),
    m_CreationTime(GetTime()),
    m_LastPingTime(GetTime()),
    m_LastRefreshTime(GetTime()),
    m_LastDownloadTicks(GetTime()),
    m_LastDownloadCounterResetTicks(GetTicks()),
    m_LastCountDownTicks(0),
    m_StartedLoadingTicks(0),
    m_FinishedLoadingTicks(0),
    m_LastActionSentTicks(0),
    m_LastActionLateBy(0),
    m_StartedLaggingTime(0),
    m_LastLagScreenTime(0),
    m_LastOwnerSeen(GetTime()),
    m_StartedKickVoteTime(0),
    m_GameOverTime(0),
    m_LastPlayerLeaveTicks(0),
    m_LastLagScreenResetTime(0),
    m_PauseCounter(0),
    m_SaveCounter(0),
    m_RandomSeed(0),
    m_HostCounter(nGameSetup->m_GameIdentifier.has_value() ? nGameSetup->m_GameIdentifier.value() : nAura->NextHostCounter()),
    m_EntryKey(0),
    m_SyncCounter(0),
    m_CheckJoinable(false),
    m_DownloadCounter(0),
    m_CountDownCounter(0),
    m_StartPlayers(0),
    m_AutoStartMinTime(0),
    m_AutoStartMaxTime(0),
    m_AutoStartPlayers(0),
    m_ControllersWithMap(0),
    m_CustomLayout(MAPLAYOUT_ANY),
    m_CustomLayoutData(make_pair(nAura->m_MaxSlots, nAura->m_MaxSlots)),
    m_HostPort(0),
    m_UDPEnabled(false),
    m_PublicHostOverride(nGameSetup->GetIsMirror()),
    m_GameDisplay(nGameSetup->m_RealmsDisplayMode),
    m_IsAutoVirtualPlayers(false),
    m_VirtualHostPID(0xFF),
    m_Exiting(false),
    m_SlotInfoChanged(0),
    m_PublicStart(false),
    m_Locked(false),
    m_RealmRefreshError(false),
    m_MuteAll(false),
    m_MuteLobby(false),
    m_IsMirror(nGameSetup->GetIsMirror()),
    m_CountDownStarted(false),
    m_GameLoading(false),
    m_GameLoaded(false),
    m_LobbyLoading(false),
    m_Lagging(false),
    m_Paused(false),
    m_Desynced(false),
    m_IsDraftMode(false),
    m_HadLeaver(false),
    m_HasMapLock(false),
    m_CheckReservation(nGameSetup->m_GameChecksReservation.has_value() ? nGameSetup->m_GameChecksReservation.value() : nGameSetup->m_RestoredGame != nullptr),
    m_UsesCustomReferees(false),
    m_SentPriorityWhois(false),
    m_SaveOnLeave(SAVE_ON_LEAVE_AUTO)
{
  m_IndexVirtualHostName = m_Aura->m_GameDefaultConfig->m_IndexVirtualHostName;
  if (m_IndexVirtualHostName.empty()) {
    m_IndexVirtualHostName = m_CreatedBy.empty() ? "Aura Bot" : m_CreatedBy;
  }
  m_LobbyVirtualHostName = m_Aura->m_GameDefaultConfig->m_LobbyVirtualHostName;
  m_Latency = m_Aura->m_GameDefaultConfig->m_Latency;
  m_SyncLimit = m_Aura->m_GameDefaultConfig->m_SyncLimit;
  m_SyncLimitSafe = m_Aura->m_GameDefaultConfig->m_SyncLimitSafe;
  m_AutoKickPing = m_Aura->m_GameDefaultConfig->m_AutoKickPing;
  m_WarnHighPing = m_Aura->m_GameDefaultConfig->m_WarnHighPing;

  m_VoteKickPercentage = m_Aura->m_GameDefaultConfig->m_VoteKickPercentage;
  m_PrivateCmdToken = m_Aura->m_GameDefaultConfig->m_PrivateCmdToken;
  m_BroadcastCmdToken = m_Aura->m_GameDefaultConfig->m_BroadcastCmdToken;
  m_LacksMapKickDelay = m_Aura->m_GameDefaultConfig->m_LacksMapKickDelay;
  m_NotifyJoins = m_Aura->m_GameDefaultConfig->m_NotifyJoins;
  m_PerfThreshold = m_Aura->m_GameDefaultConfig->m_PerfThreshold;
  m_LobbyNoOwnerTime = m_Aura->m_GameDefaultConfig->m_LobbyNoOwnerTime;
  m_LobbyTimeLimit = nGameSetup->m_LobbyTimeout.has_value() ? nGameSetup->m_LobbyTimeout.value() : m_Aura->m_GameDefaultConfig->m_LobbyTimeLimit;
  m_NumPlayersToStartGameOver = m_Aura->m_GameDefaultConfig->m_NumPlayersToStartGameOver;

  m_ExtraDiscoveryAddresses = m_Aura->m_GameDefaultConfig->m_ExtraDiscoveryAddresses;
  m_ExtraDiscoveryStrict = m_Aura->m_GameDefaultConfig->m_ExtraDiscoveryStrict;

  m_IgnoredNotifyJoinPlayers = m_Aura->m_GameDefaultConfig->m_IgnoredNotifyJoinPlayers;

  if (!nGameSetup->GetIsMirror()) {
    m_CheckJoinable = nGameSetup->m_CheckJoinable.has_value() ? nGameSetup->m_CheckJoinable.value() : m_Aura->m_GameDefaultConfig->m_CheckJoinable;

    for (const auto& playerName : nGameSetup->m_GameReservations) {
      AddToReserved(playerName);
    }

    InitPRNG();

    // wait time of 1 minute  = 0 empty actions required
    // wait time of 2 minutes = 1 empty action required...

    if (m_GProxyEmptyActions > 0) {
      m_GProxyEmptyActions = m_Aura->m_Net->m_Config->m_ReconnectWaitTimeLegacy - 1;
      if (m_GProxyEmptyActions > 9)
        m_GProxyEmptyActions = 9;
    }

    // start listening for connections

    uint16_t hostPort = nAura->m_Net->NextHostPort();
    m_Socket = m_Aura->GetGameServer(hostPort, m_GameName);

    if (m_Socket) {
      m_HostPort = m_Socket->GetPort();
    } else {
      m_Exiting = true;
    }

    if (!m_Map->GetUseStandardPaths() && !m_Map->GetMapData()->empty()) {
      filesystem::path localPath = m_Map->GetServerPath();
      const bool isFileName = localPath == localPath.filename();
      if (isFileName) {
        // Only maps with paths that are filenames but not standard set map lock
        // implied: only maps in <bot.maps_path>
        m_HasMapLock = true;
        m_Aura->m_BusyMaps.insert(m_Map->GetServerPath());
      }
    }

    m_UDPEnabled = nAura->m_GameDefaultConfig->m_UDPEnabled;
  } else {
    m_PublicHostAddress = AddressToIPv4Vector(&(nGameSetup->m_RealmsAddress));
    m_PublicHostPort = GetAddressPort(&(nGameSetup->m_RealmsAddress));
  }

  InitSlots();
}

void CGame::Reset(const bool saveStats)
{
  for (auto& entry : m_SyncPlayers) {
    entry.second.clear();
  }
  m_SyncPlayers.clear();

  while (!m_Actions.empty()) {
    delete m_Actions.front();
    m_Actions.pop();
  }

  // store the CDBGamePlayers in the database
  // add non-dota stats
  for (auto& player : m_DBGamePlayers) {
    m_Aura->m_DB->GamePlayerAdd(player->GetName(), player->GetLoadingTime(), m_GameTicks / 1000, player->GetLeft());
  }
  // store the dota stats in the database
  if (saveStats && m_Stats) {
    m_Stats->Save(m_Aura, m_Aura->m_DB);
  }

  for (auto& player : m_DBGamePlayers) {
    delete player;
  }
  m_DBGamePlayers.clear();

  for (auto& ban : m_DBBans) {
    delete ban;
  }
  m_DBBans.clear();

  delete m_Stats;
  m_Stats = nullptr;

  for (auto& ctx : m_Aura->m_ActiveContexts) {
    if (ctx->m_SourceGame == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_SourceGame = nullptr;
    }
    if (ctx->m_TargetGame == this) {
      ctx->SetPartiallyDestroyed();
      ctx->m_TargetGame = nullptr;
    }
  }

  for (auto& realm : m_Aura->m_Realms) {
    realm->ResetGameAnnouncement();
  }
}

void CGame::ReleaseMap()
{
  if (!m_Map) return;

  if (m_HasMapLock) {
    // Only maps with paths that are filenames but not standard set map lock
    // implied: only maps in <bot.maps_path>
    const string localPathString = m_Map->GetServerPath();
    const filesystem::path localPath = localPathString;
    m_Aura->m_BusyMaps.erase(localPathString);
    const bool deleteTooLarge = (
      m_Aura->m_Config->m_EnableDeleteOversizedMaps &&
      (ByteArrayToUInt32(m_Map->GetMapSize(), false) > m_Aura->m_Config->m_MaxSavedMapSize * 1024)
    );
    if (deleteTooLarge && m_Aura->m_BusyMaps.find(localPathString) == m_Aura->m_BusyMaps.end()) {
      // Ensure the mapcache cfg file has been created before trying to delete from disk
      if (m_Aura->m_CachedMaps.find(localPathString) != m_Aura->m_CachedMaps.end()) {
        // Release from disk
        m_Map->UnlinkFile();
      }
    }
  }

  m_HasMapLock = false;

  // Release from memory
  m_Map->ClearMapData();
}

CGame::~CGame()
{
  Reset(true);
  ReleaseMap();
  delete m_Map;
  for (auto& player : m_Players) {
    delete player;
  }
}

void CGame::InitPRNG()
{
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<uint32_t> dis;
  m_RandomSeed = dis(gen);
  m_EntryKey = dis(gen);
}

void CGame::InitSlots()
{
  if (m_RestoredGame) {
    m_Slots = m_RestoredGame->GetSlots();
		// reset player slots
    for (auto& slot : m_Slots) {
      if (slot.GetIsPlayerOrFake()) {
        slot.SetPID(0);
        slot.SetDownloadStatus(100);
        slot.SetSlotStatus(SLOTSTATUS_OPEN);
      }
    }
    return;
  }

  // Done at the CGame level rather than CMap,
  // so that Aura is able to deal with outdated/bugged map configs.

  m_Slots = m_Map->GetSlots();

  // Match actual observer slots to set map flags.
  if (m_Map->GetMapObservers() == MAPOBS_ALLOWED || m_Map->GetMapObservers() == MAPOBS_REFEREES) {
    OpenObserverSlots();
  } else {
    CloseObserverSlots();
  }

  const bool customForces = m_Map->GetMapOptions() & MAPOPT_CUSTOMFORCES;
  const bool fixedPlayers = m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS;
  bitset<MAX_SLOTS_MODERN> usedColors;
  for (auto& slot : m_Slots) {
    slot.SetPID(0);
    slot.SetDownloadStatus(SLOTPROG_RST);
    if (slot.GetIsComputer()) {
      // The way WC3 client treats computer slots defined from WorldEdit depends on the
      // Fixed Player Settings flag:
      //  - OFF: Any computer slots are ignored, and they are treated as Open slots instead.
      //  - ON: Computer slots are enforced. They cannot be removed, or edited in any way.
      //
      // For Aura, enforcing computer slots with Fixed Player Settings ON is a must.
      // However, we can support default editable computer slots when it's OFF, through mapcfg files.
      //
      // All this also means that when Fixed Player Settings is off, there are no unselectable slots.
      slot.SetComputer(SLOTCOMP_YES | (fixedPlayers ? SLOTCOMP_FIXED : SLOTCOMP_NO));
      slot.SetSlotStatus(SLOTSTATUS_OCCUPIED);
    } else {
      slot.SetComputer(SLOTCOMP_NO);
      slot.SetSlotStatus(slot.GetSlotStatus() & SLOTSTATUS_VALID);
    }

    if (!slot.GetIsSelectable()) {
      // There is no way to define default handicaps using WorldEdit.
      slot.SetHandicap(100);
      // There is no way to define default difficulty using WorldEdit.
      slot.SetComputerType(SLOTCOMP_NORMAL);
    } else {
      // Handicap valid engine values are 50, 60, 70, 80, 90, 100
      // The other 250 uint8 values may be set on-the-fly by Aura,
      // and are used by maps that implement HCL.
      //
      // Aura supports default handicaps through mapcfg files.
      uint8_t handicap = slot.GetHandicap() / 10;
      if (handicap < 5) handicap = 5;
      if (handicap > 10) handicap = 10;
      slot.SetHandicap(handicap * 10);
      slot.SetComputerType(slot.GetComputerType() & SLOTCOMP_VALID);
    }

    if (!customForces) {
      // default user-customizable slot is always observer
      // only when players join do we assign them a team
      // (if they leave, the slots are reset to observers)
      slot.SetTeam(m_Aura->m_MaxSlots);
    }

    // Ensure colors are unique for each playable slot.
    // Observers must have color 12 or 24, according to game version.
    if (slot.GetTeam() == m_Aura->m_MaxSlots) {
      slot.SetColor(m_Aura->m_MaxSlots);
    } else {
      const uint8_t originalColor = slot.GetColor();
      if (usedColors.test(originalColor)) {
        uint8_t testColor = originalColor;
        do {
          testColor = (testColor + 1) % m_Aura->m_MaxSlots;
        } while (usedColors.test(testColor) && testColor != originalColor);
        slot.SetColor(testColor);
        usedColors.set(testColor);
      } else {
        usedColors.set(originalColor);
      }
    }

    // When Fixed Player Settings is enabled, MAPFLAG_RANDOMRACES cannot be turned on.
    if (!fixedPlayers && (m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES)) {
      // default user-customizable slot is always random
      // only when players join do we assign them the selectable race bit
      // (if they leave, the slots are reset to pure random)
      slot.SetRace(SLOTRACE_RANDOM);
    } else {
      // Ensure race is unambiguous. It's defined as a bitfield,
      // so we gotta unset contradictory bits.
      bitset<8> slotRace(slot.GetRace());
      slotRace.reset(7);
      if (fixedPlayers) {
        // disable SLOTRACE_SELECTABLE
        slotRace.reset(6);
      } else {
        // enable SLOTRACE_SELECTABLE
        slotRace.set(6);
      }
      slotRace.reset(4);
      uint8_t chosenRaceBit = 5; // SLOTRACE_RANDOM
      bool foundRace = false;
      while (chosenRaceBit--) {
        // Iterate backwards so that SLOTRACE_RANDOM is preferred
        // Why? Because if someone edited the mapcfg with an ambiguous race,
        // it's likely they don't know what they are doing.
        if (foundRace) {
          slotRace.reset(chosenRaceBit);
        } else {
          foundRace = slotRace.test(chosenRaceBit);
        }
      }
      if (!foundRace) { // Slot is missing a default race.
        chosenRaceBit = 5; // SLOTRACE_RANDOM
        slotRace.set(chosenRaceBit);
        while (chosenRaceBit--) slotRace.reset(chosenRaceBit);
      }
      slot.SetRace(static_cast<uint8_t>(slotRace.to_ulong()));
    }
  }
}

bool CGame::MatchesCreatedFrom(const uint8_t fromType, const void* fromThing) const
{
  if (m_CreatedFromType != fromType) return false;
  switch (fromType) {
    case GAMESETUP_ORIGIN_REALM:
      return reinterpret_cast<const CRealm*>(m_CreatedFrom) == reinterpret_cast<const CRealm*>(fromThing);
    case GAMESETUP_ORIGIN_IRC:
      return reinterpret_cast<const CIRC*>(m_CreatedFrom) == reinterpret_cast<const CIRC*>(fromThing);
    case GAMESETUP_ORIGIN_DISCORD:
      return reinterpret_cast<const CDiscord*>(m_CreatedFrom) == reinterpret_cast<const CDiscord*>(fromThing);
  }
  return false;
}

uint8_t CGame::GetLayout() const
{
  if (m_RestoredGame) return MAPLAYOUT_FIXED_PLAYERS;
  return GetMap()->GetMapLayoutStyle();
}

bool CGame::GetIsCustomForces() const
{
  if (m_RestoredGame) return true;
  return GetMap()->GetMapLayoutStyle() != MAPLAYOUT_ANY;
}

CGameProtocol* CGame::GetProtocol() const
{
  return m_Aura->m_GameProtocol;
}

int64_t CGame::GetNextTimedActionTicks() const
{
  // return the number of ticks (ms) until the next "timed action", which for our purposes is the next game update
  // the main Aura loop will make sure the next loop update happens at or before this value
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

bool CGame::HasSlotsOpen() const
{
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OPEN)
      return true;
  }
  return false;
}

uint32_t CGame::GetNumHumanOrFakeControllers() const
{
  uint32_t NumHumanPlayers = static_cast<uint8_t>(m_FakePlayers.size());

  for (const auto& player : m_Players) {
    if (player->GetDeleteMe())
      continue;

    ++NumHumanPlayers;
  }

  return NumHumanPlayers;
}

uint8_t CGame::GetNumHumanPlayers() const
{
  uint8_t NumHumanPlayers = 0;

  for (const auto& player : m_Players) {
    if (player->GetDeleteMe())
      continue;
    if (player->GetIsObserver())
      continue;

    ++NumHumanPlayers;
  }

  return NumHumanPlayers;
}

uint8_t CGame::GetNumOccupiedSlots() const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      ++count;
    }
  }
  return count;
}

uint8_t CGame::GetNumPotentialControllers() const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      ++count;
    }
  }
  if (count > m_Map->GetMapNumControllers()) {
    return m_Map->GetMapNumControllers();
  }
  return count;
}

uint8_t CGame::GetNumControllers() const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetTeam() != m_Aura->m_MaxSlots) {
      ++count;
    }
  }
  return count;
}

uint8_t CGame::GetNumComputers() const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED && slot.GetIsComputer()) {
      ++count;
    }
  }
  return count;
}

uint8_t CGame::GetNumTeamControllersOrOpen(const uint8_t team) const
{
  uint8_t count = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (slot.GetTeam() == team) {
      ++count;
    }
  }
  return count;
}

string CGame::GetClientFileName() const
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
     return "[" + GetClientFileName() + "] (Mirror) \"" + m_GameName + "\"";

  string Description = "[" + GetClientFileName() + "] \"" + m_GameName + "\" - " + m_OwnerName + " - " + ToDecString(GetNumHumanPlayers()) + "/" + to_string(m_GameLoading || m_GameLoaded ? m_StartPlayers : m_Slots.size());

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

  return "[" + GetCategory() + ": " + GetGameName() + "] ";
}

string CGame::GetPlayers() const
{
  string Players;
  for (const auto& player : m_Players) {
    const uint8_t SID = GetSIDFromPID(player->GetPID());
    if (!player->GetLeftMessageSent() == false && m_Slots[SID].GetTeam() != m_Aura->m_MaxSlots) {
      // Check GetLeftMessage instead of GetDeleteMe for debugging purposes
      Players += player->GetName() + ", ";
    }
  }

  const size_t size = Players.size();

  if (size > 2)
    Players = Players.substr(0, size - 2);

  return Players;
}

string CGame::GetObservers() const
{
  string Observers;

  for (const auto& player : m_Players) {
    const uint8_t SID = GetSIDFromPID(player->GetPID());
    if (player->GetLeftMessageSent() == false && m_Slots[SID].GetTeam() == m_Aura->m_MaxSlots) {
      // Check GetLeftMessage instead of GetDeleteMe for debugging purposes
      Observers += player->GetName() + ", ";
    }
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

  if (!m_LobbyLoading && (Time - m_LastPingTime >= 5)) {
    // note: we must send pings to players who are downloading the map because Warcraft III disconnects from the lobby if it doesn't receive a ping every ~90 seconds
    // so if the player takes longer than 90 seconds to download the map they would be disconnected unless we keep sending pings

    SendAll(GetProtocol()->SEND_W3GS_PING_FROM_HOST());

    // we also broadcast the game to the local network every 5 seconds so we hijack this timer for our nefarious purposes
    // however we only want to broadcast if the countdown hasn't started

    if (!m_CountDownStarted && m_UDPEnabled)
    {
      if (m_Aura->m_Net->m_UDPMainServerEnabled && m_Aura->m_Net->m_Config->m_UDPBroadcastStrictMode) {
        SendGameDiscoveryRefresh();
      } else {
        SendGameDiscoveryInfo();
      }
    }

    m_LastPingTime = Time;
  }

  // update players

  for (auto i = begin(m_Players); i != end(m_Players);) {
    if ((*i)->Update(fd)) {
      EventPlayerDeleted(*i, fd, send_fd);
      delete *i;
      i = m_Players.erase(i);
    } else {
      ++i;
    }
  }

  if (m_LobbyLoading) {
    if (m_Players.empty()) {
      // This is a remake.
      // All players left the original game, and they can rejoin now.
      m_LobbyLoading = false;
      CreateVirtualHost();
    } else {
      // Just remove reference from m_Aura->m_Games
      // Do not actually destroy the game nor set m_Exiting
      return true;
    }
  }

  // keep track of the largest sync counter (the number of keepalive packets received by each player)
  // if anyone falls behind by more than m_SyncLimit keepalives we start the lag screen

  if (m_GameLoaded) {
    // check if anyone has started lagging
    // we consider a player to have started lagging if they're more than m_SyncLimit keepalives behind

    if (!m_Lagging) {
      string LaggingString;
      bool startedLagging = false;
      vector<uint32_t> framesBehind = GetPlayersFramesBehind();
      uint8_t i = static_cast<uint8_t>(m_Players.size());
      while (i--) {
        if (framesBehind[i] > m_SyncLimit) {
          startedLagging = true;
          break;
        }
      }
      if (startedLagging) {
        uint8_t worstLaggerIndex = 0;
        uint32_t worstLaggerFrames = 0;
        vector<CGamePlayer*> laggingPlayers;
        i = static_cast<uint8_t>(m_Players.size());
        while (i--) {
          if (framesBehind[i] > m_SyncLimitSafe) {
            m_Players[i]->SetLagging(true);
            m_Players[i]->SetStartedLaggingTicks(Ticks);
            laggingPlayers.push_back(m_Players[i]);
            if (framesBehind[i] > worstLaggerFrames) {
              worstLaggerIndex = i;
              worstLaggerFrames = framesBehind[i];
            }
          }
        }

        // start the lag screen
        float worstLaggerSeconds = static_cast<float>(worstLaggerFrames) * static_cast<float>(m_Latency) / 1000.;
        Print(GetLogPrefix() + "started lagging on [" + PlayersToNameListString(laggingPlayers) + "].");
        Print(GetLogPrefix() + "worst lagger is [" + m_Players[worstLaggerIndex]->GetName() + "] (" + to_string(worstLaggerSeconds) + " seconds behind)");
        SendAll(GetProtocol()->SEND_W3GS_START_LAG(laggingPlayers));

        // reset everyone's drop vote
        for (auto& player : m_Players) {
          player->SetDropVote(false);
        }

        m_Lagging            = true;
        m_StartedLaggingTime = Time;
        m_LastLagScreenResetTime = Time;
      }
    } else {
      const bool anyUsingLegacyGProxy = GetAnyUsingGProxyLegacy();

      int64_t WaitTime = 60;

      if (anyUsingLegacyGProxy)
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

          if (anyUsingLegacyGProxy && !(_i)->GetGProxyAny()) {
            // we must send additional empty actions to non-GProxy++ players
            // GProxy++ will insert these itself so we don't need to send them to GProxy++ players
            // empty actions are used to extend the time a player can use when reconnecting

            for (uint8_t j = 0; j < m_GProxyEmptyActions; ++j)
              Send(_i, GetProtocol()->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
          }

          Send(_i, GetProtocol()->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
        }

        // start the lag screen
        SendAll(GetProtocol()->SEND_W3GS_START_LAG(GetLaggingPlayers()));

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

        if (m_SyncCounter > player->GetSyncCounter() && m_SyncCounter - player->GetSyncCounter() >= m_SyncLimitSafe) {
          ++PlayersStillLagging;
        } else {
          Print(GetLogPrefix() + "stopped lagging on [" + player->GetName() + "] (" + player->GetDelayText() + ")");
          SendAll(GetProtocol()->SEND_W3GS_STOP_LAG(player));
          player->SetLagging(false);
          player->SetStartedLaggingTicks(0);
        }
      }

      if (PlayersStillLagging == 0) {
        m_Lagging = false;
      }
    }

    if (m_Lagging) {
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

  if (m_Players.empty() && (m_GameLoading || m_GameLoaded)) {
    Print(GetLogPrefix() + "is over (no players left)");
    Print(GetLogPrefix() + "saving game data to database");

    m_Exiting = true;
    return m_Exiting;
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
      m_FinishedLoadingTicks = Ticks;
      m_GameLoading = false;
      m_GameLoaded = true;
      EventGameLoaded();
    }
  }

  if ((!m_GameLoading && !m_GameLoaded) && (m_SlotInfoChanged & (SLOTS_ALIGNMENT_CHANGED))) {
    SendAllSlotInfo(); // Updates m_ControllersWithMap
    m_SlotInfoChanged &= ~(SLOTS_ALIGNMENT_CHANGED);
  }

  // start countdown timer if autostart is due
  if (!m_CountDownStarted && (m_AutoStartMaxTime != 0 || m_AutoStartPlayers != 0) && m_ControllersWithMap >= 2) {
    bool IsTriggerTime = 0 < m_AutoStartMinTime && m_AutoStartMinTime <= Time;
    bool IsTriggerPlayers = 0 < m_AutoStartPlayers && m_AutoStartPlayers <= m_ControllersWithMap;
    bool ShouldStart = false;
    if (IsTriggerTime || IsTriggerPlayers) {
      ShouldStart = (IsTriggerTime && IsTriggerPlayers) || (0 < m_AutoStartMaxTime && m_AutoStartMaxTime <= Time);
      if (!ShouldStart) {
        ShouldStart = 0 == (IsTriggerTime ? m_AutoStartPlayers : m_AutoStartMinTime);
      }
    }

    if (ShouldStart) {
      StartCountDown(true);
    }
  }

  // start the gameover timer if there's only a configured number of players left
  // do not count observers, but fake players are counted regardless
  uint8_t RemainingPlayers = GetNumHumanPlayers() + static_cast<uint8_t>(m_FakePlayers.size());
  if (RemainingPlayers != m_StartPlayers && m_GameOverTime == 0 && (m_GameLoading || m_GameLoaded)) {
    if (RemainingPlayers == 0 || RemainingPlayers <= m_NumPlayersToStartGameOver || (RemainingPlayers == 1 && GetNumComputers() == 0)) {
      m_GameOverTime = Time;
      Print(GetLogPrefix() + "gameover timer started (" + std::to_string(RemainingPlayers) + " player(s) left)");
      if (GetNumHumanPlayers() > 0) {
        SendAllChat("Gameover timer started (disconnecting in 60 seconds...)");
      }
    }
  }

  // finish the gameover timer
  if (m_GameOverTime != 0 && Time - m_GameOverTime >= 60) {
    // Disconnect the player socket, destroy it, but do not send W3GS_PLAYERLEAVE
    // Sending it would force them to actually quit the game, and go to the scorescreen.
    if (StopPlayers("was disconnected (gameover timer finished)", true)) {
      Print(GetLogPrefix() + "is over (gameover timer finished)");
      SendEveryoneElseLeft();
    }
  }

  // expire the votekick
  if (!m_KickVotePlayer.empty() && Time - m_StartedKickVoteTime >= 60) {
    Print(GetLogPrefix() + "votekick against player [" + m_KickVotePlayer + "] expired");
    SendAllChat("A votekick against player [" + m_KickVotePlayer + "] has expired");
    m_KickVotePlayer.clear();
    m_StartedKickVoteTime = 0;
  }

  if (m_GameLoaded) {
    return m_Exiting;
  }

  // refresh every 3 seconds

  if (!m_RealmRefreshError && !m_CountDownStarted && m_GameDisplay == GAME_PUBLIC && HasSlotsOpen() && m_LastRefreshTime + 3 <= Time) {
    // send a game refresh packet to each battle.net connection

    for (auto& realm : m_Aura->m_Realms) {
      if (m_IsMirror && realm->GetIsMirror()) {
      // A mirror realm is a realm whose purpose is to mirror games actually hosted by Aura.
      // Do not display external games in those realms.
        continue;
      }
      if (realm->GetIsChatQueuedGameAnnouncement()) {
        // Wait til we have sent a chat message first.
        continue;
      }
      if (m_RealmsExcluded.find(realm->GetServer()) != m_RealmsExcluded.end()) {
        continue;
      }
      // Send STARTADVEX3
      AnnounceToRealm(realm);
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
      m_SlotInfoChanged &= ~(SLOTS_DOWNLOAD_PROGRESS_CHANGED);
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

        if (m_Aura->m_Net->m_Config->m_MaxDownloaders > 0 && Downloaders > m_Aura->m_Net->m_Config->m_MaxDownloaders)
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

        while (player->GetLastMapPartSent() < player->GetLastMapPartAcked() + 1442 * m_Aura->m_Net->m_Config->m_MaxParallelMapPackets && player->GetLastMapPartSent() < MapSize)
        {
          if (player->GetLastMapPartSent() == 0)
          {
            // overwrite the "started download ticks" since this is the first time we've sent any map data to the player
            // prior to this we've only determined if the player needs to download the map but it's possible we could have delayed sending any data due to download limits

            player->SetStartedDownloadingTicks(Ticks);
          }

          // limit the download speed if we're sending too much data
          // the download counter is the # of map bytes downloaded in the last second (it's reset once per second)

          if (m_Aura->m_Net->m_Config->m_MaxUploadSpeed > 0 && m_DownloadCounter > m_Aura->m_Net->m_Config->m_MaxUploadSpeed * 1024)
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
      if (GetNumHumanOrFakeControllers() >= 1) {
        EventGameStarted();
      } else {
        // Some operations may remove fake players during countdown.
        // Ensure that the game doesn't start if there are neither real nor fake players.
        // (If a player leaves or joins, the countdown is stopped elsewhere.)
        Print(GetLogPrefix() + "countdown stopped - lobby is empty.");
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
      if (TimeSinceSeenOwner > static_cast<int64_t>(m_LobbyNoOwnerTime)) {
        if (!m_OwnerName.empty()) {
          ReleaseOwner();
        }
      }
      if (TimeSinceSeenOwner > static_cast<int64_t>(m_LobbyTimeLimit)) {
        Print(GetLogPrefix() + "is over (lobby time limit hit)");
        m_Exiting = true;
        return m_Exiting;
      }
    }
  }

  // last action of CGame::Update
  // try to create the virtual host player, if there are slots available
  //
  // ensures that all pending players' leave messages have already been sent
  // either at CGame::EventPlayerDeleted or at CGame::EventRequestJoin (reserve system kicks)
  if (!m_GameLoading && !m_GameLoaded && GetSlotsOpen() > 0) {
    CreateVirtualHost();
  }

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

void CGame::Send(CGameConnection* player, const std::vector<uint8_t>& data) const
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
  for (auto& player : m_Players) {
    player->Send(data);
  }
}

void CGame::SendChat(uint8_t fromPID, CGamePlayer* player, const string& message, const uint8_t logLevel) const
{
  // send a private message to one player - it'll be marked [Private] in Warcraft 3
  if (message.empty())
    return;

  if (m_Aura->MatchLogLevel(logLevel)) {
    Print(GetLogPrefix() + "sent <<" + message + ">>");
  }

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
        ExtraFlags[0] = 3 + m_Slots[SID].GetColor();

      if (message.size() > 127)
        Send(player, GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, CreateByteArray(player->GetPID()), 32, CreateByteArray(ExtraFlags, 4), message.substr(0, 127)));
      else
        Send(player, GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, CreateByteArray(player->GetPID()), 32, CreateByteArray(ExtraFlags, 4), message));
    }
  }
}

void CGame::SendChat(uint8_t fromPID, uint8_t toPID, const string& message, const uint8_t logLevel) const
{
  SendChat(fromPID, GetPlayerFromPID(toPID), message, logLevel);
}

void CGame::SendChat(CGamePlayer* player, const string& message, const uint8_t logLevel) const
{
  SendChat(GetHostPID(), player, message, logLevel);
}

void CGame::SendChat(uint8_t toPID, const string& message, const uint8_t logLevel) const
{
  SendChat(GetHostPID(), toPID, message, logLevel);
}

void CGame::SendAllChat(uint8_t fromPID, const string& message) const
{
  if (message.empty())
    return;

  vector<uint8_t> toPIDs = GetPIDs();
  if (toPIDs.empty()) {
    return;
  }

  if (m_Aura->MatchLogLevel(LOG_LEVEL_INFO)) {
    Print(GetLogPrefix() + "sent <<" + message + ">>");
  }

  // send a public message to all players - it'll be marked [All] in Warcraft 3

  uint8_t maxSize = !m_GameLoading && !m_GameLoaded ? 254 : 127;
  if (message.size() < maxSize) {
    if (!m_GameLoading && !m_GameLoaded) {
        SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, toPIDs, 16, std::vector<uint8_t>(), message));
    } else {
        SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, toPIDs, 32, CreateByteArray(static_cast<uint32_t>(0), false), message));
    }
    return;
  }

  string leftMessage = message;
  while (leftMessage.size() > maxSize) {
    if (!m_GameLoading && !m_GameLoaded) {
      SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, toPIDs, 16, std::vector<uint8_t>(), leftMessage.substr(0, maxSize)));
    } else {
      SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, toPIDs, 32, CreateByteArray(static_cast<uint32_t>(0), false), leftMessage.substr(0, maxSize)));
    }
    leftMessage = leftMessage.substr(0, maxSize);
  }

  if (!leftMessage.empty()) {
    if (!m_GameLoading && !m_GameLoaded) {
      SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, toPIDs, 16, std::vector<uint8_t>(), leftMessage));
    } else {
      SendAll(GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(fromPID, toPIDs, 32, CreateByteArray(static_cast<uint32_t>(0), false), leftMessage));
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

  SendAll(GetProtocol()->SEND_W3GS_SLOTINFO(m_Slots, m_RandomSeed, GetLayout(), m_Map->GetMapNumControllers()));

  m_ControllersWithMap = 0;
  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      CGamePlayer* Player = GetPlayerFromSID(i);
      if (!Player || (Player->GetHasMap() && !Player->GetIsObserver())) {
        ++m_ControllersWithMap;
      }
    }
    m_SlotInfoChanged = 0;
  }
}

uint8_t CGame::GetNumEnabledTeamSlots(const uint8_t team) const
{
  // Only for Custom Forces
  uint8_t counter = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (slot.GetTeam() == team) {
      ++counter;
    }
  }
  return counter;
}

vector<uint8_t> CGame::GetNumFixedComputersByTeam() const
{
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> fixedComputers(numTeams, 0);
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() == m_Aura->m_MaxSlots) continue;
    if (!slot.GetIsSelectable()) {
      ++fixedComputers[slot.GetTeam()];
    }
  }
  return fixedComputers;
}

vector<uint8_t> CGame::GetPotentialTeamSizes() const
{
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes(numTeams, 0);
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() == m_Aura->m_MaxSlots) continue;
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    ++teamSizes[slot.GetTeam()];
  }
  return teamSizes;
}


pair<uint8_t, uint8_t> CGame::GetLargestPotentialTeam() const
{
  // Only for Custom Forces
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes = GetPotentialTeamSizes();
  pair<uint8_t, uint8_t> largestTeam = make_pair(m_Aura->m_MaxSlots, 0u);
  for (uint8_t team = 0; team < numTeams; ++team) {
    if (teamSizes[team] > largestTeam.second) {
      largestTeam = make_pair(team, teamSizes[team]);
    }
  }
  return largestTeam;
}

pair<uint8_t, uint8_t> CGame::GetSmallestPotentialTeam(const uint8_t minSize, const uint8_t exceptTeam) const
{
  // Only for Custom Forces
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes = GetPotentialTeamSizes();
  pair<uint8_t, uint8_t> smallestTeam = make_pair(m_Aura->m_MaxSlots, m_Aura->m_MaxSlots);
  for (uint8_t team = 0; team < numTeams; ++team) {
    if (team == exceptTeam || teamSizes[team] < minSize) continue;
    if (teamSizes[team] < smallestTeam.second) {
      smallestTeam = make_pair(team, teamSizes[team]);
    }
  }
  return smallestTeam;
}

vector<uint8_t> CGame::GetActiveTeamSizes() const
{
  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes(numTeams, 0);
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() == m_Aura->m_MaxSlots) continue;
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      ++teamSizes[slot.GetTeam()];
    }
  }
  return teamSizes;
}

uint8_t CGame::GetSelectableTeamSlotFront(const uint8_t team, const uint8_t endOccupiedSID, const uint8_t endOpenSID, const bool force) const
{
  uint8_t forceResult = 0xFF;
  uint8_t endSID = endOccupiedSID < endOpenSID ? endOpenSID : endOccupiedSID;
  for (uint8_t i = 0; i < endSID; ++i) {
    const CGameSlot& slot = m_Slots[i];
    if (slot.GetTeam() != team) continue;
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (!slot.GetIsSelectable()) continue;
    if (slot.GetSlotStatus() != SLOTSTATUS_OPEN && i < endOccupiedSID) {
      // When force is used, fallback to the highest occupied SID
      forceResult = i;
      continue;
    }
    return i;
  }
  if (force) return forceResult;
  return 0xFF; // Player team change request
}

uint8_t CGame::GetSelectableTeamSlotBack(const uint8_t team, const uint8_t endOccupiedSID, const uint8_t endOpenSID, const bool force) const
{
  uint8_t forceResult = 0xFF;
  uint8_t SID = endOccupiedSID < endOpenSID ? endOpenSID : endOccupiedSID;
  while (SID--) {
    const CGameSlot* slot = InspectSlot(SID);
    if (!slot || slot->GetTeam() != team) continue;
    if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (!slot->GetIsSelectable()) continue;
    if (slot->GetSlotStatus() != SLOTSTATUS_OPEN && SID < endOccupiedSID) {
      // When force is used, fallback to the highest occupied SID
      if (forceResult == 0xFF) forceResult = SID;
      continue;
    }
    return SID;
  }
  if (force) return forceResult;
  return 0xFF; // Player team change request
}

uint8_t CGame::GetSelectableTeamSlotBackExceptHumanLike(const uint8_t team, const uint8_t endOccupiedSID, const uint8_t endOpenSID, const bool force) const
{
  uint8_t forceResult = 0xFF;
  uint8_t SID = endOccupiedSID < endOpenSID ? endOpenSID : endOccupiedSID;
  while (SID--) {
    const CGameSlot* slot = InspectSlot(SID);
    if (!slot || slot->GetTeam() != team) continue;
    if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (!slot->GetIsSelectable()) continue;
    if (slot->GetIsPlayerOrFake()) continue;
    if (slot->GetSlotStatus() != SLOTSTATUS_OPEN && SID < endOccupiedSID) {
      // When force is used, fallback to the highest occupied SID
      if (forceResult == 0xFF) forceResult = SID;
      continue;
    }
    return SID;
  }
  if (force) return forceResult;
  return 0xFF; // Player team change request
}

uint8_t CGame::GetSelectableTeamSlotBackExceptComputer(const uint8_t team, const uint8_t endOccupiedSID, const uint8_t endOpenSID, const bool force) const
{
  uint8_t forceResult = 0xFF;
  uint8_t SID = endOccupiedSID < endOpenSID ? endOpenSID : endOccupiedSID;
  while (SID--) {
    const CGameSlot* slot = InspectSlot(SID);
    if (!slot || slot->GetTeam() != team) continue;
    if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) continue;
    if (!slot->GetIsSelectable()) continue;
    if (slot->GetIsComputer()) continue;
    if (slot->GetSlotStatus() != SLOTSTATUS_OPEN && SID < endOccupiedSID) {
      // When force is used, fallback to the highest occupied SID
      if (forceResult == 0xFF) forceResult = SID;
      continue;
    }
    return SID;
  }
  if (force) return forceResult;
  return 0xFF; // Player team change request
}

bool CGame::FindHumanVsAITeams(const uint8_t humanCount, const uint8_t computerCount, pair<uint8_t, uint8_t>& teams) const
{
  if (!GetIsCustomForces()) {
    teams.first = 0;
    teams.second = 1;
    return true;
  } else if (!(m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)) {
    // MAPOPT_CUSTOMFORCES
    pair<uint8_t, uint8_t> largestTeam = GetLargestPotentialTeam();
    pair<uint8_t, uint8_t> smallestTeam = GetSmallestPotentialTeam(humanCount < computerCount ? humanCount : computerCount, largestTeam.first);
    if (largestTeam.second == 0 || smallestTeam.second == m_Aura->m_MaxSlots) {
      return false;
    }
    pair<uint8_t, uint8_t>& computerTeam = (computerCount > humanCount ? largestTeam : smallestTeam);
    pair<uint8_t, uint8_t>& humanTeam = (computerCount > humanCount ? smallestTeam : largestTeam);
    if (humanTeam.second < humanCount || computerTeam.second < computerCount) {
      return false;
    }
    teams.first = humanTeam.first;
    teams.second = computerTeam.first;
    return true;
  }

  // Fixed Player Settings

  vector<uint8_t> lockedTeams = GetNumFixedComputersByTeam();
  uint8_t fixedTeamsCounter = 0;
  uint8_t forcedComputerTeam = 0xFF;
  for (uint8_t team = 0; team < lockedTeams.size(); ++team) {
    if (lockedTeams[team] == 0) continue;
    if (++fixedTeamsCounter >= 2) {
      // Bail-out if there are fixed computers in different teams.
      // This is the case in DotA/AoS maps.
      return false;
    }
    forcedComputerTeam = team;
  }
  if (forcedComputerTeam != 0xFF) {
    if (GetNumEnabledTeamSlots(forcedComputerTeam) < computerCount) {
      return false;
    }
  }

  {
    const uint8_t numTeams = m_Map->GetMapNumTeams();
    vector<uint8_t> teamSizes = GetPotentialTeamSizes();
    pair<uint8_t, uint8_t> largestTeam = make_pair(m_Aura->m_MaxSlots, 0u);
    pair<uint8_t, uint8_t> smallestTeam = make_pair(m_Aura->m_MaxSlots, m_Aura->m_MaxSlots);
    for (uint8_t team = 0; team < numTeams; ++team) {
      if (team == forcedComputerTeam) continue;
      if (teamSizes[team] > largestTeam.second) {
        largestTeam = make_pair(team, teamSizes[team]);
      }
      if (teamSizes[team] < smallestTeam.second) {
        smallestTeam = make_pair(team, teamSizes[team]);
      }
    }
    if (forcedComputerTeam != 0xFF) {
      if (largestTeam.second < humanCount) {
        return false;
      }
      teams.first = largestTeam.first;
      teams.second = forcedComputerTeam;
    } else {
      // Just like MAPOPT_CUSTOMFORCES
      pair<uint8_t, uint8_t>& computerTeam = (computerCount > humanCount ? largestTeam : smallestTeam);
      pair<uint8_t, uint8_t>& humanTeam = (computerCount > humanCount ? smallestTeam : largestTeam);
      if (humanTeam.second < humanCount || computerTeam.second < computerCount) {
        return false;
      }
      teams.first = humanTeam.first;
      teams.second = humanTeam.second;
    }
    return true;
  }
}

void CGame::ResetLayout(const bool quiet)
{
  if (m_CustomLayout == CUSTOM_LAYOUT_NONE) {
    return;
  }
  m_CustomLayout = CUSTOM_LAYOUT_NONE;
  if (!quiet) {
    SendAllChat("Team restrictions automatically removed.");
  }
}

void CGame::ResetLayoutIfNotMatching()
{
  switch (m_CustomLayout) {
    case CUSTOM_LAYOUT_NONE:
      break;
    case CUSTOM_LAYOUT_ONE_VS_ALL:
    case CUSTOM_LAYOUT_HUMANS_VS_AI: {
      if (
        (GetNumTeamControllersOrOpen(m_CustomLayoutData.first) == 0) ||
        (GetNumTeamControllersOrOpen(m_CustomLayoutData.second) == 0)
      ) {
        ResetLayout(false);
        break;
      }
      bool isNotMatching = false;
      if (m_CustomLayout == CUSTOM_LAYOUT_HUMANS_VS_AI) {
        for (const auto& slot : m_Slots) {
          if (slot.GetSlotStatus() != SLOTSTATUS_CLOSED) continue;
          if (slot.GetIsComputer()) {
            if (slot.GetTeam() != m_CustomLayoutData.second) {
              isNotMatching = true;
              break;
            }
          } else {
            // Open, human, or fake player
            if (slot.GetTeam() != m_CustomLayoutData.first) {
              isNotMatching = true;
              break;
            }
          }
        }
      } else {
      }
      if (isNotMatching) {
        ResetLayout(false);
      }
      break;
    }
    case CUSTOM_LAYOUT_FFA: {
      if (GetHasAnyActiveTeam()) {
        ResetLayout(false);
      }
      break;
    }
    case CUSTOM_LAYOUT_DRAFT:
    case CUSTOM_LAYOUT_COMPACT:
    case CUSTOM_LAYOUT_ISOPLAYERS:
    default:
      break;
  }
}

bool CGame::SetLayoutCompact()
{
  m_CustomLayout = CUSTOM_LAYOUT_COMPACT;

  if (GetIsCustomForces()) {
    // Unsupported, and not very useful anyway.
    // Typical maps with fixed player settings are NvN, and compacting will just not be useful.
    return false;
  }

  const uint8_t numTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes = GetActiveTeamSizes();
  pair<uint8_t, uint8_t> largestTeam = make_pair(m_Aura->m_MaxSlots, 0u);
  for (uint8_t team = 0; team < numTeams; ++team) {
    if (largestTeam.second < teamSizes[team]) {
      largestTeam = make_pair(team, teamSizes[team]);
    }
  }
  if (largestTeam.second <= 1) {
    return false;
  }

  const uint8_t controllerCount = GetNumControllers();
  if (controllerCount < 2) {
    return false;
  }
  const uint8_t extraPlayers = controllerCount % largestTeam.second;
  const uint8_t expectedFullTeams = controllerCount / largestTeam.second;
  if (expectedFullTeams < 2) {
    // Compacting is used for NvNvN...
    return false;
  }
  //const uint8_t expectedMaxTeam = expectedFullTeams - (extraPlayers == 0);
  vector<uint8_t> premadeMappings(numTeams, m_Aura->m_MaxSlots);
  bitset<MAX_SLOTS_MODERN> fullTeams;
  for (uint8_t team = 0; team < numTeams; ++team) {
    if (teamSizes[team] == largestTeam.second) {
      if (!fullTeams.test(team)) {
        premadeMappings[team] = static_cast<uint8_t>(fullTeams.count());
        fullTeams.set(team);
      }
    }
  }

  const uint8_t autoTeamOffset = static_cast<uint8_t>(fullTeams.count());

  for (auto& slot : m_Slots) {
    uint8_t team = slot.GetTeam();
    if (fullTeams.test(team)) {
      slot.SetTeam(premadeMappings[team]);
    } else {
      slot.SetTeam(autoTeamOffset);
    }
  }

  uint8_t i = numTeams;
  while (i--) {
    if (i < autoTeamOffset) {
      teamSizes[i] = largestTeam.second;
    } else if (i == autoTeamOffset) {
      teamSizes[i] = controllerCount - (largestTeam.second * autoTeamOffset);
    } else {
      teamSizes[i] = 0;
    }
  }

  uint8_t fillingTeamNum = autoTeamOffset;
  for (auto& slot : m_Slots) {
    uint8_t team = slot.GetTeam();
    if (team < autoTeamOffset) continue;
    if (teamSizes[team] > largestTeam.second) {
      if (teamSizes[fillingTeamNum] >= largestTeam.second) {
        ++fillingTeamNum;
      }
      slot.SetTeam(fillingTeamNum);
      --teamSizes[team];
      ++teamSizes[fillingTeamNum];
    }
  }

  return true;
}

bool CGame::SetLayoutTwoTeams()
{
  m_CustomLayout = CUSTOM_LAYOUT_ISOPLAYERS;

  // TODO(IceSandslash): SetLayoutTwoTeams
  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
    if (m_Map->GetMapNumTeams() != 2) {
      return false;
    }
  }
  //m_CustomLayout = CUSTOM_LAYOUT_ISOPLAYERS;
  return false;
}

bool CGame::SetLayoutHumansVsAI(const uint8_t humanTeam, const uint8_t computerTeam)
{
  m_CustomLayout = CUSTOM_LAYOUT_HUMANS_VS_AI;
  const bool isSwap = GetIsCustomForces();
  if (isSwap) {
    uint8_t SID = static_cast<uint8_t>(m_Slots.size()) - 1;
    uint8_t endHumanSID = SID;
    uint8_t endComputerSID = SID;
    while (SID != 0xFF) {
      CGameSlot* slot = GetSlot(SID);
      if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        --SID;
        continue;
      }
      const bool isComputer = slot->GetIsComputer();
      const uint8_t currentTeam = slot->GetTeam();
      const uint8_t targetTeam = isComputer ? computerTeam : humanTeam;
      if (currentTeam == targetTeam) {
        --SID;
        continue;
      }
      uint8_t& selfEndSID = isComputer ? endComputerSID : endHumanSID;
      uint8_t& otherEndSID = isComputer ? endHumanSID : endComputerSID;
      uint8_t swapSID = 0xFF;
      if (isComputer) {
        swapSID = GetSelectableTeamSlotBackExceptComputer(targetTeam, SID, selfEndSID, true);
      } else {
        swapSID = GetSelectableTeamSlotBackExceptHumanLike(targetTeam, SID, selfEndSID, true);
      }
      if (swapSID == 0xFF) {
        return false;
      }
      const bool isTwoWays = InspectSlot(swapSID)->GetSlotStatus() == SLOTSTATUS_OCCUPIED;
      if (!SwapSlots(SID, swapSID)) {
        Print(ByteArrayToDecString(InspectSlot(SID)->GetByteArray()));
        Print(ByteArrayToDecString(InspectSlot(swapSID)->GetByteArray()));
      } else {
        // slot still points to the same SID
        selfEndSID = swapSID;
        if (isTwoWays && SID > otherEndSID) {
          otherEndSID = SID;
        }
      }
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (!isTwoWays) --SID;
    }
    CloseAllTeamSlots(computerTeam);
  } else {
    for (auto& slot : m_Slots) {
      if (slot.GetSlotStatus() != SLOTSTATUS_OCCUPIED) continue;
      uint8_t targetTeam = slot.GetIsComputer() ? computerTeam : humanTeam;
      if (slot.GetTeam() != targetTeam) {
        slot.SetTeam(targetTeam);
        m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      }
    }
  }
  m_CustomLayoutData = make_pair(humanTeam, computerTeam);
  return true;
}

bool CGame::SetLayoutFFA()
{
  m_CustomLayout = CUSTOM_LAYOUT_FFA;

  uint8_t nextTeam = GetNumControllers(); // Only arrange non-observers
  const bool isSwap = GetIsCustomForces();
  if (isSwap && nextTeam > m_Map->GetMapNumTeams()) {
    return false;
  }

  vector<uint8_t> lockedTeams = GetNumFixedComputersByTeam();
  for (const auto& count : lockedTeams) {
    if (count > 1) {
      return false;
    }
  }

  if (!FindNextMissingElementBack(nextTeam, lockedTeams)) {
    return true; // every team got 1 fixed computer slot
  }
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  bitset<MAX_SLOTS_MODERN> occupiedTeams;
  while (SID--) {
    CGameSlot* slot = GetSlot(SID);
    if (slot->GetTeam() == m_Aura->m_MaxSlots) continue;
    if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED) continue;
    if (slot->GetTeam() == nextTeam) {
      // Slot already has the right team. Skip both team and slot.
      occupiedTeams.set(nextTeam);
      if (!FindNextMissingElementBack(nextTeam, lockedTeams)) {
        break;
      }
      continue;
    }
    if (isSwap) {
      uint8_t swapSID = GetSelectableTeamSlotBack(nextTeam, SID, static_cast<uint8_t>(m_Slots.size()), true);
      if (swapSID == 0xFF) {
        return false;
      }
      if (!SwapSlots(SID, swapSID)) {
        return false;
      }
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (!FindNextMissingElementBack(nextTeam, lockedTeams)) {
        break;
      }
      occupiedTeams.set(nextTeam);
    } else {
      slot->SetTeam(nextTeam);
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      if (!FindNextMissingElementBack(nextTeam, lockedTeams)) {
        break;
      }
      occupiedTeams.set(nextTeam);
    }
  }
  if (isSwap) {
    CloseAllTeamSlots(occupiedTeams);
  }
  return true;
}

uint8_t CGame::GetOneVsAllTeamAll() const
{
  if (!GetIsCustomForces()) {
    return 1;
  }

  const uint8_t mapNumTeams = m_Map->GetMapNumTeams();
  const uint8_t expectedTeamSize = GetNumPotentialControllers() - 1;
  vector<uint8_t> lockedTeams = GetNumFixedComputersByTeam();

  // Make sure GetOneVsAllTeamAll() yields the team with the fixed computer slots.
  // Fixed computer slots in different forces are not allowed in OneVsAll mode.
  uint8_t resultTeam = 0xFF;
  uint8_t fixedTeamsCounter = 0;
  for (uint8_t team = 0; team < lockedTeams.size(); ++team) {
    if (lockedTeams[team] == 0) continue;
    if (++fixedTeamsCounter >= 2) {
      return 0xFF;
    }
    resultTeam = team;
  }

  vector<uint8_t> teamSizes = GetPotentialTeamSizes();
  if (resultTeam == 0xFF) {
    pair<uint8_t, uint8_t> largestTeam = make_pair(m_Aura->m_MaxSlots, 0u);
    for (uint8_t team = 0; team < mapNumTeams; ++team) {
      if (teamSizes[team] > largestTeam.second) {
        largestTeam = make_pair(team, teamSizes[team]);
      }
    }
    resultTeam = largestTeam.first;
  }
  if (expectedTeamSize > teamSizes[resultTeam]) {
    return 0xFF;
  } else {
    return resultTeam;
  }
}

uint8_t CGame::GetOneVsAllTeamOne(const uint8_t teamAll) const
{
  if (!GetIsCustomForces()) {
    return 0;
  }

  const uint8_t mapNumTeams = m_Map->GetMapNumTeams();
  vector<uint8_t> teamSizes = GetPotentialTeamSizes();
  pair<uint8_t, uint8_t> smallestTeam = make_pair(m_Aura->m_MaxSlots, m_Aura->m_MaxSlots);
  for (uint8_t team = 0; team < mapNumTeams; ++team) {
    if (team == teamAll) continue;
    if (teamSizes[team] < smallestTeam.second) {
      smallestTeam = make_pair(team, teamSizes[team]);
    }
  }
  // We can be sure that smallestTeam does not contain fixed computer slots, because:
  // - TeamAll contains the only allowed computer slot.
  // - CMap validator ensures that there are at least two well-defined teams in the game.
  return smallestTeam.first;
}

bool CGame::SetLayoutOneVsAll(const CGamePlayer* targetPlayer)
{
  m_CustomLayout = CUSTOM_LAYOUT_COMPACT;

  const bool isSwap = GetMap()->GetMapOptions() & MAPOPT_CUSTOMFORCES;
  uint8_t targetSID = GetSIDFromPID(targetPlayer->GetPID());
  //uint8_t targetTeam = m_Slots[targetSID].GetTeam();

  const uint8_t teamAll = GetOneVsAllTeamAll();
  if (teamAll == 0xFF) return false;
  const uint8_t teamOne = GetOneVsAllTeamOne(teamAll);

  // Move the alone player to its own team.
  if (isSwap) {
    const uint8_t swapSID = GetSelectableTeamSlotBack(teamOne, static_cast<uint8_t>(m_Slots.size()), static_cast<uint8_t>(m_Slots.size()), true);
    if (swapSID == 0xFF) {
      return false;
    }
    SwapSlots(targetSID, swapSID);
    targetSID = swapSID; // Sync slot index on swap.
  } else {
    CGameSlot* slot = GetSlot(targetSID);
    slot->SetTeam(teamOne);
    m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  }

  // Move the rest of players.
  if (isSwap) {
    uint8_t endObserverSID = static_cast<uint8_t>(m_Slots.size());
    uint8_t endAllSID = endObserverSID;
    uint8_t SID = static_cast<uint8_t>(m_Slots.size()) - 1;
    while (SID != 0xFF) {
      if (SID == targetSID || m_Slots[SID].GetTeam() == teamAll || m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OCCUPIED) {
        --SID;
        continue;
      }

      uint8_t swapSID = GetSelectableTeamSlotBack(teamAll, SID, endAllSID, true);
      bool toObservers = swapSID == 0xFF; // Alliance team is full.
      if (toObservers) {
        if (m_Slots[SID].GetIsComputer()) {
          return false;
        }
        swapSID = GetSelectableTeamSlotBack(m_Aura->m_MaxSlots, SID, endObserverSID, true);
        if (swapSID == 0xFF) {
          return false;
        }
      }
      if (!SwapSlots(SID, swapSID)) {
        Print(ByteArrayToDecString(InspectSlot(SID)->GetByteArray()));
        Print(ByteArrayToDecString(InspectSlot(swapSID)->GetByteArray()));
        return false;
      } else if (toObservers) {
        endObserverSID = swapSID;
      } else {
        endAllSID = swapSID;
      }
      CloseAllTeamSlots(teamOne);
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
      --SID;
    }
  } else {
    uint8_t SID = static_cast<uint8_t>(m_Slots.size());
    while (SID--) {
      if (SID == targetSID) continue;
      m_Slots[SID].SetTeam(teamAll);
      m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
    }
  }
  m_CustomLayout = CUSTOM_LAYOUT_ONE_VS_ALL;
  m_CustomLayoutData = make_pair(teamOne, teamAll);
  return true;
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

uint32_t CGame::GetGameType() const
{
  uint32_t mapGameType = 0;
  if (m_GameDisplay == GAME_PRIVATE) mapGameType |= MAPGAMETYPE_PRIVATEGAME;
  if (m_RestoredGame) {
    mapGameType |= MAPGAMETYPE_SAVEDGAME;
  } else {
    mapGameType |= MAPGAMETYPE_UNKNOWN0;
    mapGameType |= m_Map->GetMapGameType();
  }
  return mapGameType;
}

uint32_t CGame::GetGameFlags() const
{
  uint32_t mapGameFlags = m_Map->GetMapGameFlags();
  return mapGameFlags;
}

string CGame::GetSourceFilePath() const {
  if (m_RestoredGame) {
    return m_RestoredGame->GetClientPath();
  } else {
    return m_Map->GetClientPath();
  }
}

vector<uint8_t> CGame::GetSourceFileHash() const
{
  if (m_RestoredGame) {
    return m_RestoredGame->GetSaveHash();
  } else {
    return m_Map->GetMapHash();
  }
}

vector<uint8_t> CGame::GetSourceFileSHA1() const
{
  return m_Map->GetMapSHA1();
}

vector<uint8_t> CGame::GetAnnounceWidth() const
{
  if (GetIsProxyReconnectable()) {
    // use an invalid map width/height to indicate reconnectable games
    return m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS();
  }
  if (m_RestoredGame) return {0, 0};
  return m_Map->GetMapWidth();
}
vector<uint8_t> CGame::GetAnnounceHeight() const
{
  if (GetIsProxyReconnectable()) {
    // use an invalid map width/height to indicate reconnectable games
    return m_Aura->m_GPSProtocol->SEND_GPSS_DIMENSIONS();
  }
  if (m_RestoredGame) return {0, 0};
  return m_Map->GetMapHeight();
}

void CGame::SendVirtualHostPlayerInfo(CGameConnection* player) const
{
  if (m_VirtualHostPID == 0xFF)
    return;

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  Send(player, GetProtocol()->SEND_W3GS_PLAYERINFO(m_VirtualHostPID, m_LobbyVirtualHostName, IP, IP));
}

void CGame::SendVirtualHostPlayerInfo(CGamePlayer* player) const
{
  if (m_VirtualHostPID == 0xFF)
    return;

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  Send(player, GetProtocol()->SEND_W3GS_PLAYERINFO(m_VirtualHostPID, m_LobbyVirtualHostName, IP, IP));
}

void CGame::SendFakePlayersInfo(CGameConnection* player) const
{
  if (m_FakePlayers.empty())
    return;

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  for (const uint16_t fakePlayer : m_FakePlayers) {
    // The higher 8 bytes are the original SID the player was created at.
    // This information is important for letting hosts know which !open, !close, commands to execute.
    Send(player, GetProtocol()->SEND_W3GS_PLAYERINFO(static_cast<uint8_t>(fakePlayer), "User[" + ToDecString(1 + (fakePlayer >> 8)) + "]", IP, IP));
  }
}

void CGame::SendFakePlayersInfo(CGamePlayer* player) const
{
  if (m_FakePlayers.empty())
    return;

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  for (const uint16_t fakePlayer : m_FakePlayers) {
    // The higher 8 bytes are the original SID the player was created at.
    // This information is important for letting hosts know which !open, !close, commands to execute.
    Send(player, GetProtocol()->SEND_W3GS_PLAYERINFO(static_cast<uint8_t>(fakePlayer), "User[" + ToDecString(1 + (fakePlayer >> 8)) + "]", IP, IP));
  }
}

void CGame::SendJoinedPlayersInfo(CGameConnection* connection) const
{
  for (auto& otherPlayer : m_Players) {
    if (otherPlayer->GetDeleteMe())
      continue;
    Send(connection,
      GetProtocol()->SEND_W3GS_PLAYERINFO(otherPlayer->GetPID(), otherPlayer->GetName(), otherPlayer->GetIPv4(), otherPlayer->GetIPv4Internal())
    );
  }
}

void CGame::SendJoinedPlayersInfo(CGamePlayer* player) const
{
  for (auto& otherPlayer : m_Players) {
    if (otherPlayer == player)
      continue;
    if (otherPlayer->GetDeleteMe())
      continue;
    Send(player,
      GetProtocol()->SEND_W3GS_PLAYERINFO(otherPlayer->GetPID(), otherPlayer->GetName(), otherPlayer->GetIPv4(), otherPlayer->GetIPv4Internal())
    );
  }
}

void CGame::SendIncomingPlayerInfo(CGamePlayer* player) const
{
  for (auto& otherPlayer : m_Players) {
    if (otherPlayer == player)
      continue;
    if (otherPlayer->GetDeleteMe())
      break;
    otherPlayer->Send(GetProtocol()->SEND_W3GS_PLAYERINFO(player->GetPID(), player->GetName(), player->GetIPv4(), player->GetIPv4Internal()));
  }
}

void CGame::SendWelcomeMessage(CGamePlayer *player) const
{
  for (size_t i = 0; i < m_Aura->m_Config->m_Greeting.size(); i++) {
    string::size_type matchIndex;
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
      if (GetMapSiteURL().empty()) {
        continue;
      }
      Line = Line.substr(6);
    }
    if (Line.substr(0, 6) == "{URL!}") {
      if (!GetMapSiteURL().empty()) {
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
      if (m_CreatorText.empty()) {
        continue;
      }
      Line = Line.substr(10);
    }
    if (Line.substr(0, 10) == "{CREATOR!}") {
      if (!m_CreatorText.empty()) {
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
    if (Line.substr(0, 17) == "{CHECKLASTOWNER?}") {
      if (m_LastOwner != player->GetName()) {
        continue;
      }
      Line = Line.substr(17);
    }
    if (Line.substr(0, 17) == "{CHECKLASTOWNER!}") {
      if (m_LastOwner == player->GetName()) {
        continue;
      }
      Line = Line.substr(17);
    }
    if (Line.substr(0, 6) == "{LAN?}") {
      if (!player->GetRealm(false)) {
        continue;
      }
      Line = Line.substr(6);
    }
    if (Line.substr(0, 6) == "{LAN!}") {
      if (player->GetRealm(false)) {
        continue;
      }
      Line = Line.substr(6);
    }
    while ((matchIndex = Line.find("{CREATOR}")) != string::npos) {
      Line.replace(matchIndex, 9, m_CreatorText);
    }
    while ((matchIndex = Line.find("{HOSTREALM}")) != string::npos) {
      if (m_CreatedFromType == GAMESETUP_ORIGIN_REALM) {
        Line.replace(matchIndex, 11, "@" + reinterpret_cast<CRealm*>(m_CreatedFrom)->GetCanonicalDisplayName());
      } else if (m_CreatedFromType == GAMESETUP_ORIGIN_IRC) {
        Line.replace(matchIndex, 11, "@" + reinterpret_cast<CIRC*>(m_CreatedFrom)->m_Config->m_HostName);
      } else if (m_CreatedFromType == GAMESETUP_ORIGIN_DISCORD) {
        // TODO: {HOSTREALM} Discord
      } else {
        Line.replace(matchIndex, 11, "@@@LAN/VPN");
      }
    }
    while ((matchIndex = Line.find("{OWNER}")) != string::npos) {
      Line.replace(matchIndex, 7, m_OwnerName);
    }
    while ((matchIndex = Line.find("{OWNERREALM}")) != string::npos) {
      Line.replace(matchIndex, 12, m_OwnerRealm.empty() ? "@@@LAN/VPN" : ("@" + m_OwnerRealm));
    }
    while ((matchIndex = Line.find("{TRIGGER_PRIVATE}")) != string::npos) {
      Line.replace(matchIndex, 17, m_PrivateCmdToken);
    }
    while ((matchIndex = Line.find("{TRIGGER_BROADCAST}")) != string::npos) {
      Line.replace(matchIndex, 19, m_BroadcastCmdToken);
    }
    while ((matchIndex = Line.find("{TRIGGER_PREFER_PRIVATE}")) != string::npos) {
      Line.replace(matchIndex, 24, m_PrivateCmdToken.empty() ? m_BroadcastCmdToken : m_PrivateCmdToken);
    }
    while ((matchIndex = Line.find("{TRIGGER_PREFER_BROADCAST}")) != string::npos) {
      Line.replace(matchIndex, 26, m_BroadcastCmdToken.empty() ? m_PrivateCmdToken : m_BroadcastCmdToken);
    }
    while ((matchIndex = Line.find("{URL}")) != string::npos) {
      Line.replace(matchIndex, 5, GetMapSiteURL());
    }
    while ((matchIndex = Line.find("{FILENAME}")) != string::npos) {
      Line.replace(matchIndex, 10, GetClientFileName());
    }
    while ((matchIndex = Line.find("{SHORTDESC}")) != string::npos) {
      Line.replace(matchIndex, 11, m_Map->GetMapShortDesc());
    }
    while ((matchIndex = Line.find("{AUTOSTART}")) != string::npos) {
      Line.replace(matchIndex, 11, GetAutoStartText());
    }
    SendChat(player, Line, LOG_LEVEL_TRACE);
  }
}

void CGame::SendCommandsHelp(const string& cmdToken, CGamePlayer* player, const bool isIntro) const
{
  if (isIntro) SendChat(player, "Welcome, " + player->GetName() + ".", LOG_LEVEL_TRACE);
  SendChat(player, "Use " + cmdToken + GetTokenName(cmdToken) + " for commands.", LOG_LEVEL_TRACE);
  if (!isIntro) return;
  SendChat(player, cmdToken + "ping - view your latency", LOG_LEVEL_TRACE);
  SendChat(player, cmdToken + "start - starts the game", LOG_LEVEL_TRACE);
  if (m_OwnerName.empty()) {
    SendChat(player, cmdToken + "owner - acquire permissions over this game", LOG_LEVEL_TRACE);
  }
  if (m_OwnerName.empty() || MatchOwnerName(player->GetName())) {
    SendChat(player, cmdToken + "open [NUMBER] - opens a slot", LOG_LEVEL_TRACE);
    SendChat(player, cmdToken + "close [NUMBER] - closes a slot", LOG_LEVEL_TRACE);
    SendChat(player, cmdToken + "fill [DIFFICULTY] - adds computers", LOG_LEVEL_TRACE);
    SendChat(player, cmdToken + "ffa - sets free for all game mode", LOG_LEVEL_TRACE);
    SendChat(player, cmdToken + "vsall - sets one vs all game mode", LOG_LEVEL_TRACE);
    SendChat(player, cmdToken + "terminator - sets humans vs computers", LOG_LEVEL_TRACE);
  }
  player->SetSentAutoCommandsHelp(true);
}

void CGame::SendAllActions()
{
  m_GameTicks += m_Latency;

  if (GetAnyUsingGProxy()) {
    // we must send empty actions to non-GProxy++ players
    // GProxy++ will insert these itself so we don't need to send them to GProxy++ players
    // empty actions are used to extend the time a player can use when reconnecting

    for (auto& player : m_Players) {
      if (!player->GetGProxyAny()) {
        for (uint8_t j = 0; j < m_GProxyEmptyActions; ++j)
          Send(player, GetProtocol()->SEND_W3GS_INCOMING_ACTION(queue<CIncomingAction*>(), 0));
      }
    }
  }

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
    size_t SubActionsLength = Action->GetLength();

    while (!m_Actions.empty())
    {
      Action = m_Actions.front();
      m_Actions.pop();

      // check if adding the next action to the sub actions queue would put us over the limit (1452 because the INCOMING_ACTION and INCOMING_ACTION2 packets use an extra 8 bytes)
      bool isOverflows = SubActionsLength > 1452 || Action->GetLength() > 1452 || SubActionsLength + Action->GetLength() > 1452;
      if (isOverflows) {
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

std::string CGame::GetPrefixedGameName(const CRealm* realm) const
{
  if (realm == nullptr) return m_GameName;
  return realm->GetPrefixedGameName(m_GameName);
}

std::string CGame::GetAnnounceText(const CRealm* realm) const
{
  uint32_t mapSize = ByteArrayToUInt32(m_Map->GetMapSize(), false);
  string versionPrefix;
  if (m_Aura->m_GameVersion <= 26 && mapSize > 0x800000) {
    versionPrefix = "[1." + to_string(m_Aura->m_GameVersion) + ".UnlockMapSize] ";
  } else {
    versionPrefix = "[1." + to_string(m_Aura->m_GameVersion) + "] ";

}
  string startedPhrase;
  if (m_IsMirror || m_RestoredGame || m_OwnerName.empty()) {
    startedPhrase = ". (\"" + GetPrefixedGameName(realm) + "\")";
  } else {
    startedPhrase = ". (Started by " + m_OwnerName + ": \"" + GetPrefixedGameName(realm) + "\")";
  }

  string typeWord;
  if (m_RestoredGame) {
    typeWord = "loaded";
  } else if (m_GameDisplay == GAME_PRIVATE) {
    typeWord = "private";
  } else {
    typeWord = "public";
  }

  if (m_IsMirror) {
    return versionPrefix + "Mirroring " + typeWord + " game of " + ParseFileName(m_Map->GetServerPath()) + startedPhrase;
  } else {
    return versionPrefix + "Hosting " + typeWord + " game of " + ParseFileName(m_Map->GetServerPath()) + startedPhrase;
  }
}

uint16_t CGame::GetHostPortForDiscoveryInfo(const uint8_t protocol) const
{
  if (protocol == AF_INET)
    return m_Aura->m_Net->m_Config->m_UDPEnableCustomPortTCP4 ? m_Aura->m_Net->m_Config->m_UDPCustomPortTCP4 : m_HostPort;

  if (protocol == AF_INET6)
    return m_Aura->m_Net->m_Config->m_UDPEnableCustomPortTCP6 ? m_Aura->m_Net->m_Config->m_UDPCustomPortTCP6 : m_HostPort;

  return m_HostPort;
}

uint8_t CGame::GetActiveReconnectProtocols() const
{
  uint8_t protocols = 0;
  for (const auto& player : m_Players) {
    if (!player->GetGProxyAny()) continue;
    if (player->GetGProxyExtended()) {
      protocols |= RECONNECT_ENABLED_GPROXY_EXTENDED;
      if (protocols != RECONNECT_ENABLED_GPROXY_EXTENDED) break;
    } else {
      protocols |= RECONNECT_ENABLED_GPROXY_BASIC;
      if (protocols != RECONNECT_ENABLED_GPROXY_BASIC) break;
    }
  }
  return protocols;
}

string CGame::GetActiveReconnectProtocolsDetails() const
{
  vector<string> protocols;
  for (const auto& player : m_Players) {
    if (!player->GetGProxyAny()) {
      protocols.push_back("[" + player->GetName() + ": OFF]");
    } else if (player->GetGProxyExtended()) {
      protocols.push_back("[" + player->GetName() + ": EXT]");
    } else {
      protocols.push_back("[" + player->GetName() + ": ON]");
    }
  }
  return JoinVector(protocols, false);
}

bool CGame::GetAnyUsingGProxy() const
{
  for (const auto& player : m_Players) {
    if (player->GetGProxyAny()) {
      return true;
    }
  }
  return false;
}

bool CGame::GetAnyUsingGProxyLegacy() const
{
  for (const auto& player : m_Players) {
    if (!player->GetGProxyAny()) continue;
    if (!player->GetGProxyExtended()) {
      return true;
    }
  }
  return false;
}

uint16_t CGame::GetDiscoveryPort(const uint8_t protocol) const
{
  return m_Aura->m_Net->GetUDPPort(protocol);
}

vector<uint8_t> CGame::GetGameDiscoveryInfo(const uint16_t hostPort) const
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

  return GetProtocol()->SEND_W3GS_GAMEINFO(
    m_Aura->m_GameVersion,
    GetGameType(),
    GetGameFlags(),
    GetAnnounceWidth(),
    GetAnnounceHeight(),
    m_GameName,
    m_IndexVirtualHostName,
    GetUptime(),
    GetSourceFilePath(),
    GetSourceFileHash(),
    static_cast<uint32_t>(m_Slots.size()), // Total Slots
    static_cast<uint32_t>(m_Slots.size() == GetSlotsOpen() ? m_Slots.size() : GetSlotsOpen() + 1), // "Available" Slots
    hostPort,
    m_HostCounter,
    m_EntryKey
  );
}

void CGame::AnnounceToRealm(CRealm* realm)
{
  realm->SendGameRefresh(m_GameDisplay, this);
}

void CGame::AnnounceToAddress(string& addressLiteral) const
{
  optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(addressLiteral);
  if (!maybeAddress.has_value())
    return;

  sockaddr_storage* address = &(maybeAddress.value());
  SetAddressPort(address, 6112);
  if (isLoopbackAddress(address)) {
    m_Aura->m_Net->Send(address, GetGameDiscoveryInfo(m_HostPort));
  } else {
    m_Aura->m_Net->Send(address, GetGameDiscoveryInfo(GetHostPortForDiscoveryInfo(GetInnerIPVersion(address))));
  }
}

void CGame::ReplySearch(sockaddr_storage* address, CSocket* socket) const
{
  if (socket->m_Type == SOCK_DGRAM) {
    CUDPServer* server = reinterpret_cast<CUDPServer*>(socket);
    if (isLoopbackAddress(address)) {
      server->SendReply(address, GetGameDiscoveryInfo(m_HostPort));
    } else {
      server->SendReply(address, GetGameDiscoveryInfo(GetHostPortForDiscoveryInfo(GetInnerIPVersion(address))));
    }
  } else if (socket->m_Type == SOCK_STREAM) {
    CStreamIOSocket* tcpSocket = reinterpret_cast<CStreamIOSocket*>(socket);
    tcpSocket->SendReply(address, GetGameDiscoveryInfo(GetHostPortForDiscoveryInfo(GetInnerIPVersion(address))));
  }
}

void CGame::SendGameDiscoveryCreate() const
{
  vector<uint8_t> packet = GetProtocol()->SEND_W3GS_CREATEGAME(m_Aura->m_GameVersion, m_HostCounter);
  m_Aura->m_Net->SendGameDiscovery(packet, m_ExtraDiscoveryAddresses);
}

void CGame::SendGameDiscoveryDecreate() const
{
  vector<uint8_t> packet = GetProtocol()->SEND_W3GS_DECREATEGAME(m_HostCounter);
  m_Aura->m_Net->SendGameDiscovery(packet, m_ExtraDiscoveryAddresses);
}

void CGame::SendGameDiscoveryRefresh() const
{
  vector<uint8_t> packet = GetProtocol()->SEND_W3GS_REFRESHGAME(
    m_HostCounter,
    static_cast<uint32_t>(m_Slots.size() == GetSlotsOpen() ? 1 : m_Slots.size() - GetSlotsOpen()),
    static_cast<uint32_t>(m_Slots.size())
  );
  m_Aura->m_Net->SendGameDiscovery(packet, m_ExtraDiscoveryAddresses);
}

void CGame::SendGameDiscoveryInfo() const
{
  // See CNet::SendGameDiscovery()
  bool loopbackIsIPv4Port = m_HostPort == m_Aura->m_Net->m_Config->m_UDPCustomPortTCP4 || !m_Aura->m_Net->m_Config->m_UDPEnableCustomPortTCP4;
  bool loopbackIsIPv6Port = m_HostPort == m_Aura->m_Net->m_Config->m_UDPCustomPortTCP6 || !m_Aura->m_Net->m_SupportTCPOverIPv6 || !m_Aura->m_Net->m_Config->m_UDPEnableCustomPortTCP6;
  bool thereCouldBeUDPTunnelsOverTCP = m_Aura->m_Net->m_Config->m_EnableTCPWrapUDP && !m_Aura->m_Net->m_IncomingConnections.empty();
  if (loopbackIsIPv4Port && (loopbackIsIPv6Port || (m_ExtraDiscoveryAddresses.empty() && !thereCouldBeUDPTunnelsOverTCP))) {
    vector<uint8_t> packet = GetGameDiscoveryInfo(m_HostPort);
    m_Aura->m_Net->SendGameDiscovery(packet, m_ExtraDiscoveryAddresses);
    return;
  }

  vector<uint8_t> ipv4Packet = GetGameDiscoveryInfo(GetHostPortForDiscoveryInfo(AF_INET)); // Uses <net.game_discovery.udp.tcp4_custom_port.value>
  vector<uint8_t> ipv6Packet;
  if (m_Aura->m_Net->m_SupportTCPOverIPv6)
    ipv6Packet = GetGameDiscoveryInfo(GetHostPortForDiscoveryInfo(AF_INET6)); // Uses <net.game_discovery.udp.tcp6_custom_port.value>

  if (!m_Aura->m_Net->SendBroadcast(ipv4Packet)) {
    // Ensure the game is available at loopback.
    if (loopbackIsIPv4Port) {
      if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
        Print(GetLogPrefix() + "sending IPv4 GAMEINFO packet to IPv4 Loopback (game port " + to_string(GetHostPortForDiscoveryInfo(AF_INET)) + ")");
      }
      m_Aura->m_Net->Send("127.0.0.1", ipv4Packet);
    } else {
      if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE2)) {
        Print(GetLogPrefix() + "ending IPv4 GAMEINFO packet to IPv4 Loopback (game port " + to_string(m_HostPort) + ")");
      }
      vector<uint8_t> hostPortPacket = GetGameDiscoveryInfo(m_HostPort);
      m_Aura->m_Net->Send("127.0.0.1", hostPortPacket);
    }
  }

  for (auto& clientIp : m_ExtraDiscoveryAddresses) {
    optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(clientIp);
    if (!maybeAddress.has_value()) continue; // Should never happen.
    sockaddr_storage* address = &(maybeAddress.value());
    if (isLoopbackAddress(address)) continue; // We already secure sending loopback packets above.
    bool isIPv6 = GetInnerIPVersion(address) == AF_INET6;
    if (isIPv6 && !m_Aura->m_Net->m_SupportTCPOverIPv6) {
      continue;
    }
    m_Aura->m_Net->Send(address, isIPv6 ? ipv6Packet : ipv4Packet);
  }

  // Send to active UDP in TCP tunnels
  if (thereCouldBeUDPTunnelsOverTCP) for (auto& pair : m_Aura->m_Net->m_IncomingConnections) {
    for (auto& connection : pair.second) {
      if (connection->GetDeleteMe()) continue;
      if (connection->m_IsUDPTunnel) {
        connection->Send(connection->GetUsingIPv6() ? ipv6Packet : ipv4Packet);
      }
    }
  }
}

void CGame::EventPlayerDeleted(CGamePlayer* player, void* fd, void* send_fd)
{
  Print(GetLogPrefix() + "deleting player [" + player->GetName() + "]: " + player->GetLeftReason());

  m_LastPlayerLeaveTicks = GetTicks();

  if (m_GameLoading || m_GameLoaded) {
    for (auto& otherPlayer : m_SyncPlayers[player]) {
      std::vector<CGamePlayer*>& BackList = m_SyncPlayers[otherPlayer];
      auto BackIterator = std::find(BackList.begin(), BackList.end(), player);
      if (BackIterator == BackList.end()) {
      } else {
        *BackIterator = std::move(BackList.back());
        BackList.pop_back();
      }
    }
    m_SyncPlayers.erase(player);
    m_HadLeaver = true;
  } else if (!m_LobbyLoading) {
    if (MatchOwnerName(player->GetName()) && m_OwnerRealm == player->GetRealmHostName() && player->GetRealmHostName().empty()) {
      ReleaseOwner();
    }
  }

  // in some cases we're forced to send the left message early so don't send it again
  if (!player->GetLeftMessageSent()) {
    if (player->GetLagging()) {
      SendAll(GetProtocol()->SEND_W3GS_STOP_LAG(player));
    }
    SendLeftMessage(player, m_GameLoaded);
  }

  // abort the countdown if there was one in progress
  if (m_CountDownStarted && !m_GameLoading && !m_GameLoaded) {
    SendAllChat("Countdown stopped because [" + player->GetName() + "] left!");
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

    m_DBGamePlayers.push_back(new CDBGamePlayer(player->GetName(), player->GetFinishedLoading() ? player->GetFinishedLoadingTicks() - m_StartedLoadingTicks : 0, m_GameTicks / 1000, m_Slots[SID].GetColor()));

    // also keep track of the last player to leave for the !banlast command

    for (auto& ban : m_DBBans) {
      if (ban->GetName() == player->GetName())
        m_DBBanLast = ban;
    }
  }

  // Flush queued data before the socket is destroyed.
  player->GetSocket()->DoSend(static_cast<fd_set*>(send_fd));
}

void CGame::EventLobbyLastPlayerLeaves()
{
  ResetLayout(false);
}

void CGame::ReportPlayerDisconnected(CGamePlayer* player)
{
  int64_t Time = GetTime(), Ticks = GetTicks();
  if (!player->GetLagging()) {
    for (auto& eachPlayer : m_Players)
      eachPlayer->SetDropVote(false);

    if (!GetLagging()) {
      m_Lagging = true;
      m_StartedLaggingTime = Time;
      m_LastLagScreenResetTime = Time;
    }

    // Report lagging players:
    // - Just disconnected player
    // - Players outside safe sync limit
    // Since the disconnected player has already been flagged with SetGProxyDisconnectNoticeSent, they get
    // excluded from the output vector of CalculateNewLaggingPlayers(),
    // So we have to add them afterwards.
    vector<CGamePlayer*> laggingPlayers = CalculateNewLaggingPlayers();
    laggingPlayers.push_back(player);
    for (auto& laggingPlayer : laggingPlayers) {
      laggingPlayer->SetLagging(true);
      laggingPlayer->SetStartedLaggingTicks(Ticks);
    }
    SendAll(GetProtocol()->SEND_W3GS_START_LAG(laggingPlayers));
  }
  int64_t TimeRemaining = (m_GProxyEmptyActions + 1) * 60 - (Time - m_StartedLaggingTime);

  if (TimeRemaining > (m_GProxyEmptyActions + 1) * 60)
    TimeRemaining = (m_GProxyEmptyActions + 1) * 60;
  else if (TimeRemaining < 0)
    TimeRemaining = 0;

  SendAllChat(player->GetPID(), "Please wait for me to reconnect (" + to_string(TimeRemaining) + " seconds remain)");
  player->SetLastGProxyWaitNoticeSentTime(Time);
}

void CGame::EventPlayerDisconnectTimedOut(CGamePlayer* player)
{
  if (player->GetGProxyAny() && m_GameLoaded) {
    if (!player->GetGProxyDisconnectNoticeSent()) {
      if (player->GetGProxyExtended()) {
        SendAllChat(player->GetName() + " " + "has disconnected, but is using GProxyDLL and may reconnect while others keep playing");
      } else {
        SendAllChat(player->GetName() + " " + "has disconnected, but is using GProxy++ and may reconnect");
      }
      player->SetGProxyDisconnectNoticeSent(true);
    }

    if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20 && !player->GetGProxyExtended()) {
      ReportPlayerDisconnected(player);
    }

    return;
  }

  // not only do we not do any timeouts if the game is lagging, we allow for an additional grace period of 10 seconds
  // this is because Warcraft 3 stops sending packets during the lag screen
  // so when the lag screen finishes we would immediately disconnect everyone if we didn't give them some extra time

  if (GetTime() - m_LastLagScreenTime >= 10 && !player->GetGProxyExtended()) {
    TrySaveOnDisconnect(player, false);
    player->SetDeleteMe(true);
    player->SetLeftReason("has lost the connection (timed out)");
    player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

    if (!m_GameLoading && !m_GameLoaded) {
      const uint8_t SID = GetSIDFromPID(player->GetPID());
      OpenSlot(SID, false);
    }
  }
}

void CGame::EventPlayerDisconnectSocketError(CGamePlayer* player)
{
  if (player->GetGProxyAny() && m_GameLoaded) {
    if (!player->GetGProxyDisconnectNoticeSent()) {
      SendAllChat(player->GetName() + " " + "has disconnected (connection error - " + player->GetSocket()->GetErrorString() + ") but is using GProxy++ and may reconnect");
      player->SetGProxyDisconnectNoticeSent(true);
    }

    if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20 && !player->GetGProxyExtended()) {
      ReportPlayerDisconnected(player);
    }

    return;
  }

  TrySaveOnDisconnect(player, false);
  player->SetDeleteMe(true);
  player->SetLeftReason("has lost the connection (connection error - " + player->GetSocket()->GetErrorString() + ")");
  player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

  if (!m_GameLoading && !m_GameLoaded) {
    const uint8_t SID = GetSIDFromPID(player->GetPID());
    OpenSlot(SID, false);
  }
}

void CGame::EventPlayerDisconnectConnectionClosed(CGamePlayer* player)
{
  if (player->GetGProxyAny() && m_GameLoaded) {
    if (!player->GetGProxyDisconnectNoticeSent()) {
      SendAllChat(player->GetName() + " " + "has terminated the connection, but is using GProxy++ and may reconnect");
      player->SetGProxyDisconnectNoticeSent(true);
    }

    if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20 && !player->GetGProxyExtended()) {
      ReportPlayerDisconnected(player);
    }

    return;
  }

  TrySaveOnDisconnect(player, false);
  player->SetDeleteMe(true);
  player->SetLeftReason("has terminated the connection");
  player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

  if (!m_GameLoading && !m_GameLoaded) {
    const uint8_t SID = GetSIDFromPID(player->GetPID());
    OpenSlot(SID, false);
  }
}

void CGame::EventPlayerDisconnectGameProtocolError(CGamePlayer* player)
{
  if (player->GetGProxyAny() && m_GameLoaded) {
    if (!player->GetGProxyDisconnectNoticeSent()) {
      SendAllChat(player->GetName() + " " + "has disconnected (protocol error) but is using GProxy++ and may reconnect");
      player->SetGProxyDisconnectNoticeSent(true);
    }

    if (GetTime() - player->GetLastGProxyWaitNoticeSentTime() >= 20 && !player->GetGProxyExtended()) {
      ReportPlayerDisconnected(player);
    }

    return;
  }

  TrySaveOnDisconnect(player, false);
  player->SetDeleteMe(true);
  player->SetLeftReason("has lost the connection (protocol error)");
  player->SetLeftCode(PLAYERLEAVE_DISCONNECT);

  if (!m_GameLoading && !m_GameLoaded) {
    const uint8_t SID = GetSIDFromPID(player->GetPID());
    OpenSlot(SID, false);
  }
}

void CGame::SendLeftMessage(CGamePlayer* player, const bool sendChat) const
{
  // This function, together with GetLeftMessage and SetLeftMessageSent,
  // controls which PIDs Aura considers available.
  if (sendChat) {
    if (player->GetQuitGame()) {
      SendAllChat(player->GetPID(), player->GetLeftReason() + ".");
    } else {
      SendAllChat(player->GetName() + " " + player->GetLeftReason() + ".");
    }
  }
  player->SetLeftMessageSent(true);
  SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(player->GetPID(), player->GetLeftCode()));
}

void CGame::SendEveryoneElseLeft() const
{
  for (auto& p1 : m_Players) {
    for (auto& p2 : m_Players) {
      if (p1 == p2 || p2->GetLeftMessageSent()) {
        continue;
      }
      Send(p1, GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(p2->GetPID(), p2->GetLeftCode()));
    }
  }
}

void CGame::EventPlayerKickHandleQueued(CGamePlayer* player)
{
  if (player->GetDeleteMe())
    return;

  if (m_CountDownStarted) {
    player->SetKickByTime(0);
    return;
  }

  player->SetDeleteMe(true);
  // left reason, left code already assigned when queued

  const uint8_t SID = GetSIDFromPID(player->GetPID());
  OpenSlot(SID, false);
}

void CGame::EventPlayerCheckStatus(CGamePlayer* player)
{
  if (player->GetDeleteMe())
    return;

  if (m_CountDownStarted) {
    player->SetStatusMessageSent(true);
    return;
  }

  bool IsOwnerName = MatchOwnerName(player->GetName());
  string OwnerFragment;
  if (player->GetIsOwner(nullopt)) {
    OwnerFragment = " (game owner)";
  } else if (IsOwnerName) {
    OwnerFragment = " (unverified game owner, send me a whisper: \"sc\")";
  }

  string GProxyFragment;
  if (m_Aura->m_Net->m_Config->m_AnnounceGProxy && GetIsProxyReconnectable()) {
    if (player->GetGProxyExtended()) {
      GProxyFragment = " is using GProxyDLL, a Warcraft III plugin to protect against disconnections. See: <" + m_Aura->m_Net->m_Config->m_AnnounceGProxySite + ">";
    } else if (player->GetGProxyAny()) {
      if (GetIsProxyReconnectableLong()) {
        GProxyFragment = " is using an outdated GProxy++. Please upgrade to GProxyDLL at: <" + m_Aura->m_Net->m_Config->m_AnnounceGProxySite + ">";
      } else {
        GProxyFragment = " is using GProxy, a Warcraft III plugin to protect against disconnections. See: <" + m_Aura->m_Net->m_Config->m_AnnounceGProxySite + ">";
      }
    } else if (m_Aura->m_GameVersion < 26 || 29 < m_Aura->m_GameVersion) {
      GProxyFragment = " is not using disconnection protection. You may download it at: <" + m_Aura->m_Net->m_Config->m_AnnounceGProxySite + ">";
    }
  }
  
  player->SetStatusMessageSent(true);
  if (OwnerFragment.empty() && GProxyFragment.empty()) {
    if (m_Aura->m_Net->m_Config->m_AnnounceIPv6 && player->GetUsingIPv6()) {
      Print(player->GetName() + " joined the game over IPv6 (" + player->GetIPStringStrict() + ").");
      SendAllChat(player->GetName() + " joined the game over IPv6.");
    }
    return;
  }

  string IPv6Fragment;
  if (player->GetUsingIPv6()) {
    Print(player->GetName() + " joined the game over IPv6 (" + player->GetIPStringStrict() + ").");
    IPv6Fragment = ". (Joined over IPv6).";
  }
  if (!OwnerFragment.empty() && !GProxyFragment.empty()) {
    SendAllChat(player->GetName() + OwnerFragment + GProxyFragment + IPv6Fragment);
  } else if (!OwnerFragment.empty()) {
    if (player->GetUsingIPv6()) {
      Print(player->GetName() + OwnerFragment + " joined the game over IPv6 (" + player->GetIPStringStrict() + ").");
      SendAllChat(player->GetName() + OwnerFragment + " joined the game over IPv6.");
    } else {
      SendAllChat(player->GetName() + OwnerFragment + " joined the game.");
    }
  } else {
    SendAllChat(player->GetName() + GProxyFragment + IPv6Fragment);
  }
}

CGamePlayer* CGame::JoinPlayer(CGameConnection* connection, CIncomingJoinRequest* joinRequest, const uint8_t SID, const uint8_t PID, const uint8_t HostCounterID, const string JoinedRealm, const bool IsReserved, const bool IsUnverifiedAdmin)
{
  // If realms are reloaded, HostCounter may change.
  // However, internal realm IDs maps to constant realm input IDs.
  // Hence, CGamePlayers are created with references to internal realm IDs.
  uint32_t internalRealmId = HostCounterID;
  CRealm* matchingRealm = nullptr;
  if (HostCounterID >= 0x10) {
    matchingRealm = m_Aura->GetRealmByHostCounter(HostCounterID);
    if (matchingRealm) internalRealmId = matchingRealm->GetInternalID();
  }

  CGamePlayer* Player = new CGamePlayer(this, connection, PID == 0xFF ? GetNewPID() : PID, internalRealmId, JoinedRealm, joinRequest->GetName(), joinRequest->GetIPv4Internal(), IsReserved);
  // Now, socket belongs to CGamePlayer. Don't look for it in CGameConnection.

  m_Players.push_back(Player);
  connection->SetSocket(nullptr);
  connection->SetDeleteMe(true);

  if (matchingRealm) {
    Player->SetWhoisShouldBeSent(
      IsUnverifiedAdmin || MatchOwnerName(Player->GetName()) || !HasOwnerSet() ||
      matchingRealm->GetIsFloodImmune() || matchingRealm->GetHasEnhancedAntiSpoof()
    );
  }

  if (GetIsCustomForces()) {
    m_Slots[SID] = CGameSlot(Player->GetPID(), SLOTPROG_RST, SLOTSTATUS_OCCUPIED, 0, m_Slots[SID].GetTeam(), m_Slots[SID].GetColor(), m_Map->GetLobbyRace(&m_Slots[SID]));
  } else {
    m_Slots[SID] = CGameSlot(Player->GetPID(), SLOTPROG_RST, SLOTSTATUS_OCCUPIED, 0, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, m_Map->GetLobbyRace(&m_Slots[SID]));
    SetSlotTeamAndColorAuto(SID);
  }

  // send slot info to the new player
  // the SLOTINFOJOIN packet also tells the client their assigned PID and that the join was successful

  Player->Send(GetProtocol()->SEND_W3GS_SLOTINFOJOIN(Player->GetPID(), Player->GetSocket()->GetPortLE(), Player->GetIPv4(), m_Slots, m_RandomSeed, GetLayout(), m_Map->GetMapNumControllers()));

  SendIncomingPlayerInfo(Player);

  // send virtual host info and fake players info (if present) to the new player

  SendVirtualHostPlayerInfo(Player);
  SendFakePlayersInfo(Player);
  SendJoinedPlayersInfo(Player);

  // send a map check packet to the new player

  if (m_Aura->m_GameVersion >= 23) {
    Player->Send(GetProtocol()->SEND_W3GS_MAPCHECK(m_MapPath, m_Map->GetMapSize(), m_Map->GetMapCRC32(), m_Map->GetMapHash(), m_Map->GetMapSHA1()));
  } else {
    Player->Send(GetProtocol()->SEND_W3GS_MAPCHECK(m_MapPath, m_Map->GetMapSize(), m_Map->GetMapCRC32(), m_Map->GetMapHash()));
  }

  // send slot info to everyone, so the new player gets this info twice but everyone else still needs to know the new slot layout
  SendAllSlotInfo();

  // send a welcome message

  if (!m_RestoredGame)
    SendWelcomeMessage(Player);

  // check for multiple IP usage
  if (!Player->GetSocket()->GetIsLoopback()) {
    string Others;
    for (auto& player : m_Players){
      if (Player == player)
        continue;
      if (Player->GetIPString() != player->GetIPString())
        continue;

      if (Others.empty())
        Others = player->GetName();
      else
        Others += ", " + player->GetName();
    }

    if (!Others.empty()) {
      SendAllChat("Player [" + joinRequest->GetName() + "] has the same IP address as: " + Others);
    }
  }

  // abort the countdown if there was one in progress

  if (m_CountDownStarted && !m_GameLoading && !m_GameLoaded)
  {
    SendAllChat("Countdown stopped!");
    m_CountDownStarted = false;
  }

  string notifyString = "";
  if (m_NotifyJoins && m_IgnoredNotifyJoinPlayers.find(joinRequest->GetName()) == m_IgnoredNotifyJoinPlayers.end()) {
    notifyString = "\x07";
  }
  Print(GetLogPrefix() + "player joined (P" + to_string(SID + 1) + "): [" + joinRequest->GetName() + "@" + Player->GetRealmHostName() + "#" + to_string(Player->GetPID()) + "] from [" + Player->GetIPString() + "] (" + Player->GetSocket()->GetName() + ")" + notifyString);

  return Player;
}

bool CGame::EventRequestJoin(CGameConnection* connection, CIncomingJoinRequest* joinRequest)
{
  if (joinRequest->GetName().empty() || joinRequest->GetName().size() > 15) {
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "] invalid name - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")");
    connection->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  // identify their joined realm
  // this is only possible because when we send a game refresh via LAN or battle.net we encode an ID value in the 4 most significant bits of the host counter
  // the client sends the host counter when it joins so we can extract the ID value here
  // note: this is not a replacement for spoof checking since it doesn't verify the player's name and it can be spoofed anyway

  string         JoinedRealm;
  uint8_t HostCounterID = joinRequest->GetHostCounter() >> 24;
  bool IsUnverifiedAdmin = false;

  CRealm* matchingRealm = nullptr;
  if (HostCounterID >= 0x10) {
    matchingRealm = m_Aura->GetRealmByHostCounter(HostCounterID);
    if (matchingRealm) {
      JoinedRealm = matchingRealm->GetServer();
      IsUnverifiedAdmin = matchingRealm->GetIsModerator(joinRequest->GetName()) || matchingRealm->GetIsAdmin(joinRequest->GetName());
    } else {
      // Trying to join from an unknown realm.
      HostCounterID = 0xF;
    }
  }

  if (HostCounterID < 0x10 && joinRequest->GetEntryKey() != m_EntryKey) {
    // check if the player joining via LAN knows the entry key
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "@" + JoinedRealm + "] used a wrong LAN key (" + to_string(joinRequest->GetEntryKey()) + ") - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")");
    connection->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_WRONGPASSWORD));
    return false;
  }

  if (HostCounterID < 0x10 && HostCounterID != 0) {
    // 0x1: Information
    // others: undefined
    if (HostCounterID == 0x1) {
      connection->Send(GetProtocol()->SEND_W3GS_SLOTINFOJOIN(GetNewPID(), connection->GetSocket()->GetPortLE(), connection->GetIPv4(), m_Slots, m_RandomSeed, GetLayout(), m_Map->GetMapNumControllers()));
      SendVirtualHostPlayerInfo(connection);
      SendFakePlayersInfo(connection);
      SendJoinedPlayersInfo(connection);
      return false;
    }
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "@" + JoinedRealm + "] is trying to join over reserved realm " + to_string(HostCounterID) + " - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")");
    if (HostCounterID > 0x2) {
      connection->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_WRONGPASSWORD));
      return false;
    }
  }

  if (GetPlayerFromName(joinRequest->GetName(), false)) {
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "] invalid name (taken) - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")");
    connection->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  } else if (joinRequest->GetName() == m_LobbyVirtualHostName || (joinRequest->GetName().length() >= 7 && joinRequest->GetName().substr(0, 5) == "User[")) {
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "] spoofer (matches host name) - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")");
    connection->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  } else if (joinRequest->GetName() == m_OwnerName && !m_OwnerRealm.empty() && !JoinedRealm.empty() && m_OwnerRealm != JoinedRealm) {
    // Prevent owner homonyms from other realms from joining. This doesn't affect LAN.
    // But LAN has its own rules, e.g. a LAN owner that leaves the game is immediately demoted.
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "@" + JoinedRealm + "] spoofer (matches owner name, but realm mismatch, expected " + m_OwnerRealm + ") - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")");
    connection->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  // check if the new player's name is banned
  // don't allow the player to spam the chat by attempting to join the game multiple times in a row

  if (matchingRealm) {
    if (matchingRealm->IsBannedName(joinRequest->GetName())){
      Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "|" + connection->GetIPString() + "] is banned");

      if (m_ReportedJoinFailNames.find(joinRequest->GetName()) == end(m_ReportedJoinFailNames)) {
        SendAllChat("[" + joinRequest->GetName() + "@" + JoinedRealm + "] is trying to join the game, but is banned");
        m_ReportedJoinFailNames.insert(joinRequest->GetName());
      }

      // let banned players "join" the game with an arbitrary PID then immediately close the connection
      // this causes them to be kicked back to the chat channel on battle.net

      vector<CGameSlot> Slots = m_Map->GetSlots();
      connection->Send(GetProtocol()->SEND_W3GS_SLOTINFOJOIN(1, connection->GetSocket()->GetPortLE(), connection->GetIPv4(), Slots, 0, GetLayout(), m_Map->GetMapNumControllers()));
      return false;
    }
    matchingRealm = nullptr;
  }

  // Check if the player is an admin or root admin on any connected realm for determining reserved status
  // Note: vulnerable to spoofing
  const uint8_t reservedIndex = GetReservedIndex(joinRequest->GetName());
  const bool isReserved = reservedIndex < m_Reserved.size() || (!m_RestoredGame && MatchOwnerName(joinRequest->GetName()) && JoinedRealm == m_OwnerRealm);

  if (m_CheckReservation && !isReserved) {
    Print(GetLogPrefix() + "player [" + joinRequest->GetName() + "] missing reservation - [" + connection->GetSocket()->GetName() + "] (" + connection->GetIPString() + ")");
    connection->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  uint8_t SID = 0xFF;
  uint8_t PID = 0xFF;

  if (m_RestoredGame) {
    uint8_t matchCounter = 0xFF;
    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      if (!m_RestoredGame->GetSlots()[i].GetIsPlayerOrFake()) {
        continue;
      }
      if (++matchCounter == reservedIndex) {
        SID = i;
        PID = m_RestoredGame->GetSlots()[i].GetPID();
        break;
      }
    }
  } else {
    SID = GetEmptySID(false);

    if (SID == 0xFF && isReserved) {
      // a reserved player is trying to join the game but it's full, try to find a reserved slot

      SID = GetEmptySID(true);
      if (SID != 0xFF) {
        CGamePlayer* kickedPlayer = GetPlayerFromSID(SID);

        if (kickedPlayer) {
          kickedPlayer->SetDeleteMe(true);
          kickedPlayer->SetLeftReason("was kicked to make room for a reserved player [" + joinRequest->GetName() + "]");
          kickedPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);

          // Ensure the playerleave message is sent before the reserved playerjoin message.
          SendLeftMessage(kickedPlayer, true);
        }
      }
    }

    if (SID == 0xFF && MatchOwnerName(joinRequest->GetName()) && JoinedRealm == m_OwnerRealm) {
      // the owner player is trying to join the game but it's full and we couldn't even find a reserved slot, kick the player in the lowest numbered slot
      // updated this to try to find a player slot so that we don't end up kicking a computer

      SID = 0;

      for (uint8_t i = 0; i < m_Slots.size(); ++i) {
        if (m_Slots[i].GetIsPlayerOrFake()) {
          SID = i;
          break;
        }
      }

      CGamePlayer* kickedPlayer = GetPlayerFromSID(SID);

      if (kickedPlayer) {
        kickedPlayer->SetDeleteMe(true);
        kickedPlayer->SetLeftReason("was kicked to make room for the owner player [" + joinRequest->GetName() + "]");
        kickedPlayer->SetLeftCode(PLAYERLEAVE_LOBBY);
        // Ensure the playerleave message is sent before the game owner' playerjoin message.
        SendLeftMessage(kickedPlayer, true);
      }
    }
  }

  if (SID >= static_cast<uint8_t>(m_Slots.size())) {
    connection->Send(GetProtocol()->SEND_W3GS_REJECTJOIN(REJECTJOIN_FULL));
    return false;
  }

  // we have a slot for the new player
  // make room for them by deleting the virtual host player if we have to

  if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN && GetSlotsOpen() == 1 && GetNumHumanOrFakeControllers() > 1)
    DeleteVirtualHost();

  JoinPlayer(connection, joinRequest, SID, PID, HostCounterID, JoinedRealm, isReserved, IsUnverifiedAdmin);
  return true;
}

void CGame::EventPlayerLeft(CGamePlayer* player)
{
  // this function is only called when a player leave packet is received, not when there's a socket error, kick, etc...
  TrySaveOnDisconnect(player, true);
  player->SetDeleteMe(true);
  player->SetLeftReason("Leaving the game voluntarily");
  player->SetLeftCode(PLAYERLEAVE_LOST);
  player->SetQuitGame(true);

  if (!m_GameLoading && !m_GameLoaded) {
    const uint8_t SID = GetSIDFromPID(player->GetPID());
    OpenSlot(SID, false);
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

  if (!action->GetAction()->empty()) {
    switch((*action->GetAction())[0]) {
      case ACTION_SAVE:
        Print(GetLogPrefix() + "player [" + player->GetName() + "] is saving the game");
        SendAllChat("Player [" + player->GetName() + "] is saving the game");
        player->SetSaved(true);
        break;
      case ACTION_PAUSE:
        Print(GetLogPrefix() + "player [" + player->GetName() + "] paused the game");
        player->AddPauseCounter();
        m_Paused = true;
        break;
      case ACTION_RESUME:
        m_Paused = false;
        break;
      default:
        break;
    }
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
  vector<CGamePlayer*> DesyncedPlayers;
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
      } else {
        *BackIterator = std::move(BackList.back());
        BackList.pop_back();
      }

      DesyncedPlayers.push_back(*it);
      std::iter_swap(it, OtherPlayers.end() - 1);
      OtherPlayers.pop_back();
    }
  }
  if (DesyncDetected) {
    m_Desynced = true;
    string syncListText = PlayersToNameListString(m_SyncPlayers[player]);
    string desyncListText = PlayersToNameListString(DesyncedPlayers);
    uint8_t GProxyMode = GetActiveReconnectProtocols();
    Print(GetLogPrefix() + " [GProxy=" + ToDecString(GProxyMode) + "] " + player->GetName() + " (" + player->GetDelayText() + ") is synchronized with " + to_string(m_SyncPlayers[player].size()) + " player(s): " + syncListText);
    Print(GetLogPrefix() + " [GProxy=" + ToDecString(GProxyMode) + "] " + player->GetName() + " (" + player->GetDelayText() + ") no longer synchronized with " + desyncListText);
    if (GProxyMode > 0) {
      Print(GetLogPrefix() + "GProxy: " + GetActiveReconnectProtocolsDetails());
    }
    SendAllChat("Warning! Desync detected (" + player->GetName() + " (" + player->GetDelayText() + ") may not be in the same game as " + desyncListText);
  }
}

void CGame::EventPlayerChatToHost(CGamePlayer* player, CIncomingChatPlayer* chatPlayer)
{
  if (chatPlayer->GetFromPID() == player->GetPID())
  {
    player->ClearLastCommand();

    if (chatPlayer->GetType() == CIncomingChatPlayer::CTH_MESSAGE || chatPlayer->GetType() == CIncomingChatPlayer::CTH_MESSAGEEXTRA)
    {
      // relay the chat message to other players

      bool Relay = !player->GetMuted();
      bool OnlyToObservers = player->GetIsObserver() && (m_GameLoading || m_GameLoaded) && !player->GetIsPowerObserver();
      const vector<uint8_t> ExtraFlags = chatPlayer->GetExtraFlags();
      const bool isLobbyChat = ExtraFlags.empty();

      // calculate timestamp

      if (isLobbyChat) {
        Print(GetLogPrefix() + "[" + player->GetName() + "] " + chatPlayer->GetMessage());

        if (m_MuteLobby)
          Relay = false;
      } else {
        if (ExtraFlags[0] == CHAT_RECV_ALL) {
          Print(GetLogPrefix() + "[All] [" + player->GetName() + "] " + chatPlayer->GetMessage());

          if (m_MuteAll) {
            // don't relay ingame messages targeted for all players if we're currently muting all
            // note that any commands will still be processed
            Relay = false;
          }
        } else if (ExtraFlags[0] == CHAT_RECV_ALLY) {
          Print(GetLogPrefix() + "[Allies] [" + player->GetName() + "] " + chatPlayer->GetMessage());
        } else if (ExtraFlags[0] == CHAT_RECV_OBS) {
          // [Observer] or [Referees]
					Print(GetLogPrefix() + "[Observer] [" + player->GetName() + "] " + chatPlayer->GetMessage());
        } else {
          uint8_t privateTarget = ExtraFlags[0] - 2;
          Print(GetLogPrefix() + "[Private " + ToDecString(privateTarget) + "] [" + player->GetName() + "] " + chatPlayer->GetMessage());
        }
      }

      if (Relay) {
        if (OnlyToObservers) {
          std::vector<uint8_t> TargetPIDs = chatPlayer->GetToPIDs();
          std::vector<uint8_t> ObserversPIDs;
          std::vector<string> ForbiddenNames;
          for (uint8_t i = 0; i < TargetPIDs.size(); ++i) {
            CGamePlayer* TargetPlayer = GetPlayerFromPID(TargetPIDs[i]);
            if (!TargetPlayer) continue;
            if (TargetPlayer->GetIsObserver()) {
              ObserversPIDs.push_back(TargetPIDs[i]);
            } else {
              ForbiddenNames.push_back(TargetPlayer->GetName());
            }
          }
          if (!ObserversPIDs.empty()) {
            Send(ObserversPIDs, GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromPID(), ObserversPIDs, chatPlayer->GetFlag(), chatPlayer->GetExtraFlags(), chatPlayer->GetMessage()));
          } else if (ForbiddenNames.empty()) {
            Print(GetLogPrefix() + "[Obs/Ref] --nobody listening--");
          } else {
            Print(GetLogPrefix() + "[Private] [" + player->GetName() + "] --X- >[" + JoinVector(ForbiddenNames, false) + "] " + chatPlayer->GetMessage());
          }
        } else {
          Send(chatPlayer->GetToPIDs(), GetProtocol()->SEND_W3GS_CHAT_FROM_HOST(chatPlayer->GetFromPID(), chatPlayer->GetToPIDs(), chatPlayer->GetFlag(), chatPlayer->GetExtraFlags(), chatPlayer->GetMessage()));
        }
      }

      // handle bot commands
      {
        CRealm* realm = player->GetRealm(false);
        CCommandConfig* commandCFG = realm ? realm->GetCommandConfig() : m_Aura->m_Config->m_LANCommandCFG;
        const bool commandsEnabled = commandCFG->m_Enabled && (
          !realm || !(commandCFG->m_RequireVerified && !player->IsRealmVerified())
        );
        if (commandsEnabled) {
          const string message = chatPlayer->GetMessage();
          string cmdToken, command, payload;
          uint8_t tokenMatch = ExtractMessageTokensAny(message, m_PrivateCmdToken, m_BroadcastCmdToken, cmdToken, command, payload);
          if (tokenMatch != COMMAND_TOKEN_MATCH_NONE) {
            player->SetUsedAnyCommands(true);
            CCommandContext* ctx = new CCommandContext(m_Aura, commandCFG, this, player, tokenMatch == COMMAND_TOKEN_MATCH_BROADCAST, &std::cout);
            ctx->Run(cmdToken, command, payload);
            m_Aura->UnholdContext(ctx);
          } else if (isLobbyChat && !player->GetUsedAnyCommands() && !player->GetSentAutoCommandsHelp()) {
            SendCommandsHelp(m_BroadcastCmdToken.empty() ? m_PrivateCmdToken : m_BroadcastCmdToken, player, true);
          } else if (payload == "?trigger") {
            SendCommandsHelp(m_BroadcastCmdToken.empty() ? m_PrivateCmdToken : m_BroadcastCmdToken, player, false);
          }
        }
      }
    }
    else
    {
      if (!m_CountDownStarted && !m_RestoredGame) {
        switch (chatPlayer->GetType()) {
          case CIncomingChatPlayer::CTH_TEAMCHANGE:
            EventPlayerChangeTeam(player, chatPlayer->GetByte());
            break;
          case CIncomingChatPlayer::CTH_COLOURCHANGE:
            EventPlayerChangeColor(player, chatPlayer->GetByte());
            break;
          case CIncomingChatPlayer::CTH_RACECHANGE:
            EventPlayerChangeRace(player, chatPlayer->GetByte());
            break;
          case CIncomingChatPlayer::CTH_HANDICAPCHANGE:
            EventPlayerChangeHandicap(player, chatPlayer->GetByte());
            break;
          default:
            break;
        }
      }
    }
  }
}

void CGame::EventPlayerChangeTeam(CGamePlayer* player, uint8_t team)
{
  // player is requesting a team change

  if (team > m_Aura->m_MaxSlots) {
    return;
  }

  if (team == m_Aura->m_MaxSlots) {
    if (m_Map->GetMapObservers() != MAPOBS_ALLOWED && m_Map->GetMapObservers() != MAPOBS_REFEREES) {
      return;
    }
  } else if (team >= m_Map->GetMapNumTeams()) {
    return;
  }

  uint8_t SID = GetSIDFromPID(player->GetPID());
  const CGameSlot* slot = InspectSlot(SID);
  if (!slot) {
    return;
  }

  if (team == slot->GetTeam()) {
    if (!SwapEmptyAllySlot(SID)) return;
  } else if (m_CustomLayout & CUSTOM_LAYOUT_LOCKTEAMS) {
    if (m_IsDraftMode) {
      SendChat(player, "This lobby has draft mode enabled. Only team captains may assign players.");
    } else {
      switch (m_CustomLayout) {
        case CUSTOM_LAYOUT_ONE_VS_ALL:
        SendChat(player, "This is a One-VS-All lobby. You may not switch to another team.");
          break;
        case CUSTOM_LAYOUT_HUMANS_VS_AI:
          SendChat(player, "This is a humans VS AI lobby. You may not switch to another team.");
          break;
        case CUSTOM_LAYOUT_FFA:
          SendChat(player, "This is a free-for-all lobby. You may not switch to another team.");
          break;
        default:
          SendChat(player, "This lobby has a custom teams layout. You may not switch to another team.");
          break;
      }
    } 
  } else {
    SetSlotTeam(GetSIDFromPID(player->GetPID()), team, false);
  }
}

void CGame::EventPlayerChangeColor(CGamePlayer* player, uint8_t colour)
{
  // player is requesting a colour change

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
    // player should directly choose a different slot instead.
    return;
  }

  if (colour >= m_Aura->m_MaxSlots) {
    return;
  }

  uint8_t SID = GetSIDFromPID(player->GetPID());

  if (SID < m_Slots.size()) {
    // make sure the player isn't an observer

    if (m_Slots[SID].GetTeam() == m_Aura->m_MaxSlots) {
      return;
    }

    if (!SetSlotColor(SID, colour, false)) {
      Print(GetLogPrefix() + player->GetName() + " failed to switch to color " + to_string(static_cast<uint16_t>(colour)));
    }
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

  CGameSlot* slot = GetSlot(GetSIDFromPID(player->GetPID()));
  if (slot) {
    slot->SetRace(race | SLOTRACE_SELECTABLE);
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }
}

void CGame::EventPlayerChangeHandicap(CGamePlayer* player, uint8_t handicap)
{
  // player is requesting a handicap change

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS)
    return;

  if (handicap != 50 && handicap != 60 && handicap != 70 && handicap != 80 && handicap != 90 && handicap != 100)
    return;

  CGameSlot* slot = GetSlot(GetSIDFromPID(player->GetPID()));
  if (slot) {
    slot->SetHandicap(handicap);
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
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
  uint32_t MaxUploadSize = m_Aura->m_Net->m_Config->m_MaxUploadSize;
  if (JoinedRealm)
    MaxUploadSize = JoinedRealm->GetMaxUploadSize();

  if (mapSize->GetSizeFlag() != 1 || mapSize->GetMapSize() != MapSize) {
    // the player doesn't have the map

    string* MapData = m_Map->GetMapData();
    bool IsMapAvailable = !MapData->empty() && !m_Map->HasMismatch();
    bool IsMapTooLarge = MapSize > MaxUploadSize * 1024;
    if (IsMapAvailable && m_Aura->m_Net->m_Config->m_AllowTransfers != MAP_TRANSFERS_NEVER && (player->GetDownloadAllowed() || (m_Aura->m_Net->m_Config->m_AllowTransfers == MAP_TRANSFERS_AUTOMATIC && !IsMapTooLarge))) {
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
        if (m_Aura->m_Net->m_Config->m_AllowTransfers != MAP_TRANSFERS_AUTOMATIC) {
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
        if (GetMapSiteURL().empty()) {
          SendChat(player, "" + player->GetName() + ", please download the map before joining. (Kick in " + to_string(m_LacksMapKickDelay) + " seconds...)");
        } else {
          SendChat(player, "" + player->GetName() + ", please download the map from <" + GetMapSiteURL() + "> before joining. (Kick in " + to_string(m_LacksMapKickDelay) + " seconds...)");
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
      if (!player->GetIsObserver())
        ++m_ControllersWithMap;
    }
  } else if (!player->GetHasMap()) {
    player->SetHasMap(true);
    if (!player->GetIsObserver())
      ++m_ControllersWithMap;
  }

  uint8_t       NewDownloadStatus = static_cast<uint8_t>(static_cast<float>(mapSize->GetMapSize()) / MapSize * 100.f);
  if (NewDownloadStatus > 100) {
    NewDownloadStatus = 100;
  }

  CGameSlot* slot = GetSlot(GetSIDFromPID(player->GetPID()));
  if (slot && slot->GetDownloadStatus() != NewDownloadStatus) {
    // only send the slot info if the download status changed
    slot->SetDownloadStatus(NewDownloadStatus);

    // we don't actually send the new slot info here
    // this is an optimization because it's possible for a player to download a map very quickly
    // if we send a new slot update for every percentage change in their download status it adds up to a lot of data
    // instead, we mark the slot info as "out of date" and update it only once in awhile
    // (once per second when this comment was made)

    m_SlotInfoChanged |= (SLOTS_DOWNLOAD_PROGRESS_CHANGED);
  }
}

void CGame::EventPlayerPongToHost(CGamePlayer* player)
{
  if (m_GameLoading || m_GameLoaded || player->GetDeleteMe() || player->GetIsReserved()) {
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
      const uint8_t SID = GetSIDFromPID(player->GetPID());
      OpenSlot(SID, false);
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
  Print(GetLogPrefix() + "started loading with " + ToDecString(GetNumHumanPlayers()) + " players");

  if (m_UDPEnabled)
    SendGameDiscoveryDecreate();

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

  // Remove the virtual host player to ensure consistent game state and networking.
  DeleteVirtualHost();

  if (m_RestoredGame) {
    const uint8_t activePlayers = static_cast<uint8_t>(GetNumHumanOrFakeControllers()); // though it shouldn't be possible to manually add fake players
    const uint8_t expectedPlayers = m_RestoredGame->GetNumHumanSlots();
    if (activePlayers < expectedPlayers) {
      if (m_IsAutoVirtualPlayers) {
        // Restored games do not allow custom fake players, so we should only reach this point with actual players joined.
        // This code path is triggered by !fp enable.
        const uint8_t addedCounter = FakeAllSlots();
        Print(GetLogPrefix() + "resuming " + to_string(expectedPlayers) + "-player game. " + to_string(addedCounter) + " virtual players added.");
      } else {
        Print(GetLogPrefix() + "resuming " + to_string(expectedPlayers) + "-player game. " + ToDecString(expectedPlayers - activePlayers) + " missing.");
      }
    }
  }

  if (GetNumHumanOrFakeControllers() >= 2) {
    /*
    // This is an attempt to "rename" a fake player into our virtual host.
    // Unfortunately, it makes game clients quit after the game loads.
    if (m_FakePlayers.size()) {
      DeleteFakePlayer(GetSIDFromPID(m_FakePlayers[m_FakePlayers.size() - 1]));
      CreateFakePlayer(true);
    }
    */
  } else if (!m_RestoredGame && GetSlotsOpen() > 0) {
    // Assign an available slot to our virtual host.
    // That makes it a fake player.
    if (m_Map->GetMapObservers() == MAPOBS_REFEREES) {
      CreateFakeObserver(true);
    } else if (m_IsAutoVirtualPlayers) {
      CreateFakePlayer(true);
    }
  } else {
    // This is a single-player game. Neither chat events nor bot commands will work.
    // Keeping the virtual host does no good - The game client then refuses to remain in the game.
  }

  // send a final slot info update for HCL, or in case there are pending updates
  if (m_SlotInfoChanged != 0)
    SendAllSlotInfo();

  m_GameLoading = true;

  // since we use a fake countdown to deal with leavers during countdown the COUNTDOWN_START and COUNTDOWN_END packets are sent in quick succession
  // send a start countdown packet

  SendAll(GetProtocol()->SEND_W3GS_COUNTDOWN_START());

  // send an end countdown packet

  SendAll(GetProtocol()->SEND_W3GS_COUNTDOWN_END());

  // send a game loaded packet for any fake players

  for (auto& fakePlayer : m_FakePlayers)
    SendAll(GetProtocol()->SEND_W3GS_GAMELOADED_OTHERS(static_cast<uint8_t>(fakePlayer)));

  // record the number of starting players

  m_StartPlayers = GetNumHumanPlayers() + static_cast<uint8_t>(m_FakePlayers.size());

  // enable stats

  if (!m_RestoredGame && m_Map->GetMapType() == "dota") {
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
    for (auto& connection : pair.second)
      delete connection;

    pair.second.clear();
  }

  // delete the map data
  ReleaseMap();

  // move the game to the games in progress vector

  m_Aura->m_CurrentLobby = nullptr;
  m_Aura->m_Games.push_back(this);

  // and finally reenter battle.net chat

  for (auto& realm : m_Aura->m_Realms) {
    if (m_IsMirror && realm->GetIsMirror())
      continue;

    realm->ResetGameAnnouncement();
    realm->QueueGameUncreate();
    realm->SendEnterChat();
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
  Print(GetLogPrefix() + "finished loading with " + ToDecString(GetNumHumanPlayers()) + " players");

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

  if (GetNumHumanOrFakeControllers() < 2) {
    SendAllChat("HINT: Single-player game detected. In-game commands will be DISABLED.");
    // TODO: This creates a lag spike client-side.
    StopPlayers("single-player game untracked", true);
  }
}

bool CGame::GetIsRemakeable()
{
  if (!m_Map || m_RestoredGame) {
    return false;
  }
  return true;
}

void CGame::Remake()
{
  Reset(false);

  int64_t Time = GetTime();
  int64_t Ticks = GetTicks();

  m_GameTicks = 0;
  m_CreationTime = Time;
  m_LastPingTime = Time;
  m_LastRefreshTime = Time;
  m_LastDownloadTicks = Time;
  m_LastDownloadCounterResetTicks = Ticks;
  m_LastCountDownTicks = 0;
  m_StartedLoadingTicks = 0;
  m_FinishedLoadingTicks = 0;
  m_LastActionSentTicks = 0;
  m_LastActionLateBy = 0;
  m_StartedLaggingTime = 0;
  m_LastLagScreenTime = 0;
  m_LastOwnerSeen = Time;
  m_StartedKickVoteTime = 0;
  m_GameOverTime = 0;
  m_LastPlayerLeaveTicks = 0;
  m_LastLagScreenResetTime = 0;
  m_PauseCounter = 0;
  m_SaveCounter = 0;
  m_SyncCounter = 0;

  m_DownloadCounter = 0;
  m_CountDownCounter = 0;
  m_StartPlayers = 0;
  m_AutoStartMinTime = 0;
  m_AutoStartMaxTime = 0;
  m_AutoStartPlayers = 0;
  m_ControllersWithMap = 0;
  m_CustomLayout = 0;

  m_IsAutoVirtualPlayers = false;
  m_VirtualHostPID = 0xFF;
  m_SlotInfoChanged = 0;
  m_PublicStart = false;
  m_Locked = false;
  m_RealmRefreshError = false;
  m_CountDownStarted = false;
  m_GameLoading = false;
  m_GameLoaded = false;
  m_LobbyLoading = true;
  m_Lagging = false;
  m_Desynced = false;
  m_IsDraftMode = false;
  m_HadLeaver = false;
  m_HasMapLock = false;
  m_UsesCustomReferees = false;
  m_SentPriorityWhois = false;

  m_HostCounter = m_Aura->NextHostCounter();
  InitPRNG();
  InitSlots();

  m_KickVotePlayer.clear();

  m_Aura->m_CurrentLobby = this;
}

uint8_t CGame::GetSIDFromPID(uint8_t PID) const
{
  if (m_Slots.size() > 0xFF)
    return 0xFF;

  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetPID() == PID)
      return i;
  }

  return 0xFF;
}

CGamePlayer* CGame::GetPlayerFromPID(uint8_t PID) const
{
  for (auto& player : m_Players) {
    if (!player->GetLeftMessageSent() && player->GetPID() == PID)
      return player;
  }

  return nullptr;
}

CGamePlayer* CGame::GetPlayerFromSID(uint8_t SID) const
{
  if (SID >= static_cast<uint8_t>(m_Slots.size()))
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
  return MaybeOwner->GetIsOwner(nullopt);
}

CGamePlayer* CGame::GetPlayerFromName(string name, bool sensitive) const
{
  if (!sensitive)
    transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });

  for (auto& player : m_Players)
  {
    if (!player->GetDeleteMe())
    {
      string TestName = player->GetName();

      if (!sensitive)
        transform(begin(TestName), end(TestName), begin(TestName), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (TestName == name)
        return player;
    }
  }

  return nullptr;
}

uint8_t CGame::GetPlayerFromNamePartial(string name, CGamePlayer*& player) const
{
  uint8_t Matches = 0;
  if (name.empty()) return Matches;

  transform(begin(name), end(name), begin(name), [](char c) { return static_cast<char>(std::tolower(c)); });

  // try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")

  for (auto& realplayer : m_Players)
  {
    if (!realplayer->GetDeleteMe())
    {
      string TestName = realplayer->GetName();
      transform(begin(TestName), end(TestName), begin(TestName), [](char c) { return static_cast<char>(std::tolower(c)); });

      if (TestName.find(name) != string::npos)
      {
        ++Matches;
        player = realplayer;

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

string CGame::GetDBPlayerNameFromColor(uint8_t colour) const
{
  for (const auto& player : m_DBGamePlayers)
  {
    if (player->GetColor() == colour)
      return player->GetName();
  }

  return string();
}

CGamePlayer* CGame::GetPlayerFromColor(uint8_t colour) const
{
  for (uint8_t i = 0; i < m_Slots.size(); ++i)
  {
    if (m_Slots[i].GetColor() == colour)
      return GetPlayerFromSID(i);
  }

  return nullptr;
}

uint8_t CGame::GetNewPID() const
{
  // find an unused PID for a new player to use

  for (uint8_t TestPID = 1; TestPID < 0xFF; ++TestPID)
  {
    if (TestPID == m_VirtualHostPID)
      continue;

    bool InUse = false;

    for (const uint16_t fakePlayer : m_FakePlayers) {
      if (static_cast<uint8_t>(fakePlayer) == TestPID) {
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

  return 0xFF;
}

uint8_t CGame::GetNewTeam() const
{
  bitset<MAX_SLOTS_MODERN> usedTeams;
  for (auto& slot : m_Slots) {
    if (slot.GetColor() == m_Aura->m_MaxSlots) continue;
    if (slot.GetSlotStatus() != SLOTSTATUS_OCCUPIED) continue;
    usedTeams.set(slot.GetTeam());
  }
  const uint8_t endTeam = m_Map->GetMapNumTeams();
  for (uint8_t team = 0; team < endTeam; ++team) {
    if (!usedTeams.test(team)) {
      return team;
    }
  }
  return m_Aura->m_MaxSlots;
}

uint8_t CGame::GetNewColor() const
{
  bitset<MAX_SLOTS_MODERN> usedColors;
  for (auto& slot : m_Slots) {
    if (slot.GetColor() == m_Aura->m_MaxSlots) continue;
    usedColors.set(slot.GetColor());
  }
  for (uint8_t color = 0; color < m_Aura->m_MaxSlots; ++color) {
    if (!usedColors.test(color)) {
      return color;
    }
  }
  return m_Aura->m_MaxSlots; // should never happen
}

uint8_t CGame::SimulateActionPID(const uint8_t actionType, CGamePlayer* player, const bool isDisconnect)
{
  // Note that the game client desyncs if the PID of an actual player is used.
  switch (actionType) {
    case ACTION_PAUSE: {
      if (player && isDisconnect && !player->GetLeftMessageSent() && player->GetCanPause()) {
        return player->GetPID();
      }
      if (m_PauseCounter < m_FakePlayers.size() * 3) {
        return static_cast<uint8_t>(m_FakePlayers[m_PauseCounter++ % m_FakePlayers.size()]);
      }
      return 0xFF;
    }
    case ACTION_RESUME: {
      if (!m_FakePlayers.empty()) {
        return static_cast<uint8_t>(m_FakePlayers[m_FakePlayers.size() - 1]);
      }
      return 0xFF;
    }

    case ACTION_SAVE: {
      if (player && isDisconnect && !player->GetLeftMessageSent() && !player->GetSaved()) {
        return player->GetPID();
      }
      if (m_SaveCounter < m_FakePlayers.size()) {
        return static_cast<uint8_t>(m_FakePlayers[m_SaveCounter++]);
      }
      return 0xFF;
    }

    default: {
      return 0xFF;
    }
  }
}

bool CGame::GetHasAnyActiveTeam() const
{
  bitset<MAX_SLOTS_MODERN> usedTeams;
  for (const auto& slot : m_Slots) {
    const uint8_t team = slot.GetTeam();
    if (team == m_Aura->m_MaxSlots) continue;
    if (slot.GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
      if (usedTeams.test(team)) {
        return true;
      } else {
        usedTeams.set(team);
      }
    }
  }
  return false;
}

bool CGame::GetHasAnyPlayer() const
{
  if (m_Players.empty()) {
    return false;
  }

  for (const auto& player : m_Players) {
    if (!player->GetDeleteMe()) {
      return true;
    }
  }
  return false;
}

bool CGame::GetIsPlayerSlot(const uint8_t SID) const
{
  const CGameSlot* slot = InspectSlot(SID);
  if (!slot || !slot->GetIsPlayerOrFake()) return false;
  const CGamePlayer* player = GetPlayerFromSID(SID);
  if (player == nullptr) return false;
  return !player->GetDeleteMe();
}

bool CGame::GetHasAnotherPlayer(const uint8_t ExceptSID) const
{
  uint8_t SID = ExceptSID;
  do {
    SID = (SID + 1) % m_Slots.size();
  } while (!GetIsPlayerSlot(SID) && SID != ExceptSID);
  return SID != ExceptSID;
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

  if (m_VirtualHostPID != 0xFF)
    return m_VirtualHostPID;

  // try to find the fakeplayer next

  if (!m_FakePlayers.empty())
    return static_cast<uint8_t>(m_FakePlayers[m_FakePlayers.size() - 1]);

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

  return 0xFF;
}

CGameSlot* CGame::GetSlot(const uint8_t SID)
{
  if (SID > m_Slots.size()) return nullptr;
  return &(m_Slots[SID]);
}

const CGameSlot* CGame::InspectSlot(const uint8_t SID) const
{
  if (SID > m_Slots.size()) return nullptr;
  return &(m_Slots[SID]);
}

uint8_t CGame::GetEmptySID(bool reserved) const
{
  if (m_Slots.size() > 0xFF)
    return 0xFF;

  // look for an empty slot for a new player to occupy
  // if reserved is true then we're willing to use closed or occupied slots as long as it wouldn't displace a player with a reserved slot

  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetSlotStatus() != SLOTSTATUS_OPEN) {
      continue;
    }
    return i;
  }

  if (reserved)
  {
    // no empty slots, but since player is reserved give them a closed slot

    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_CLOSED) {
        return i;
      }
    }

    // no closed slots either, give them an occupied slot but not one occupied by another reserved player
    // first look for a player who is downloading the map and has the least amount downloaded so far

    uint8_t LeastSID = 0xFF;
    uint8_t LeastDownloaded = 100;

    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      CGamePlayer* Player = GetPlayerFromSID(i);

      if (Player && !Player->GetIsReserved() && m_Slots[i].GetDownloadStatus() < LeastDownloaded) {
        LeastSID = i;
        LeastDownloaded = m_Slots[i].GetDownloadStatus();
      }
    }

    if (LeastSID != 0xFF) {
      return LeastSID;
    }

    // nobody who isn't reserved is downloading the map, just choose the first player who isn't reserved

    for (uint8_t i = 0; i < m_Slots.size(); ++i) {
      CGamePlayer* Player = GetPlayerFromSID(i);

      if (Player && !Player->GetIsReserved()) {
        return i;
      }
    }
  }

  return 0xFF;
}

uint8_t CGame::GetEmptySID(uint8_t team, uint8_t PID) const
{
  if (m_Slots.size() > 0xFF) {
    return 0xFF;
  }

  // find an empty slot based on player's current slot

  uint8_t StartSlot = GetSIDFromPID(PID);

  if (StartSlot < m_Slots.size()) {
    if (m_Slots[StartSlot].GetTeam() != team) {
      // player is trying to move to another team so start looking from the first slot on that team
      // we actually just start looking from the very first slot since the next few loops will check the team for us

      StartSlot = 0;
    }

    // find an empty slot on the correct team starting from StartSlot

    for (uint8_t i = StartSlot; i < m_Slots.size(); ++i) {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team)
        return i;
    }

    // didn't find an empty slot, but we could have missed one with SID < StartSlot
    // e.g. in the DotA case where I am in slot 4 (yellow), slot 5 (orange) is occupied, and slot 1 (blue) is open and I am trying to move to another slot

    for (uint8_t i = 0; i < StartSlot; ++i) {
      if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == team) {
        return i;
      }
    }
  }

  return 0xFF;
}

uint8_t CGame::GetEmptyObserverSID() const
{
  if (m_Slots.size() > 0xFF)
    return 0xFF;

  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    if (m_Slots[i].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[i].GetTeam() == m_Aura->m_MaxSlots) {
      return i;
    }
  }

  return 0xFF;
}

bool CGame::SwapEmptyAllySlot(const uint8_t SID)
{
  if (!GetIsCustomForces()) {
    return false;
  }
  const uint8_t team = m_Slots[SID].GetTeam();

  // Look for the next ally, starting from the current SID, and wrapping over.
  uint8_t allySID = SID;
  do {
    allySID = (allySID + 1) % m_Slots.size();
  } while (allySID != SID && !(m_Slots[allySID].GetTeam() == team && m_Slots[allySID].GetSlotStatus() == SLOTSTATUS_OPEN));

  if (allySID == SID) {
    return false;
  }
  return SwapSlots(SID, allySID);
}

bool CGame::SwapSlots(const uint8_t SID1, const uint8_t SID2)
{
  if (SID1 >= static_cast<uint8_t>(m_Slots.size()) || SID2 >= static_cast<uint8_t>(m_Slots.size()) || SID1 == SID2) {
    return false;
  }

  CGameSlot Slot1 = m_Slots[SID1];
  CGameSlot Slot2 = m_Slots[SID2];

  if (!Slot1.GetIsSelectable() || !Slot2.GetIsSelectable()) {
    return false;
  }

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
    // don't swap the team, colour, race, or handicap
    m_Slots[SID1] = CGameSlot(Slot2.GetPID(), Slot2.GetDownloadStatus(), Slot2.GetSlotStatus(), Slot2.GetComputer(), Slot1.GetTeam(), Slot1.GetColor(), Slot1.GetRace(), Slot2.GetComputerType(), Slot1.GetHandicap());
    m_Slots[SID2] = CGameSlot(Slot1.GetPID(), Slot1.GetDownloadStatus(), Slot1.GetSlotStatus(), Slot1.GetComputer(), Slot2.GetTeam(), Slot2.GetColor(), Slot2.GetRace(), Slot1.GetComputerType(), Slot2.GetHandicap());
  } else {
    // swap everything
    if (GetIsCustomForces()) {
      // except if custom forces is set, then we don't swap teams...
      Slot1.SetTeam(m_Slots[SID2].GetTeam());
      Slot2.SetTeam(m_Slots[SID1].GetTeam());
    }

    m_Slots[SID1] = Slot2;
    m_Slots[SID2] = Slot1;
  }

  CGamePlayer* PlayerOne = GetPlayerFromSID(SID1);
  CGamePlayer* PlayerTwo = GetPlayerFromSID(SID2);
  if (PlayerOne) {
    PlayerOne->SetObserver(Slot2.GetTeam() == m_Aura->m_MaxSlots);
    if (PlayerOne->GetIsObserver())
      PlayerOne->SetPowerObserver(PlayerOne->GetIsObserver() && m_Map->GetMapObservers() == MAPOBS_REFEREES);
  }
  if (PlayerTwo) {
    PlayerTwo->SetObserver(Slot1.GetTeam() == m_Aura->m_MaxSlots);
    if (PlayerTwo->GetIsObserver())
      PlayerTwo->SetPowerObserver(PlayerTwo->GetIsObserver() && m_Map->GetMapObservers() == MAPOBS_REFEREES);
  }

  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  return true;
}

bool CGame::OpenSlot(const uint8_t SID, const bool kick)
{
  const CGameSlot* slot = InspectSlot(SID);
  if (!slot || !slot->GetIsSelectable()) {
    return false;
  }

  CGamePlayer* player = GetPlayerFromSID(SID);
  if (player && !player->GetDeleteMe()) {
    if (!kick) return false;
    player->SetDeleteMe(true);
    player->SetLeftReason("was kicked when opening a slot");
    player->SetLeftCode(PLAYERLEAVE_LOBBY);
  } else if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) {
    ResetLayout(false);
  }
  if (player && m_CustomLayout == CUSTOM_LAYOUT_ONE_VS_ALL && slot->GetTeam() == m_CustomLayoutData.first) {
    ResetLayout(false);
  }
  if (!player && slot->GetIsPlayerOrFake()) {
    DeleteFakePlayer(SID);
  }
  if (GetIsCustomForces()) {
    m_Slots[SID] = CGameSlot(0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, slot->GetTeam(), slot->GetColor(), m_Map->GetLobbyRace(slot));
  } else {
    m_Slots[SID] = CGameSlot(0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, SLOTRACE_RANDOM);
  }
  if (player && !GetHasAnotherPlayer(SID)) {
    EventLobbyLastPlayerLeaves();
  }
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  return true;
}

bool CGame::OpenSlot()
{
  uint8_t SID = 0;
  while (SID < m_Slots.size()) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_CLOSED) {
      return OpenSlot(SID, false);
    }
    ++SID;
  }
  return false;
}

bool CGame::CanLockSlotForJoins(const uint8_t SID)
{
  CGameSlot* slot = GetSlot(SID);
  if (!slot || !slot->GetIsSelectable()) {
    return false;
  }
  if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) {
    // Changing a closed slot for anything doesn't decrease the
    // amount of slots available for humans.
    return true;
  }
  const uint8_t openSlots = static_cast<uint8_t>(GetSlotsOpen());
  if (openSlots >= 2) {
    return true;
  }
  if (slot->GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
    if (openSlots >= 1) return true;
    return GetHasAnotherPlayer(SID);
  }

  return GetHasAnyPlayer();
}

bool CGame::CloseSlot(const uint8_t SID, const bool kick)
{
  if (!CanLockSlotForJoins(SID)) {
    return false;
  }
  const CGameSlot* slot = InspectSlot(SID);
  const uint8_t openSlots = static_cast<uint8_t>(GetSlotsOpen());
  CGamePlayer* player = GetPlayerFromSID(SID);
  if (player && !player->GetDeleteMe()) {
    if (!kick) return false;
    player->SetDeleteMe(true);
    player->SetLeftReason("was kicked when closing a slot");
    player->SetLeftCode(PLAYERLEAVE_LOBBY);
  }
  if (slot->GetSlotStatus() == SLOTSTATUS_OPEN && openSlots == 1 && GetNumHumanOrFakeControllers() > 1) {
    DeleteVirtualHost();
  }
  if (!player && slot->GetIsPlayerOrFake()) {
    DeleteFakePlayer(SID);
  }

  if (GetIsCustomForces()) {
    m_Slots[SID] = CGameSlot(0, SLOTPROG_RST, SLOTSTATUS_CLOSED, SLOTCOMP_NO, slot->GetTeam(), slot->GetColor(), m_Map->GetLobbyRace(slot));
  } else {
    m_Slots[SID] = CGameSlot(0, SLOTPROG_RST, SLOTSTATUS_CLOSED, SLOTCOMP_NO, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, SLOTRACE_RANDOM);
  }
  
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  return true;
}

bool CGame::CloseSlot()
{
  uint8_t SID = 0;
  while (SID < m_Slots.size()) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN) {
      return CloseSlot(SID, false);
    }
    ++SID;
  }
  return false;
}

bool CGame::ComputerSlot(uint8_t SID, uint8_t skill, bool kick)
{
  if (SID >= static_cast<uint8_t>(m_Slots.size()) || skill > SLOTCOMP_HARD) {
    return false;
  }

  CGameSlot Slot = m_Slots[SID];
  if (!Slot.GetIsSelectable()) {
    return false;
  }
  if (Slot.GetSlotStatus() != SLOTSTATUS_OCCUPIED && GetNumControllers() == m_Map->GetMapNumControllers()) {
    return false;
  }
  if (Slot.GetTeam() == m_Aura->m_MaxSlots) {
    if (GetIsCustomForces()) {
      return false;
    }
  }
  if (!CanLockSlotForJoins(SID)) {
    return false;
  }
  CGamePlayer* Player = GetPlayerFromSID(SID);
  if (Player && !Player->GetDeleteMe()) {
    if (!kick) return false;
    Player->SetDeleteMe(true);
    Player->SetLeftReason("was kicked when creating a computer in a slot");
    Player->SetLeftCode(PLAYERLEAVE_LOBBY);
  }

  // ignore layout, override computers
  if (ComputerSlotInner(SID, skill, true, true)) {
    if (GetSlotsOpen() == 0 && GetNumHumanOrFakeControllers() > 1) DeleteVirtualHost();
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }
  return true;
}

bool CGame::SetSlotTeam(const uint8_t SID, const uint8_t team, const bool force)
{
  CGameSlot* slot = GetSlot(SID);
  if (!slot || slot->GetTeam() == team || !slot->GetIsSelectable()) {
    return false;
  }
  if (GetIsCustomForces()) {
    const uint8_t newSID = GetSelectableTeamSlotFront(team, static_cast<uint8_t>(m_Slots.size()), static_cast<uint8_t>(m_Slots.size()), force);
    if (newSID == 0xFF) return false;
    return SwapSlots(SID, newSID);
  } else {
    const bool fromObservers = slot->GetTeam() == m_Aura->m_MaxSlots;
    const bool toObservers = team == m_Aura->m_MaxSlots;
    if (toObservers && !slot->GetIsPlayerOrFake()) return false;
    if (fromObservers && !toObservers && GetNumControllers() >= m_Map->GetMapNumControllers()) {
      // Observer cannot become controller if the map's controller limit has been reached.
      return false;
    }

    slot->SetTeam(team);
    if (toObservers || fromObservers) {
      if (toObservers) {
        slot->SetColor(m_Aura->m_MaxSlots);
        slot->SetRace(SLOTRACE_RANDOM);
      } else {
        slot->SetColor(GetNewColor());
        if (m_Map->GetMapFlags() & MAPFLAG_RANDOMRACES) {
          slot->SetRace(SLOTRACE_RANDOM);
        } else {
          slot->SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
        }
      }

      CGamePlayer* player = GetPlayerFromPID(slot->GetPID());
      if (player) {
        player->SetObserver(toObservers);
        if (toObservers) {
          player->SetPowerObserver(!m_UsesCustomReferees && m_Map->GetMapObservers() == MAPOBS_REFEREES);
        } else {
          player->SetPowerObserver(false);
        }
      }
    }

    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
    return true;
  }
}

bool CGame::SetSlotColor(const uint8_t SID, const uint8_t colour, const bool force)
{
  CGameSlot* slot = GetSlot(SID);
  if (!slot || slot->GetColor() == colour || !slot->GetIsSelectable()) {
    return false;
  }

  if (slot->GetSlotStatus() != SLOTSTATUS_OCCUPIED || slot->GetTeam() == m_Aura->m_MaxSlots) {
    // Only allow active players to choose their colors.
    //
    // Open/closed slots do actually have a color when Fixed Player Settings is enabled,
    // but I'm still not providing API for it.
    return false;
  }

  CGameSlot* takenSlot = nullptr;
  uint8_t takenSID = 0xFF;

  // if the requested color is taken, try to exchange colors
  for (uint8_t i = 0; i < m_Slots.size(); ++i) {
    CGameSlot* matchSlot = &(m_Slots[i]);
    if (matchSlot->GetColor() != colour) continue;
    if (!matchSlot->GetIsSelectable()) {
      return false;
    }
    if (!force) {
      // player request - may only use the color of an unoccupied slot
      // closed slots are okay, too (note that they only have a valid color when using Custom Forces)
      if (matchSlot->GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
        return false;
      }
    }
    takenSlot = matchSlot;
    takenSID = i;
    break;
  }

  if (m_Map->GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS) {
    // With fixed player settings we try to swap slots.
    // This is not exposed to EventPlayerChangeColor,
    // but it's useful for !color.
    //
    // Old: !swap 3 7
    // Now: !color Arthas, teal
    if (!takenSlot) {
      // But we found no slot to swap with.
      return false;
    } else {
      SwapSlots(SID, takenSID); // Guaranteed to succeed at this point;
      m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
      return true;
    }
  } else {
    if (takenSlot) takenSlot->SetColor(m_Slots[SID].GetColor());
    m_Slots[SID].SetColor(colour);
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
    return true;
  }
}

void CGame::SetSlotTeamAndColorAuto(const uint8_t SID)
{
  // Custom Forces must use m_Slots[SID].GetColor() / m_Slots[SID].GetTeam()
  if (GetLayout() != MAPLAYOUT_ANY) return;
  CGameSlot* slot = GetSlot(SID);
  if (!slot) return;
  if (GetNumControllers() >= m_Map->GetMapNumControllers()) {
    return;
  }
  switch (GetCustomLayout()) {
    case CUSTOM_LAYOUT_ONE_VS_ALL:
      slot->SetTeam(m_CustomLayoutData.second);
      break;
    case CUSTOM_LAYOUT_HUMANS_VS_AI:
      if (slot->GetIsPlayerOrFake()) {
        slot->SetTeam(m_CustomLayoutData.first);
      } else {
        slot->SetTeam(m_CustomLayoutData.second);
      }      
      break;
    case CUSTOM_LAYOUT_FFA:
      slot->SetTeam(GetNewTeam());
      break;
    case CUSTOM_LAYOUT_DRAFT:
      // Player remains as observer until someone picks them.
      break;
    default: {
      slot->SetTeam(SID % m_Map->GetMapNumTeams());
      break;
    }
  }
  slot->SetColor(GetNewColor());
}

void CGame::OpenAllSlots()
{
  bool anyChanged = false;
  for (auto& slot : m_Slots) {
    if (slot.GetSlotStatus() == SLOTSTATUS_CLOSED) {
      slot.SetSlotStatus(SLOTSTATUS_OPEN);
      anyChanged = true;
    }
  }

  if (anyChanged) {
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }
}

uint8_t CGame::GetFirstCloseableSlot()
{
  bool hasPlayer = false;
  uint8_t firstSID = 0xFF;
  for (uint8_t SID = 0; SID < m_Slots.size(); ++SID) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN) {
      if (firstSID == 0xFF) firstSID = SID + 1;
      if (hasPlayer) break;
    } else if (GetIsPlayerSlot(SID)) {
      hasPlayer = true;
      if (firstSID != 0xFF) break;
    }
  }

  if (hasPlayer) return 0;
  return firstSID;
}

bool CGame::CloseAllTeamSlots(const uint8_t team)
{
  const uint8_t firstSID = GetFirstCloseableSlot();
  if (firstSID == 0xFF) return false;

  bool anyChanged = false;
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  while (firstSID < SID--) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN && m_Slots[SID].GetTeam() == team) {
      m_Slots[SID].SetSlotStatus(SLOTSTATUS_CLOSED);
      anyChanged = true;
    }
  }

  if (anyChanged) {
    if (GetNumHumanOrFakeControllers() > 1)
      DeleteVirtualHost();
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }

  return anyChanged;
}

bool CGame::CloseAllTeamSlots(const bitset<MAX_SLOTS_MODERN> occupiedTeams)
{
  if (!GetIsCustomForces()) return false;
  const uint8_t firstSID = GetFirstCloseableSlot();
  if (firstSID == 0xFF) return false;

  bool anyChanged = false;
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  while (firstSID < SID--) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN && occupiedTeams.test(m_Slots[SID].GetTeam())) {
      m_Slots[SID].SetSlotStatus(SLOTSTATUS_CLOSED);
      anyChanged = true;
    }
  }

  if (anyChanged) {
    if (GetNumHumanOrFakeControllers() > 1)
      DeleteVirtualHost();
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }

  return anyChanged;
}

bool CGame::CloseAllSlots()
{
  const uint8_t firstSID = GetFirstCloseableSlot();
  if (firstSID == 0xFF) return false;

  bool anyChanged = false;
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  while (firstSID < SID--) {
    if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN) {
      m_Slots[SID].SetSlotStatus(SLOTSTATUS_CLOSED);
      anyChanged = true;
    }
  }

  if (anyChanged) {
    if (GetNumHumanOrFakeControllers() > 1)
      DeleteVirtualHost();
    m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
  }

  return anyChanged;
}

bool CGame::ComputerSlotInner(const uint8_t SID, const uint8_t skill, const bool ignoreLayout, const bool overrideComputers)
{
  const CGameSlot* slot = InspectSlot(SID);
  if ((!ignoreLayout || GetIsPlayerSlot(SID)) && slot->GetSlotStatus() == SLOTSTATUS_OCCUPIED) {
    return false;
  }
  if (!overrideComputers && slot->GetIsComputer()) {
    return false;
  }

  // !comp NUMBER bypasses current layout, so it may
  // add computers in closed slots (in regular layouts), or
  // in open slots (in HUMANS_VS_AI)
  // if it does, reset the layout
  bool resetLayout = false;
  if (m_CustomLayout == CUSTOM_LAYOUT_HUMANS_VS_AI) {
    if (slot->GetSlotStatus() == SLOTSTATUS_OPEN || (GetIsCustomForces() && slot->GetTeam() != m_CustomLayoutData.second)) {
      if (ignoreLayout) {
        resetLayout = true;
      } else {
        return false;
      }
    }
  } else {
    if (slot->GetSlotStatus() == SLOTSTATUS_CLOSED) {
      if (!ignoreLayout) {
        return false;
      }
    }
  }
  if (GetIsCustomForces()) {
    if (slot->GetTeam() == m_Aura->m_MaxSlots) {
      return false;
    }
    if (slot->GetIsPlayerOrFake()) DeleteFakePlayer(SID);
    m_Slots[SID] = CGameSlot(0, SLOTPROG_RDY, SLOTSTATUS_OCCUPIED, SLOTCOMP_YES, slot->GetTeam(), slot->GetColor(), m_Map->GetLobbyRace(slot), skill);
    if (resetLayout) ResetLayout(false);
  } else {
    if (slot->GetIsPlayerOrFake()) DeleteFakePlayer(SID);
    m_Slots[SID] = CGameSlot(0, SLOTPROG_RDY, SLOTSTATUS_OCCUPIED, SLOTCOMP_YES, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, m_Map->GetLobbyRace(slot), skill);
    SetSlotTeamAndColorAuto(SID);
  }
  return true;
}

bool CGame::ComputerNSlots(const uint8_t skill, const uint8_t expectedCount, const bool ignoreLayout, const bool overrideComputers)
{
  uint8_t currentCount = GetNumComputers();
  if (expectedCount == currentCount) {
    // noop
    return true;
  }

  if (expectedCount < currentCount) {
    uint8_t SID = static_cast<uint8_t>(m_Slots.size());
    while (SID--) {
      if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetIsComputer()) {
        if (OpenSlot(SID, false) && --currentCount == expectedCount) {
          if (m_CustomLayout == CUSTOM_LAYOUT_HUMANS_VS_AI && currentCount == 0) ResetLayout(false);
          return true;
        }
      }
    }
    return false;
  }

  if (m_Map->GetMapNumControllers() <= GetNumControllers()) {
    return false;
  }

  const bool hasPlayers = GetHasAnyPlayer(); // Ensure this is called outside the loop.
  uint8_t remainingControllers = m_Map->GetMapNumControllers() - GetNumControllers();
  if (!hasPlayers) --remainingControllers; // Refuse to lock the last slot
  if (expectedCount - currentCount > remainingControllers) {
    return false;
  }
  uint8_t remainingComputers = overrideComputers ? expectedCount : (expectedCount - currentCount);
  uint8_t SID = 0;
  while (0 < remainingComputers && SID < m_Slots.size()) {
    // overrideComputers false means only newly added computers are counted in remainingComputers
    if (ComputerSlotInner(SID, skill, ignoreLayout, overrideComputers)) {
      --remainingComputers;
    }
    ++SID;
  }

  if (GetSlotsOpen() == 0 && GetNumHumanOrFakeControllers() > 1) DeleteVirtualHost();
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);

  return remainingComputers == 0;
}

bool CGame::ComputerAllSlots(const uint8_t skill)
{
  if (m_Map->GetMapNumControllers() <= GetNumControllers()) {
    return false;
  }

  const bool hasPlayers = GetHasAnyPlayer(); // Ensure this is called outside the loop.
  uint32_t remainingSlots = m_Map->GetMapNumControllers() - GetNumControllers();

  // Refuse to lock the last slot
  if (!hasPlayers && m_Slots.size() == m_Map->GetMapNumControllers()) {
    --remainingSlots;
  }

  if (remainingSlots == 0) {
    return false;
  }

  uint8_t SID = 0;
  while (0 < remainingSlots && SID < m_Slots.size()) {
    // don't ignore layout, don't override computers
    if (ComputerSlotInner(SID, skill, false, false)) {
      --remainingSlots;
    }
    ++SID;
  }

  if (GetSlotsOpen() == 0 && GetNumHumanOrFakeControllers() > 1) DeleteVirtualHost();
  m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
  return true;
}

void CGame::ShuffleSlots()
{
  // we only want to shuffle the player slots (exclude observers)
  // that means we need to prevent this function from shuffling the open/closed/computer slots too
  // so we start by copying the player slots to a temporary vector

  vector<CGameSlot> PlayerSlots;

  for (auto& slot : m_Slots) {
    if (slot.GetIsPlayerOrFake() && slot.GetTeam() != m_Aura->m_MaxSlots) {
      PlayerSlots.push_back(slot);
    }
  }

  // now we shuffle PlayerSlots

  if (GetIsCustomForces())
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
      Slots.emplace_back(PlayerSlots[SIDs[i]].GetPID(), PlayerSlots[SIDs[i]].GetDownloadStatus(), PlayerSlots[SIDs[i]].GetSlotStatus(), PlayerSlots[SIDs[i]].GetComputer(), PlayerSlots[i].GetTeam(), PlayerSlots[i].GetColor(), PlayerSlots[i].GetRace());

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

  for (auto& slot : m_Slots) {
    if (slot.GetIsPlayerOrFake() && slot.GetTeam() != m_Aura->m_MaxSlots) {
      Slots.push_back(*CurrentPlayer);
      ++CurrentPlayer;
    } else {
      Slots.push_back(slot);
    }
  }

  m_Slots = Slots;
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
}

void CGame::ReportSpoofed(const string& server, CGamePlayer* player)
{
  SendAllChat("Name spoof detected. The real [" + player->GetName() + "@" + server + "] is not in this game.");
  if (GetIsLobby() && MatchOwnerName(player->GetName())) {
    player->SetDeleteMe(true);
    player->SetLeftReason("was kicked for spoofing the game owner");
    player->SetLeftCode(PLAYERLEAVE_LOBBY);
    const uint8_t SID = GetSIDFromPID(player->GetPID());
    OpenSlot(SID, false);
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

void CGame::AddToReserved(const string& name)
{
  string inputLower = name;
  if (m_RestoredGame && m_Reserved.size() >= m_Aura->m_MaxSlots) {
    return;
  }

  transform(begin(inputLower), end(inputLower), begin(inputLower), [](char c) { return static_cast<char>(std::tolower(c)); });

  // check that the user is not already reserved

  for (const auto& element : m_Reserved) {
    string matchLower = element;
    transform(begin(matchLower), end(matchLower), begin(matchLower), [](char c) { return static_cast<char>(std::tolower(c)); });
    if (matchLower == inputLower) {
      return;
    }
  }

  m_Reserved.push_back(name);

  // upgrade the user if they're already in the game

  for (auto& player : m_Players) {
    string matchLower = player->GetName();
    transform(begin(matchLower), end(matchLower), begin(matchLower), [](char c) { return static_cast<char>(std::tolower(c)); });

    if (matchLower == inputLower) {
      player->SetReserved(true);
      break;
    }
  }
}

void CGame::RemoveFromReserved(const string& name)
{
  if (m_Reserved.empty()) return;

  uint8_t index = GetReservedIndex(name);
  if (index == 0xFF) {
    return;
  }
  m_Reserved.erase(m_Reserved.begin() + index);

  // demote the user if they're already in the game
  CGamePlayer* matchPlayer = GetPlayerFromName(name, false);
  if (matchPlayer) {
    matchPlayer->SetReserved(false);
  }
}

void CGame::RemoveAllReserved()
{
  m_Reserved.clear();
  for (auto& player : m_Players) {
    player->SetReserved(false);
  }
}

bool CGame::MatchOwnerName(const string& name) const
{
  string matchLower = name;
  string ownerLower = m_OwnerName;
  transform(begin(matchLower), end(matchLower), begin(matchLower), [](char c) { return static_cast<char>(std::tolower(c)); });
  transform(begin(ownerLower), end(ownerLower), begin(ownerLower), [](char c) { return static_cast<char>(std::tolower(c)); });
  return matchLower == ownerLower;
}

uint8_t CGame::GetReservedIndex(const string& name) const
{
  string inputLower = name;
  transform(begin(inputLower), end(inputLower), begin(inputLower), [](char c) { return static_cast<char>(std::tolower(c)); });

  uint8_t index = 0;
  while (index < m_Reserved.size()) {
    string matchLower = m_Reserved[index];
    transform(begin(matchLower), end(matchLower), begin(matchLower), [](char c) { return static_cast<char>(std::tolower(c)); });
    if (matchLower == inputLower) {
      break;
    }
    ++index;
  }

  if (index == m_Reserved.size()) {
    return 0xFF;
  }

  return index;
}

vector<uint32_t> CGame::GetPlayersFramesBehind() const
{
  uint8_t i = static_cast<uint8_t>(m_Players.size());
  vector<uint32_t> framesBehind(i, 0);
  while (i--) {
    if (m_Players[i]->GetIsObserver()) {
      continue;
    }
    if (m_SyncCounter <= m_Players[i]->GetSyncCounter()) {
      continue;
    }
    framesBehind[i] = m_SyncCounter - m_Players[i]->GetSyncCounter();
  }
  return framesBehind;
}

vector<CGamePlayer*> CGame::GetLaggingPlayers() const
{
  vector<CGamePlayer*> laggingPlayers;
  if (!m_Lagging) return laggingPlayers;
  for (const auto& player : m_Players) {
    if (!player->GetLagging()) {
      continue;
    }
    laggingPlayers.push_back(player);
  }
  return laggingPlayers;
}

vector<CGamePlayer*> CGame::CalculateNewLaggingPlayers() const
{
  vector<CGamePlayer*> laggingPlayers;
  if (!m_Lagging) return laggingPlayers;
  for (const auto& player : m_Players) {
    if (player->GetIsObserver()) {
      continue;
    }
    if (player->GetLagging() || player->GetGProxyDisconnectNoticeSent()) {
      continue;
    }
    if (m_SyncCounter <= player->GetSyncCounter()) {
      continue;
    }
    if (m_SyncCounter - player->GetSyncCounter() > m_SyncLimitSafe) {
      laggingPlayers.push_back(player);
    }
  }
  return laggingPlayers;
}

bool CGame::GetIsReserved(const string& name) const
{
  return GetReservedIndex(name) < m_Reserved.size();
}

bool CGame::GetIsProxyReconnectable() const
{
  return 0 != (m_Aura->m_Net->m_Config->m_ProxyReconnect & m_Map->GetProxyReconnect());
}

bool CGame::GetIsProxyReconnectableLong() const
{
  return 0 != ((m_Aura->m_Net->m_Config->m_ProxyReconnect & m_Map->GetProxyReconnect()) & RECONNECT_ENABLED_GPROXY_EXTENDED);
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

void CGame::UncacheOwner()
{
  for (auto& player : m_Players) {
    player->SetOwner(false);
  }
}

void CGame::SetOwner(const string& name, const string& realm)
{
  m_OwnerName = name;
  m_OwnerRealm = realm;
  UncacheOwner();
}

void CGame::ReleaseOwner()
{
  Print("[LOBBY: "  + m_GameName + "] Owner \"" + m_OwnerName + "@" + (m_OwnerRealm.empty() ? "@@LAN/VPN" : m_OwnerRealm) + "\" removed.");
  m_LastOwner = m_OwnerName;
  m_OwnerName.clear();
  m_OwnerRealm.clear();
  UncacheOwner();
  ResetLayout(false);
  m_Locked = false;
  SendAllChat("This game is now ownerless. Type " + GetCmdToken() + "owner to take ownership of this game.");
}

void CGame::ResetDraft()
{
  m_IsDraftMode = true;
  for (auto& player : m_Players) {
    player->SetDraftCaptain(0);
  }
}

void CGame::ResetTeams(const bool alsoCaptains)
{
  if (!(m_Map->GetMapObservers() == MAPOBS_ALLOWED || m_Map->GetMapObservers() == MAPOBS_REFEREES)) {
    return;
  }
  uint8_t SID = static_cast<uint8_t>(m_Slots.size());
  while (SID--) {
    CGameSlot* slot = GetSlot(SID);
    if (slot->GetTeam() == m_Aura->m_MaxSlots) continue;
    if (!slot->GetIsPlayerOrFake()) continue;
    if (!alsoCaptains) {
      CGamePlayer* player = GetPlayerFromSID(SID);
      if (player && player->GetIsDraftCaptain()) continue;
    }
    if (!SetSlotTeam(SID, m_Aura->m_MaxSlots, false)) {
      break;
    }
  }
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
    CGamePlayer* victim = GetPlayerFromName(m_KickVotePlayer, true);

    if (victim) {
      victim->SetDeleteMe(true);
      victim->SetLeftReason("was kicked by vote");

      if (GetIsLobby())
        victim->SetLeftCode(PLAYERLEAVE_LOBBY);
      else
        victim->SetLeftCode(PLAYERLEAVE_LOST);

      if (GetIsLobby()) {
        const uint8_t SID = GetSIDFromPID(victim->GetPID());
        OpenSlot(SID, false);
      }

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
    for (const auto& player : m_Players) {
      bool shouldKick = !player->GetHasMap();
      if (!shouldKick) {
        CRealm* realm = player->GetRealm(false);
        if (realm && realm->GetUnverifiedCannotStartGame() && !player->IsRealmVerified()) {
          shouldKick = true;
        }
      }
      if (shouldKick) {
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
      SendAllChat("The HCL command string is too long. Use [" + GetCmdToken() + "start force] to start anyway");
      ChecksPassed = false;
    }

    // check if everyone has the map
    string StillDownloading;
    for (const auto& slot : m_Slots) {
      if (slot.GetIsPlayerOrFake() && slot.GetDownloadStatus() != 100) {
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
    } else if (0 == m_ControllersWithMap) {
      SendAllChat("Nobody has downloaded the map yet.");
      ChecksPassed = false;
    } else if (m_ControllersWithMap < 2 && !m_RestoredGame) {
      SendAllChat("Only " + to_string(m_ControllersWithMap) + " player has the map.");
      ChecksPassed = false;
    }

    // check if everyone has been pinged enough (3 times) that the autokicker would have kicked them by now
    // see function EventPlayerPongToHost for the autokicker code
    string NotPinged;
    for (const auto& player : m_Players) {
      if (!player->GetIsReserved() && player->GetNumPings() < 3) {
        if (NotPinged.empty())
          NotPinged = player->GetName();
        else
          NotPinged += ", " + player->GetName();
      }
    }

    string NotVerified;
    for (const auto& player : m_Players) {
      CRealm* realm = player->GetRealm(false);
      if (realm && realm->GetUnverifiedCannotStartGame() && !player->IsRealmVerified()) {
        if (NotVerified.empty())
          NotVerified = player->GetName();
        else
          NotVerified += ", " + player->GetName();
      }
    }

    if (!NotPinged.empty()) {
      SendAllChat("Players NOT yet pinged thrice: " + NotPinged);
      ChecksPassed = false;
    }
    if (!NotVerified.empty()) {
      SendAllChat("Players NOT verified (whisper sc): " + NotVerified);
      ChecksPassed = false;
    }
    if (GetTicks() < m_LastPlayerLeaveTicks + 2000) {
      SendAllChat("Someone left the game less than two seconds ago!");
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

  if (GetNumHumanOrFakeControllers() == 1 && (0 == GetSlotsOpen() || m_Map->GetMapObservers() != MAPOBS_REFEREES)) {
    SendAllChat("HINT: Single-player game detected. In-game commands will be DISABLED.");
    if (GetNumOccupiedSlots() != m_Aura->m_MaxSlots) {
      SendAllChat("HINT: To avoid this, you may enable map referees, or add a fake player [" + GetCmdToken() + "fp]");
    }
  }

  if (!m_FakePlayers.empty()) {
    SendAllChat("HINT: " + to_string(m_FakePlayers.size()) + " slots are occupied by fake players.");
  }
}

bool CGame::StopPlayers(const string& reason, const bool allowLocal)
{
  // disconnect every player and set their left reason to the passed string
  // we use this function when we want the code in the Update function to run before the destructor (e.g. saving players to the database)
  // therefore calling this function when m_GameLoading || m_GameLoaded is roughly equivalent to setting m_Exiting = true
  // the only difference is whether the code in the Update function is executed or not

  bool anyStopped = false;
  for (auto& player : m_Players) {
    if (player->GetDeleteMe()) continue;
    player->SetDeleteMe(true);
    player->SetLeftReason(reason);
    player->SetLeftCode(PLAYERLEAVE_LOST);
    if (allowLocal) player->SetLeftMessageSent(true);
    anyStopped = true;
  }
  return anyStopped;
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

bool CGame::Pause(CGamePlayer* player, const bool isDisconnect)
{
  const uint8_t PID = SimulateActionPID(ACTION_PAUSE, player, isDisconnect);
  if (PID == 0xFF) return false;

  vector<uint8_t> CRC, Action;
  Action.push_back(ACTION_PAUSE);
  m_Actions.push(new CIncomingAction(PID, CRC, Action));
  m_Paused = true;
  return true;
}

bool CGame::Resume()
{
  const uint8_t PID = SimulateActionPID(ACTION_RESUME, nullptr, false);
  if (PID == 0xFF) return false;

  vector<uint8_t> CRC, Action;
  Action.push_back(ACTION_RESUME);
  m_Actions.push(new CIncomingAction(PID, CRC, Action));
  m_Paused = false;
  return true;
}

string CGame::GetSaveFileName(const uint8_t PID) const
{
  auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
  struct tm timeinfo;
#ifdef _WIN32
  localtime_s(&timeinfo, &now);
#else
  localtime_r(&now, &timeinfo);
#endif

  ostringstream oss;
  oss << put_time(&timeinfo, "%m-%d_%H-%M");
  return "auto_p" + ToDecString(GetSIDFromPID(PID) + 1) + "_" + oss.str() + ".w3z";
}

bool CGame::Save(CGamePlayer* player, const bool isDisconnect)
{
  const uint8_t PID = SimulateActionPID(ACTION_SAVE, player, isDisconnect);
  if (PID == 0xFF) return false;
  string fileName = GetSaveFileName(PID);
  Print(GetLogPrefix() + "saving as " + fileName);
  vector<uint8_t> CRC, Action;
  Action.push_back(ACTION_SAVE);
  AppendByteArray(Action, fileName);
  m_Actions.push(new CIncomingAction(PID, CRC, Action));
  return true;
}

bool CGame::TrySaveOnDisconnect(CGamePlayer* player, const bool isVoluntary)
{
  if (!m_GameLoaded || m_Players.size() <= 1) {
    // Nobody can actually save this game.
    return false;
  }

  if (GetNumControllers() <= 2) {
    // 1v1 never auto-saves, not even if there are observers,
    // not even if !save enable is active.
    return false;
  }

  if (m_SaveOnLeave != SAVE_ON_LEAVE_ALWAYS) {
    if (isVoluntary) {
      // Is saving on voluntary leaves pointless?
      //
      // Not necessarily.
      //
      // Even if rage quits are unlikely to be reverted,
      // leavers may be replaced by fake players
      // This is impactful in maps such as X Hero Siege,
      // where leavers' heroes are automatically removed.
      //
      // However, since voluntary leaves are the rule, even
      // when games end normally, saving EVERYTIME will turn out to be annoying.
      //
      // Instead, we can do it only if the game has been running for a preconfigured time.
      // But how much time will be relative according to the map.
      // And it would force usage of mapcfg files...
      //
      // Considering that it's also not exactly trivial to load a game,
      // then automating this would bring about too many cons.
      //
      // Which is why allow autosaving on voluntary leaves,
      // but only if users want so by using !save enable.
      // Sadly, that's not the case in this branch, so no save for you.
      return false;
    } else if (GetTicks() < m_FinishedLoadingTicks + 420000) {
      // By default, leaves before the 7th minute do not autosave.
      return false;
    }
  }

  if (!Save(player, true)) return false;
  Pause(player, true);
  SendAllChat("Game saved on " + player->GetName() + "'s disconnection.");
  SendAllChat("They may rejoin on reload if an ally sends them their save. Foes' save files will NOT work.");
  return true;
}

void CGame::OpenObserverSlots()
{
  if (m_Slots.size() >= m_Aura->m_MaxSlots) return;
  Print(GetLogPrefix() + "adding " + to_string(m_Aura->m_MaxSlots - m_Slots.size()) + " observer slots");
  while (m_Slots.size() < m_Aura->m_MaxSlots) {
    m_Slots.emplace_back(0u, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, SLOTRACE_RANDOM);
  }
}

void CGame::CloseObserverSlots()
{
  uint8_t count = 0;
  uint8_t i = static_cast<uint8_t>(m_Slots.size());
  while (i--) {
    if (m_Slots[i].GetTeam() == m_Aura->m_MaxSlots) {
      m_Slots.erase(m_Slots.begin() + i);
      ++count;
    }
  }
  if (count > 0) {
    Print(GetLogPrefix() + "deleted " + to_string(count) + " observer slots");
  }
}

// Virtual host is needed to generate network traffic when only one player is in the game or lobby.
// Fake players may also accomplish the same purpose.
bool CGame::CreateVirtualHost()
{
  if (m_VirtualHostPID != 0xFF)
    return false;

  m_VirtualHostPID = GetNewPID();

  const std::vector<uint8_t> IP = {0, 0, 0, 0};

  // When this message is sent because an slot is made available by a leaving player,
  // we gotta ensure that the virtual host join message is sent after the player's leave message.
  SendAll(GetProtocol()->SEND_W3GS_PLAYERINFO(m_VirtualHostPID, m_LobbyVirtualHostName, IP, IP));
  return true;
}

bool CGame::DeleteVirtualHost()
{
  if (m_VirtualHostPID == 0xFF) {
    return false;
  }

  // When this message is sent because the last slot is filled by an incoming player,
  // we gotta ensure that the virtual host leave message is sent before the player's join message.
  SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(m_VirtualHostPID, PLAYERLEAVE_LOBBY));
  m_VirtualHostPID = 0xFF;
  return true;
}

void CGame::CreateFakePlayerInner(const uint8_t SID, const uint8_t PID, const string& name)
{
  const bool isCustomForces = GetIsCustomForces();
  const std::vector<uint8_t> IP = {0, 0, 0, 0};
  SendAll(GetProtocol()->SEND_W3GS_PLAYERINFO(PID, name, IP, IP));
  m_Slots[SID] = CGameSlot(
    PID,
    SLOTPROG_RDY,
    SLOTSTATUS_OCCUPIED,
    SLOTCOMP_NO,
    isCustomForces ? m_Slots[SID].GetTeam() : m_Aura->m_MaxSlots,
    isCustomForces ? m_Slots[SID].GetColor() : m_Aura->m_MaxSlots,
    m_Map->GetLobbyRace(&m_Slots[SID])
  );
  if (!isCustomForces) SetSlotTeamAndColorAuto(SID);

  m_FakePlayers.push_back(PID | (SID << 8));
  m_SlotInfoChanged |= SLOTS_ALIGNMENT_CHANGED;
}

bool CGame::CreateFakePlayer(const bool useVirtualHostName)
{
  // Fake players need not be explicitly restricted in any layout, so let's just use an empty slot.
  uint8_t SID = GetEmptySID(false);
  if (SID >= static_cast<uint8_t>(m_Slots.size())) return false;
  if (!CanLockSlotForJoins(SID)) return false;

  if (GetSlotsOpen() == 1)
    DeleteVirtualHost();

  CreateFakePlayerInner(SID, GetNewPID(), useVirtualHostName ? m_LobbyVirtualHostName : ("User[" + ToDecString(SID + 1) + "]"));
  return true;
}

bool CGame::CreateFakeObserver(const bool useVirtualHostName)
{
  if (!(m_Map->GetMapObservers() == MAPOBS_ALLOWED || m_Map->GetMapObservers() == MAPOBS_REFEREES)) {
    return false;
  }

  const bool isCustomForces = GetIsCustomForces();
  uint8_t SID = isCustomForces ? GetEmptyObserverSID() : GetEmptySID(false);
  if (SID >= static_cast<uint8_t>(m_Slots.size())) return false;

  if (isCustomForces && (m_Slots[SID].GetTeam() != m_Aura->m_MaxSlots)) {
    return false;
  }
  if (!CanLockSlotForJoins(SID)) {
    return false;
  }
  if (GetSlotsOpen() == 1)
    DeleteVirtualHost();

  CreateFakePlayerInner(SID, GetNewPID(), useVirtualHostName ? m_LobbyVirtualHostName : ("User[" + ToDecString(SID + 1) + "]"));
  return true;
}

bool CGame::DeleteFakePlayer(uint8_t SID)
{
  CGameSlot* slot = GetSlot(SID);
  if (!slot) return false;
  for (auto it = begin(m_FakePlayers); it != end(m_FakePlayers); ++it) {
    if (slot->GetPID() == static_cast<uint8_t>(*it)) {
      if (GetIsCustomForces()) {
        m_Slots[SID] = CGameSlot(0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, slot->GetTeam(), slot->GetColor(), /* only important if MAPOPT_FIXEDPLAYERSETTINGS */ m_Map->GetLobbyRace(slot));
      } else {
        m_Slots[SID] = CGameSlot(0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, SLOTRACE_RANDOM);
      }
      // Ensure this is sent before virtual host rejoins
      SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(static_cast<uint8_t>(*it), PLAYERLEAVE_LOBBY));
      m_FakePlayers.erase(it);
      CreateVirtualHost();
      m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
      return true;
    }
  }
  return false;
}

uint8_t CGame::FakeAllSlots()
{
  // Ensure this is called outside any loops.
  const bool hasPlayers = GetHasAnyPlayer();

  uint8_t addedCounter = 0;
  if (m_RestoredGame) {
    if (m_Reserved.empty()) return 0;
    uint8_t reservedIndex = 0;
    uint8_t reservedEnd = static_cast<uint8_t>(m_Reserved.size()) - static_cast<uint8_t>(!hasPlayers);
    for (uint8_t SID = 0; SID < m_Slots.size(); ++SID) {
      if (m_Slots[SID].GetIsPlayerOrFake()) {
        if (++reservedIndex >= reservedEnd) break;
        continue;
      }
      if (m_Slots[SID].GetSlotStatus() == SLOTSTATUS_OPEN) {
        const CGameSlot* savedSlot = m_RestoredGame->InspectSlot(SID);
        CreateFakePlayerInner(SID, savedSlot->GetPID(), m_Reserved[reservedIndex]);
        ++addedCounter;
        if (++reservedIndex >= reservedEnd) break;
      }
    }
  } else {
    uint8_t remainingControllers = m_Map->GetMapNumControllers() - GetNumControllers();
    if (!hasPlayers && m_Slots.size() == m_Map->GetMapNumControllers()) {
      --remainingControllers;
    }
    for (uint8_t SID = 0; SID < m_Slots.size(); ++SID) {
      if (m_Slots[SID].GetSlotStatus() != SLOTSTATUS_OPEN) {
        continue;
      }
      CreateFakePlayerInner(SID, GetNewPID(), "User[" + ToDecString(SID + 1) + "]");
      ++addedCounter;
      if (0 == --remainingControllers) {
        break;
      }
    }
  }
  if (GetSlotsOpen() == 0 && GetNumHumanOrFakeControllers() > 1) DeleteVirtualHost();
  return addedCounter;
}

void CGame::DeleteFakePlayers()
{
  if (m_FakePlayers.empty())
    return;

  for (uint16_t fakePlayer : m_FakePlayers) {
    for (auto& slot : m_Slots) {
      if (slot.GetPID() != static_cast<uint8_t>(fakePlayer)) continue;
      if (GetIsCustomForces()) {
        slot = CGameSlot(0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, slot.GetTeam(), slot.GetColor(), /* only important if MAPOPT_FIXEDPLAYERSETTINGS */ m_Map->GetLobbyRace(&slot));
      } else {
        slot = CGameSlot(0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, m_Aura->m_MaxSlots, m_Aura->m_MaxSlots, SLOTRACE_RANDOM);
      }
      // Ensure this is sent before virtual host rejoins
      SendAll(GetProtocol()->SEND_W3GS_PLAYERLEAVE_OTHERS(static_cast<uint8_t>(fakePlayer), PLAYERLEAVE_LOBBY));
      break;
    }
  }

  m_FakePlayers.clear();
  CreateVirtualHost();
  m_SlotInfoChanged |= (SLOTS_ALIGNMENT_CHANGED);
}

void CGame::RemoveCreator()
{
  m_CreatedBy.clear();
  m_CreatedFrom = nullptr;
  m_CreatedFromType = GAMESETUP_ORIGIN_INVALID;
}
