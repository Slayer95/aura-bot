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

#include "map.h"
#include "aura.h"
#include "util.h"
#include "file_util.h"
#include "game_setup.h"
#include "game_slot.h"
#include "pjass.h"
#include <crc32/crc32.h>
#include <sha1/sha1.h>
#include "config/config.h"
#include "config/config_bot.h"
#include "config/config_game.h"

#define __STORMLIB_SELF__
#include <StormLib.h>

#define ROTL(x, n) ((x) << (n)) | ((x) >> (32 - (n))) // this won't work with signed types
#define ROTR(x, n) ((x) >> (n)) | ((x) << (32 - (n))) // this won't work with signed types

using namespace std;

//
// CMap
//

CMap::CMap(CAura* nAura, CConfig* CFG)
  : m_Aura(nAura),
    m_MapSize(0),
    m_MapServerPath(CFG->GetPath("map.local_path", filesystem::path())),
    m_MapFileIsValid(false),
    m_MapLoaderIsPartial(CFG->GetBool("map.cfg.partial", false)), // from CGameSetup or !cachemaps
    m_MapLocale(CFG->GetUint32("map.locale", 0)),
    m_MapOptions(0),
    m_MapEditorVersion(0),
    m_MapDataSet(MAP_DATASET_DEFAULT),
    m_MapRequiresExpansion(false),
    m_MapIsLua(false),
    m_MapIsMelee(false),
    m_MapMinGameVersion(GAMEVER(1u, 0u)),
    m_MapMinSuggestedGameVersion(GAMEVER(1u, 0u)),
    m_MapNumControllers(0),
    m_MapNumDisabled(0),
    m_MapNumTeams(0),
    m_MapObservers(MAPOBS_NONE),
    m_GameFlags(MAPFLAG_TEAMSTOGETHER | MAPFLAG_FIXEDTEAMS),
    m_MapFilterType(MAPFILTER_TYPE_SCENARIO),
    m_MapFilterObs(MAPFILTER_OBS_NONE),
    m_MapPreviewImageSize(0),
    m_MapPreviewImagePathType(MAP_FILE_SOURCE_CATEGORY_NONE),
    //m_MapPrologueImageSize(0),
    //m_MapLoadingImageSize(0),
    m_MapMPQ(nullptr),
    m_UseStandardPaths(CFG->GetBool("map.standard_path", false)),
    m_JASSValid(false)
{
  m_MapWidth.fill(0);
  m_MapHeight.fill(0);
  m_MapCRC32.fill(0);
  m_MapSHA1.fill(0);
  m_MapContentMismatch.fill(0);

  Load(CFG);
}

CMap::~CMap() = default;

const array<uint8_t, 4>& CMap::GetMapScriptsBlizz(const Version& nVersion) const
{
  Version headVersion = GetScriptsVersionRangeHead(nVersion);
  auto it = m_MapScriptsBlizz.find(headVersion);
  return it->second;
};

const array<uint8_t, 20>& CMap::GetMapScriptsSHA1(const Version& nVersion) const
{
  Version headVersion = GetScriptsVersionRangeHead(nVersion);
  auto it = m_MapScriptsSHA1.find(headVersion);
  return it->second;
};

bool CMap::GetMapIsGameVersionSupported(const Version& nVersion) const
{
  Version headVersion = GetScriptsVersionRangeHead(nVersion);
  return m_MapMinGameVersion < nVersion && (m_MapScriptsBlizz.find(headVersion) != m_MapScriptsBlizz.end()) && (m_MapScriptsSHA1.find(headVersion) != m_MapScriptsSHA1.end());
};

uint32_t CMap::GetMapSizeClamped(const Version& nVersion) const
{
  if (nVersion <= GAMEVER(1u, 23u)) {
    if (m_MapSize > MAX_MAP_SIZE_1_23) return MAX_MAP_SIZE_1_23;
  } else if (nVersion <= GAMEVER(1u, 26u)) {
    if (m_MapSize > MAX_MAP_SIZE_1_26) return MAX_MAP_SIZE_1_26;
  } else if (nVersion <= GAMEVER(1u, 28u)) {
    if (m_MapSize > MAX_MAP_SIZE_1_28) return MAX_MAP_SIZE_1_28;
  }
  return m_MapSize;
}

bool CMap::GetMapSizeIsNativeSupported(const Version& nVersion) const
{
  if (nVersion <= GAMEVER(1u, 23u)) {
    if (m_MapSize > MAX_MAP_SIZE_1_23) return false;
  } else if (nVersion <= GAMEVER(1u, 26u)) {
    if (m_MapSize > MAX_MAP_SIZE_1_26) return false;
  } else if (nVersion <= GAMEVER(1u, 28u)) {
    if (m_MapSize > MAX_MAP_SIZE_1_28) return false;
  }
  return true;
}

uint32_t CMap::GetGameConvertedFlags() const
{
  uint32_t gameFlags = 0;

  // speed

  if (m_MapSpeed == MAPSPEED_SLOW)
    gameFlags = 0x00000000;
  else if (m_MapSpeed == MAPSPEED_NORMAL)
    gameFlags = 0x00000001;
  else
    gameFlags = 0x00000002;

  // visibility

  if (m_MapVisibility == MAPVIS_HIDETERRAIN)
    gameFlags |= 0x00000100;
  else if (m_MapVisibility == MAPVIS_EXPLORED)
    gameFlags |= 0x00000200;
  else if (m_MapVisibility == MAPVIS_ALWAYSVISIBLE)
    gameFlags |= 0x00000400;
  else
    gameFlags |= 0x00000800;

  // observers

  if (m_MapObservers == MAPOBS_ONDEFEAT)
    gameFlags |= 0x00002000;
  else if (m_MapObservers == MAPOBS_ALLOWED)
    gameFlags |= 0x00003000;
  else if (m_MapObservers == MAPOBS_REFEREES)
    gameFlags |= 0x40000000;

  // teams/units/hero/race

  if (m_GameFlags & MAPFLAG_TEAMSTOGETHER) {
    gameFlags |= 0x00004000;
  }

  if (m_GameFlags & MAPFLAG_FIXEDTEAMS)
    gameFlags |= 0x00060000;

  if (m_GameFlags & MAPFLAG_UNITSHARE)
    gameFlags |= 0x01000000;

  if (m_GameFlags & MAPFLAG_RANDOMHERO)
    gameFlags |= 0x02000000;

  if (!(m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS)) {
    // WC3 GUI is misleading in displaying the Random Races tickbox when creating LAN games.
    // It even shows Random Races: Yes in the game lobby.
    // However, this flag is totally ignored when Fixed Player Settings is enabled.
    if (m_GameFlags & MAPFLAG_RANDOMRACES)
      gameFlags |= 0x04000000;
  }

  return gameFlags;
}

uint32_t CMap::GetMapGameType() const
{
  /* spec by Strilanc as follows:

    Public Enum GameTypes As UInteger
        None = 0
        Unknown0 = 1 << 0 '[always seems to be set?]

        '''<summary>Setting this bit causes wc3 to check the map and disc if it is not signed by Blizzard</summary>
        AuthenticatedMakerBlizzard = 1 << 3
        OfficialMeleeGame = 1 << 5

        SavedGame = 1 << 9
        PrivateGame = 1 << 11

        MakerUser = 1 << 13
        MakerBlizzard = 1 << 14
        TypeMelee = 1 << 15
        TypeScenario = 1 << 16
        SizeSmall = 1 << 17
        SizeMedium = 1 << 18
        SizeLarge = 1 << 19
        ObsFull = 1 << 20
        ObsOnDeath = 1 << 21
        ObsNone = 1 << 22

        MaskObs = ObsFull Or ObsOnDeath Or ObsNone
        MaskMaker = MakerBlizzard Or MakerUser
        MaskType = TypeMelee Or TypeScenario
        MaskSize = SizeLarge Or SizeMedium Or SizeSmall
        MaskFilterable = MaskObs Or MaskMaker Or MaskType Or MaskSize
    End Enum

   */

  // note: we allow "conflicting" flags to be set at the same time (who knows if this is a good idea)
  // we also don't set any flags this class is unaware of such as Unknown0, SavedGame, and PrivateGame

  uint32_t GameType = 0;

  // maker

  if (m_MapFilterMaker & MAPFILTER_MAKER_USER)
    GameType |= MAPGAMETYPE_MAKERUSER;

  if (m_MapFilterMaker & MAPFILTER_MAKER_BLIZZARD)
    GameType |= MAPGAMETYPE_MAKERBLIZZARD;

  // type

  if (m_MapFilterType & MAPFILTER_TYPE_MELEE)
    GameType |= MAPGAMETYPE_TYPEMELEE;

  if (m_MapFilterType & MAPFILTER_TYPE_SCENARIO)
    GameType |= MAPGAMETYPE_TYPESCENARIO;

  // size

  if (m_MapFilterSize & MAPFILTER_SIZE_SMALL)
    GameType |= MAPGAMETYPE_SIZESMALL;

  if (m_MapFilterSize & MAPFILTER_SIZE_MEDIUM)
    GameType |= MAPGAMETYPE_SIZEMEDIUM;

  if (m_MapFilterSize & MAPFILTER_SIZE_LARGE)
    GameType |= MAPGAMETYPE_SIZELARGE;

  // obs

  if (m_MapFilterObs & MAPFILTER_OBS_FULL)
    GameType |= MAPGAMETYPE_OBSFULL;

  if (m_MapFilterObs & MAPFILTER_OBS_ONDEATH)
    GameType |= MAPGAMETYPE_OBSONDEATH;

  if (m_MapFilterObs & MAPFILTER_OBS_NONE)
    GameType |= MAPGAMETYPE_OBSNONE;

  return GameType;
}

uint8_t CMap::GetMapLayoutStyle() const
{
  // 0 = melee
  // 1 = custom forces
  // 2 = fixed player settings (not possible with the Warcraft III design)
  // 3 = custom forces + fixed player settings

  if (!(m_MapOptions & MAPOPT_CUSTOMFORCES))
    return MAPLAYOUT_ANY;

  if (!(m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS))
    return MAPLAYOUT_CUSTOM_FORCES;

  return MAPLAYOUT_FIXED_PLAYERS;
}

string CMap::GetServerFileName() const
{
  return PathToString(m_MapServerPath.filename());
}

string CMap::GetClientFileName() const
{
  size_t LastSlash = m_ClientMapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_ClientMapPath;
  }
  return m_ClientMapPath.substr(LastSlash + 1);
}

[[nodiscard]] filesystem::path CMap::GetResolvedServerPath() const
{
  filesystem::path resolvedFilePath(m_MapServerPath);
  if (resolvedFilePath.filename() == resolvedFilePath && !m_UseStandardPaths) {
    resolvedFilePath = m_Aura->m_Config.m_MapPath / resolvedFilePath;
  }    
  return resolvedFilePath;
}

SharedByteArray CMap::GetMapPreviewContents()
{
  switch (GetMapPreviewImagePathType()) {
    case MAP_FILE_SOURCE_CATEGORY_MPQ:
    {
      bool isTempMPQ = m_MapMPQ == nullptr;
      if (isTempMPQ && !OpenMPQArchive(&m_MapMPQ, GetResolvedServerPath())) {
        return nullptr;
      }
      SharedByteArray fileContentsPtr = make_shared<vector<uint8_t>>();
      fileContentsPtr->reserve(GetMapPreviewImageSize());
      ReadFileFromArchive(*(fileContentsPtr.get()), GetMapPreviewImagePath());
      if (isTempMPQ) {
        SFileCloseArchive(m_MapMPQ);
        m_MapMPQ = nullptr;
      }
      return fileContentsPtr;
    }
    case MAP_FILE_SOURCE_CATEGORY_FS:
      return m_Aura->ReadFile(GetMapPreviewImagePath(), GetMapPreviewImageSize());
    default:
      return nullptr;
  }
}

bool CMap::GetMapFileIsFromManagedFolder() const
{
  if (m_UseStandardPaths) return false;
  if (m_MapServerPath.empty()) return false;
  return m_MapServerPath == m_MapServerPath.filename();
}

bool CMap::IsObserverSlot(const CGameSlot* slot) const
{
  if (slot->GetUID() != 0 || slot->GetDownloadStatus() != 255) {
    return false;
  }
  if (slot->GetSlotStatus() != SLOTSTATUS_OPEN || !slot->GetIsSelectable()) {
    return false;
  }
  return slot->GetTeam() >= m_MapNumControllers && slot->GetColor() >= m_MapNumControllers;
}

bool CMap::NormalizeSlots()
{
  uint8_t i = static_cast<uint8_t>(m_Slots.size());

  bool updated = false;
  bool anyNonObserver = false;
  while (i--) {
    const CGameSlot slot = m_Slots[i];
    if (!IsObserverSlot(&slot)) {
      anyNonObserver = true;
      break;
    }
  }

  i = static_cast<uint8_t>(m_Slots.size());
  while (i--) {
    CGameSlot slot = m_Slots[i];
    if (anyNonObserver && IsObserverSlot(&slot)) {
      m_Slots.erase(m_Slots.begin() + i);
      updated = true;
      continue;
    }
    uint8_t race = GetLobbyRace(&slot);
    if (race != slot.GetRace()) {
      slot.SetRace(race);
      updated = true;
    }
  }

  return updated;
}

bool CMap::SetMapObservers(const uint8_t nMapObservers)
{
  switch (nMapObservers) {
    case MAPOBS_ALLOWED:
    case MAPOBS_REFEREES:
      m_MapObservers = nMapObservers;
      m_MapFilterObs = MAPFILTER_OBS_FULL;
      break;
    case MAPOBS_NONE:
      m_MapObservers = nMapObservers;
      m_MapFilterObs = MAPFILTER_OBS_NONE;
      break;
    case MAPOBS_ONDEFEAT:
      m_MapObservers = nMapObservers;
      m_MapFilterObs = MAPFILTER_OBS_ONDEATH;
      break;
    default:
      m_MapObservers = nMapObservers;
      return false;
  }
  return true;
}

bool CMap::SetMapVisibility(const uint8_t nMapVisibility)
{
  m_MapVisibility = nMapVisibility;
  return true;
}

