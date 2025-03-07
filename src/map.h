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

#ifndef AURA_MAP_H_
#define AURA_MAP_H_

#include <sha1/sha1.h>

#include "includes.h"
#include "file_util.h"
#include "game_slot.h"
#include "util.h"

#include <iterator>
#include <cctype>

#pragma once

#define ROTL(x, n) ((x) << (n)) | ((x) >> (32 - (n))) // this won't work with signed types
#define ROTR(x, n) ((x) >> (n)) | ((x) << (32 - (n))) // this won't work with signed types

//
// MapCrypto
//

struct MapCrypto
{
  bool errored;
  CSHA1 sha1;
  uint32_t blizz;

  MapCrypto()
   : errored(false),
     blizz(0)
  {
  }

  ~MapCrypto() = default;
};

//
// MapFragmentHashes
//

struct MapFragmentHashes
{
  std::optional<std::array<uint8_t, 4>> blizz;
  std::optional<std::array<uint8_t, 20>> sha1;
};

//
// MapEssentials
//

struct MapEssentials
{
  bool melee;
  uint8_t dataSet;
  uint8_t numPlayers;
  uint8_t numDisabled;
  uint8_t numTeams;
  Version minCompatibleGameVersion;
  Version minSuggestedGameVersion;
  bool isLua;
  uint32_t editorVersion;
  uint32_t options;
  uint32_t loadingImgSize;
  uint32_t prologueImgSize;
  std::string prologueImgPath;
  std::string loadingImgPath;
  std::string name;
  std::string author;
  std::string desc;
  std::optional<std::array<uint8_t, 2>> width;
  std::optional<std::array<uint8_t, 2>> height;
  std::map<Version, MapFragmentHashes> fragmentHashes;
  std::vector<CGameSlot> slots;

  MapEssentials()
   : melee(false),
     numPlayers(0),
     numDisabled(0),
     numTeams(0),
     minCompatibleGameVersion(Version(1u, 0u)),
     minSuggestedGameVersion(Version(1u, 0u)),
     isLua(false),
     editorVersion(0),
     options(0),
     loadingImgSize(0),
     prologueImgSize(0)
  {
  }
  ~MapEssentials() = default;
};

//
// CMap
//

class CMap
{
public:
  CAura* m_Aura;

  std::optional<Version>                m_MapTargetGameVersion;
  std::optional<uint8_t>                m_NumPlayersToStartGameOver;
  std::optional<uint8_t>                m_PlayersReadyMode;
  std::optional<bool>                   m_AutoStartRequiresBalance;
  std::optional<uint32_t>               m_LatencyMaxFrames;
  std::optional<uint32_t>               m_LatencySafeFrames;
  std::optional<uint32_t>               m_AutoKickPing;
  std::optional<uint32_t>               m_WarnHighPing;
  std::optional<uint32_t>               m_SafeHighPing;

  std::optional<uint8_t>                m_LobbyTimeoutMode;
  std::optional<uint8_t>                m_LobbyOwnerTimeoutMode;
  std::optional<uint8_t>                m_LoadingTimeoutMode;
  std::optional<uint8_t>                m_PlayingTimeoutMode;

  std::optional<uint32_t>               m_LobbyTimeout;
  std::optional<uint32_t>               m_LobbyOwnerTimeout;
  std::optional<uint32_t>               m_LoadingTimeout;
  std::optional<uint32_t>               m_PlayingTimeout;

  std::optional<uint8_t>                m_PlayingTimeoutWarningShortCountDown;
  std::optional<uint32_t>               m_PlayingTimeoutWarningShortInterval;
  std::optional<uint8_t>                m_PlayingTimeoutWarningLargeCountDown;
  std::optional<uint32_t>               m_PlayingTimeoutWarningLargeInterval;

  std::optional<bool>                   m_LobbyOwnerReleaseLANLeaver;

  std::optional<uint32_t>               m_LobbyCountDownInterval;
  std::optional<uint32_t>               m_LobbyCountDownStartValue;

  std::optional<uint16_t>               m_Latency;
  std::optional<bool>                   m_LatencyEqualizerEnabled;
  std::optional<uint8_t>                m_LatencyEqualizerFrames;

  std::optional<int64_t>                m_AutoStartSeconds;
  std::optional<uint8_t>                m_AutoStartPlayers;
  std::optional<bool>                   m_HideLobbyNames;
  std::optional<uint8_t>                m_HideInGameNames;
  std::optional<bool>                   m_LoadInGame;
  std::optional<bool>                   m_EnableJoinObserversInProgress;
  std::optional<bool>                   m_EnableJoinPlayersInProgress;

