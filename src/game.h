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

#ifndef AURA_GAME_H_
#define AURA_GAME_H_

#include "gameslot.h"
#include "gamesetup.h"
#include "savegame.h"
#include "socket.h"

#include <set>
#include <queue>
#include <algorithm>
#include <random>
#include <map>

#define SLOTS_ALIGNMENT_CHANGED (1 << 0)
#define SLOTS_DOWNLOAD_PROGRESS_CHANGED (1 << 1)
#define SLOTS_HCL_INJECTED (1 << 2)

//
// CGame
//

class CAura;
class CTCPServer;
class CUDPServer;
class CCommandContext;
class CGameProtocol;
class CGameConnection;
class CGamePlayer;
class CGameSetup;
class CMap;
class CIncomingJoinRequest;
class CIncomingAction;
class CIncomingChatPlayer;
class CIncomingMapSize;
class CDBBan;
class CDBGamePlayer;
class CStats;
class CRealm;
class CSaveGame;

class CGame
{
public:
  CAura* m_Aura;

private:
  friend class CCommandContext;

protected:
  bool                           m_Verbose;
  CTCPServer*                    m_Socket;                        // listening socket
  CDBBan*                        m_DBBanLast;                     // last ban for the !banlast command - this is a pointer to one of the items in m_DBBans
  std::vector<CDBBan*>           m_DBBans;                        // std::vector of potential ban data for the database
  CStats*                        m_Stats;                         // class to keep track of game stats such as kills/deaths/assists in dota
  CSaveGame*                     m_RestoredGame;
  std::vector<CGameSlot>         m_Slots;                         // std::vector of slots
  std::vector<CDBGamePlayer*>    m_DBGamePlayers;                 // std::vector of potential gameplayer data for the database
  std::vector<CGamePlayer*>      m_Players;                       // std::vector of players
  std::queue<CIncomingAction*>   m_Actions;                       // queue of actions to be sent
  std::vector<std::string>       m_Reserved;                      // std::vector of player names with reserved slots (from the !hold command)
  std::set<std::string>          m_ReportedJoinFailNames;                  // set of player names to NOT print ban messages for when joining because they've already been printed
  std::vector<uint8_t>           m_FakePlayers;                   // the fake player's PIDs (if present)
  CMap*                          m_Map;                           // map data
  std::string                    m_GameName;                      // game name
  std::string                    m_LastGameName;                  // last game name (the previous game name before it was rehosted)
  std::string                    m_IndexVirtualHostName;          // host's name
  std::string                    m_LobbyVirtualHostName;          // host's name
  std::string                    m_LastOwner;                     // name of the player who was owner last time the owner was released
  std::string                    m_OwnerName;                     // name of the player who owns this game (should be considered an admin)
  std::string                    m_OwnerRealm;                    // self-identified realm of the player who owns the game (spoofable)
  std::string                    m_CreatorText;                   // who created this game
  std::string                    m_CreatedBy;                     // name of the battle.net user who created this game
  void*                          m_CreatedFrom;                   // battle.net or IRC server the player who created this game was on
  uint8_t                        m_CreatedFromType;               // type of server m_CreatedFrom is
  std::set<std::string>          m_RealmsExcluded;                // battle.net servers where the mirrored game is not to be broadcasted
  std::string                    m_KickVotePlayer;                // the player to be kicked with the currently running kick vote
  std::string                    m_HCLCommandString;              // the "HostBot Command Library" command std::string, used to pass a limited amount of data to specially designed maps
  std::string                    m_MapPath;                       // store the map path to save in the database on game end
  std::string                    m_MapSiteURL;
  int64_t                        m_GameTicks;                     // ingame ticks
  int64_t                        m_CreationTime;                  // GetTime when the game was created
  int64_t                        m_LastPingTime;                  // GetTime when the last ping was sent
  int64_t                        m_LastRefreshTime;               // GetTime when the last game refresh was sent
  int64_t                        m_LastDownloadTicks;             // GetTicks when the last map download cycle was performed
  int64_t                        m_LastDownloadCounterResetTicks; // GetTicks when the download counter was last reset
  int64_t                        m_LastCountDownTicks;            // GetTicks when the last countdown message was sent
  int64_t                        m_StartedLoadingTicks;           // GetTicks when the game started loading
  int64_t                        m_LastActionSentTicks;           // GetTicks when the last action packet was sent
  int64_t                        m_LastActionLateBy;              // the number of ticks we were late sending the last action packet by
  int64_t                        m_StartedLaggingTime;            // GetTime when the last lag screen started
  int64_t                        m_LastLagScreenTime;             // GetTime when the last lag screen was active (continuously updated)
  int64_t                        m_LastOwnerSeen;                 // GetTime when the last reserved player was seen in the lobby
  int64_t                        m_StartedKickVoteTime;           // GetTime when the kick vote was started
  int64_t                        m_GameOverTime;                  // GetTime when the game was over
  int64_t                        m_LastPlayerLeaveTicks;          // GetTicks when the most recent player left the game
  int64_t                        m_LastLagScreenResetTime;        // GetTime when the "lag" screen was last reset
  uint8_t                        m_PauseCounter;
  uint32_t                       m_RandomSeed;                    // the random seed sent to the Warcraft III clients
  uint32_t                       m_HostCounter;                   // a unique game number
  uint32_t                       m_EntryKey;                      // random entry key for LAN, used to prove that a player is actually joining from LAN
  uint16_t                       m_Latency;                       // the number of ms to wait between sending action packets (we queue any received during this time)
  uint32_t                       m_SyncLimit;                     // the maximum number of packets a player can fall out of sync before starting the lag screen
  uint32_t                       m_SyncLimitSafe;                 // stop lag screen if players are within this same amount of packets
  uint32_t                       m_SyncCounter;                   // the number of actions sent so far (for determining if anyone is lagging)
  uint32_t                       m_AutoKickPing;                  //
  uint32_t                       m_WarnHighPing;                  //

