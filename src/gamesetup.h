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

#ifndef AURA_GAMESETUP_H_
#define AURA_GAMESETUP_H_

#include <string>
#include <cstdint>
#include <vector>
#include <optional>
#include <utility>
#ifndef DISABLE_CPR
#include <cpr/cpr.h>
#endif

#include "aura.h"
#include "command.h"
#include "irc.h"
#include "map.h"
#include "realm.h"
#include "savegame.h"

#define SEARCH_TYPE_ONLY_MAP 1
#define SEARCH_TYPE_ONLY_CONFIG 2
#define SEARCH_TYPE_ONLY_FILE 3
#define SEARCH_TYPE_ANY 7

#define MATCH_TYPE_NONE 0u
#define MATCH_TYPE_MAP 1u
#define MATCH_TYPE_CONFIG 2u
#define MATCH_TYPE_INVALID 4u
#define MATCH_TYPE_FORBIDDEN 8u

#define RESOLUTION_OK 0
#define RESOLUTION_ERR 1
#define RESOLUTION_BAD_NAME 2

#define SETUP_USE_STANDARD_PATHS true
#define SETUP_PROTECT_ARBITRARY_TRAVERSAL false

#ifdef _WIN32
#define FILE_EXTENSIONS_MAP {L".w3x", L".w3m"}
#define FILE_EXTENSIONS_CONFIG {L".cfg"}
#else
#define FILE_EXTENSIONS_MAP {".w3x", ".w3m"}
#define FILE_EXTENSIONS_CONFIG {".cfg"}
#endif

#define GAMESETUP_ORIGIN_NONE 0
#define GAMESETUP_ORIGIN_REALM 1
#define GAMESETUP_ORIGIN_IRC 2
#define GAMESETUP_ORIGIN_DISCORD 3
#define GAMESETUP_ORIGIN_INVALID 255

#define GAMESETUP_STEP_MAIN 0
#define GAMESETUP_STEP_RESOLUTION 1
#define GAMESETUP_STEP_SUGGESTIONS 2
#define GAMESETUP_STEP_DOWNLOAD 3

#define MAP_ONREADY_SET_ACTIVE 1
#define MAP_ONREADY_HOST 2
#define MAP_ONREADY_ALIAS 3

class CAura;
class CCLI;
class CCommandContext;
class CIRC;
class CDiscord;
class CMap;
class CRealm;
class CSaveGame;

inline std::vector<std::pair<std::string, int>> ExtractEpicWarMaps(const std::string &s, const int maxCount) {
  std::regex pattern(R"(<a href="/maps/(\d+)/"><b>([^<\n]+)</b></a>)");
  std::vector<std::pair<std::string, int>> output;

  std::sregex_iterator iter(s.begin(), s.end(), pattern);
  std::sregex_iterator end;

  int count = 0;
  while (iter != end && count < maxCount) {
    std::smatch match = *iter;
    output.emplace_back(std::make_pair(match[2], std::stoi(match[1])));
    ++iter;
    ++count;
  }

  return output;
}

//
// CGameExtraOptions
//

class CGameExtraOptions
{
public:
  CGameExtraOptions();
  CGameExtraOptions(const std::optional<bool>& nRandomRaces, const std::optional<bool>& nRandomHeroes, const std::optional<uint8_t>& nVisibility, const std::optional<uint8_t>& nObservers);
  ~CGameExtraOptions();

  std::optional<bool>         m_TeamsLocked;
  std::optional<bool>         m_TeamsTogether;
  std::optional<bool>         m_AdvancedSharedUnitControl;
  std::optional<bool>         m_RandomRaces;
  std::optional<bool>         m_RandomHeroes;
  std::optional<uint8_t>      m_Visibility;
  std::optional<uint8_t>      m_Observers;

  bool ParseMapObservers(const std::string& s);
  bool ParseMapVisibility(const std::string& s);
  bool ParseMapRandomRaces(const std::string& s);
  bool ParseMapRandomHeroes(const std::string& s);

  void AcquireCLI(const CCLI* nCLI);
};

//
// CGameSetup
//

class CGameSetup
{
public:
  CAura*                                          m_Aura;
  CSaveGame*                                      m_RestoredGame;
  CMap*                                           m_Map;
  CCommandContext*                                m_Ctx;

  std::string                                     m_Attribution;
  std::string                                     m_SearchRawTarget;
  uint8_t                                         m_SearchType;
  bool                                            m_AllowPaths;
  bool                                            m_StandardPaths;
  bool                                            m_LuckyMode;
  bool                                            m_Verbose;
  std::pair<std::string, std::string>             m_SearchTarget;