  std::optional<bool>                   m_LogCommands;
  std::optional<uint8_t>                m_ReconnectionMode;
  std::optional<uint8_t>                m_IPFloodHandler;
  std::optional<uint8_t>                m_UnsafeNameHandler;
  std::optional<uint8_t>                m_BroadcastErrorHandler;
  std::optional<bool>                   m_PipeConsideredHarmful;

private:
  std::array<uint8_t, 20>   m_MapScriptsSHA1;         // config value: <map.scripts_hash.sha1> (20 bytes)
  std::array<uint8_t, 4>    m_MapSize;                // config value: <map.size> (4 bytes)
  std::array<uint8_t, 4>    m_MapCRC32;               // config value: <map.file_hash.crc32> (4 bytes) -> this is the real full CRC
  std::array<uint8_t, 20>   m_MapSHA1;                // config value: <map.file_hash.sha1> (20 bytes) -> this is the real full SHA1
  std::array<uint8_t, 4>    m_MapScriptsWeakHash;     // config value: <map.scripts_hash.crc32> (4 bytes) -> this is not the real CRC, it's the "xoro" value
  std::array<uint8_t, 2>    m_MapWidth;               // config value: <map.width> (2 bytes)
  std::array<uint8_t, 2>    m_MapHeight;              // config value: <map.height> (2 bytes)
  std::vector<CGameSlot> m_Slots;
  std::string                     m_CFGName;
  std::string                     m_ClientMapPath;       // config value: map path
  std::string                     m_MapType;       // config value: map type (for stats class)
  bool                            m_MapMetaDataEnabled;
  std::string                     m_MapDefaultHCL; // config value: map default HCL to use
  std::filesystem::path           m_MapServerPath;  // config value: map local path
  SharedByteArray                 m_MapFileContents;       // the map data itself, for sending the map to players
  bool                            m_MapFileIsValid;
  bool                            m_MapLoaderIsPartial;
  uint32_t                        m_MapLocale;
  uint32_t                        m_MapOptions;
  uint32_t                        m_MapEditorVersion;
  uint8_t                         m_MapDataSet;
  bool                            m_MapIsLua;
  Version                         m_MapMinGameVersion;
  Version                         m_MapMinSuggestedGameVersion;
  uint8_t                         m_MapNumControllers; // config value: max map number of players
  uint8_t                         m_MapNumDisabled; // config value: slots that cannot be used - not even by observers
  uint8_t                         m_MapNumTeams;   // config value: max map number of teams
  uint8_t                         m_MapVersionMaxSlots;
  uint8_t                         m_MapSpeed;
  uint8_t                         m_MapVisibility;
  uint8_t                         m_MapObservers;
  uint8_t                         m_GameFlags;
  uint8_t                         m_MapFilterMaker;
  uint8_t                         m_MapFilterType;
  uint8_t                         m_MapFilterSize;
  uint8_t                         m_MapFilterObs;
  std::array<uint8_t, 5>          m_MapContentMismatch;
  uint32_t                        m_MapPrologueImageSize;
  uint32_t                        m_MapLoadingImageSize;
  std::string                     m_MapPrologueImagePath;
  std::string                     m_MapLoadingImagePath;
  std::string                     m_MapPrologueImageMimeType;
  std::string                     m_MapLoadingImageMimeType;
  std::string                     m_MapTitle;
  std::string                     m_MapAuthor;
  std::string                     m_MapDescription;
  std::string                     m_MapURL;
  std::string                     m_MapSiteURL;
  std::string                     m_MapShortDesc;
  void*                           m_MapMPQ;
  std::optional<bool>             m_MapMPQResult;
  bool                            m_UseStandardPaths;
  bool                            m_Valid;
  std::string                     m_ErrorMessage;
  uint8_t                         m_HMCMode;
  uint8_t                         m_HMCTrigger1;
  uint8_t                         m_HMCTrigger2;
  uint8_t                         m_HMCSlot;
  std::string                     m_HMCPlayerName;

public:
  CMap(CAura* nAura, CConfig* CFG);
  ~CMap();