  std::string                    m_PrivateCmdToken;
  std::string                    m_BroadcastCmdToken;
  uint32_t                       m_VoteKickPercentage;            // percentage of players required to vote yes for a votekick to pass
  uint32_t                       m_LacksMapKickDelay;
  bool                           m_NotifyJoins;                   // whether the bot should beep when a player joins a hosted game
  uint32_t                       m_PerfThreshold;                 // the max expected delay between updates - if exceeded it means performance is suffering
  uint32_t                       m_LobbyNoOwnerTime;              // relinquish game ownership after this many minutes
  uint32_t                       m_LobbyTimeLimit;                // auto close the game lobby after this many minutes without any owner
  uint32_t                       m_NumPlayersToStartGameOver;     // when this player count is reached, the game over timer will start

  bool                           m_CheckJoinable;
  std::set<std::string>          m_ExtraDiscoveryAddresses;
  bool                           m_ExtraDiscoveryStrict;

  uint32_t                       m_DownloadCounter;               // # of map bytes downloaded in the last second
  uint32_t                       m_CountDownCounter;              // the countdown is finished when this reaches zero
  uint8_t                        m_StartPlayers;                  // number of players when the game started
  int64_t                        m_AutoStartMinTime;
  int64_t                        m_AutoStartMaxTime;
  uint8_t                        m_AutoStartPlayers;
  uint8_t                        m_ControllersWithMap;
  uint16_t                       m_HostPort;                      // the port to host games on
  bool                           m_UDPEnabled;                    // whether this game should be listed in "Local Area Network"
  bool                           m_PublicHostOverride;            // whether to use own m_PublicHostAddress, m_PublicHostPort instead of CRealm's (disables hosting on CRealm mirror instances)
  std::vector<uint8_t>           m_PublicHostAddress;
  uint16_t                       m_PublicHostPort;
  uint8_t                        m_GameDisplay;                   // game state, public or private
  bool                           m_IsAutoVirtualPlayers;       // if we should try to add the virtual host as a second (fake) player in single-player games
  uint8_t                        m_VirtualHostPID;                // host's PID
  uint8_t                        m_GProxyEmptyActions;            // empty actions used for gproxy protocol
  bool                           m_Exiting;                       // set to true and this class will be deleted next update
  bool                           m_Saving;                        // if we're currently saving game data to the database
  uint8_t                        m_SlotInfoChanged;               // if the slot info has changed and hasn't been sent to the players yet (optimization)
  bool                           m_PublicStart;                   // if the game owner is the only one allowed to run game commands or not
  bool                           m_Locked;                        // if the game owner is the only one allowed to run game commands or not
  bool                           m_RealmRefreshError;                  // if the game had a refresh error
  bool                           m_MuteAll;                       // if we should stop forwarding ingame chat messages targeted for all players or not
  bool                           m_MuteLobby;                     // if we should stop forwarding lobby chat messages
  bool                           m_IsMirror;                      // if we aren't actually hosting the game, but just broadcasting it
  bool                           m_CountDownStarted;              // if the game start countdown has started or not
  bool                           m_GameLoading;                   // if the game is currently loading or not
  bool                           m_GameLoaded;                    // if the game has loaded or not
  bool                           m_Lagging;                       // if the lag screen is active or not
  bool                           m_Desynced;                      // if the game has desynced or not
  bool                           m_HadLeaver;                     // if the game has desynced or not
  bool                           m_HasMapLock;                    // ensures that the map isn't deleted while the game lobby is active
  bool                           m_CheckReservation;
  bool                           m_UsesCustomReferees;
  bool                           m_SentPriorityWhois;
  std::map<CGamePlayer*, std::vector<CGamePlayer*>>  m_SyncPlayers;     //
  std::set<std::string>          m_IgnoredNotifyJoinPlayers;
  

public:
  CGame(CAura* nAura, CGameSetup* nGameSetup);
  ~CGame();
  CGame(CGame&) = delete;