bool CMap::SetMapSpeed(const uint8_t nMapSpeed)
{
  m_MapSpeed = nMapSpeed;
  return true;
}

bool CMap::SetTeamsLocked(const bool nEnable)
{
  if (nEnable) {
    m_GameFlags |= MAPFLAG_FIXEDTEAMS;
  } else {
    m_GameFlags &= ~MAPFLAG_FIXEDTEAMS;
  }
  return true;
}

bool CMap::SetTeamsTogether(const bool nEnable)
{
  if (nEnable) {
    m_GameFlags |= MAPFLAG_TEAMSTOGETHER;
  } else {
    m_GameFlags &= ~MAPFLAG_TEAMSTOGETHER;
  }
  return true;
}

bool CMap::SetAdvancedSharedUnitControl(const bool nEnable)
{
  if (nEnable) {
    m_GameFlags |= MAPFLAG_UNITSHARE;
  } else {
    m_GameFlags &= ~MAPFLAG_UNITSHARE;
  }
  return true;
}

bool CMap::SetRandomHeroes(const bool nEnable)
{
  if (nEnable) {
    m_GameFlags |= MAPFLAG_RANDOMHERO;
  } else {
    m_GameFlags &= ~MAPFLAG_RANDOMHERO;
  }
  return true;
}

bool CMap::SetRandomRaces(const bool nEnable)
{
  if (m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) {
    return false;
  }
  if (nEnable) {
    m_GameFlags |= MAPFLAG_RANDOMRACES;
  } else {
    m_GameFlags &= ~MAPFLAG_RANDOMRACES;
  }
  return true;
}

optional<MapEssentials> CMap::ParseMPQFromPath(const filesystem::path& filePath)
{
  m_MapMPQResult = OpenMPQArchive(&m_MapMPQ, filePath);
  if (GetMPQSucceeded()) {
    optional<MapEssentials> mapEssentials = ParseMPQ();
    SFileCloseArchive(m_MapMPQ);
    m_MapMPQ = nullptr;
    return mapEssentials;
  }

  m_MapMPQ = nullptr;
#ifdef _WIN32
  uint32_t errorCode = (uint32_t)GetLastOSError();
  string errorCodeString = (
    errorCode == 2 ? "Map not found" : (
    errorCode == 11 ? "File is corrupted." : (
    (errorCode == 3 || errorCode == 15) ? "Config error: <bot.maps_path> is not a valid directory" : (
    (errorCode == 32 || errorCode == 33) ? "File is currently opened by another process." : (
    "Error code " + to_string(errorCode)
    ))))
  );
#else
  int32_t errorCode = static_cast<int32_t>(GetLastOSError());
  string errorCodeString = "Error code " + to_string(errorCode);
#endif
  Print("[MAP] warning - unable to load MPQ archive [" + PathToString(filePath) + "] - " + errorCodeString);

  return nullopt;
}

void CMap::UpdateCryptoModule(map<Version, MapCrypto>::iterator& versionCrypto, const string& fileContents) const
{
  versionCrypto->second.blizz = versionCrypto->second.blizz ^ XORRotateLeft((const uint8_t*)fileContents.data(), static_cast<uint32_t>(fileContents.size()));
  versionCrypto->second.sha1.Update((const uint8_t*)fileContents.data(), static_cast<uint32_t>(fileContents.size()));
}

void CMap::UpdateCryptoEndModules(map<Version, MapCrypto>::iterator& versionCrypto) const
{
  versionCrypto->second.blizz = ROTL(versionCrypto->second.blizz, 3);
  versionCrypto->second.blizz = ROTL(versionCrypto->second.blizz ^ 0x03F1379E, 3);
  versionCrypto->second.sha1.Update((const uint8_t*)"\x9E\x37\xF1\x03", 4);
}

void CMap::UpdateCryptoScripts(map<Version, MapCrypto>& cryptos, const Version& version, const string& commonJ, const string& blizzardJ, const string& war3mapJ) const
{
  auto match = cryptos.find(version);
  if (match == cryptos.end()) return; // should never happen

  // Scripts\common.j either from map file or from file system
  if (commonJ.empty()) {
    match->second.errored = true;
  } else {
    UpdateCryptoModule(match, commonJ);
  }

  // Scripts\Blizzard.j either from map file or from file system
  if (blizzardJ.empty()) {
    match->second.errored = true;
  } else {
    UpdateCryptoModule(match, blizzardJ);
  }

  // Padding sequence between blizzard.j and war3map.j (0x03F1379E)
  UpdateCryptoEndModules(match);

  if (version >= GAMEVER(1u, 32u)) {
    // Credits to Fingon for the checksum algorithm
    match->second.blizz = XORRotateLeft(reinterpret_cast<const uint8_t*>(war3mapJ.data()), war3mapJ.size());
  } else {
    match->second.blizz = ROTL(match->second.blizz ^ XORRotateLeft(reinterpret_cast<const uint8_t*>(war3mapJ.data()), war3mapJ.size()), 3);
  }

  match->second.sha1.Update(reinterpret_cast<const uint8_t*>(war3mapJ.data()), war3mapJ.size());
}

void CMap::UpdateCryptoNonScripts(map<Version, MapCrypto>& cryptos, const Version& version, const string& fileContents) const
{
  auto match = cryptos.find(version);
  if (match == cryptos.end()) return; // should never happen

  // Credits to Fingon, BogdanW3 for the checksum algorithm
  if (version == GAMEVER(1u, 32u)) {
    match->second.blizz = ChunkedChecksum(reinterpret_cast<const uint8_t*>(fileContents.data()), fileContents.size(), match->second.blizz);
  } else {
    match->second.blizz = ROTL(match->second.blizz ^ XORRotateLeft(reinterpret_cast<const uint8_t*>(fileContents.data()), fileContents.size()), 3);
  }
  match->second.sha1.Update(reinterpret_cast<const uint8_t*>(fileContents.data()), fileContents.size());
}

#ifndef DISABLE_PJASS
void CMap::OnLoadMPQSubFile(optional<MapEssentials>& mapEssentials, map<Version, MapCrypto>& cryptos, const vector<Version>& supportedVersionHeads, const string& fileContents, const bool isMapScript)
#else
void CMap::OnLoadMPQSubFile(optional<MapEssentials>& /*mapEssentials*/, map<Version, MapCrypto>& cryptos, const vector<Version>& supportedVersionHeads, const string& fileContents, const bool isMapScript)
#endif
{
  if (!isMapScript) {
    for (const auto& version: supportedVersionHeads) {
      UpdateCryptoNonScripts(cryptos, version, fileContents);
    }
  } else {
    // Load common.j, blizzard.j as soon as we load war3map.j
    string mapCommonJ, mapBlizzardJ;
    ReadFileFromArchive(mapCommonJ, R"(Scripts\common.j)");
    ReadFileFromArchive(mapBlizzardJ, R"(Scripts\blizzard.j)");

    for (const auto& version: supportedVersionHeads) {
      string baseCommonJ, baseBlizzardJ;
      string* commonJ = &mapCommonJ;
      string* blizzardJ = &mapBlizzardJ;
      if (mapCommonJ.empty()) {
        filesystem::path commonPath = m_Aura->m_Config.m_JASSPath / filesystem::path("common-" + ToVersionString(version) +".j");
        if (FileRead(commonPath, baseCommonJ, MAX_READ_FILE_SIZE) && !baseCommonJ.empty()) {
          commonJ = &baseCommonJ;
        }
      }

      if (mapBlizzardJ.empty()) {
        filesystem::path blizzardPath = m_Aura->m_Config.m_JASSPath / filesystem::path("blizzard-" + ToVersionString(version) +".j");
        if (FileRead(blizzardPath, baseBlizzardJ, MAX_READ_FILE_SIZE) && !baseBlizzardJ.empty()) {
          blizzardJ = &baseBlizzardJ;
        }
      }

      UpdateCryptoScripts(cryptos, version, *commonJ, *blizzardJ, fileContents);

      if (!m_Aura->m_Config.m_ValidateJASS) {
        m_JASSValid = true;
#ifndef DISABLE_PJASS
      } else if (!m_JASSValid) {
        pair<bool, string> result = ParseJASS(*commonJ, *blizzardJ, fileContents, m_Aura->m_Config.m_ValidateJASSFlags, version);
        if (!result.first) {
          m_JASSErrorMessage = ExtractFirstJASSError(result.second);
        } else {
          mapEssentials->minCompatibleGameVersion = version;
          m_JASSValid = true;
          m_JASSErrorMessage.clear();
        }
#endif
      }
    }
  }
}

void CMap::ReadFileFromArchive(vector<uint8_t>& container, const string& fileSubPath) const
{
  const char* path = fileSubPath.c_str();
  ReadMPQFile(m_MapMPQ, path, container, m_MapLocale);
}

void CMap::ReadFileFromArchive(string& container, const string& fileSubPath) const
{
  const char* path = fileSubPath.c_str();
  ReadMPQFile(m_MapMPQ, path, container, m_MapLocale);
}

optional<uint32_t> CMap::GetFileSizeFromArchive(const std::string& fileSubPath) const
{
  const char* path = fileSubPath.c_str();
  return GetMPQFileSize(m_MapMPQ, path, m_MapLocale);
}

void CMap::ReplaceTriggerStrings(string& container, vector<string*>& maybeWTSRefs) const
{
  set<uint32_t> trigStrTargets;
  map<uint32_t, vector<string*>> numToStrings;
  for (string* maybeWTSRef : maybeWTSRefs) {
    optional<uint32_t> maybeWTSNum = CMap::GetTrigStrNum(*maybeWTSRef);
    if (!maybeWTSNum.has_value()) {
      continue;
    }
    uint32_t num = maybeWTSNum.value();
    if (trigStrTargets.find(num) == trigStrTargets.end()) {
      numToStrings[num] = vector<string*>();
      trigStrTargets.insert(num);
    }
    auto it = numToStrings.find(num);
    it->second.push_back(maybeWTSRef);
  }
  if (trigStrTargets.empty()) {
    return;
  }

  ReadFileFromArchive(container, "war3map.wts");
  if (container.empty()) {
    return;
  }

  map<uint32_t, string> mappings = CMap::GetTrigStrMulti(container, trigStrTargets);

  for (const auto& mapping : mappings) {
    auto wtsBackRefs = numToStrings.find(mapping.first);
    for (string* wtsRef : wtsBackRefs->second) {
      *wtsRef = mapping.second;
    }
  }
}