  [[nodiscard]] inline bool                       GetValid() const { return m_Valid; }
  [[nodiscard]] inline bool                       HasMismatch() const { return m_MapContentMismatch[0] != 0 || m_MapContentMismatch[1] != 0 || m_MapContentMismatch[2] != 0 || m_MapContentMismatch[3] != 0 || m_MapContentMismatch[4] != 0; }
  [[nodiscard]] inline bool                       GetMPQSucceeded() const { return m_MapMPQResult.has_value() && m_MapMPQResult.value(); }
  [[nodiscard]] inline bool                       GetMPQErrored() const { return m_MapMPQResult.has_value() && !m_MapMPQResult.value(); }
  [[nodiscard]] inline std::string                GetConfigName() const { return m_CFGName; }
  [[nodiscard]] inline std::string                GetClientPath() const { return m_ClientMapPath; }
  [[nodiscard]] inline std::array<uint8_t, 4>     GetMapSize() const { return m_MapSize; }
  [[nodiscard]] inline std::array<uint8_t, 4>     GetMapCRC32() const { return m_MapCRC32; } // <map.file_hash.crc32>, but also legacy <map_hash> and <map.crc32>
  [[nodiscard]] inline std::array<uint8_t, 20>    GetMapSHA1() const { return m_MapSHA1; } // <map.file_hash.sha1>
  [[nodiscard]] inline std::array<uint8_t, 4>     GetMapScriptsWeakHash() const { return m_MapScriptsWeakHash; } // <map.scripts_hash.blizz>, but also legacy <map_crc>, <map.weak_hash>
  [[nodiscard]] inline std::array<uint8_t, 20>    GetMapScriptsSHA1() const { return m_MapScriptsSHA1; } // <map.scripts_hash.sha1>, but also legacy <map.sha1>
  [[nodiscard]] inline uint8_t                    GetMapVisibility() const { return m_MapVisibility; }
  [[nodiscard]] inline uint8_t                    GetMapSpeed() const { return m_MapSpeed; }
  [[nodiscard]] inline uint8_t                    GetMapObservers() const { return m_MapObservers; }
  [[nodiscard]] inline uint8_t                    GetMapFlags() const { return m_GameFlags; }
  [[nodiscard]] uint32_t                          GetGameConvertedFlags() const;
  [[nodiscard]] uint32_t                          GetMapGameType() const;
  [[nodiscard]] inline bool                       GetMapHasGameVersion() const { return m_MapTargetGameVersion.has_value(); }
  [[nodiscard]] inline Version                    GetMapTargetGameVersion() const { return m_MapTargetGameVersion.value(); }
  [[nodiscard]] inline uint32_t                   GetMapLocale() const { return m_MapLocale; }
  [[nodiscard]] inline uint32_t                   GetMapOptions() const { return m_MapOptions; }
  [[nodiscard]] inline uint8_t                    GetMapDataSet() const { return m_MapDataSet; }
  [[nodiscard]] inline const Version&             GetMapMinGameVersion() const { return m_MapMinGameVersion; }
  [[nodiscard]] inline const Version&             GetMapMinSuggestedGameVersion() const { return m_MapMinSuggestedGameVersion; }
  [[nodiscard]] uint8_t                           GetMapLayoutStyle() const;
  [[nodiscard]] inline std::array<uint8_t, 2>     GetMapWidth() const { return m_MapWidth; }
  [[nodiscard]] inline std::array<uint8_t, 2>     GetMapHeight() const { return m_MapHeight; }
  [[nodiscard]] inline std::string                GetMapType() const { return m_MapType; }
  [[nodiscard]] inline bool                       GetMapMetaDataEnabled() const { return m_MapMetaDataEnabled; }
  [[nodiscard]] inline std::string                GetMapDefaultHCL() const { return m_MapDefaultHCL; }
  [[nodiscard]] inline const std::filesystem::path&     GetServerPath() const { return m_MapServerPath; }
  [[nodiscard]] inline bool                       HasServerPath() const { return !m_MapServerPath.empty(); }
  [[nodiscard]] std::string                       GetServerFileName() const;
  [[nodiscard]] std::string                       GetClientFileName() const;
  [[nodiscard]] inline uint32_t                   GetMapPrologueImageSize() const { return m_MapPrologueImageSize; }
  [[nodiscard]] inline uint32_t                   GetMapLoadingImageSize() const { return m_MapLoadingImageSize; }
  [[nodiscard]] inline std::string                GetMapPrologueImagePath() const { return m_MapPrologueImagePath; }
  [[nodiscard]] inline std::string                GetMapLoadingImagePath() const { return m_MapLoadingImagePath; }
  [[nodiscard]] inline std::string                GetMapPrologueImageMimeType() const { return m_MapPrologueImageMimeType; }
  [[nodiscard]] inline std::string                GetMapLoadingImageMimeType() const { return m_MapLoadingImageMimeType; }
  [[nodiscard]] inline std::string                GetMapTitle() const { return m_MapTitle.empty() ? "Just another Warcraft 3 Map" : m_MapTitle; }
  [[nodiscard]] inline std::string                GetMapAuthor() const { return m_MapAuthor.empty() ? "Unknown" : m_MapAuthor; }
  [[nodiscard]] inline std::string                GetMapDescription() const { return m_MapDescription.empty() ? "Nondescript" : m_MapDescription; }
  [[nodiscard]] inline std::string                GetMapURL() const { return m_MapURL; }
  [[nodiscard]] inline std::string                GetMapSiteURL() const { return m_MapSiteURL; }
  [[nodiscard]] inline std::string                GetMapShortDesc() const { return m_MapShortDesc; }
  [[nodiscard]] inline bool                       GetMapFileIsValid() const { return m_MapFileIsValid; }
  [[nodiscard]] inline const SharedByteArray&     GetMapFileContents() { return m_MapFileContents; }
  [[nodiscard]] inline bool                       HasMapFileContents() const { return m_MapFileContents != nullptr && !m_MapFileContents->empty(); }
  [[nodiscard]] bool                              GetMapFileIsFromManagedFolder() const;
  [[nodiscard]] inline uint8_t                    GetMapNumDisabled() const { return m_MapNumDisabled; }
  [[nodiscard]] inline uint8_t                    GetMapNumControllers() const { return m_MapNumControllers; }
  [[nodiscard]] inline uint8_t                    GetMapNumTeams() const { return m_MapNumTeams; }
  [[nodiscard]] inline uint8_t                    GetVersionMaxSlots() const { return m_MapVersionMaxSlots; }
  [[nodiscard]] inline std::vector<CGameSlot>     GetSlots() const { return m_Slots; }
  [[nodiscard]] bool                              GetHMCEnabled() const { return m_HMCMode != W3HMC_MODE_DISABLED; }
  [[nodiscard]] bool                              GetHMCRequired() const { return m_HMCMode == W3HMC_MODE_REQUIRED; }
  [[nodiscard]] uint8_t                           GetHMCMode() const { return m_HMCMode; }
  [[nodiscard]] uint8_t                           GetHMCTrigger1() const { return m_HMCTrigger1; }
  [[nodiscard]] uint8_t                           GetHMCTrigger2() const { return m_HMCTrigger2; }
  [[nodiscard]] uint8_t                           GetHMCSlot() const { return m_HMCSlot; }
  [[nodiscard]] std::string                       GetHMCPlayerName() const { return m_HMCPlayerName; }
  [[nodiscard]] uint8_t                           GetLobbyRace(const CGameSlot* slot) const;
  [[nodiscard]] bool                              GetUseStandardPaths() const { return m_UseStandardPaths; }
  void                                            ClearMapFileContents() { m_MapFileContents.reset(); }
  bool                                            SetTeamsLocked(const bool nEnable);
  bool                                            SetTeamsTogether(const bool nEnable);
  bool                                            SetAdvancedSharedUnitControl(const bool nEnable);
  bool                                            SetRandomRaces(const bool nEnable);
  bool                                            SetRandomHeroes(const bool nEnable);
  bool                                            SetMapVisibility(const uint8_t nMapVisibility);
  bool                                            SetMapSpeed(const uint8_t nMapSpeed);
  bool                                            SetMapObservers(const uint8_t nMapObservers);
  void                                            SetUseStandardPaths(const bool nValue) { m_UseStandardPaths = nValue; }
  [[nodiscard]] bool                              IsObserverSlot(const CGameSlot* slot) const;
  bool                                            NormalizeSlots();
  [[nodiscard]] inline std::string                GetErrorString() { return m_ErrorMessage; }