  bool                  GetExiting() const { return m_Exiting; }
  inline CMap*          GetMap() const { return m_Map; }
  inline uint32_t       GetEntryKey() const { return m_EntryKey; }
  inline uint16_t       GetHostPort() const { return m_HostPort; }
  uint16_t              GetDiscoveryPort(const uint8_t protocol) const;
  inline bool           GetUDPEnabled() const { return m_UDPEnabled; }
  inline bool           GetPublicHostOverride() const { return m_PublicHostOverride; }
  inline std::vector<uint8_t>    GetPublicHostAddress() const { return m_PublicHostAddress; }
  inline uint16_t       GetPublicHostPort() const { return m_PublicHostPort; }
  inline uint8_t        GetDisplayMode() const { return m_GameDisplay; }
  inline uint8_t        GetGProxyEmptyActions() const { return m_GProxyEmptyActions; }
  inline std::string    GetGameName() const { return m_GameName; }
  inline std::string    GetLastGameName() const { return m_LastGameName; }
  inline std::string    GetIndexVirtualHostName() const { return m_IndexVirtualHostName; }
  inline std::string    GetLobbyVirtualHostName() const { return m_LobbyVirtualHostName; }
  std::string           GetPrefixedGameName(const CRealm* realm = nullptr) const;
  std::string           GetAnnounceText(const CRealm* realm = nullptr) const;
  inline std::string    GetOwnerName() const { return m_OwnerName; }
  inline std::string    GetCreatorName() const { return m_CreatedBy; }
  inline uint8_t        GetCreatedFromType() const { return m_CreatedFromType; }
  inline void*          GetCreatedFrom() const { return m_CreatedFrom; }
  bool                  MatchesCreatedFrom(const uint8_t fromType, const void* fromThing) const;
  inline uint32_t       GetHostCounter() const { return m_HostCounter; }
  inline int64_t        GetLastLagScreenTime() const { return m_LastLagScreenTime; }
  inline bool           GetLocked() const { return m_Locked; }
  inline bool           GetCountDownStarted() const { return m_CountDownStarted; }
  inline bool           GetIsMirror() const { return m_IsMirror; }
  inline bool           GetGameLoading() const { return m_GameLoading; }
  inline bool           GetGameLoaded() const { return m_GameLoaded; }
  inline bool           GetIsLobby() const { return !m_IsMirror && !m_GameLoading && !m_GameLoaded; }
  inline bool           GetIsRestored() const { return m_RestoredGame != nullptr; }
  inline bool           GetLagging() const { return m_Lagging; }
  inline bool           GetIsGameOver() const { return m_GameOverTime != 0; }
  CGameProtocol*        GetProtocol() const;
  int64_t               GetNextTimedActionTicks() const;
  uint32_t              GetSlotsOccupied() const;
  uint32_t              GetSlotsOpen() const;
  uint32_t              GetNumConnectionsOrFake() const;
  uint32_t              GetNumHumanPlayers() const;
  uint32_t              GetNumOccupiedSlots() const;
  uint32_t              GetNumControllers() const;
  std::string           GetMapFileName() const;
  std::string           GetMapSiteURL() const { return m_MapSiteURL; }
  std::string           GetDescription() const;
  std::string           GetCategory() const;
  uint32_t              GetGameType() const;
  std::string           GetSourceFilePath() const;
  std::vector<uint8_t>  GetSourceFileHash() const;
  std::vector<uint8_t>  GetSourceFileSHA1() const;
  std::vector<uint8_t>  GetAnnounceWidth() const;
  std::vector<uint8_t>  GetAnnounceHeight() const;

  std::string           GetLogPrefix() const;
  std::string           GetPlayers() const;
  std::string           GetObservers() const;
  std::string           GetAutoStartText() const;
  CTCPServer*           GetSocket() const { return m_Socket; };