optional<MapEssentials> CMap::ParseMPQ()
{
  optional<MapEssentials> mapEssentials;
  if (!m_MapMPQ) return mapEssentials;

  mapEssentials.emplace();

  // calculate <map.scripts_hash.blizz.vN>, and <map.scripts_hash.sha1.vN>
  // a big thank you to Strilanc for figuring the <map.scripts_hash.blizz.vN> algorithm out

  vector<Version> supportedVersionHeads = m_Aura->GetSupportedVersionsCrossPlayRangeHeads();
  map<Version, MapCrypto> cryptos;
  for (const auto& version : supportedVersionHeads) {
    //cryptos[version].emplace(version, MapCrypto());
    cryptos[version];
  }

  string fileContents;
  {
    bool foundScript = false;
    vector<string> fileList;
    fileList.emplace_back("war3map.j");
    fileList.emplace_back(R"(scripts\war3map.j)");
    fileList.emplace_back("war3map.lua");
    fileList.emplace_back(R"(scripts\war3map.lua)");
    fileList.emplace_back("war3map.w3e");
    fileList.emplace_back("war3map.wpm");
    fileList.emplace_back("war3map.doo");
    fileList.emplace_back("war3map.w3u");
    fileList.emplace_back("war3map.w3b");
    fileList.emplace_back("war3map.w3d");
    fileList.emplace_back("war3map.w3a");
    fileList.emplace_back("war3map.w3q");

    for (const auto& fileName : fileList) {
      const bool isMapScript = GetMPQPathIsMapScript(fileName);
      if (isMapScript) {
        // only one map script file is used if there are more than one
        // JASS is preferred over Lua; root is preferred over Scripts folder
        if (foundScript) continue;
      } else if (!foundScript) {
        m_Valid = false;
        m_ErrorMessage = "war3map.j or war3map.lua not found in MPQ archive";
        break;
      }
      ReadFileFromArchive(fileContents, fileName);
      if (fileContents.empty()) continue;
      if (isMapScript) {
        foundScript = true;
        mapEssentials->foundLua = fileName.substr(fileName.size() - 4) == ".lua";
      }
      OnLoadMPQSubFile(mapEssentials, cryptos, supportedVersionHeads, fileContents, isMapScript);
    }

    for (const auto& version : supportedVersionHeads) {
      auto mapCryptoProcessor = cryptos.find(version);
      //mapEssentials->fragmentHashes.emplace(version, MapFragmentHashes());
      mapEssentials->fragmentHashes[version];
      // make sure to instantiate MapFragmentHashes anyway, so that mapEssentials is in a valid state
      // (note: contents are wrapped in std::optional)
      if (mapCryptoProcessor->second.errored) {
        PRINT_IF(LOG_LEVEL_WARNING, "[MAP] unable to calculate <map.scripts_hash.blizz.v" + ToVersionString(version) + ">, and <map.scripts_hash.sha1.v" + ToVersionString(version) + ">")
        continue;
      }
      auto mapCryptoResults = mapEssentials->fragmentHashes.find(version);
      EnsureFixedByteArray(mapCryptoResults->second.blizz, mapCryptoProcessor->second.blizz, false);
      DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.scripts_hash.blizz.v" + ToVersionString(version) + " = " + ByteArrayToDecString(mapCryptoResults->second.blizz.value()) + ">")

      mapCryptoProcessor->second.sha1.Final();
      mapCryptoResults->second.sha1.emplace();
      mapCryptoResults->second.sha1->fill(0);
      mapCryptoProcessor->second.sha1.GetHash(mapCryptoResults->second.sha1->data());
      DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.scripts_hash.sha1.v" + ToVersionString(version) + " = " + ByteArrayToDecString(mapCryptoResults->second.sha1.value()) + ">")
    }

    if (!m_JASSValid && m_ErrorMessage.empty()) {
      m_Valid = false;
      m_ErrorMessage = "map script is not valid JASS - " + m_JASSErrorMessage;
    }
  }

  // try to calculate <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams>, <map.filter_type>

  if (m_MapLoaderIsPartial) {
    ReadFileFromArchive(fileContents, "war3map.w3i");
    if (fileContents.empty()) {
      Print("[MAP] unable to calculate <map.options>, <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams> - unable to extract war3map.w3i from map file");
    } else {
      istringstream ISS(fileContents);

      // war3map.w3i format found at http://www.wc3campaigns.net/tools/specs/index.html by Zepir/PitzerMike

      string   GarbageString;
      string   RawMapName, RawMapAuthor, RawMapDescription;
      string   RawMapLoadingScreen, RawMapPrologue;
      uint32_t FileFormat = 0;
      uint32_t RawEditorVersion = 0;
      uint32_t RawMapFlags = 0;
      uint32_t RawMapWidth = 0, RawMapHeight = 0;
      uint32_t RawMapNumPlayers = 0, RawMapNumTeams = 0;
      uint32_t RawGameDataSet = MAP_DATASET_DEFAULT;
      uint32_t RawScriptingLanguage = 0;

      ISS.read(reinterpret_cast<char*>(&FileFormat), 4); // file format (18 = ROC, 25 = TFT, 28 = TFT+, 31 = RF)

      if (FileFormat == 18 || FileFormat == 25 || FileFormat == 28 || FileFormat == 31) {
        ISS.seekg(4, ios::cur);            // number of saves
        if (FileFormat >= 28) {
          ISS.seekg(16, ios::cur);         // game version
        }
        ISS.read(reinterpret_cast<char*>(&RawEditorVersion), 4); // editor version
        getline(ISS, RawMapName, '\0');         // map name
        getline(ISS, RawMapAuthor, '\0');       // map author
        getline(ISS, RawMapDescription, '\0');  // map description
        getline(ISS, GarbageString, '\0');      // players recommended
        ISS.seekg(32, ios::cur);                // camera bounds
        ISS.seekg(16, ios::cur);                // camera bounds complements
        ISS.read(reinterpret_cast<char*>(&RawMapWidth), 4);  // map width
        ISS.read(reinterpret_cast<char*>(&RawMapHeight), 4); // map height
        ISS.read(reinterpret_cast<char*>(&RawMapFlags), 4);  // flags
        ISS.seekg(1, ios::cur);                 // map main ground type

        if (FileFormat >= 25) {
          ISS.seekg(4, ios::cur);                   // loading screen background number
          getline(ISS, RawMapLoadingScreen, '\0');  // path of custom loading screen model
        } else {
          ISS.seekg(4, ios::cur);                   // campaign background number
        }

        getline(ISS, GarbageString, '\0'); // map loading screen text
        getline(ISS, GarbageString, '\0'); // map loading screen title
        getline(ISS, GarbageString, '\0'); // map loading screen subtitle

        if (FileFormat >= 25) {
          ISS.read(reinterpret_cast<char*>(&RawGameDataSet), 4);  // used game data set
          getline(ISS, RawMapPrologue, '\0');                     // prologue screen path
        } else {
          ISS.seekg(4, ios::cur);                                 // map loading screen number
        }

        getline(ISS, GarbageString, '\0'); // prologue screen text
        getline(ISS, GarbageString, '\0'); // prologue screen title
        getline(ISS, GarbageString, '\0'); // prologue screen subtitle

        if (FileFormat >= 25) {
          ISS.seekg(4, ios::cur);            // uses terrain fog
          ISS.seekg(4, ios::cur);            // fog start z height
          ISS.seekg(4, ios::cur);            // fog end z height
          ISS.seekg(4, ios::cur);            // fog density
          ISS.seekg(1, ios::cur);            // fog red value
          ISS.seekg(1, ios::cur);            // fog green value
          ISS.seekg(1, ios::cur);            // fog blue value
          ISS.seekg(1, ios::cur);            // fog alpha value
          ISS.seekg(4, ios::cur);            // global weather id
          getline(ISS, GarbageString, '\0'); // custom sound environment
          ISS.seekg(1, ios::cur);            // tileset id of the used custom light environment
          ISS.seekg(1, ios::cur);            // custom water tinting red value
          ISS.seekg(1, ios::cur);            // custom water tinting green value
          ISS.seekg(1, ios::cur);            // custom water tinting blue value
          ISS.seekg(1, ios::cur);            // custom water tinting alpha value
        }

        if (FileFormat >= 28) {
          ISS.read(reinterpret_cast<char*>(&RawScriptingLanguage), 4);   // scripting language
        }

        if (FileFormat >= 31) {
          ISS.seekg(4, ios::cur);            // supported graphics modes
          ISS.seekg(4, ios::cur);            // game data version
        }

        mapEssentials->dataSet = static_cast<uint8_t>(RawGameDataSet);
        mapEssentials->editorVersion = RawEditorVersion;
        mapEssentials->isExpansion = FileFormat >= 25;
        mapEssentials->isLua = RawScriptingLanguage > 0;

        if (mapEssentials->isLua != mapEssentials->foundLua && m_ErrorMessage.empty()) {
          m_Valid = false;
          if (mapEssentials->isLua) {
            m_ErrorMessage = "map is declared as Lua, but war3map.j was found";
          } else {
            m_ErrorMessage = "map is declared as JASS, but war3map.lua was found";
          }
        }

        mapEssentials->name = RawMapName;
        mapEssentials->author = RawMapAuthor;
        mapEssentials->desc = RawMapDescription;
        mapEssentials->prologueImgPath = RawMapPrologue;
        mapEssentials->loadingImgPath = RawMapLoadingScreen;

        ISS.read(reinterpret_cast<char*>(&RawMapNumPlayers), 4); // number of players
        if (RawMapNumPlayers > MAX_SLOTS_MODERN) RawMapNumPlayers = 0;
        uint8_t closedSlots = 0;
        uint8_t disabledSlots = 0;

        for (uint32_t i = 0; i < RawMapNumPlayers; ++i)
        {
          CGameSlot Slot(SLOTTYPE_AUTO, 0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, 0, 1, SLOTRACE_RANDOM);
          uint32_t  Color = 0, Type = 0, Race = 0;
          ISS.read(reinterpret_cast<char*>(&Color), 4); // colour
          Slot.SetColor(static_cast<uint8_t>(Color));
          ISS.read(reinterpret_cast<char*>(&Type), 4); // type

          if (Type == SLOTTYPE_NONE) {
            Slot.SetType(static_cast<uint8_t>(Type));
            Slot.SetSlotStatus(SLOTSTATUS_CLOSED);
            ++closedSlots;
          } else {
            if (!(RawMapFlags & MAPOPT_FIXEDPLAYERSETTINGS)) {
              // WC3 ignores slots defined in WorldEdit if Fixed Player Settings is disabled.
              Type = SLOTTYPE_USER;
            }
            if (Type <= SLOTTYPE_RESCUEABLE) {
              Slot.SetType(static_cast<uint8_t>(Type));
            }
            if (Type == SLOTTYPE_USER) {
              Slot.SetSlotStatus(SLOTSTATUS_OPEN);
            } else if (Type == SLOTTYPE_COMP) {
              Slot.SetSlotStatus(SLOTSTATUS_OCCUPIED);
              Slot.SetComputer(SLOTCOMP_YES);
              Slot.SetComputerType(SLOTCOMP_NORMAL);
            } else {
              Slot.SetSlotStatus(SLOTSTATUS_CLOSED);
              ++closedSlots;
              ++disabledSlots;
            }
          }

          ISS.read(reinterpret_cast<char*>(&Race), 4); // race

          if (Race == 1)
            Slot.SetRace(SLOTRACE_HUMAN);
          else if (Race == 2)
            Slot.SetRace(SLOTRACE_ORC);
          else if (Race == 3)
            Slot.SetRace(SLOTRACE_UNDEAD);
          else if (Race == 4)
            Slot.SetRace(SLOTRACE_NIGHTELF);
          else
            Slot.SetRace(SLOTRACE_RANDOM);

          ISS.seekg(4, ios::cur);            // fixed start position
          getline(ISS, GarbageString, '\0'); // player name
          ISS.seekg(4, ios::cur);            // start position x
          ISS.seekg(4, ios::cur);            // start position y
          ISS.seekg(4, ios::cur);            // ally low priorities
          ISS.seekg(4, ios::cur);            // ally high priorities
          if (FileFormat >= 31) {
            ISS.seekg(4, std::ios::cur);     // enemy low priorities
            ISS.seekg(4, std::ios::cur);     // enemy high priorities
          }

          if (Slot.GetSlotStatus() != SLOTSTATUS_CLOSED)
            mapEssentials->slots.push_back(Slot);
        }

        ISS.read(reinterpret_cast<char*>(&RawMapNumTeams), 4); // number of teams
        if (RawMapNumTeams > MAX_SLOTS_MODERN) RawMapNumTeams = 0;

        if (RawMapNumPlayers > 0 && RawMapNumTeams > 0) {
          // the bot only cares about the following options: melee, fixed player settings, custom forces
          // let's not confuse the user by displaying erroneous map options so zero them out now
          mapEssentials->options = RawMapFlags & (MAPOPT_MELEE | MAPOPT_FIXEDPLAYERSETTINGS | MAPOPT_CUSTOMFORCES);
          if (mapEssentials->options & MAPOPT_FIXEDPLAYERSETTINGS) mapEssentials->options |= MAPOPT_CUSTOMFORCES;

          DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.options = " + to_string(mapEssentials->options) + ">")

          if (!(mapEssentials->options & MAPOPT_CUSTOMFORCES)) {
            mapEssentials->numTeams = static_cast<uint8_t>(RawMapNumPlayers);
          } else {
            mapEssentials->numTeams = static_cast<uint8_t>(RawMapNumTeams);
          }

          for (uint32_t i = 0; i < mapEssentials->numTeams; ++i) {
            uint32_t PlayerMask = 0;
            if (i < RawMapNumTeams) {
              ISS.seekg(4, ios::cur);                            // flags
              ISS.read(reinterpret_cast<char*>(&PlayerMask), 4); // player mask
            }
            if (!(mapEssentials->options & MAPOPT_CUSTOMFORCES)) {
              PlayerMask = 1 << i;
            }
            DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated team " + to_string(i) + " mask = " + ToHexString(PlayerMask))

            for (auto& Slot : mapEssentials->slots) {
              if (0 != (PlayerMask & (1 << static_cast<uint32_t>((Slot).GetColor())))) {
                Slot.SetTeam(static_cast<uint8_t>(i));
              }
            }

            if (i < RawMapNumTeams) {
              getline(ISS, GarbageString, '\0'); // team name
            }
          }

          EnsureFixedByteArray(mapEssentials->width, static_cast<uint16_t>(RawMapWidth), false);
          EnsureFixedByteArray(mapEssentials->height, static_cast<uint16_t>(RawMapHeight), false);
          mapEssentials->numPlayers = static_cast<uint8_t>(RawMapNumPlayers) - closedSlots;
          mapEssentials->numDisabled = disabledSlots;
          mapEssentials->melee = (mapEssentials->options & MAPOPT_MELEE) != 0;

          if (!(mapEssentials->options & MAPOPT_FIXEDPLAYERSETTINGS)) {
            // make races selectable

            for (auto& slot : mapEssentials->slots)
              slot.SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
          }

#ifdef DEBUG
          uint32_t SlotNum = 1;
          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
            Print("[MAP] calculated <map.width = " + ByteArrayToDecString(mapEssentials->width.value()) + ">");
            Print("[MAP] calculated <map.height = " + ByteArrayToDecString(mapEssentials->height.value()) + ">");
            Print("[MAP] calculated <map.num_players = " + ToDecString(mapEssentials->numPlayers) + ">");
            Print("[MAP] calculated <map.num_disabled = " + ToDecString(mapEssentials->numDisabled) + ">");
            Print("[MAP] calculated <map.num_teams = " + ToDecString(mapEssentials->numTeams) + ">");
          }

          for (const auto& slot : mapEssentials->slots) {
            DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.slot_" + to_string(SlotNum) + " = " + ByteArrayToDecString(slot.GetProtocolArray()) + ">")
            ++SlotNum;
          }
#endif
        }
      } else {
        // Some rare maps with other file formats exist 8 10 11 15 23 24 26 27
        // See https://www.hiveworkshop.com/threads/parsing-metadata-from-w3m-w3x-w3n.322007/
        m_Valid = false;
        m_ErrorMessage = "unsupported map file format " + to_string(FileFormat);
      }

      if (FileFormat > 25) {
        mapEssentials->minCompatibleGameVersion = m_Aura->m_Config.m_TargetCommunity ? GAMEVER(1u, 29u) : GAMEVER(1u, 31u);
      } else if (FileFormat > 18) {
        mapEssentials->minCompatibleGameVersion = GAMEVER(1u, 7u);
      }

      vector<string*> maybeTriggerStrings;
      maybeTriggerStrings.push_back(&mapEssentials->name);
      maybeTriggerStrings.push_back(&mapEssentials->author);
      maybeTriggerStrings.push_back(&mapEssentials->desc);
      ReplaceTriggerStrings(fileContents, maybeTriggerStrings);

      /*
      if (!mapEssentials->prologueImgPath.empty()) {
        ReadFileFromArchive(fileContents, mapEssentials->prologueImgPath);
        if (!fileContents.empty()) {
          mapEssentials->prologueImgSize = fileContents.size();
        }
      }
      if (!mapEssentials->loadingImgPath.empty()) {
        ReadFileFromArchive(fileContents, mapEssentials->loadingImgPath);
        if (!fileContents.empty()) {
          mapEssentials->loadingImgSize = fileContents.size();
        }
      }
      */
    } // end war3map.w3i

    optional<uint32_t> previewImgSize = GetFileSizeFromArchive("war3mapPreview.tga");
    if (previewImgSize.has_value()) {
      mapEssentials->previewImgSize = previewImgSize.value();
    }
  } else { // end m_MapLoaderIsPartial
    DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] using mapcfg for <map.options>, <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams>")
  }

  fileContents.clear();

  if (mapEssentials->minCompatibleGameVersion < GAMEVER(1u, 29u) && (mapEssentials->slots.size() > 12 || mapEssentials->numPlayers > 12 || mapEssentials->numTeams > 12)) {
    mapEssentials->minCompatibleGameVersion = GAMEVER(1u, 29u);
  }

  if (mapEssentials->editorVersion > 0) {
    if (6060 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 29u);
    } else if (6059 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 24u);
    } else if (6058 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 23u);
    } else if (6057 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 22u);
    } else if (6053 <= mapEssentials->editorVersion && mapEssentials->editorVersion <= 6056) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 22u); // not released
    } else if (6050 <= mapEssentials->editorVersion && mapEssentials->editorVersion <= 6052) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1, 17 + static_cast<uint8_t>(mapEssentials->editorVersion - 6050));
    } else if (6046 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 16u);
    } else if (6043 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 15u);
    } else if (6039 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 14u);
    } else if (6038 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 14u); // not released
    } else if (6034 <= mapEssentials->editorVersion && mapEssentials->editorVersion <= 6037) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1, 10 + static_cast<uint8_t>(mapEssentials->editorVersion - 6034));
    } else if (6031 <= mapEssentials->editorVersion) {
      mapEssentials->minSuggestedGameVersion = GAMEVER(1u, 7u);
    }
  }

  if (mapEssentials->minSuggestedGameVersion < mapEssentials->minCompatibleGameVersion) {
    mapEssentials->minSuggestedGameVersion = mapEssentials->minCompatibleGameVersion;
  }
  return mapEssentials;
}