  void                                            UpdateCrypto(std::map<Version, MapCrypto>& cryptos, const std::string& fileContents) const;
  void                                            UpdateCryptoEndModules(std::map<Version, MapCrypto>& cryptos) const;
  void                                            ErroredCrypto(std::map<Version, MapCrypto>& cryptos) const;

  void                                            ReadFileFromArchive(std::vector<uint8_t>& container, const std::string& fileSubPath) const;
  void                                            ReadFileFromArchive(std::string& container, const std::string& fileSubPath) const;
  void                                            ReplaceTriggerStrings(std::string& container, std::vector<std::string*>& maybeWTSRefs) const;
  std::optional<MapEssentials>                    ParseMPQFromPath(const std::filesystem::path& filePath);
  std::optional<MapEssentials>                    ParseMPQ();
  bool AcquireGameVersion(CConfig* CFG);
  void Load(CConfig* CFG);
  void LoadGameConfigOverrides(CConfig& CFG);
  void LoadMapSpecificConfig(CConfig& CFG);

  [[nodiscard]] bool                              TryLoadMapFilePersistent(std::optional<uint32_t>& fileSize, std::optional<uint32_t>& crc32);
  [[nodiscard]] bool                              TryLoadMapFileChunked(std::optional<uint32_t>& fileSize, std::optional<uint32_t>& crc32, std::optional<std::array<uint8_t, 20>>& sha1);
  [[nodiscard]] bool                              CheckMapFileIntegrity();
  void                                            InvalidateMapFile() { m_MapFileIsValid = false; }
  [[nodiscard]] FileChunkTransient                GetMapFileChunk(size_t start);
  [[nodiscard]] std::pair<bool, uint32_t>         ProcessMapChunked(const std::filesystem::path& filePath, std::function<void(FileChunkTransient, size_t, size_t)> processChunk);
  bool                                            UnlinkFile();
  [[nodiscard]] std::string                       CheckProblems();