  uint16_t              GetHostPortForDiscoveryInfo(const uint8_t protocol) const;
  inline bool           GetIsRealmRefreshError() { return m_RealmRefreshError; }
  inline bool           GetIsRealmExcluded(const std::string& hostName) const { return m_RealmsExcluded.find(hostName) != m_RealmsExcluded.end() ; }

  inline void           SetExiting(bool nExiting) { m_Exiting = nExiting; }
  inline void           SetRefreshError(bool nRefreshError) { m_RealmRefreshError = nRefreshError; }
  inline void           SetMapSiteURL(const std::string& nMapSiteURL) { m_MapSiteURL = nMapSiteURL; }

  int64_t               GetUptime() const { return GetTime() - m_CreationTime; }

  // processing functions

  uint32_t SetFD(void* fd, void* send_fd, int32_t* nfds);
  bool Update(void* fd, void* send_fd);
  void UpdatePost(void* send_fd);

  // generic functions to send packets to players

  void Send(CGameConnection* player, const std::vector<uint8_t>& data) const;
  void Send(CGamePlayer* player, const std::vector<uint8_t>& data) const;
  void Send(uint8_t PID, const std::vector<uint8_t>& data) const;
  void Send(const std::vector<uint8_t>& PIDs, const std::vector<uint8_t>& data) const;
  void SendAll(const std::vector<uint8_t>& data) const;
 

  // functions to send packets to players

  void SendChat(uint8_t fromPID, CGamePlayer* player, const std::string& message) const;
  void SendChat(uint8_t fromPID, uint8_t toPID, const std::string& message) const;
  void SendChat(CGamePlayer* player, const std::string& message) const;
  void SendChat(uint8_t toPID, const std::string& message) const;
  void SendAllChat(uint8_t fromPID, const std::string& message) const;
  void SendAllChat(const std::string& message) const;
  void SendAllSlotInfo();
  void SendVirtualHostPlayerInfo(CGamePlayer* player) const;
  void SendFakePlayersInfo(CGamePlayer* player) const;
  void SendJoinedPlayersInfo(CGamePlayer* player) const;
  void SendVirtualHostPlayerInfo(CGameConnection* player) const;
  void SendFakePlayersInfo(CGameConnection* player) const;
  void SendJoinedPlayersInfo(CGameConnection* player) const;
  void SendWelcomeMessage(CGamePlayer* player) const;
  void SendAllActions();
  void SendAllAutoStart() const;

  std::vector<uint8_t> GetGameDiscoveryInfo(const uint16_t hostPort) const;

  void AnnounceToRealm(CRealm* realm);
  void AnnounceToAddress(std::string& address) const;
  void ReplySearch(sockaddr_storage* address, CSocket* socket) const;
  void SendGameDiscoveryInfo() const;
  void SendGameDiscoveryRefresh() const;
  void SendGameDiscoveryCreate() const;
  void SendGameDiscoveryDecreate() const;

  // events
  // note: these are only called while iterating through the m_Potentials or m_Players std::vectors
  // therefore you can't modify those std::vectors and must use the player's m_DeleteMe member to flag for deletion

  void EventPlayerDeleted(CGamePlayer* player);
  void EventPlayerDisconnectTimedOut(CGamePlayer* player);
  void EventPlayerDisconnectSocketError(CGamePlayer* player);
  void EventPlayerDisconnectConnectionClosed(CGamePlayer* player);
  void EventPlayerDisconnectGameProtocolError(CGamePlayer* player);
  void EventPlayerKickHandleQueued(CGamePlayer* player);
  void EventPlayerCheckStatus(CGamePlayer* player);
  bool EventRequestJoin(CGameConnection* connection, CIncomingJoinRequest* joinRequest);
  void EventPlayerLeft(CGamePlayer* player);
  void EventPlayerLoaded(CGamePlayer* player);
  void EventPlayerAction(CGamePlayer* player, CIncomingAction* action);
  void EventPlayerKeepAlive(CGamePlayer* player);
  void EventPlayerChatToHost(CGamePlayer* player, CIncomingChatPlayer* chatPlayer);
  void EventPlayerChangeTeam(CGamePlayer* player, uint8_t team);
  void EventPlayerChangeColour(CGamePlayer* player, uint8_t colour);
  void EventPlayerChangeRace(CGamePlayer* player, uint8_t race);
  void EventPlayerChangeHandicap(CGamePlayer* player, uint8_t handicap);
  void EventPlayerDropRequest(CGamePlayer* player);
  void EventPlayerMapSize(CGamePlayer* player, CIncomingMapSize* mapSize);
  void EventPlayerPongToHost(CGamePlayer* player);