void CMap::Load(CConfig* CFG)
{
  m_Valid   = true;
  m_CFGName = PathToString(CFG->GetFile().filename());
  bool isLatestSchema = CFG->GetUint8("map.cfg.schema_number", 0) == MAP_CONFIG_SCHEMA_NUMBER;
  bool ignoreMPQ = !HasServerPath();
  optional<uint32_t> mapFileSize;
  optional<uint32_t> mapFileCRC32;
  optional<array<uint8_t, 20>> mapFileSHA1;
  if (m_MapLoaderIsPartial || m_Aura->m_Net.m_Config.m_AllowTransfers != MAP_TRANSFERS_NEVER || !isLatestSchema) {
    if (!TryLoadMapFileChunked(mapFileSize, mapFileCRC32, mapFileSHA1)) {
      // Map file does not exist or failed to read
      if (m_MapLoaderIsPartial) {
        // We are trying to figure out what this map is about - map config provided is a stub.
        // Since there is no actual map file, map loading fails.
        return;
      } else if (!ignoreMPQ) {
        ignoreMPQ = isLatestSchema;
      }
    }
  }

  if (!ignoreMPQ) {
    ignoreMPQ = (
      (!m_MapLoaderIsPartial && isLatestSchema) &&
      m_Aura->m_Config.m_CFGCacheRevalidateAlgorithm == CACHE_REVALIDATION_NEVER
    );
  }

  filesystem::path resolvedFilePath = GetResolvedServerPath();

  {
    optional<int64_t> cachedModifiedTime = CFG->GetMaybeInt64("map.local_mod_time");
    optional<int64_t> fileModifiedTime;

    if (!ignoreMPQ) {
      fileModifiedTime = GetMaybeModifiedTime(resolvedFilePath);
      ignoreMPQ = (
        (!m_MapLoaderIsPartial && isLatestSchema) && (
          m_Aura->m_Config.m_CFGCacheRevalidateAlgorithm == CACHE_REVALIDATION_MODIFIED && (
            !fileModifiedTime.has_value() || (
              cachedModifiedTime.has_value() && fileModifiedTime.has_value() &&
              fileModifiedTime.value() <= cachedModifiedTime.value()
            )
          )
        )
      );
    }
    if (fileModifiedTime.has_value()) {
      if (!cachedModifiedTime.has_value() || fileModifiedTime.value() != cachedModifiedTime.value()) {
        CFG->SetInt64("map.local_mod_time", fileModifiedTime.value());
        CFG->SetIsModified();
      }
    }
  }

  // calculate <map.file_hash.crc32>
  optional<array<uint8_t, 4>> crc32;
  if (mapFileCRC32.has_value()) {
    EnsureFixedByteArray(crc32, mapFileCRC32.value(), true); // big-endian, matching SHA1
  }

  // calculate <map.file_hash.sha1>
  optional<array<uint8_t, 20>> sha1;
  if (mapFileSHA1.has_value()) {
    sha1 = mapFileSHA1.value();
  }

  AcquireGameIsExpansion(CFG);
  AcquireGameVersion(CFG);

  optional<MapEssentials> mapEssentials;
  if (!ignoreMPQ) {
    optional<MapEssentials> mapEssentialsParsed = ParseMPQFromPath(resolvedFilePath);
    mapEssentials.swap(mapEssentialsParsed);
    if (!mapEssentials.has_value()) {
      if (m_MapLoaderIsPartial) {
        Print("[MAP] failed to parse map");
        return;
      }
      Print("[MAP] failed to parse map, using config file for <map.scripts_hash.blizz.vN>, <map.scripts_hash.sha1.vN>");
    }
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE2, "[MAP] MPQ archive ignored");
  }

  if (mapEssentials.has_value()) {
    // If map has Melee flag, group it with other Melee maps in Battle.net game search filter
    m_MapIsMelee = mapEssentials->melee;
    if (mapEssentials->dataSet == 0) {
      m_MapDataSet = mapEssentials->melee ? MAP_DATASET_MELEE : MAP_DATASET_CUSTOM;
    } else {
      m_MapDataSet = mapEssentials->dataSet;
    }
    m_MapTitle = mapEssentials->name;
    m_MapAuthor = mapEssentials->author;
    m_MapDescription = mapEssentials->desc;
    m_MapPreviewImageSize = mapEssentials->previewImgSize;
    /*
    m_MapPrologueImageSize = mapEssentials->prologueImgSize;
    m_MapPrologueImagePath = mapEssentials->prologueImgPath;
    m_MapLoadingImageSize = mapEssentials->loadingImgSize;
    m_MapLoadingImagePath = mapEssentials->loadingImgPath;
    */
    m_MapNumControllers = mapEssentials->numPlayers;
    m_MapNumDisabled = mapEssentials->numDisabled;
    m_MapNumTeams = mapEssentials->numTeams;
    m_MapMinGameVersion = mapEssentials->minCompatibleGameVersion;
    m_MapMinSuggestedGameVersion = mapEssentials->minSuggestedGameVersion;
    m_MapRequiresExpansion = mapEssentials->isExpansion;
    m_MapIsLua = mapEssentials->isLua;
    m_MapEditorVersion = mapEssentials->editorVersion;
    m_MapOptions = mapEssentials->options;

    if (mapEssentials->width.has_value()) {
      copy_n(mapEssentials->width->begin(), 2, m_MapWidth.begin());
    }
    if (mapEssentials->height.has_value()) {
      copy_n(mapEssentials->height->begin(), 2, m_MapHeight.begin());
    }

    m_Slots = mapEssentials->slots;
  } else {
    DPRINT_IF(LOG_LEVEL_TRACE2, "[MAP] MPQ archive ignored/missing/errored");
  }

  array<uint8_t, 5> mapContentMismatch = {0, 0, 0, 0, 0};

  optional<uint32_t> cfgFileSize = CFG->GetMaybeUint32("map.size");
  if (cfgFileSize.has_value() == mapFileSize.has_value()) {
    if (!cfgFileSize.has_value()) {
      CFG->SetFailed();
      if (m_ErrorMessage.empty()) {
        if (CFG->Exists("map.size")) {
          m_ErrorMessage = "invalid <map.size> detected";
        } else {
          m_ErrorMessage = "cannot calculate <map.size>";
        }
      }
    } else {
      mapContentMismatch[0] = cfgFileSize.value() != mapFileSize.value();
      m_MapSize = cfgFileSize.value();
    }
  } else if (mapFileSize.has_value()) {
    cfgFileSize = static_cast<uint32_t>(mapFileSize.value());
    CFG->SetUint32("map.size", cfgFileSize.value());
    m_MapSize = cfgFileSize.value();
  } else {
    m_MapSize = cfgFileSize.value();
  }

  vector<uint8_t> cfgCRC32 = CFG->GetUint8Vector("map.file_hash.crc32", 4);
  if (cfgCRC32.empty() == !crc32.has_value()) {
    if (cfgCRC32.empty()) {
      CFG->SetFailed();
      if (m_ErrorMessage.empty()) {
        if (CFG->Exists("map.file_hash.crc32")) {
          m_ErrorMessage = "invalid <map.file_hash.crc32> detected";
        } else {
          m_ErrorMessage = "cannot calculate <map.file_hash.crc32>";
        }
      }
    } else {
      mapContentMismatch[1] = ByteArrayToUInt32(cfgCRC32, 0, true) != ByteArrayToUInt32(crc32.value(), true);
      copy_n(cfgCRC32.rbegin(), 4, m_MapCRC32.begin());
    }
  } else if (crc32.has_value()) {
    CFG->SetUint8Array("map.file_hash.crc32", crc32->data(), 4);
    copy_n(crc32->rbegin(), 4, m_MapCRC32.begin());
  } else {
    copy_n(cfgCRC32.rbegin(), 4, m_MapCRC32.begin());
  }

  vector<uint8_t> cfgSHA1 = CFG->GetUint8Vector("map.file_hash.sha1", 20);
  if (cfgSHA1.empty() == !sha1.has_value()) {
    if (cfgSHA1.empty()) {
      CFG->SetFailed();
      if (m_ErrorMessage.empty()) {
        if (CFG->Exists("map.file_hash.sha1")) {
          m_ErrorMessage = "invalid <map.file_hash.sha1> detected";
        } else {
          m_ErrorMessage = "cannot calculate <map.file_hash.sha1>";
        }
      }
    } else {
      mapContentMismatch[2] = memcmp(cfgSHA1.data(), sha1->data(), 20) != 0;
      copy_n(cfgSHA1.begin(), 20, m_MapSHA1.begin());
    }
  } else if (sha1.has_value()) {
    CFG->SetUint8Array("map.file_hash.sha1", sha1->data(), 20);
    copy_n(sha1->begin(), 20, m_MapSHA1.begin());
  } else {
    copy_n(cfgSHA1.begin(), 20, m_MapSHA1.begin());
  }

  optional<Version> targetGameVersionRangeHead;
  if (m_MapTargetGameVersion.has_value()) {
    targetGameVersionRangeHead = GetScriptsVersionRangeHead(m_MapTargetGameVersion.value());
  }
  for (const auto& version : m_Aura->GetSupportedVersionsCrossPlayRangeHeads()) {
    array<uint8_t, 4> scriptsHashBlizz;
    scriptsHashBlizz.fill(0);
    vector<uint8_t> cfgScriptsWeakHash = CFG->GetUint8Vector("map.scripts_hash.blizz.v" + ToVersionString(version), 4);
    if (cfgScriptsWeakHash.empty() == !(mapEssentials.has_value() && mapEssentials->fragmentHashes[version].blizz.has_value())) {
      if (cfgScriptsWeakHash.empty()) {
        bool isTargetVersion = targetGameVersionRangeHead.has_value() && version == targetGameVersionRangeHead.value();
        if (isTargetVersion) CFG->SetFailed();
        if (m_ErrorMessage.empty()) {
          if (CFG->Exists("map.scripts_hash.blizz.v" + ToVersionString(version))) {
            m_ErrorMessage = "invalid <map.scripts_hash.blizz.v" + ToVersionString(version) + "> detected";
          } else if (isTargetVersion) {
            m_ErrorMessage = "cannot calculate <map.scripts_hash.blizz.v" + ToVersionString(version) + ">";
          }
        }
      } else {
        if (mapContentMismatch[3] == 0) {
          mapContentMismatch[3] = ByteArrayToUInt32(cfgScriptsWeakHash, 0, false) != ByteArrayToUInt32(mapEssentials->fragmentHashes[version].blizz.value(), false);
        }
        copy_n(cfgScriptsWeakHash.begin(), 4, scriptsHashBlizz.begin());
      }
    } else if (mapEssentials.has_value() && mapEssentials->fragmentHashes[version].blizz.has_value()) {
      CFG->SetUint8Array("map.scripts_hash.blizz.v" + ToVersionString(version), mapEssentials->fragmentHashes[version].blizz->data(), 4);
      copy_n(mapEssentials->fragmentHashes[version].blizz->begin(), 4, scriptsHashBlizz.begin());
    } else {
      copy_n(cfgScriptsWeakHash.begin(), 4, scriptsHashBlizz.begin());
    }
    m_MapScriptsBlizz[version] = scriptsHashBlizz;

    array<uint8_t, 20> scriptsHashSHA1;
    scriptsHashSHA1.fill(0);
    vector<uint8_t> cfgScriptsSHA1 = CFG->GetUint8Vector("map.scripts_hash.sha1.v" + ToVersionString(version), 20);
    if (cfgScriptsSHA1.empty() == !(mapEssentials.has_value() && mapEssentials->fragmentHashes[version].sha1.has_value())) {
      if (cfgScriptsSHA1.empty()) {
        bool isTargetVersion = targetGameVersionRangeHead.has_value() && version == targetGameVersionRangeHead.value();
        if (isTargetVersion) CFG->SetFailed();
        if (m_ErrorMessage.empty()) {
          if (CFG->Exists("map.scripts_hash.sha1.v" + ToVersionString(version))) {
            m_ErrorMessage = "invalid <map.scripts_hash.sha1.v" + ToVersionString(version) + "> detected";
          } else if (isTargetVersion) {
            m_ErrorMessage = "cannot calculate <map.scripts_hash.sha1.v" + ToVersionString(version) + ">";
          }
        }
      } else {
        if (mapContentMismatch[4] == 0) {
          mapContentMismatch[4] = memcmp(cfgScriptsSHA1.data(), mapEssentials->fragmentHashes[version].sha1->data(), 20) != 0;
        }
        copy_n(cfgScriptsSHA1.begin(), 20, scriptsHashSHA1.begin());
      }
    } else if (mapEssentials.has_value() && mapEssentials->fragmentHashes[version].sha1.has_value()) {
      CFG->SetUint8Array("map.scripts_hash.sha1.v" + ToVersionString(version), mapEssentials->fragmentHashes[version].sha1->data(), 20);
      copy_n(mapEssentials->fragmentHashes[version].sha1->begin(), 20, scriptsHashSHA1.begin());
    } else {
      copy_n(cfgScriptsSHA1.begin(), 20, scriptsHashSHA1.begin());
    }
    m_MapScriptsSHA1[version] = scriptsHashSHA1;
  }

  if (HasMismatch()) {
    m_MapContentMismatch.swap(mapContentMismatch);
    PRINT_IF(LOG_LEVEL_WARNING, "[CACHE] error - map content mismatch");
  } else if (crc32.has_value() && sha1.has_value()) {
    m_MapFileIsValid = true;
  }

  if (CFG->Exists("map.melee")) {
    m_MapIsMelee = CFG->GetBool("map.melee", m_MapIsMelee);
  } else {
    CFG->SetBool("map.melee", m_MapIsMelee);
  }  

  m_MapFilterType = m_MapIsMelee ? MAPFILTER_TYPE_MELEE : MAPFILTER_TYPE_SCENARIO;
  if (CFG->Exists("map.filter_type")) {
    // If map has Melee flag, group it with other Melee maps in Battle.net game search filter
    m_MapFilterType = CFG->GetUint8("map.filter_type", m_MapFilterType);
  } else {
    CFG->SetUint8("map.filter_type", m_MapFilterType);
  }  

  if (CFG->Exists("map.options")) {
    // Note: maps with any given layout style defined from WorldEdit
    // may have their layout further constrained arbitrarily when hosting games
    m_MapOptions = CFG->GetUint32("map.options", m_MapOptions);
    if (m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) m_MapOptions |= MAPOPT_CUSTOMFORCES;
  } else {
    CFG->SetUint32("map.options", m_MapOptions);
  }

  if (CFG->Exists("map.flags")) {
    m_GameFlags = CFG->GetUint8("map.flags", m_GameFlags);
  } else {
    CFG->SetUint8("map.flags", m_GameFlags);
  }

  vector<uint8_t> cfgWidth = CFG->GetUint8Vector("map.width", 2);
  if (cfgWidth.size() == 2) {
    copy_n(cfgWidth.begin(), 2, m_MapWidth.begin());
  } else {
    CFG->SetUint8Array("map.width", m_MapWidth.data(), 2);
    // already copied to m_MapWidth
  }
  if (ByteArrayToUInt16(m_MapWidth, false) == 0) {
    // Default invalid <map.width> values to 1
    m_MapWidth = {1, 0};
  }

  vector<uint8_t> cfgHeight = CFG->GetUint8Vector("map.height", 2);
  if (cfgHeight.size() == 2) {
    copy_n(cfgHeight.begin(), 2, m_MapHeight.begin());
  } else {
    CFG->SetUint8Array("map.height", m_MapHeight.data(), 2);
  }
  if (ByteArrayToUInt16(m_MapHeight, false) == 0) {
    // Default invalid <map.height> values to 1
    m_MapHeight = {1, 0};
  }

  if (CFG->Exists("map.data_set")) {
    m_MapDataSet = CFG->GetUint8("map.data_set", m_MapDataSet);
  } else {
    CFG->SetUint8("map.data_set", m_MapDataSet);
  }

  if (CFG->Exists("map.expansion")) {
    m_MapRequiresExpansion = CFG->GetBool("map.expansion", m_MapRequiresExpansion);
  } else {
    CFG->SetBool("map.expansion", m_MapRequiresExpansion);
  }

  if (CFG->Exists("map.lua")) {
    m_MapIsLua = CFG->GetBool("map.lua", m_MapIsLua);
  } else {
    CFG->SetBool("map.lua", m_MapIsLua);
  }

  if (CFG->Exists("map.editor_version")) {
    m_MapEditorVersion = CFG->GetUint32("map.editor_version", m_MapEditorVersion);
  } else {
    CFG->SetUint32("map.editor_version", m_MapEditorVersion);
  }

  if (CFG->Exists("map.title")) {
    m_MapTitle = CFG->GetString("map.title", "Just another Warcraft 3 Map");
  } else {
    CFG->SetString("map.title", m_MapTitle);
  }

  if (CFG->Exists("map.meta.author")) {
    m_MapAuthor = CFG->GetString("map.meta.author", "Unknown");
  } else {
    CFG->SetString("map.meta.author", m_MapAuthor);
  }

  if (CFG->Exists("map.meta.desc")) {
    m_MapDescription = CFG->GetString("map.meta.desc", "Nondescript");
  } else {
    CFG->SetString("map.meta.desc", m_MapDescription);
  }

  if (CFG->Exists("map.preview.image.size")) {
    m_MapPreviewImageSize = CFG->GetUint32("map.preview.image.size", 0);
  } else {
    CFG->SetUint32("map.preview.image.size", m_MapPreviewImageSize);
  }

  if (CFG->Exists("map.preview.image.path")) {
    m_MapPreviewImagePath = CFG->GetString("map.preview.image.path", string());
  } else {
    CFG->SetString("map.preview.image.path", m_MapPreviewImageSize > 0 ? "war3mapPreview.tga" : string());
  }

  if (CFG->Exists("map.preview.image.path_type")) {
    m_MapPreviewImagePathType = CFG->GetStringIndex("map.preview.image.source", {"none", "mpq", "fs"}, MAP_FILE_SOURCE_CATEGORY_NONE);
  } else {
    CFG->SetString("map.preview.image.path_type", m_MapPreviewImageSize > 0 ? "mpq" : "none");
  }

  if (CFG->Exists("map.preview.image.mime_type")) {
    m_MapPreviewImageMimeType = CFG->GetString("map.preview.image.mime_type", string());
  } else {
    CFG->SetString("map.preview.image.mime_type", m_MapPreviewImageSize > 0 ? "image/tga" : "example/example");
  }

  /*
  if (CFG->Exists("map.prologue.image.size")) {
    m_MapPrologueImageSize = CFG->GetUint32("map.prologue.image.size", 0);
  } else {
    CFG->SetUint32("map.prologue.image.size", m_MapPrologueImageSize);
  }

  if (CFG->Exists("map.prologue.image.path")) {
    m_MapPrologueImagePath = CFG->GetString("map.prologue.image.path", string());
  } else {
    CFG->SetString("map.prologue.image.path", m_MapPrologueImagePath);
  }

  if (CFG->Exists("map.prologue.image.mime_type")) {
    m_MapPrologueImageMimeType = CFG->GetString("map.prologue.image.mime_type", string());
  } else {
    CFG->SetString("map.prologue.image.mime_type", !m_MapPrologueImagePath.empty() && m_MapPrologueImageMimeType.empty() ? "image/" : m_MapPrologueImageMimeType);
  }

  if (CFG->Exists("map.load_screen.image.size")) {
    m_MapLoadingImageSize = CFG->GetUint32("map.load_screen.image.size", 0);
  } else {
    CFG->SetUint32("map.load_screen.image.size", m_MapLoadingImageSize);
  }

  if (CFG->Exists("map.load_screen.image.path")) {
    m_MapLoadingImagePath = CFG->GetString("map.load_screen.image.path", string());
  } else {
    CFG->SetString("map.load_screen.image.path", m_MapLoadingImagePath);
  }

  if (CFG->Exists("map.load_screen.image.mime_type")) {
    m_MapLoadingImageMimeType = CFG->GetString("map.load_screen.image.mime_type", string());
  } else {
    CFG->SetString("map.load_screen.image.mime_type", !m_MapLoadingImagePath.empty() && m_MapLoadingImageMimeType.empty() ? "image/" : m_MapLoadingImageMimeType);
  }
  */

  if (CFG->Exists("map.num_disabled")) {
    m_MapNumDisabled = CFG->GetUint8("map.num_disabled", m_MapNumDisabled);
  } else {
    CFG->SetUint8("map.num_disabled", m_MapNumDisabled);
  }

  if (CFG->Exists("map.num_players")) {
    m_MapNumControllers = CFG->GetUint8("map.num_players", m_MapNumControllers);
  } else {
    CFG->SetUint8("map.num_players", m_MapNumControllers);
  }

  if (CFG->Exists("map.num_teams")) {
    m_MapNumTeams = CFG->GetUint8("map.num_teams", m_MapNumTeams);
  } else {
    CFG->SetUint8("map.num_teams", m_MapNumTeams);
  }

  // Game version compatibility and suggestions
  optional<Version> minGameVersion = CFG->GetMaybeVersion("map.game_version.min");
  if (minGameVersion.has_value()) {
    m_MapMinGameVersion = minGameVersion.value();
  } else {
    CFG->SetString("map.game_version.min", ToVersionString(m_MapMinGameVersion));
  }

  if (m_MapMinGameVersion >= GAMEVER(1u, 29u)) {
    m_MapVersionMaxSlots = static_cast<uint8_t>(MAX_SLOTS_MODERN);
  } else {
    m_MapVersionMaxSlots = static_cast<uint8_t>(MAX_SLOTS_LEGACY);
  }

  if (CFG->Exists("map.slot_1")) {
    vector<CGameSlot> cfgSlots;

    for (uint32_t slotNum = 1; slotNum <= m_MapVersionMaxSlots; ++slotNum) {
      string encodedSlot = CFG->GetString("map.slot_" + to_string(slotNum));
      if (encodedSlot.empty()) {
        break;
      }
      vector<uint8_t> slotData = ExtractNumbers(encodedSlot, 10);
      if (slotData.size() < 9) {
        // Last (10th) element is optional for backwards-compatibility
        // it's the type of slot (SLOTTYPE_USER by default)
        break;
      }
      cfgSlots.emplace_back(slotData);
    }
    if (!cfgSlots.empty()) {
      if (m_Slots.empty() || cfgSlots.size() == m_MapVersionMaxSlots) {
        // No slot data from MPQ - or config supports observers
        m_Slots.swap(cfgSlots);
      } else if (m_Slots.size() == cfgSlots.size()) {
        // Override MPQ slot data with slots from config
        m_Slots.swap(cfgSlots);
      } else {
        // Slots from config are not compatible with slots parsed from MPQ
        CFG->SetFailed();
        if (m_ErrorMessage.empty()) {
          m_ErrorMessage = "<map.slots> do not match the map";
        }
      }
    }
  } else {
    uint32_t slotNum = 0;
    for (const auto& slot : m_Slots) {
      CFG->SetUint8Vector("map.slot_" + to_string(++slotNum), slot.GetByteArray());
    }
  }

  {
    Version minVanillaVersionFromMapSize = GAMEVER(1u, 0u);
    if (m_MapSize > MAX_MAP_SIZE_1_28) {
      minVanillaVersionFromMapSize = GAMEVER(1u, 29u);
    } else if (m_MapSize > MAX_MAP_SIZE_1_26) {
      minVanillaVersionFromMapSize = GAMEVER(1u, 27u);
    } else if (m_MapSize > MAX_MAP_SIZE_1_23) {
      minVanillaVersionFromMapSize = GAMEVER(1u, 24u);
    }
    if (m_MapMinSuggestedGameVersion < minVanillaVersionFromMapSize) {
      m_MapMinSuggestedGameVersion = minVanillaVersionFromMapSize;
    }

    if (m_Slots.size() > MAX_SLOTS_LEGACY && m_MapMinSuggestedGameVersion < GAMEVER(1u, 29u)) {
      m_MapMinSuggestedGameVersion = GAMEVER(1u, 29u);
    }
  }

  optional<Version> overrideMinSuggestedVersion = CFG->GetMaybeVersion("map.game_version.suggested.min");
  if (overrideMinSuggestedVersion.has_value()) {
    m_MapMinSuggestedGameVersion = overrideMinSuggestedVersion.value();
  } else {
    CFG->SetString("map.game_version.suggested.min", ToVersionString(m_MapMinSuggestedGameVersion));
  }

  // Maps supporting observer slots enable them by default.
  m_MapCustomizableObserverTeam = m_MapVersionMaxSlots + 1;
  if (m_Slots.size() + m_MapNumDisabled < m_MapVersionMaxSlots) {
    SetMapObservers(MAPOBS_ALLOWED);
  }

  if (m_MapType == "dota") {
    m_EnableLagScreen = false;
  }

  LoadGameConfigOverrides(*CFG);
  LoadMapSpecificConfig(*CFG); // as in not overrides
  m_GameResultConstraints = GameResultConstraints(this, *CFG);

  // Out of the box support for auto-starting maps using the Host Force + Others Force pattern (e.g. Warlock)
  if (m_MapNumTeams == 2 && m_MapNumControllers > 2 && !m_AutoStartRequiresBalance.has_value()) {
    uint8_t refTeam = 0xFF;
    uint8_t playersRefTeam = 0;
    uint8_t i = static_cast<uint8_t>(m_Slots.size());
    while (i--) {
      if (refTeam == 0xFF) {
        refTeam = m_Slots[i].GetTeam();
        ++playersRefTeam;
      } else if (refTeam == m_Slots[i].GetTeam()) {
        ++playersRefTeam;
      }
    }
    if (playersRefTeam == 1 || playersRefTeam + 1u == static_cast<uint8_t>(m_Slots.size())) {
      m_AutoStartRequiresBalance = false;
      CFG->SetBool("map.hosting.autostart.requires_balance", false);
    }
  }

  if (!CFG->GetSuccess()) {
    m_Valid = false;
    if (m_ErrorMessage.empty()) m_ErrorMessage = "invalid map config file";
    Print("[MAP] " + m_ErrorMessage);
  } else {
    string ErrorMessage = CheckProblems();
    if (!ErrorMessage.empty()) {
      Print("[MAP] " + ErrorMessage);
    } else if (m_MapLoaderIsPartial) {
      CFG->Delete("map.cfg.partial");
      CGameSetup::DeleteTemporaryFromMap(CFG);
      m_MapLoaderIsPartial = false;
    }
  }

  //ClearMapFileContents();
}