  [[nodiscard]] inline static std::optional<uint32_t> GetTrigStrNum(const std::string& text)
  {
    std::optional<uint32_t> result;
    if (text.size() < 9 || text.substr(0, 8) != "TRIGSTR_") {
      return result;
    }
    std::string encodedNum = text.substr(8);
    int64_t num;
    try {
      num = std::stol(encodedNum);
    } catch (...) {
      return result;
    }
    if (num < 0 || num > 0xFFFFFFFF) {
      return result;
    }
    result = num;
    return result;
  }

  [[nodiscard]] static std::string SanitizeTrigStr(const std::string& input);
  [[nodiscard]] static std::optional<std::string> GetTrigStr(const std::string& fileContents, const uint32_t targetNum);
  [[nodiscard]] static std::map<uint32_t, std::string> GetTrigStrMulti(const std::string& fileContents, const std::set<uint32_t> captureTargets);
};

[[nodiscard]] inline Version GetNextVersion(const Version& version)
{
  if (version.first == 1 && version.second == 36) {
    // v1.36 .. v2.0
    return Version(2u, 0u);
  } else {
    return Version(version.first, version.second + 1);
  }
}

[[nodiscard]] inline Version GetScriptsVersionRangeHead(const Version& version)
{
  if (version.first != 1) return version;
  if (19 <= version.second && version.second <= 21) {
    return Version(1u, 19u);
  }
  if (24 <= version.second && version.second <= 28) {
    return Version(1u, 24u);
  }
  return version;
}

[[nodiscard]] inline std::string GetScriptsVersionRangeHeadString(const Version& version)
{
  if (version.first != 1) return ToVersionString(version);
  if (19 <= version.second && version.second <= 21) {
    return ToVersionString(Version(1u, 19u));
  }
  if (24 <= version.second && version.second <= 28) {
    return ToVersionString(Version(1u, 24u));
  }
  return ToVersionString(version);
}

[[nodiscard]] inline uint32_t XORRotateLeft(const uint8_t* data, const uint32_t length)
{
  // a big thank you to Strilanc for figuring this out

  uint32_t i   = 0;
  uint32_t Val = 0;

  if (length > 3) {
    while (i < length - 3) {
      Val = ROTL(Val ^ ((uint32_t)data[i] + (uint32_t)(data[i + 1] << 8) + (uint32_t)(data[i + 2] << 16) + (uint32_t)(data[i + 3] << 24)), 3);
      i += 4;
    }
  }

  while (i < length) {
    Val = ROTL(Val ^ data[i], 3);
    ++i;
  }

  return Val;
}

[[nodiscard]] inline uint32_t ChunkedChecksum(const uint8_t* data, const int32_t length, uint32_t checksum)
{
  int32_t cursor = 0;
  int32_t t = length - 0x400;
  while (cursor <= t) {
    checksum = ROTL(checksum ^ XORRotateLeft(data + cursor, 0x400), 3);
    cursor += 0x400;
  }
  return checksum;
}

#undef ROTL
#undef ROTR

#endif // AURA_MAP_H_