  // these events are called outside of any iterations

  void EventGameStarted();
  void EventGameLoaded();

  // other functions

  uint8_t GetSIDFromPID(uint8_t PID) const;
  CGamePlayer* GetPlayerFromPID(uint8_t PID) const;
  CGamePlayer* GetPlayerFromSID(uint8_t SID) const;
  CGamePlayer* GetPlayerFromName(std::string name, bool sensitive) const;
  bool         HasOwnerSet() const;
  bool         HasOwnerInGame() const;
  uint8_t      GetPlayerFromNamePartial(std::string name, CGamePlayer** player) const;
  std::string  GetDBPlayerNameFromColour(uint8_t colour) const;
  CGamePlayer* GetPlayerFromColour(uint8_t colour) const;
  uint8_t              GetNewPID() const;
  uint8_t              GetNewColour() const;
  std::vector<uint8_t> GetPIDs() const;
  std::vector<uint8_t> GetPIDs(uint8_t excludePID) const;
  uint8_t GetHostPID() const;
  uint8_t GetEmptySlot(bool reserved) const;
  uint8_t GetEmptySlot(uint8_t team, uint8_t PID) const;
  uint8_t GetEmptyObserverSlot() const;
  void SwapSlots(uint8_t SID1, uint8_t SID2);
  bool OpenSlot(uint8_t SID, bool kick);
  bool CloseSlot(uint8_t SID, bool kick);
  void SendIncomingPlayerInfo(CGamePlayer* player) const;
  CGamePlayer* JoinPlayer(CGameConnection* connection, CIncomingJoinRequest* joinRequest, const uint8_t SID, const uint8_t PID, const uint8_t HostCounterID, const std::string JoinedRealm, const bool IsReserved, const bool IsUnverifiedAdmin);
  bool ComputerSlot(uint8_t SID, uint8_t skill, bool kick);
  void ColorSlot(uint8_t SID, uint8_t colour);
  void OpenAllSlots();
  void CloseAllSlots();
  void ComputerAllSlots(uint8_t skill);
  void ShuffleSlots();
  void ReportSpoofed(const std::string& server, CGamePlayer* player);
  void AddToRealmVerified(const std::string& server, CGamePlayer* player, bool sendMessage);
  void AddToReserved(const std::string& name);
  void RemoveFromReserved(const std::string& name);
  void RemoveAllReserved();
  bool MatchOwnerName(const std::string& name) const;
  uint8_t GetReservedIndex(const std::string& name) const;
  bool GetIsReserved(const std::string& name) const;
  bool IsDownloading() const;
  void SetOwner(const std::string& name, const std::string& realm);
  void ReleaseOwner();
  void ResetSync();
  void CountKickVotes();
  void StartCountDown(bool force);
  void StopPlayers(const std::string& reason);
  void StopLaggers(const std::string& reason);
  bool Pause();
  bool PauseSinglePlayer();
  void Resume();
  inline bool GetIsCheckJoinable() { return m_CheckJoinable; }
  inline bool GetIsVerbose() { return m_Verbose; }
  inline void SetIsCheckJoinable(const bool nCheckIsJoinable) { m_CheckJoinable = nCheckIsJoinable; }
  inline bool GetSentPriorityWhois() const { return m_SentPriorityWhois; }
  void SetSentPriorityWhois(const bool nValue) { m_SentPriorityWhois = nValue; }
  void SetCheckReservation(const bool nValue) { m_CheckReservation = nValue; }
  void SetUsesCustomReferees(const bool nValue) { m_UsesCustomReferees = nValue; }

  void OpenObserverSlots();
  void CloseObserverSlots();
  bool CreateVirtualHost();
  bool DeleteVirtualHost();
  void CreateFakePlayerInner(const uint8_t SID, const uint8_t PID, const uint8_t team, const std::string& name);
  bool CreateFakePlayer(const bool useVirtualHostName);
  bool CreateFakeObserver(const bool useVirtualHostName);
  bool DeleteFakePlayer(uint8_t SID);
  void DeleteFakePlayers();
  uint8_t FakeAllSlots();
  bool GetIsAutoVirtualPlayers() const { return m_IsAutoVirtualPlayers; }
  void SetAutoVirtualPlayers(const bool nEnableVirtualHostPlayer) { m_IsAutoVirtualPlayers = nEnableVirtualHostPlayer; }
  void RemoveCreator();
};

#endif // AURA_GAME_H_