bool CMap::AcquireGameIsExpansion(CConfig* CFG)
{
  if (CFG->Exists("map.cfg.hosting.game_versions.expansion.default")) { // from CGameSetup
    bool isExpansion = static_cast<bool>(CFG->GetStringIndex("map.cfg.hosting.game_versions.expansion.default", {"roc", "tft"}, SELECT_EXPANSION_TFT));
    if (!CFG->GetErrorLast()) {
      m_MapTargetGameIsExpansion = isExpansion;
    }
  }

  if (!m_MapTargetGameIsExpansion.has_value() && CFG->Exists("map.hosting.game_versions.expansion.default")) { // from map.ini
    bool isExpansion = static_cast<bool>(CFG->GetStringIndex("map.hosting.game_versions.expansion.default", {"roc", "tft"}, SELECT_EXPANSION_TFT));
    if (!CFG->GetErrorLast()) {
      m_MapTargetGameIsExpansion = isExpansion;
    }
  }

  if (!m_MapTargetGameIsExpansion.has_value()) {
    // Guaranteed to have a value,
    // because we don't error anywhere just because the bot owner forgot to specify TFT,
    // and just default to TFT.
    m_MapTargetGameIsExpansion = m_Aura->m_GameDefaultConfig->m_GameIsExpansion;
  }

  return true;
}