  bool                                            m_FoundSuggestions;
  bool                                            m_IsDownloadable;
  bool                                            m_IsStepDownloading;
  bool                                            m_IsStepDownloaded;
  std::string                                     m_BaseDownloadFileName;
  std::string                                     m_MapDownloadUri;
  uint32_t                                        m_MapDownloadSize;
  std::string                                     m_MapSiteUri;
  std::filesystem::path                           m_DownloadFilePath;
  std::ofstream*                                  m_DownloadFileStream;
#ifndef DISABLE_CPR
  std::future<uint32_t>                           m_DownloadFuture;
#endif
  int32_t                                         m_DownloadTimeout;
  std::string                                     m_ErrorMessage;
  uint8_t                                         m_AsyncStep;

  bool                                            m_SkipVersionCheck;
  bool                                            m_IsMapDownloaded;

  std::filesystem::path                           m_SaveFile;

  std::string                                     m_Name;
  std::string                                     m_BaseName;
  bool                                            m_OwnerLess;
  std::pair<std::string, std::string>             m_Owner;
  std::optional<uint32_t>                         m_Identifier;
  std::optional<uint32_t>                         m_ChannelKey;
  std::optional<bool>                             m_ChecksReservation;
  std::vector<std::string>                        m_Reservations;
  bool                                            m_IsMirror;
  uint8_t                                         m_RealmsDisplayMode;
  sockaddr_storage                                m_RealmsAddress;
  std::set<std::string>                           m_RealmsExcluded;
  std::vector<uint8_t>                            m_SupportedGameVersions;

  bool                                            m_LobbyReplaceable;
  bool                                            m_LobbyAutoRehosted;
  uint16_t                                        m_CreationCounter;
  std::optional<uint32_t>                         m_LobbyTimeout;
  std::optional<uint8_t>                          m_AutoStartPlayers;
  std::optional<int64_t>                          m_AutoStartMinSeconds;
  std::optional<int64_t>                          m_AutoStartMaxSeconds;
  std::optional<uint8_t>                          m_IPFloodHandler;
  std::optional<uint16_t>                         m_LatencyAverage;
  std::optional<uint16_t>                         m_LatencyMaxFrames;
  std::optional<uint16_t>                         m_LatencySafeFrames;
  std::optional<std::string>                      m_HCL;
  std::optional<uint8_t>                          m_CustomLayout;
  std::optional<bool>                             m_CheckJoinable;

  std::string                                     m_CreatedBy;
  void*                                           m_CreatedFrom;
  uint8_t                                         m_CreatedFromType;

  CGameExtraOptions*                              m_MapExtraOptions;
  uint8_t                                         m_MapReadyCallbackAction;
  std::string                                     m_MapReadyCallbackData;

  bool                                            m_DeleteMe;

  CGameSetup(CAura* nAura, CCommandContext* nCtx, CConfig* mapCFG);
  CGameSetup(CAura* nAura, CCommandContext* nCtx, const std::string nSearchRawTarget, const uint8_t nSearchType, const bool nAllowPaths, const bool nUseStandardPaths, const bool nUseLuckyMode, const bool nSkipVersionCheck);
  ~CGameSetup();

  std::string GetInspectName() const;
  bool GetDeleteMe() const { return m_DeleteMe; }

  void ParseInputLocal();
  void ParseInput();
  std::pair<uint8_t, std::filesystem::path> SearchInputStandard();
  std::pair<uint8_t, std::filesystem::path> SearchInputAlias();
  std::pair<uint8_t, std::filesystem::path> SearchInputLocalExact();
  std::pair<uint8_t, std::filesystem::path> SearchInputLocalTryExtensions();
  std::pair<uint8_t, std::filesystem::path> SearchInputLocalFuzzy(std::vector<std::string>& fuzzyMatches);
#ifndef DISABLE_CPR
  void SearchInputRemoteFuzzy(std::vector<std::string>& fuzzyMatches);
#endif
  std::pair<uint8_t, std::filesystem::path> SearchInputLocal(std::vector<std::string>& fuzzyMatches);
  std::pair<uint8_t, std::filesystem::path> SearchInput();
  inline CMap* GetMap() const { return m_Map; }
  CMap* GetBaseMapFromConfig(CConfig* mapCFG, const bool silent);
  CMap* GetBaseMapFromConfigFile(const std::filesystem::path& filePath, const bool isCache, const bool silent);
  CMap* GetBaseMapFromMapFile(const std::filesystem::path& filePath, const bool silent);
  CMap* GetBaseMapFromMapFileOrCache(const std::filesystem::path& mapPath, const bool silent);
  bool ApplyMapModifiers(CGameExtraOptions* extraOptions);
#ifndef DISABLE_CPR
  uint32_t ResolveMapRepositoryTask();
  void RunResolveMapRepository();
  uint32_t RunResolveMapRepositorySync();
  void SetDownloadFilePath(std::filesystem::path&& filePath);
  bool PrepareDownloadMap();
  void RunDownloadMap();
  uint32_t DownloadMapTask();
  uint32_t RunDownloadMapSync();
  void OnResolveMapSuccess();
  void OnDownloadMapSuccess();
  void OnFetchSuggestionsEnd();
  std::vector<std::pair<std::string, std::string>> GetMapRepositorySuggestions(const std::string & pattern, const uint8_t maxCount);


#endif
  void LoadMap();
  bool LoadMapSync();  
  void OnLoadMapSuccess();
  void OnLoadMapError();
  bool SetActive();
  bool RestoreFromSaveFile();
  bool RunHost();