bool CMap::AcquireGameVersion(CConfig* CFG)
{
  if (CFG->Exists("map.cfg.hosting.game_versions.main")) { // from CGameSetup
    optional<Version> version = CFG->GetMaybeVersion("map.cfg.hosting.game_versions.main");
    if (version.has_value()) {
      m_MapTargetGameVersion.swap(version);
    }
  }

  if (!m_MapTargetGameVersion.has_value() && CFG->Exists("map.hosting.game_versions.main")) { // from map.ini
    optional<Version> version = CFG->GetMaybeVersion("map.hosting.game_versions.main");
    if (version.has_value()) {
      m_MapTargetGameVersion.swap(version);
    }
  }

  if (!m_MapTargetGameVersion.has_value() && m_Aura->m_GameDefaultConfig->m_GameVersion.has_value()) { // from config.ini
    m_MapTargetGameVersion = m_Aura->m_GameDefaultConfig->m_GameVersion.value();
  }

  return m_MapTargetGameVersion.has_value();
}

// @deprecated
bool CMap::TryLoadMapFilePersistent(optional<uint32_t>& fileSize, optional<uint32_t>& crc32)
{
  if (m_MapServerPath.empty()) {
    DPRINT_IF(LOG_LEVEL_TRACE2, "m_MapServerPath missing - map data not loaded")
    return false;
  }
  filesystem::path resolvedPath(m_MapServerPath);
  if (m_MapServerPath.filename() == m_MapServerPath && !m_UseStandardPaths) {
    resolvedPath = m_Aura->m_Config.m_MapPath / m_MapServerPath;
  }
  m_MapFileContents = m_Aura->ReadFile(resolvedPath, MAX_READ_FILE_SIZE);
  if (!HasMapFileContents()) {
    PRINT_IF(LOG_LEVEL_INFO, "[MAP] Failed to read [" + PathToString(resolvedPath) + "]")
    return false;
  }

  static_assert(MAX_READ_FILE_SIZE <= 0xFFFFFFFF, "Deprecated method CMap::TryLoadMapFilePersistent() expects MAX_READ_FILE_SIZE to fit in uint32_t");
  fileSize = (uint32_t)m_MapFileContents->size();
#ifdef DEBUG
  array<uint8_t, 4> mapFileSizeBytes = CreateFixedByteArray(fileSize.value(), false);
  DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.size = " + ByteArrayToDecString(mapFileSizeBytes) + ">")
#endif

  crc32 = CRC32::CalculateCRC((uint8_t*)m_MapFileContents->data(), m_MapFileContents->size());
  optional<array<uint8_t, 4>> crc32Bytes;
  EnsureFixedByteArray(crc32Bytes, crc32.value(), true); // Big endian, matching SHA1
  DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.file_hash.crc32 = " + ByteArrayToDecString(crc32Bytes.value()) + ">")

  return true;
}

bool CMap::TryLoadMapFileChunked(optional<uint32_t>& fileSize, optional<uint32_t>& crc32, optional<array<uint8_t, 20>>& sha1)
{
  if (m_MapServerPath.empty()) {
    DPRINT_IF(LOG_LEVEL_TRACE2, "m_MapServerPath missing - map data not loaded")
    return false;
  }
  filesystem::path resolvedPath(m_MapServerPath);
  if (m_MapServerPath.filename() == m_MapServerPath && !m_UseStandardPaths) {
    resolvedPath = m_Aura->m_Config.m_MapPath / m_MapServerPath;
  }

  m_Aura->m_SHA.Reset();
  uint32_t rollingCRC32 = 0;
  pair<bool, size_t> result = ProcessMapChunked(resolvedPath, [this, &rollingCRC32](FileChunkTransient cachedChunk, size_t cursor, size_t size) {
    rollingCRC32 = CRC32::CalculateCRC(cachedChunk.GetDataAtCursor(cursor), size, rollingCRC32);
    m_Aura->m_SHA.Update(cachedChunk.GetDataAtCursor(cursor), size);
  });
  m_Aura->m_SHA.Final();

  if (!result.first || result.second == 0) {
    PRINT_IF(LOG_LEVEL_INFO, "[MAP] Failed to read [" + PathToString(resolvedPath) + "]")
    m_Aura->m_SHA.Reset();
    return false;
  }

  fileSize = result.second;
#ifdef DEBUG
  array<uint8_t, 4> mapFileSizeBytes = CreateFixedByteArray(fileSize.value(), false);
  DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.size = " + ByteArrayToDecString(mapFileSizeBytes) + ">")
#endif

  crc32 = rollingCRC32;
  optional<array<uint8_t, 4>> crc32Bytes;
  EnsureFixedByteArray(crc32Bytes, rollingCRC32, true); // Big endian, matching SHA1
  DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.file_hash.crc32 = " + ByteArrayToDecString(crc32Bytes.value()) + ">")

  sha1.emplace();
  sha1->fill(0);
  m_Aura->m_SHA.GetHash(sha1->data());
  DPRINT_IF(LOG_LEVEL_TRACE, "[MAP] calculated <map.file_hash.sha1 = " + ByteArrayToDecString(sha1.value()) + ">")

  m_Aura->m_SHA.Reset();
  return true;
}

bool CMap::CheckMapFileIntegrity()
{
  if (!m_MapFileIsValid) {
    return m_MapFileIsValid;
  }
  optional<uint32_t> reloadedFileSize, reloadedCRC;
  optional<array<uint8_t, 20>> reloadedSHA1;
  if (!TryLoadMapFileChunked(reloadedFileSize, reloadedCRC, reloadedSHA1)) {
    m_MapFileIsValid = false;
    return m_MapFileIsValid;
  }

  bool sizeOK = reloadedFileSize.has_value() && reloadedFileSize.value() == m_MapSize;
  bool crcOK = reloadedCRC.has_value() && reloadedCRC.value() == ByteArrayToUInt32(m_MapCRC32, true);
  bool shaOK = reloadedSHA1.has_value() && memcmp(reloadedSHA1->data(), m_MapSHA1.data(), 20) == 0;
  if (!sizeOK) {
    m_MapContentMismatch[0] = 1;
    m_MapFileIsValid = false;
  }
  if (!crcOK) {
    m_MapContentMismatch[1] = 1;
    m_MapFileIsValid = false;
  }
  if (!shaOK) {
    m_MapContentMismatch[2] = 1;
    m_MapFileIsValid = false;
  }
  if (!sizeOK || !crcOK || !shaOK) {
    PRINT_IF(LOG_LEVEL_WARNING, "Map file [" + PathToString(m_MapServerPath) + "] integrity check failure - file has been tampered")
  }
  return m_MapFileIsValid;
}

FileChunkTransient CMap::GetMapFileChunk(size_t start)
{
  if (HasMapFileContents()) {
    return FileChunkTransient(0, GetMapFileContents());
  } else if (m_MapServerPath.empty()) {
    return FileChunkTransient(0, SharedByteArray());
  } else {
    filesystem::path resolvedPath(m_MapServerPath);
    if (m_MapServerPath.filename() == m_MapServerPath && !m_UseStandardPaths) {
      resolvedPath = m_Aura->m_Config.m_MapPath / m_MapServerPath;
    }
    // Load up to 8 MB at a time
    return m_Aura->ReadFileChunkCacheable(resolvedPath, start, start + MAP_FILE_MAX_CHUNK_SIZE);
  }
}

pair<bool, size_t> CMap::ProcessMapChunked(const filesystem::path& filePath, function<void(FileChunkTransient, size_t, size_t)> processChunk)
{
  uintmax_t fileSize = FileSize(filePath);
  size_t cursor = 0;
  if (fileSize == 0 || fileSize > 0xFFFFFFFF) return make_pair(false, cursor);
  while (cursor < fileSize) {
    FileChunkTransient cachedChunk = GetMapFileChunk(cursor);
    if (!cachedChunk.bytes) {
      return make_pair(false, cursor);
    }
    size_t availableBytes = cachedChunk.GetSizeFromCursor(cursor);
    size_t stepBytes = MAP_FILE_PROCESSING_CHUNK_SIZE;
    if (stepBytes > availableBytes) {
      stepBytes = availableBytes;
    }
    processChunk(cachedChunk, cursor, stepBytes);
    cursor += stepBytes;
  }
  return make_pair(fileSize == cursor, cursor);
}

bool CMap::UnlinkFile()
{
  if (m_MapServerPath.empty()) return false;
  bool result = false;
  filesystem::path mapLocalPath = m_MapServerPath;
  if (mapLocalPath.is_absolute()) {
    result = FileDelete(mapLocalPath);
  } else {
    filesystem::path resolvedPath =  m_Aura->m_Config.m_MapPath / mapLocalPath;
    result = FileDelete(resolvedPath.lexically_normal());
  }
  if (result) {
    PRINT_IF(LOG_LEVEL_NOTICE, "[MAP] Deleted [" + PathToString(m_MapServerPath) + "]");
  }
  return result;
}

string CMap::CheckProblems()
{
  if (!m_Valid) {
    return m_ErrorMessage;
  }

  if (m_ClientMapPath.empty()) {
    m_Valid = false;
    m_ErrorMessage = "<map.path> not found";
    return m_ErrorMessage;
  }

  if (m_ClientMapPath.length() > 53) {
    m_Valid = false;
    m_ErrorMessage = "<map.path> too long";
    return m_ErrorMessage;
  }

  if (m_MapTargetGameVersion.has_value() && !m_Aura->GetIsSupportedGameVersion(m_MapTargetGameVersion.value())) {
    m_Valid = false;
    m_ErrorMessage = "hosting in v" + ToVersionString(m_MapTargetGameVersion.value()) + " is not supported";
    return m_ErrorMessage;
  }

  if (!m_MapIsLua && !m_Aura->m_Config.m_AllowJASS) {
    m_Valid = false;
    m_ErrorMessage = "only Lua maps are allowed";
    return m_ErrorMessage;
  }

  if (m_MapIsLua) {
    switch (m_Aura->m_Config.m_AllowLua) {
      case MAP_ALLOW_LUA_NEVER: {
        m_Valid = false;
        m_ErrorMessage = "map script uses Lua, which is not allowed";
        return m_ErrorMessage;
      }
      case MAP_ALLOW_LUA_AUTO: {
        Version minVersion = m_Aura->m_Config.m_TargetCommunity ? GAMEVER(1u, 29u) : GAMEVER(1u, 31u);
        if (m_MapTargetGameVersion.has_value() && m_MapTargetGameVersion.value() < minVersion) {
          m_Valid = false;
          m_ErrorMessage = "map script uses Lua, which is not allowed";
          return m_ErrorMessage;
        }
      }
    }
  }

  if (m_ClientMapPath.find('/') != string::npos)
    Print(R"(warning - map.path contains forward slashes '/' but it must use Windows style back slashes '\')");

  else/* if (HasMapFileContents() && m_MapFileContents->size() != m_MapSize)
  {
    m_Valid = false;
    m_ErrorMessage = "nonmatching <map.size> detected";
    return m_ErrorMessage;
  }*/

  if (m_MapSpeed != MAPSPEED_SLOW && m_MapSpeed != MAPSPEED_NORMAL && m_MapSpeed != MAPSPEED_FAST)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.speed> detected";
    return m_ErrorMessage;
  }

  if (m_MapVisibility != MAPVIS_HIDETERRAIN && m_MapVisibility != MAPVIS_EXPLORED && m_MapVisibility != MAPVIS_ALWAYSVISIBLE && m_MapVisibility != MAPVIS_DEFAULT)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.visibility> detected";
    return m_ErrorMessage;
  }

  if (m_MapObservers != MAPOBS_NONE && m_MapObservers != MAPOBS_ONDEFEAT && m_MapObservers != MAPOBS_ALLOWED && m_MapObservers != MAPOBS_REFEREES)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.observers> detected";
    return m_ErrorMessage;
  }

  if (m_MapNumDisabled > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.num_disabled> detected";
    return m_ErrorMessage;
  }

  if (m_MapNumControllers < 2 || m_MapNumControllers > MAX_SLOTS_MODERN || m_MapNumControllers + m_MapNumDisabled > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.num_players> detected";
    return m_ErrorMessage;
  }

  if (m_MapNumTeams < 2 || m_MapNumTeams > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.num_teams> detected";
    return m_ErrorMessage;
  }

  if (m_Slots.size() < 2 || m_Slots.size() > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.slot_N> detected";
    return m_ErrorMessage;
  }

  if (
    m_MapNumControllers + m_MapNumDisabled > m_MapVersionMaxSlots ||
    m_MapNumTeams > m_MapVersionMaxSlots ||
    m_Slots.size() > m_MapVersionMaxSlots
  ) {
    m_Valid = false;
    m_ErrorMessage = "map uses an invalid amount of slots";
    return m_ErrorMessage;
  }

  if (!m_Aura->m_SupportsModernSlots) {
    if (
      m_MapNumControllers + m_MapNumDisabled > MAX_SLOTS_LEGACY ||
      m_MapNumTeams > MAX_SLOTS_LEGACY ||
      m_Slots.size() > MAX_SLOTS_LEGACY
    ) {
      m_Valid = false;
      m_ErrorMessage = "map uses too many slots - v1.29+ required";
      return m_ErrorMessage;
    }
  }

  bitset<MAX_SLOTS_MODERN> usedTeams;
  uint8_t controllerSlotCount = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() > m_MapVersionMaxSlots || slot.GetColor() > m_MapVersionMaxSlots) {
      m_Valid = false;
      m_ErrorMessage = "map uses an invalid amount of players";
      return m_ErrorMessage;
    }
    if (!m_Aura->m_SupportsModernSlots && (slot.GetTeam() > MAX_SLOTS_LEGACY || slot.GetColor() > MAX_SLOTS_LEGACY)) {
      m_Valid = false;
      m_ErrorMessage = "map uses too many players - v1.29+ required";
      return m_ErrorMessage;
    }
    if (slot.GetTeam() == m_MapVersionMaxSlots) {
      continue;
    }
    if (slot.GetTeam() > m_MapNumTeams) {
      m_Valid = false;
      m_ErrorMessage = "invalid <map.slot_N> detected";
      return m_ErrorMessage;
    }
    usedTeams.set(slot.GetTeam());
    ++controllerSlotCount;
  }
  if (controllerSlotCount != m_MapNumControllers) {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.slot_N> detected"; 
    return m_ErrorMessage;
  }
  if ((m_MapOptions & MAPOPT_CUSTOMFORCES) && usedTeams.count() <= 1) {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.slot_N> detected";
    return m_ErrorMessage;
  }

  if (!m_Valid) {
    return m_ErrorMessage;
  }

  return string();
}

void CMap::LoadGameConfigOverrides(CConfig& CFG)
{
  const bool wasStrict = CFG.GetStrictMode();
  CFG.SetStrictMode(true);

  if (CFG.Exists("map.hosting.game_over.player_count")) {
    m_NumPlayersToStartGameOver = CFG.GetUint8("map.hosting.game_over.player_count", 1);
  }
  if (CFG.Exists("map.hosting.game_ready.mode")) {
    m_PlayersReadyMode = CFG.GetStringIndex("map.hosting.game_ready.mode", {"fast", "race", "explicit"}, READY_MODE_EXPECT_RACE);
  }
  if (CFG.Exists("map.hosting.autostart.requires_balance")) {
    m_AutoStartRequiresBalance = CFG.GetBool("map.hosting.autostart.requires_balance", false);
  }

  if (CFG.Exists("map.net.lag_screen.enabled")) {
    m_EnableLagScreen = CFG.GetBool("map.net.lag_screen.enabled", false);
    CFG.FailIfErrorLast();
  }
  if (CFG.Exists("map.net.start_lag.sync_limit")) {
    m_LatencyMaxFrames = CFG.GetUint32("map.net.start_lag.sync_limit", 32);
  }
  if (CFG.Exists("map.net.stop_lag.sync_limit")) {
    m_LatencySafeFrames = CFG.GetUint32("map.net.stop_lag.sync_limit", 8);
  }

  if (CFG.Exists("map.hosting.high_ping.kick_ms")) {
    m_AutoKickPing = CFG.GetUint32("map.hosting.high_ping.kick_ms", 300);
  }
  if (CFG.Exists("map.hosting.high_ping.warn_ms")) {
    m_WarnHighPing = CFG.GetUint32("map.hosting.high_ping.warn_ms", 200);
  }
  if (CFG.Exists("map.hosting.high_ping.safe_ms")) {
    m_SafeHighPing = CFG.GetUint32("map.hosting.high_ping.safe_ms", 150);
  }

  if (CFG.Exists("map.hosting.expiry.lobby.mode")) {
    m_LobbyTimeoutMode = CFG.GetStringIndex("map.hosting.expiry.lobby.mode", {"never", "empty", "ownerless", "strict"}, LOBBY_TIMEOUT_OWNERLESS);
  }
  if (CFG.Exists("map.hosting.expiry.owner.mode")) {
    m_LobbyOwnerTimeoutMode = CFG.GetStringIndex("map.hosting.expiry.owner.mode", {"never", "absent", "strict"}, LOBBY_OWNER_TIMEOUT_ABSENT);
  }
  if (CFG.Exists("map.hosting.expiry.loading.mode")) {
    m_LoadingTimeoutMode = CFG.GetStringIndex("map.hosting.expiry.loading.mode", {"never", "strict"}, GAME_LOADING_TIMEOUT_STRICT);
  }
  if (CFG.Exists("map.hosting.expiry.playing.mode")) {
    m_PlayingTimeoutMode = CFG.GetStringIndex("map.hosting.expiry.playing.mode", {"never", "dry", "strict"}, GAME_PLAYING_TIMEOUT_STRICT);
  }

  if (CFG.Exists("map.hosting.expiry.lobby.timeout")) {
    m_LobbyTimeout = CFG.GetUint32("map.hosting.expiry.lobby.timeout", 600);
  }
  if (CFG.Exists("map.hosting.expiry.owner.timeout")) {
    m_LobbyOwnerTimeout = CFG.GetUint32("map.hosting.expiry.owner.timeout", 120);
  }
  if (CFG.Exists("map.hosting.expiry.loading.timeout")) {
    m_LoadingTimeout = CFG.GetUint32("map.hosting.expiry.loading.timeout", 900);
  }
  if (CFG.Exists("map.hosting.expiry.playing.timeout")) {
    m_PlayingTimeout = CFG.GetUint32("map.hosting.expiry.playing.timeout", 18000);
  }

  if (CFG.Exists("map.hosting.expiry.playing.timeout.warnings")) {
    m_PlayingTimeoutWarningShortCountDown = CFG.GetUint8("map.hosting.expiry.playing.timeout.soon_warnings", 10);
  }
  if (CFG.Exists("map.hosting.expiry.playing.timeout.interval")) {
    m_PlayingTimeoutWarningShortInterval = CFG.GetUint32("map.hosting.expiry.playing.timeout.soon_interval", 60);
  }
  if (CFG.Exists("map.hosting.expiry.playing.timeout.warnings")) {
    m_PlayingTimeoutWarningLargeCountDown = CFG.GetUint8("map.hosting.expiry.playing.timeout.eager_warnings", 5);
  }
  if (CFG.Exists("map.hosting.expiry.playing.timeout.interval")) {
    m_PlayingTimeoutWarningLargeInterval = CFG.GetUint32("map.hosting.expiry.playing.timeout.eager_interval", 900);
  }

  if (CFG.Exists("map.hosting.expiry.owner.lan")) {
    m_LobbyOwnerReleaseLANLeaver = CFG.GetBool("map.hosting.expiry.owner.lan", true);
  }

  if (CFG.Exists("map.hosting.game_start.count_down_interval")) {
    m_LobbyCountDownInterval = CFG.GetUint32("map.hosting.game_start.count_down_interval", 500);
  }
  if (CFG.Exists("map.hosting.game_start.count_down_ticks")) {
    m_LobbyCountDownStartValue = CFG.GetUint32("map.hosting.game_start.count_down_ticks", 5);
  }

  if (CFG.Exists("map.hosting.save_game.allowed")) {
    m_SaveGameAllowed = CFG.GetBool("map.hosting.save_game.allowed", false);
  }

  if (CFG.Exists("map.hosting.latency.default")) {
    m_Latency = CFG.GetUint16("map.hosting.latency.default", 100);
  }
  if (CFG.Exists("map.hosting.latency.equalizer.enabled")) {
    m_LatencyEqualizerEnabled = CFG.GetBool("map.hosting.latency.equalizer.enabled", false);
  }
  if (CFG.Exists("map.hosting.latency.equalizer.frames")) {
    m_LatencyEqualizerFrames = CFG.GetUint8("map.hosting.latency.equalizer.frames", PING_EQUALIZER_DEFAULT_FRAMES);
  }

  if (CFG.Exists("map.reconnection.mode")) {
    m_ReconnectionMode = CFG.GetStringIndex("map.reconnection.mode", {"disabled", "basic", "extended"}, RECONNECT_DISABLED);
    if (m_ReconnectionMode.value() == RECONNECT_ENABLED_GPROXY_EXTENDED) m_ReconnectionMode = m_ReconnectionMode.value() | RECONNECT_ENABLED_GPROXY_BASIC;
  }
  if (CFG.Exists("map.hosting.ip_filter.flood_handler")) {
    m_IPFloodHandler = CFG.GetStringIndex("map.hosting.ip_filter.flood_handler", {"none", "notify", "deny"}, ON_IPFLOOD_DENY);
  }
  if (CFG.Exists("map.hosting.game_protocol.leaver_handler")) {
    m_LeaverHandler = CFG.GetStringIndex("map.hosting.game_protocol.leaver_handler", {"none", "native", "share"}, ON_PLAYER_LEAVE_NATIVE);
  }
  if (CFG.Exists("map.hosting.game_protocol.share_handler")) {
    m_ShareUnitsHandler = CFG.GetStringIndex("map.hosting.game_protocol.share_handler", {"native", "kick", "restrict"}, ON_SHARE_UNITS_NATIVE);
  }
  if (CFG.Exists("map.hosting.name_filter.unsafe_handler")) {
    m_UnsafeNameHandler = CFG.GetStringIndex("map.hosting.name_filter.unsafe_handler", {"none", "censor", "deny"}, ON_UNSAFE_NAME_DENY);
  }
  if (CFG.Exists("map.hosting.realm_broadcast.error_handler")) {
    m_BroadcastErrorHandler = CFG.GetStringIndex("map.hosting.realm_broadcast.error_handler", {"ignore", "exit_main_error", "exit_empty_main_error", "exit_any_error", "exit_empty_any_error", "exit_max_errors"}, ON_ADV_ERROR_EXIT_ON_MAX_ERRORS);
  }
  if (CFG.Exists("map.hosting.name_filter.is_pipe_harmful")) {
    m_PipeConsideredHarmful = CFG.GetBool("map.hosting.name_filter.is_pipe_harmful", false);
  }
  if (CFG.Exists("map.auto_start.seconds")) {
    m_AutoStartSeconds = CFG.GetInt64("map.auto_start.seconds", 180);
  }
  if (CFG.Exists("map.auto_start.players")) {
    m_AutoStartPlayers = CFG.GetUint8("map.auto_start.players", 2);
  }
  if (CFG.Exists("map.hosting.apm_limiter.max.average")) {
    m_MaxAPM = CFG.GetUint16("map.hosting.apm_limiter.max.average", 800);
  }
  if (CFG.Exists("map.hosting.apm_limiter.max.burst")) {
    m_MaxBurstAPM = CFG.GetUint16("map.hosting.apm_limiter.max.burst", (uint16_t)APM_RATE_LIMITER_BURST_ACTIONS);
  }
  if (CFG.Exists("map.hosting.nicknames.hide_lobby")) {
    m_HideLobbyNames = CFG.GetBool("map.hosting.nicknames.hide_lobby", false);
  }
  if (CFG.Exists("map.hosting.nicknames.hide_in_game")) {
    m_HideInGameNames = CFG.GetStringIndex("map.hosting.nicknames.hide_in_game", {"never", "host", "always", "auto"}, HIDE_IGN_AUTO);
  }
  if (CFG.Exists("map.hosting.load_in_game.enabled")) {
    m_LoadInGame = CFG.GetBool("map.hosting.load_in_game.enabled", false);
  }
  if (CFG.Exists("map.hosting.join_in_progress.observers")) {
    m_EnableJoinObserversInProgress = CFG.GetBool("map.hosting.join_in_progress.observers", false);
  }
  if (CFG.Exists("map.hosting.join_in_progress.players")) {
    m_EnableJoinPlayersInProgress = CFG.GetBool("map.hosting.join_in_progress.players", false);
  }
  if (CFG.Exists("map.hosting.log_commands")) {
    m_LogCommands = CFG.GetBool("map.hosting.log_commands", false);
  }

  CFG.SetStrictMode(wasStrict);
}