  inline bool GetIsMirror() const { return m_IsMirror; }
  inline bool GetIsDownloading() const { return m_IsStepDownloading; }

  bool SetMirrorSource(const sockaddr_storage& nSourceAddress, const uint32_t nGameIdentifier);
  bool SetMirrorSource(const std::string& nInput);
  void AddIgnoredRealm(const CRealm* nRealm);
  void RemoveIgnoredRealm(const CRealm* nRealm);
  void SetDisplayMode(const uint8_t nDisplayMode);
  void SetOwner(const std::string& nOwner, const CRealm* nRealm);
  void SetOwnerLess(const bool nValue) { m_OwnerLess = nValue; }
  void SetCreator(const std::string& nCreator);
  void SetCreator(const std::string& nCreator, CRealm* nRealm);
  void SetCreator(const std::string& nCreator, CIRC* nIRC);
  void SetCreator(const std::string& nCreator, CDiscord* nDiscord);
  void RemoveCreator();
  bool MatchesCreatedFrom(const uint8_t fromType, const void* fromThing) const;
  void SetName(const std::string& nName) { m_Name = nName; }
  void SetBaseName(const std::string& nName) {
    m_Name = nName;
    m_BaseName = nName;
  }
  void SetOwner(const std::string& ownerName, const std::string& ownerRealm) {
    m_Owner = std::make_pair(ownerName, ownerRealm);
  }
  void SetLobbyTimeout(const uint32_t nTimeout) { m_LobbyTimeout = nTimeout; }
  void SetLobbyReplaceable(const bool nReplaceable) { m_LobbyReplaceable = nReplaceable; }
  void SetLobbyAutoRehosted(const bool nRehosted) { m_LobbyAutoRehosted = nRehosted; }
  void SetDownloadTimeout(const uint32_t nTimeout) { m_DownloadTimeout = nTimeout; }
  void SetIsCheckJoinable(const bool nCheckJoinable) { m_CheckJoinable = nCheckJoinable; }
  void SetVerbose(const bool nVerbose) { m_Verbose = nVerbose; }
  void SetContext(CCommandContext* nCtx) { m_Ctx = nCtx; }
  void SetMapReadyCallback(const uint8_t action, const std::string& data) {
    m_MapReadyCallbackAction = action;
    m_MapReadyCallbackData = data;
  }
  void SetMapExtraOptions(CGameExtraOptions* opts) { m_MapExtraOptions = opts; }
  void SetGameSavedFile(const std::filesystem::path& filePath);
  void SetCheckReservation(const bool nChecksReservation) { m_ChecksReservation = nChecksReservation; }
  void SetReservations(const std::vector<std::string>& nReservations) { m_Reservations = nReservations; }
  void SetSupportedGameVersions(const std::vector<uint8_t>& nVersions) { m_SupportedGameVersions = nVersions; }
  void SetAutoStartPlayers(const uint8_t nValue) { m_AutoStartPlayers = nValue; }
  void SetAutoStartMinSeconds(const int64_t nValue) {
    m_AutoStartMinSeconds = nValue;
    if (m_AutoStartMaxSeconds < nValue) m_AutoStartMaxSeconds = nValue;
  }
  void SetAutoStartMaxSeconds(const int64_t nValue) {
    m_AutoStartMaxSeconds = nValue;
    if (m_AutoStartMinSeconds > nValue) m_AutoStartMinSeconds = nValue;
  }
  void SetIPFloodHandler(const uint8_t nValue) { m_IPFloodHandler = nValue;}
  void SetLatencyAverage(const uint16_t nValue) { m_LatencyAverage = nValue; }
  void SetLatencyMaxFrames(const uint16_t nValue) { m_LatencyMaxFrames = nValue; }
  void SetLatencySafeFrames(const uint16_t nValue) { m_LatencySafeFrames = nValue; }
  void SetHCL(const std::string& nHCL) { m_HCL = nHCL; }
  void SetCustomLayout(const uint8_t nLayout) { m_CustomLayout = nLayout; }
  void ResetExtraOptions();

  void OnGameCreate();
  bool Update();
};

#endif // AURA_GAMESETUP_H_