void CMap::LoadMapSpecificConfig(CConfig& CFG)
{
  const bool wasStrict = CFG.GetStrictMode();
  CFG.SetStrictMode(true);

  // Note: m_ClientMapPath can be computed from m_MapServerPath - this is a cache
  m_ClientMapPath = CFG.GetString("map.path");

  // These aren't necessarily passed verbatim to CGameConfig
  // (CGameSetup members may be used instead)
  m_MapSpeed = CFG.GetUint8("map.speed", MAPSPEED_FAST);
  m_MapVisibility = CFG.GetUint8("map.visibility", MAPVIS_DEFAULT);
  if (CFG.Exists("map.observers")) {
    SetMapObservers(CFG.GetUint8("map.observers", m_MapObservers));
    CFG.FailIfErrorLast();
  }
  if (CFG.Exists("map.filter_obs")) {
    m_MapFilterObs = CFG.GetUint8("map.filter_obs", m_MapFilterObs);
    CFG.FailIfErrorLast();
  }
  m_MapFilterMaker = CFG.GetUint8("map.filter_maker", MAPFILTER_MAKER_USER);
  m_MapFilterSize = CFG.GetUint8("map.filter_size", MAPFILTER_SIZE_LARGE);

  m_MapSiteURL = CFG.GetString("map.meta.site");
  m_MapShortDesc = CFG.GetString("map.meta.short_desc");
  m_MapURL = CFG.GetString("map.meta.url");

  m_MapType = CFG.GetString("map.type");

  vector<string> aiTypes = {"none", "melee", "amai", "custom"};
  m_MapAIType = m_MapType == "evergreen" || m_MapType == "amai" ? AI_TYPE_AMAI : AI_TYPE_MELEE;
  if (CFG.Exists("map.ai.type")) {
    m_MapAIType = CFG.GetStringIndex("map.ai.type", aiTypes, m_MapAIType);
  } else {
    CFG.SetString("map.ai.type", aiTypes[m_MapAIType]);
  }

  if (m_MapOptions & MAPOPT_CUSTOMFORCES) {
    // Custom observer-team (one-based)
    m_MapCustomizableObserverTeam = CFG.GetUint8("map.custom_forces.observer_team", m_MapCustomizableObserverTeam);
    if (m_MapCustomizableObserverTeam == 0 || (m_MapNumTeams < m_MapCustomizableObserverTeam  && m_MapCustomizableObserverTeam != m_MapVersionMaxSlots + 1)) {
      Print("[MAP] <map.custom_forces.observer_team> invalid team number");
      CFG.SetFailed();
    }
  }
  --m_MapCustomizableObserverTeam;

  // HostBot Command Library (HCL)
  //
  // https://gist.github.com/Slayer95/a15fc75f38d0b3fdf356613ede96cf7f

  m_HCL.supported = m_MapType == "dota" || m_MapType == "evergreen";
  if (CFG.Exists("map.hcl.supported")) {
    m_HCL.supported = CFG.GetBool("map.hcl.supported", m_HCL.supported);
  } else {
    CFG.SetBool("map.hcl.supported", m_HCL.supported);
  }

  m_HCL.aboutVirtualPlayers = CFG.GetBool("map.hcl.info.virtual_players", false);

  const vector<string> toggleOptions = {"disabled", "optional", "required"};
  m_HCL.toggle = m_HCL.supported ? MAP_FEATURE_TOGGLE_OPTIONAL : MAP_FEATURE_TOGGLE_DISABLED;
  if (CFG.Exists("map.hcl.toggle")) {
    m_HCL.toggle = CFG.GetStringIndex("map.hcl.toggle", toggleOptions, m_HCL.toggle);
  } else {
    CFG.SetString("map.hcl.toggle", toggleOptions[m_HCL.toggle]);
  }

  m_HCL.defaultValue = ToLowerCase(CFG.GetString("map.hcl.default", 1, m_MapVersionMaxSlots, string()));

  if (!m_HCL.defaultValue.empty()) {
    if (!m_HCL.supported) {
      Print("[MAP] HCL cannot be enabled - map does not support it.");
      CFG.SetFailed();
    }

    if (m_HCL.aboutVirtualPlayers) {
      if (!CheckIsValidHCLSmall(m_HCL.defaultValue).empty()) {
        // short HCL may be a misnomer
        // the charset is short, which means we need longer strings for the same amount of information
        Print("[MAP] HCL string [" + m_HCL.defaultValue + "] is not valid virtual HCL (hexadecimal or in -\" \\).");
        CFG.SetFailed();
      }
    } else {
      if (!CheckIsValidHCLStandard(m_HCL.defaultValue).empty()) {
        Print("[MAP] HCL string [" + m_HCL.defaultValue + "] is not valid standard HCL (alphanumeric or in -= .,).");
        CFG.SetFailed();
      }
    }
  }

  // Warcraft 3 map metadata (W3MMD)
  //
  // https://wc3stats.com/docs/w3mmd

  m_MMD.supported = m_MapType == "dota" || m_MapType == "evergreen";
  if (CFG.Exists("map.w3mmd.supported")) {
    m_MMD.supported = CFG.GetBool("map.w3mmd.supported", m_MMD.supported);
  } else {
    CFG.SetBool("map.w3mmd.supported", m_MMD.supported);
  }

  m_MMD.enabled = m_MMD.supported;
  if (CFG.Exists("map.w3mmd.enabled")) {
    m_MMD.enabled = CFG.GetBool("map.w3mmd.enabled", m_MMD.enabled);
  } else {
    CFG.SetBool("map.w3mmd.enabled", m_MMD.enabled);
  }

  const vector<string> mmdTypes = {"standard", "dota"};
  m_MMD.type = m_MapType == "dota" ? MMD_TYPE_DOTA : MMD_TYPE_STANDARD;
  if (CFG.Exists("map.w3mmd.type")) {
    m_MMD.type = CFG.GetStringIndex("map.w3mmd.type", mmdTypes, m_MMD.type);
  } else {
    CFG.SetString("map.w3mmd.type", mmdTypes[m_MMD.type]);
  }

  // W3MMD v1 does not support attaching data to computer-controlled players
  m_MMD.aboutComputers = CFG.GetBool("map.w3mmd.subjects.computers.enabled", m_MapType == "evergreen");
  // W3MMD v1 has no way of distinguishing virtual from real players, so it expects some frames to be sent by virtual players
  m_MMD.emitSkipsVirtualPlayers = CFG.GetBool("map.w3mmd.features.virtual_players", false);

  m_MMD.emitPrioritizePlayers = m_MapType == "evergreen";
  if (CFG.Exists("map.w3mmd.features.prioritize_players")) {
    m_MMD.emitPrioritizePlayers = CFG.GetBool("map.w3mmd.features.prioritize_players", m_MMD.emitPrioritizePlayers);
  } else {
    CFG.SetBool("map.w3mmd.features.prioritize_players", m_MMD.emitPrioritizePlayers);
  }

  m_MMD.useGameOver = CFG.GetBool("map.w3mmd.features.game_over", false);

  if (m_MMD.enabled) {
    if (!m_MMD.supported) {
      Print("[MAP] W3MMD cannot be enabled - map does not support it.");
      CFG.SetFailed();
    }
  }

  // Host to bot map communication (W3HMC)
  //
  // https://github.com/cipherxof/th-ghost/wiki/1.-W3HMC
  // https://www.hiveworkshop.com/pastebin/41be696537187bb3b209c20dafeb2a81.16058

  m_HMC.supported = CFG.GetBool("map.w3hmc.supported", false);
  m_HMC.toggle = CFG.GetStringIndex("map.w3hmc.toggle", {"disabled", "optional", "required"}, MAP_FEATURE_TOGGLE_DISABLED);
  m_HMC.dwordA = CFG.GetUint32("map.w3hmc.trigger.main", 0);
  m_HMC.dwordB = CFG.GetUint32("map.w3hmc.trigger.complement", m_HMC.dwordA); // often these are the same, but not always!
  m_HMC.slot = CFG.GetSlot("map.w3hmc.slot", 0xFF); // cannot be observer
  m_HMC.playerName = CFG.GetString("map.w3hmc.player_name", 1, 15, "[HMC]Aura");
  m_HMC.fileName = CFG.GetString("map.w3hmc.file_name", 1, 15, "W3HMC");
  m_HMC.secret = CFG.GetString("map.w3hmc.secret", 1, 15, string());

  if (m_HMC.toggle != MAP_FEATURE_TOGGLE_DISABLED) {
    if (!m_HMC.supported) {
      Print("[MAP] W3HMC cannot be enabled - map does not support it.");
      CFG.SetFailed();
    } else if (m_HMC.slot == 0xFF || m_MapVersionMaxSlots <= m_HMC.slot) {
      // We want a slot specified even if custom forces is not set,
      // in order to solve conflicts with other virtual-player systems.
      Print("[MAP] <map.w3hmc.slot> is not properly configured.");
      CFG.SetFailed();
    } else if ((m_MapOptions & MAPOPT_CUSTOMFORCES) && (m_Slots.size() <= m_HMC.slot || m_Slots[m_HMC.slot].GetTeam() == m_MapVersionMaxSlots)) {
      Print("[MAP] <map.w3hmc.slot> cannot use an observer slot.");
      CFG.SetFailed();
    }
  }

  // AHCL
  //
  // http://web.archive.org/web/20230204221729/https://community.w3gh.ru/threads/%D0%9F%D1%80%D0%BE%D0%B4%D0%B2%D0%B8%D0%BD%D1%83%D1%82%D1%8B%D0%B9-hcl-beta.6241/

  m_AHCL.supported = CFG.GetBool("map.ahcl.supported", false);
  m_AHCL.toggle = CFG.GetStringIndex("map.ahcl.toggle", {"disabled", "optional", "required"}, MAP_FEATURE_TOGGLE_DISABLED);
  m_AHCL.slot = CFG.GetSlot("map.ahcl.slot", 0xFF); // cannot be observer
  m_AHCL.playerName = CFG.GetString("map.ahcl.player_name", 1, 15, "HostBot");
  m_AHCL.fileName = CFG.GetString("map.ahcl.file_name", 1, 15, "Asuna.Dat");
  m_AHCL.mission = CFG.GetString("map.ahcl.mission", 1, 15, "HostBot");
  m_AHCL.charset = CFG.GetString("map.ahcl.charset");

  if (m_AHCL.toggle != MAP_FEATURE_TOGGLE_DISABLED) {
    if (!m_AHCL.supported) {
      Print("[MAP] AHCL cannot be enabled - map does not support it.");
      CFG.SetFailed();
    } else if (m_AHCL.slot == 0xFF || m_MapVersionMaxSlots <= m_AHCL.slot) {
      // We want a slot specified even if custom forces is not set,
      // in order to solve conflicts with other virtual-player systems.
      Print("[MAP] <map.ahcl.slot> is not properly configured.");
      CFG.SetFailed();
    } else if ((m_MapOptions & MAPOPT_CUSTOMFORCES) && (m_Slots.size() <= m_AHCL.slot || m_Slots[m_AHCL.slot].GetTeam() == m_MapVersionMaxSlots)) {
      Print("[MAP] <map.ahcl.slot> cannot use an observer slot.");
      CFG.SetFailed();
    }
  }

  CFG.SetStrictMode(wasStrict);
}

uint8_t CMap::GetLobbyRace(const CGameSlot* slot) const
{
  bool isFixedRace = GetMapOptions() & MAPOPT_FIXEDPLAYERSETTINGS;
  bool isRandomRace = GetMapFlags() & MAPFLAG_RANDOMRACES;
  if (isFixedRace) return slot->GetRaceFixed();
  // If the map has fixed player settings, races cannot be randomized.
  if (isRandomRace) return SLOTRACE_RANDOM;
  // Note: If the slot was never selectable, it isn't promoted to selectable.
  return slot->GetRaceSelectable();
}

string CMap::SanitizeTrigStr(const string& input)
{
  string::size_type cursor = 0;
  string::size_type end = input.size();
  string output;
  while (cursor < end) {
    if (input[cursor] == '\\') {
      if (cursor + 1 < end) {
        output.push_back(input[cursor + 1]);
        cursor += 2;
        continue;
      }
      output.push_back(input[cursor]);
    } else if (input[cursor] == '|') {
      if (cursor + 1 < end && static_cast<char>(tolower(input[cursor + 1])) == 'r') {
        cursor += 2;
        continue;
      }
      if (cursor + 1 < end && static_cast<char>(tolower(input[cursor + 1])) == 'n') {
        output.push_back('\n');
        cursor += 2;
        continue;
      }
      if (cursor + 1 < end && static_cast<char>(tolower(input[cursor + 1])) == 'c') {
        cursor += 10;
        continue;
      }
    }
    if (input[cursor] == '\r') {
      cursor += 1;
      continue;
    }
    output.push_back(input[cursor]);
    cursor += 1;
  }
  return TrimStringExtended(output);
}

optional<string> CMap::GetTrigStr(const string& fileContents, const uint32_t targetNum)
{
  constexpr char openToken = '{';
  constexpr char closeToken = '}';

  optional<string> result;
  string line;
  istringstream ISS(fileContents);
  optional<uint32_t> stringCtx;
  bool inBraces = false;
  bool inTarget = false;
  while (true) {
    getline(ISS, line);
    if (ISS.fail()) {
      break;
    }
    if (!stringCtx.has_value()) {
      if (line.size() >= 8 && line.substr(0, 7) == "STRING ") {
        try {
          stringCtx = stol(line.substr(7));
        } catch (...) {}
      }
      if (stringCtx.has_value()) {
        inTarget = stringCtx.value() == targetNum;
      }
    } else {
      string trimmed = TrimStringExtended(line);
      if (!inBraces && trimmed.size() == 1 && trimmed[0] == openToken) {
        inBraces = true;
        if (inTarget) result.emplace();
      } else if (inBraces && trimmed.size() == 1 && trimmed[0] == closeToken) {
        inBraces = false;
        stringCtx.reset();
      } else if (inTarget && inBraces) {
        result->append(line);
      }
    }
  }

  result = CMap::SanitizeTrigStr(result.value());
  return result;
}

map<uint32_t, string> CMap::GetTrigStrMulti(const string& fileContents, const set<uint32_t> captureTargets)
{
  constexpr char openToken = '{';
  constexpr char closeToken = '}';

  map<uint32_t, string> result;
  optional<pair<uint32_t, string>> currentTarget;

  bool inBraces = false;
  bool firstLine = true;
  string line;
  istringstream ISS(fileContents);

  while (true) {
    getline(ISS, line);
    if (ISS.fail()) {
      break;
    }
    if (firstLine && line.substr(0, 3) == "\xEF\xBB\xBF") {
      // Strip UTF-8 BOM
      line = line.substr(3);
    }
    if (!currentTarget.has_value()) {
      if (line.size() >= 8 && line.substr(0, 7) == "STRING ") {
        optional<int64_t> strNum;
        try {
          strNum = stol(line.substr(7));
        } catch (...) {}
        if (strNum.has_value() && strNum >= 0 && strNum <= 0xFFFFFFFF) {
          uint32_t num = static_cast<uint32_t>(strNum.value());
          if (captureTargets.find(num) != captureTargets.end()) {
            currentTarget = make_pair(num, string());
          }
        }
      }
    } else {
      string trimmed = TrimStringExtended(line);
      if (!inBraces && trimmed.size() == 1 && trimmed[0] == openToken) {
        inBraces = true;
      } else if (inBraces && trimmed.size() == 1 && trimmed[0] == closeToken) {
        inBraces = false;
        if (currentTarget.has_value()) {
          currentTarget->second = CMap::SanitizeTrigStr(currentTarget->second);
          auto it = result.find(currentTarget->first);
          if (it == result.end()) {
            result[currentTarget->first] = currentTarget->second;
          } else {
            it->second = currentTarget->second;
          }
          currentTarget.reset();
        }
      } else if (currentTarget.has_value() && inBraces) {
        currentTarget->second.append(line);
      }
    }
    firstLine = false;
  }

  return result;
}
