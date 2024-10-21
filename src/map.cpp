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
#include "fileutil.h"
#include "crc32.h"
#include "sha1.h"
#include "config.h"
#include "config_bot.h"
#include "config_game.h"
#include "gameslot.h"

#define __STORMLIB_SELF__
#include <StormLib.h>

#define ROTL(x, n) ((x) << (n)) | ((x) >> (32 - (n))) // this won't work with signed types
#define ROTR(x, n) ((x) >> (n)) | ((x) << (32 - (n))) // this won't work with signed types

using namespace std;

//
// CMap
//

CMap::CMap(CAura* nAura, CConfig* CFG, const bool skipVersionCheck)
  : m_Aura(nAura),
    m_MapObservers(MAPOBS_NONE),
    m_MapFilterObs(MAPFILTER_OBS_NONE),
    m_MapMPQLoaded(false),
    m_MapMPQErrored(false),
    m_ProxyReconnect(RECONNECT_ENABLED_GPROXY_BASIC | RECONNECT_ENABLED_GPROXY_EXTENDED),
    m_UseStandardPaths(false),
    m_SkipVersionCheck(skipVersionCheck),
    m_HMCMode(W3HMC_MODE_DISABLED)
{
  m_MapScriptsSHA1.fill(0);
  m_MapSize.fill(0);
  m_MapCRC32.fill(0);
  m_MapScriptsWeakHash.fill(0);
  m_MapWidth.fill(0);
  m_MapHeight.fill(0);
  m_MapContentMismatch.fill(0);

  Load(CFG);
}

CMap::~CMap() = default;

uint32_t CMap::GetMapGameFlags() const
{
  uint32_t GameFlags = 0;

  // speed

  if (m_MapSpeed == MAPSPEED_SLOW)
    GameFlags = 0x00000000;
  else if (m_MapSpeed == MAPSPEED_NORMAL)
    GameFlags = 0x00000001;
  else
    GameFlags = 0x00000002;

  // visibility

  if (m_MapVisibility == MAPVIS_HIDETERRAIN)
    GameFlags |= 0x00000100;
  else if (m_MapVisibility == MAPVIS_EXPLORED)
    GameFlags |= 0x00000200;
  else if (m_MapVisibility == MAPVIS_ALWAYSVISIBLE)
    GameFlags |= 0x00000400;
  else
    GameFlags |= 0x00000800;

  // observers

  if (m_MapObservers == MAPOBS_ONDEFEAT)
    GameFlags |= 0x00002000;
  else if (m_MapObservers == MAPOBS_ALLOWED)
    GameFlags |= 0x00003000;
  else if (m_MapObservers == MAPOBS_REFEREES)
    GameFlags |= 0x40000000;

  // teams/units/hero/race

  if (m_MapFlags & MAPFLAG_TEAMSTOGETHER) {
    GameFlags |= 0x00004000;
  }

  if (m_MapFlags & MAPFLAG_FIXEDTEAMS)
    GameFlags |= 0x00060000;

  if (m_MapFlags & MAPFLAG_UNITSHARE)
    GameFlags |= 0x01000000;

  if (m_MapFlags & MAPFLAG_RANDOMHERO)
    GameFlags |= 0x02000000;

  if (!(m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS)) {
    // WC3 GUI is misleading in displaying the Random Races tickbox when creating LAN games.
    // It even shows Random Races: Yes in the game lobby.
    // However, this flag is totally ignored when Fixed Player Settings is enabled.
    if (m_MapFlags & MAPFLAG_RANDOMRACES)
      GameFlags |= 0x04000000;
  }

  return GameFlags;
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
  // 2 = fixed player settings (not possible with the Warcraft III map editor)
  // 3 = custom forces + fixed player settings

  if (!(m_MapOptions & MAPOPT_CUSTOMFORCES))
    return MAPLAYOUT_ANY;

  if (!(m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS))
    return MAPLAYOUT_CUSTOM_FORCES;

  return MAPLAYOUT_FIXED_PLAYERS;
}

string CMap::GetServerFileName() const
{
  filesystem::path filePath = m_MapServerPath;
  return PathToString(filePath.filename());
}

string CMap::GetClientFileName() const
{
  size_t LastSlash = m_ClientMapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_ClientMapPath;
  }
  return m_ClientMapPath.substr(LastSlash + 1);
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
    m_MapFlags |= MAPFLAG_FIXEDTEAMS;
  } else {
    m_MapFlags &= ~MAPFLAG_FIXEDTEAMS;
  }
  return true;
}

bool CMap::SetTeamsTogether(const bool nEnable)
{
  if (nEnable) {
    m_MapFlags |= MAPFLAG_TEAMSTOGETHER;
  } else {
    m_MapFlags &= ~MAPFLAG_TEAMSTOGETHER;
  }
  return true;
}

bool CMap::SetAdvancedSharedUnitControl(const bool nEnable)
{
  if (nEnable) {
    m_MapFlags |= MAPFLAG_UNITSHARE;
  } else {
    m_MapFlags &= ~MAPFLAG_UNITSHARE;
  }
  return true;
}

bool CMap::SetRandomHeroes(const bool nEnable)
{
  if (nEnable) {
    m_MapFlags |= MAPFLAG_RANDOMHERO;
  } else {
    m_MapFlags &= ~MAPFLAG_RANDOMHERO;
  }
  return true;
}

bool CMap::SetRandomRaces(const bool nEnable)
{
  if (m_MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) {
    return false;
  }
  if (nEnable) {
    m_MapFlags |= MAPFLAG_RANDOMRACES;
  } else {
    m_MapFlags &= ~MAPFLAG_RANDOMRACES;
  }
  return true;
}

void CMap::Load(CConfig* CFG)
{
  m_Valid   = true;
  m_CFGName = PathToString(CFG->GetFile().filename());
  const static string emptyString;

  // load the map data

  m_UseStandardPaths = CFG->GetBool("map.standard_path", false);
  m_MapServerPath = CFG->GetString("map.local_path", emptyString);
  m_MapData.clear();

  bool isPartial = CFG->GetBool("map.cfg.partial", false);
  bool ignoreMPQ = m_MapServerPath.empty() || (!isPartial && m_Aura->m_Config->m_CFGCacheRevalidateAlgorithm == CACHE_REVALIDATION_NEVER);

  size_t RawMapSize = 0;
  if (isPartial || m_Aura->m_Net->m_Config->m_AllowTransfers != MAP_TRANSFERS_NEVER) {
    if (m_MapServerPath.empty()) {
      return;
    }
    filesystem::path mapServerPath(m_MapServerPath);
    if (mapServerPath.filename() == mapServerPath && !m_UseStandardPaths) {
      m_MapData = FileRead(m_Aura->m_Config->m_MapPath / mapServerPath, &RawMapSize);
    } else {
      m_MapData = FileRead(m_MapServerPath, &RawMapSize);
    }
    if (isPartial && m_MapData.empty()) {
      Print("[AURA] Local map not found for partial config file");
      return;
    }
    if (RawMapSize > 0x18000000) {
      Print("[AURA] warning - map exceeds maximum file size");
      m_MapData.clear();
      return;
    }
    if (RawMapSize == 0) {
      ignoreMPQ = true;
    }
  }

  optional<int64_t> CachedModifiedTime = CFG->GetMaybeInt64("map.local_mod_time");
  optional<int64_t> FileModifiedTime;
  filesystem::path MapMPQFilePath(m_MapServerPath);

  if (!ignoreMPQ) {
    if (MapMPQFilePath.filename() == MapMPQFilePath && !m_UseStandardPaths) {
      MapMPQFilePath = m_Aura->m_Config->m_MapPath / MapMPQFilePath;
    } else {
      MapMPQFilePath = m_MapServerPath;
    }    
    FileModifiedTime = GetMaybeModifiedTime(MapMPQFilePath);
    ignoreMPQ = (
      !isPartial && m_Aura->m_Config->m_CFGCacheRevalidateAlgorithm == CACHE_REVALIDATION_MODIFIED && (
        !FileModifiedTime.has_value() || (
          CachedModifiedTime.has_value() && FileModifiedTime.has_value() &&
          FileModifiedTime.value() <= CachedModifiedTime.value()
        )
      )
    );
  }
  if (FileModifiedTime.has_value()) {
    if (!CachedModifiedTime.has_value() || FileModifiedTime.value() != CachedModifiedTime.value()) {
      CFG->SetInt64("map.local_mod_time", FileModifiedTime.value());
      CFG->SetIsModified();
    }
  }

  uint32_t mapLocale = CFG->GetUint32("map.locale", 0);
  SFileSetLocale(mapLocale);

  void* MapMPQ;
  if (!ignoreMPQ) {
    // load the map MPQ file
    m_MapMPQLoaded = OpenMPQArchive(&MapMPQ, MapMPQFilePath);
    if (!m_MapMPQLoaded) {
      m_MapMPQErrored = true;
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
      Print("[MAP] warning - unable to load MPQ file [" + PathToString(MapMPQFilePath) + "] - " + errorCodeString);
    }
  }

  // try to calculate <map.size>, <map.crc32>, <map.weak_hash>, <map.sha1>

  std::vector<uint8_t> MapSize, MapCRC32, MapScriptsWeakHash, MapScriptsSHA1;

  if (!m_MapData.empty()) {
    m_Aura->m_SHA->Reset();

    // calculate <map.crc32>

    MapCRC32 = CreateByteArray(m_Aura->m_CRC->CalculateCRC((uint8_t*)m_MapData.c_str(), m_MapData.size()), false);
    if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
      Print("[MAP] calculated <map.crc32 = " + ByteArrayToDecString(MapCRC32) + ">");
    }

    // calculate <map.weak_hash>, and <map.sha1>
    // a big thank you to Strilanc for figuring the <map.weak_hash> algorithm out

    filesystem::path commonPath = m_Aura->m_Config->m_JASSPath / filesystem::path("common-" + to_string(m_Aura->m_GameVersion) +".j");
    string CommonJ = FileRead(commonPath, nullptr);

    if (CommonJ.empty())
      Print("[MAP] unable to calculate <map.weak_hash>, and <map.sha1> - unable to read file [" + PathToString(commonPath) + "]");
    else
    {
      filesystem::path blizzardPath = m_Aura->m_Config->m_JASSPath / filesystem::path("blizzard-" + to_string(m_Aura->m_GameVersion) +".j");
      string BlizzardJ = FileRead(blizzardPath, nullptr);

      if (BlizzardJ.empty())
        Print("[MAP] unable to calculate <map.weak_hash>, and <map.sha1> - unable to read file [" + PathToString(blizzardPath) + "]");
      else
      {
        uint32_t Val = 0;

        // update: it's possible for maps to include their own copies of common.j and/or blizzard.j
        // this code now overrides the default copies if required

        bool OverrodeCommonJ   = false;
        bool OverrodeBlizzardJ = false;

        if (m_MapMPQLoaded)
        {
          void* SubFile;

          // override common.j

          if (SFileOpenFileEx(MapMPQ, R"(Scripts\common.j)", 0, &SubFile))
          {
            uint32_t FileLength = SFileGetFileSize(SubFile, nullptr);

            if (FileLength > 0 && FileLength != 0xFFFFFFFF)
            {
              auto  SubFileData = new char[FileLength];
#ifdef _WIN32
              unsigned long BytesRead = 0;
#else
              uint32_t BytesRead = 0;
#endif

              if (SFileReadFile(SubFile, SubFileData, FileLength, &BytesRead, nullptr))
              {
                Print("[MAP] overriding default common.j with map copy while calculating <map.weak_hash>, and <map.sha1>");
                OverrodeCommonJ = true;
                Val             = Val ^ XORRotateLeft(reinterpret_cast<uint8_t*>(SubFileData), BytesRead);
                m_Aura->m_SHA->Update(reinterpret_cast<uint8_t*>(SubFileData), BytesRead);
              }

              delete[] SubFileData;
            }

            SFileCloseFile(SubFile);
          }
        }

        if (!OverrodeCommonJ)
        {
          Val = Val ^ XORRotateLeft((uint8_t*)CommonJ.c_str(), static_cast<uint32_t>(CommonJ.size()));
          m_Aura->m_SHA->Update((uint8_t*)CommonJ.c_str(), static_cast<uint32_t>(CommonJ.size()));
        }

        if (m_MapMPQLoaded)
        {
          void* SubFile;

          // override blizzard.j

          if (SFileOpenFileEx(MapMPQ, R"(Scripts\blizzard.j)", 0, &SubFile))
          {
            uint32_t FileLength = SFileGetFileSize(SubFile, nullptr);

            if (FileLength > 0 && FileLength != 0xFFFFFFFF)
            {
              auto  SubFileData = new char[FileLength];
#ifdef _WIN32
              unsigned long BytesRead = 0;
#else
              uint32_t BytesRead = 0;
#endif

              if (SFileReadFile(SubFile, SubFileData, FileLength, &BytesRead, nullptr))
              {
                Print("[MAP] overriding default blizzard.j with map copy while calculating <map.weak_hash>, and <map.sha1>");
                OverrodeBlizzardJ = true;
                Val               = Val ^ XORRotateLeft(reinterpret_cast<uint8_t*>(SubFileData), BytesRead);
                m_Aura->m_SHA->Update(reinterpret_cast<uint8_t*>(SubFileData), BytesRead);
              }

              delete[] SubFileData;
            }

            SFileCloseFile(SubFile);
          }
        }

        if (!OverrodeBlizzardJ)
        {
          Val = Val ^ XORRotateLeft((uint8_t*)BlizzardJ.c_str(), static_cast<uint32_t>(BlizzardJ.size()));
          m_Aura->m_SHA->Update((uint8_t*)BlizzardJ.c_str(), static_cast<uint32_t>(BlizzardJ.size()));
        }

        Val = ROTL(Val, 3);
        Val = ROTL(Val ^ 0x03F1379E, 3);
        m_Aura->m_SHA->Update((uint8_t*)"\x9E\x37\xF1\x03", 4);

        if (m_MapMPQLoaded)
        {
          vector<string> FileList;
          FileList.emplace_back("war3map.j");
          FileList.emplace_back(R"(scripts\war3map.j)");
          FileList.emplace_back("war3map.w3e");
          FileList.emplace_back("war3map.wpm");
          FileList.emplace_back("war3map.doo");
          FileList.emplace_back("war3map.w3u");
          FileList.emplace_back("war3map.w3b");
          FileList.emplace_back("war3map.w3d");
          FileList.emplace_back("war3map.w3a");
          FileList.emplace_back("war3map.w3q");
          bool FoundScript = false;

          for (auto& fileName : FileList)
          {
            // don't use scripts\war3map.j if we've already used war3map.j (yes, some maps have both but only war3map.j is used)

            if (FoundScript && fileName == R"(scripts\war3map.j)")
              continue;

            void* SubFile;

            if (SFileOpenFileEx(MapMPQ, fileName.c_str(), 0, &SubFile))
            {
              uint32_t FileLength = SFileGetFileSize(SubFile, nullptr);

              if (FileLength > 0 && FileLength != 0xFFFFFFFF)
              {
                auto  SubFileData = new char[FileLength];
#ifdef _WIN32
                unsigned long BytesRead = 0;
#else
                uint32_t BytesRead = 0;
#endif

                if (SFileReadFile(SubFile, SubFileData, FileLength, &BytesRead, nullptr))
                {
                  if (fileName == "war3map.j" || fileName == R"(scripts\war3map.j)")
                    FoundScript = true;

                  Val = ROTL(Val ^ XORRotateLeft((uint8_t*)SubFileData, BytesRead), 3);
                  m_Aura->m_SHA->Update(reinterpret_cast<uint8_t*>(SubFileData), BytesRead);
                }

                delete[] SubFileData;
              }

              SFileCloseFile(SubFile);
            }
          }

          if (!FoundScript)
            Print(R"([MAP] couldn't find war3map.j or scripts\war3map.j in MPQ file, calculated <map.weak_hash>, and <map.sha1> is probably wrong)");

          MapScriptsWeakHash = CreateByteArray(Val, false);
          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
            Print("[MAP] calculated <map.weak_hash = " + ByteArrayToDecString(MapScriptsWeakHash) + ">");
          }

          m_Aura->m_SHA->Final();
          uint8_t SHA1[20];
          memset(SHA1, 0, sizeof(uint8_t) * 20);
          m_Aura->m_SHA->GetHash(SHA1);
          MapScriptsSHA1 = CreateByteArray(SHA1, 20);
          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
            Print("[MAP] calculated <map.sha1 = " + ByteArrayToDecString(MapScriptsSHA1) + ">");
          }
        }
        else
          Print("[MAP] skipping <map.weak_hash>, and <map.sha1> calculation - map not decompressed");
      }
    }
  }
  else
    Print("[MAP] no map data available, using config file for <map.size>, <map.crc32>, <map.weak_hash>, <map.sha1>");

  // try to calculate <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams>, <map.filter_type>

  std::vector<uint8_t> MapWidth;
  std::vector<uint8_t> MapHeight;
  uint32_t             MapEditorVersion = 0;
  uint32_t             MapOptions    = 0;
  uint8_t              MapNumPlayers = 0;
  uint8_t              MapNumDisabled = 0;
  uint8_t              MapFilterType = MAPFILTER_TYPE_SCENARIO;
  uint8_t              MapNumTeams   = 0;
  uint8_t              MapMinGameVersion = 0;
  vector<CGameSlot>    Slots;

  if (isPartial && m_MapMPQLoaded) {
    void* SubFile;

    if (SFileOpenFileEx(MapMPQ, "war3map.w3i", 0, &SubFile))
    {
      uint32_t FileLength = SFileGetFileSize(SubFile, nullptr);

      if (FileLength > 0 && FileLength != 0xFFFFFFFF)
      {
        auto  SubFileData = new char[FileLength];
#ifdef _WIN32
        unsigned long BytesRead = 0;
#else
        uint32_t BytesRead = 0;
#endif

        if (SFileReadFile(SubFile, SubFileData, FileLength, &BytesRead, nullptr))
        {
          istringstream ISS(string(SubFileData, BytesRead));

          // war3map.w3i format found at http://www.wc3campaigns.net/tools/specs/index.html by Zepir/PitzerMike

          string   GarbageString;
          uint32_t FileFormat;
          uint32_t RawEditorVersion;
          uint32_t RawMapFlags;
          uint32_t RawMapWidth, RawMapHeight;
          uint32_t RawMapNumPlayers, RawMapNumTeams;

          ISS.read(reinterpret_cast<char*>(&FileFormat), 4); // file format (18 = ROC, 25 = TFT)

          if (FileFormat == 18 || FileFormat == 25)
          {
            ISS.seekg(4, ios::cur);            // number of saves
            ISS.read(reinterpret_cast<char*>(&RawEditorVersion), 4); // editor version
            getline(ISS, GarbageString, '\0'); // map name
            getline(ISS, GarbageString, '\0'); // map author
            getline(ISS, GarbageString, '\0'); // map description
            getline(ISS, GarbageString, '\0'); // players recommended
            ISS.seekg(32, ios::cur);           // camera bounds
            ISS.seekg(16, ios::cur);           // camera bounds complements
            ISS.read(reinterpret_cast<char*>(&RawMapWidth), 4);  // map width
            ISS.read(reinterpret_cast<char*>(&RawMapHeight), 4); // map height
            ISS.read(reinterpret_cast<char*>(&RawMapFlags), 4);  // flags
            ISS.seekg(1, ios::cur);            // map main ground type

            if (FileFormat == 18)
              ISS.seekg(4, ios::cur); // campaign background number
            else if (FileFormat == 25)
            {
              ISS.seekg(4, ios::cur);            // loading screen background number
              getline(ISS, GarbageString, '\0'); // path of custom loading screen model
            }

            getline(ISS, GarbageString, '\0'); // map loading screen text
            getline(ISS, GarbageString, '\0'); // map loading screen title
            getline(ISS, GarbageString, '\0'); // map loading screen subtitle

            if (FileFormat == 18)
              ISS.seekg(4, ios::cur); // map loading screen number
            else if (FileFormat == 25)
            {
              ISS.seekg(4, ios::cur);            // used game data set
              getline(ISS, GarbageString, '\0'); // prologue screen path
            }

            getline(ISS, GarbageString, '\0'); // prologue screen text
            getline(ISS, GarbageString, '\0'); // prologue screen title
            getline(ISS, GarbageString, '\0'); // prologue screen subtitle

            if (FileFormat == 25)
            {
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

            MapEditorVersion = RawEditorVersion;

            ISS.read(reinterpret_cast<char*>(&RawMapNumPlayers), 4); // number of players
            if (RawMapNumPlayers > MAX_SLOTS_MODERN) RawMapNumPlayers = 0;
            uint8_t closedSlots = 0;
            uint8_t disabledSlots = 0;

            for (uint32_t i = 0; i < RawMapNumPlayers; ++i)
            {
              CGameSlot Slot(SLOTTYPE_AUTO, 0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, 0, 1, SLOTRACE_RANDOM);
              uint32_t  Color, Type, Race;
              ISS.read(reinterpret_cast<char*>(&Color), 4); // colour
              Slot.SetColor(static_cast<uint8_t>(Color));
              ISS.read(reinterpret_cast<char*>(&Type), 4); // type

              if (Type == SLOTTYPE_NONE) {
                Slot.SetType(Type);
                Slot.SetSlotStatus(SLOTSTATUS_CLOSED);
                ++closedSlots;
              } else {
                if (!(RawMapFlags & MAPOPT_FIXEDPLAYERSETTINGS)) {
                  // WC3 ignores slots defined in WorldEdit if Fixed Player Settings is disabled.
                  Type = SLOTTYPE_USER;
                }
                if (Type <= SLOTTYPE_RESCUEABLE) {
                  Slot.SetType(Type);
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

              if (Slot.GetSlotStatus() != SLOTSTATUS_CLOSED)
                Slots.push_back(Slot);
            }

            ISS.read(reinterpret_cast<char*>(&RawMapNumTeams), 4); // number of teams
            if (RawMapNumTeams > MAX_SLOTS_MODERN) RawMapNumTeams = 0;

            if (RawMapNumPlayers > 0 && RawMapNumTeams > 0) {
              // the bot only cares about the following options: melee, fixed player settings, custom forces
              // let's not confuse the user by displaying erroneous map options so zero them out now
              MapOptions = RawMapFlags & (MAPOPT_MELEE | MAPOPT_FIXEDPLAYERSETTINGS | MAPOPT_CUSTOMFORCES);
              if (MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) MapOptions |= MAPOPT_CUSTOMFORCES;

              if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                Print("[MAP] calculated <map.options = " + to_string(MapOptions) + ">");
              }

              if (!(MapOptions & MAPOPT_CUSTOMFORCES)) {
                MapNumTeams = static_cast<uint8_t>(RawMapNumPlayers);
              } else {
                MapNumTeams = static_cast<uint8_t>(RawMapNumTeams);
              }

              for (uint32_t i = 0; i < MapNumTeams; ++i) {
                uint32_t PlayerMask = 0;
                if (i < RawMapNumTeams) {
                  ISS.seekg(4, ios::cur);                            // flags
                  ISS.read(reinterpret_cast<char*>(&PlayerMask), 4); // player mask
                }
                if (!(MapOptions & MAPOPT_CUSTOMFORCES)) {
                  PlayerMask = 1 << i;
                }

                for (auto& Slot : Slots) {
                  if (0 != (PlayerMask & (1 << static_cast<uint32_t>((Slot).GetColor())))) {
                    Slot.SetTeam(static_cast<uint8_t>(i));
                  }
                }

                if (i < RawMapNumTeams) {
                  getline(ISS, GarbageString, '\0'); // team name
                }
              }

              MapWidth = CreateByteArray(static_cast<uint16_t>(RawMapWidth), false);
              MapHeight = CreateByteArray(static_cast<uint16_t>(RawMapHeight), false);
              MapNumPlayers = static_cast<uint8_t>(RawMapNumPlayers) - closedSlots;
              MapNumDisabled = disabledSlots;

              if (MapOptions & MAPOPT_MELEE) {
                if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                  Print("[MAP] found melee map");
                }
                MapFilterType = MAPFILTER_TYPE_MELEE;
              }

              if (!(MapOptions & MAPOPT_FIXEDPLAYERSETTINGS)) {
                // make races selectable

                for (auto& Slot : Slots)
                  (Slot).SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
              }

              uint32_t SlotNum = 1;

              if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                Print("[MAP] calculated <map.width = " + ByteArrayToDecString(MapWidth) + ">");
                Print("[MAP] calculated <map.height = " + ByteArrayToDecString(MapHeight) + ">");
                Print("[MAP] calculated <map.num_disabled = " + ToDecString(MapNumDisabled) + ">");
                Print("[MAP] calculated <map.num_players = " + ToDecString(MapNumPlayers) + ">");
                Print("[MAP] calculated <map.num_teams = " + ToDecString(MapNumTeams) + ">");
              }

              for (auto& Slot : Slots) {
                if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                  Print("[MAP] calculated <map.slot_" + to_string(SlotNum) + " = " + ByteArrayToDecString((Slot).GetProtocolArray()) + ">");
                }
                ++SlotNum;
              }
            } else {
              Print("[MAP] unable to calculate <map.slot_N>, <map.num_players>, <map.num_teams> - unable to extract war3map.w3i from map file");
            }
          }
        }
        else
          Print("[MAP] unable to calculate <map.options>, <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams> - unable to extract war3map.w3i from map file");

        delete[] SubFileData;
      }

      SFileCloseFile(SubFile);
    } else {
      Print("[MAP] unable to calculate <map.options>, <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams> - couldn't find war3map.w3i in map file");
    }
  } else {
    if (!isPartial) {
      //This is debug log
      if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
        Print("[MAP] using mapcfg for <map.options>, <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams>");
      }
    } else if (!m_MapMPQLoaded) {
      Print("[MAP] unable to calculate <map.options>, <map.width>, <map.height>, <map.slot_N>, <map.num_players>, <map.num_teams> - map archive not loaded");
    }
  }

  // close the map MPQ

  if (m_MapMPQLoaded)
    SFileCloseArchive(MapMPQ);

  m_ClientMapPath = CFG->GetString("map.path", emptyString);
  array<uint8_t, 4> MapContentMismatch = {0, 0, 0, 0};

  if (CFG->Exists("map.size")) {
    string CFGValue = CFG->GetString("map.size", emptyString);
    if (RawMapSize != 0) {
      string MapValue = ByteArrayToDecString(CreateByteArray(static_cast<uint32_t>(RawMapSize), false));
      MapContentMismatch[0] = CFGValue != MapValue;
    }
    MapSize = ExtractNumbers(CFGValue, 4);
  } else if (RawMapSize != 0) {
    MapSize = CreateByteArray(static_cast<uint32_t>(RawMapSize), false);
    CFG->SetUint8Vector("map.size", MapSize);
  }

  if (MapSize.size() != 4) {
    CFG->SetFailed();
    m_ErrorMessage = "invalid <map.size> detected";
  } else {
    copy_n(MapSize.begin(), 4, m_MapSize.begin());
  }

  if (CFG->Exists("map.crc32")) {
    string CFGValue = CFG->GetString("map.crc32", emptyString);
    if (!MapCRC32.empty()) {
      string MapValue = ByteArrayToDecString(MapCRC32);
      MapContentMismatch[1] = CFGValue != MapValue;
    }
    MapCRC32 = ExtractNumbers(CFGValue, 4);
  } else if (!MapCRC32.empty()) {
    CFG->SetUint8Vector("map.crc32", MapCRC32);
  }

  if (MapCRC32.size() != 4) {
    CFG->SetFailed();
    m_ErrorMessage = "invalid <map.crc32> detected";
  } else {
    copy_n(MapCRC32.begin(), 4, m_MapCRC32.begin());
  }

  if (CFG->Exists("map.weak_hash")) {
    string CFGValue = CFG->GetString("map.weak_hash", emptyString);
    if (!MapScriptsWeakHash.empty()) {
      string MapValue = ByteArrayToDecString(MapScriptsWeakHash);
      MapContentMismatch[2] = CFGValue != MapValue;
    }
    MapScriptsWeakHash = ExtractNumbers(CFGValue, 4);
  } else if (!MapScriptsWeakHash.empty()) {
    CFG->SetUint8Vector("map.weak_hash", MapScriptsWeakHash);
  }

  if (MapScriptsWeakHash.size() != 4) {
    CFG->SetFailed();
    m_ErrorMessage = "invalid <map.weak_hash> detected";
  } else {
    copy_n(MapScriptsWeakHash.begin(), 4, m_MapScriptsWeakHash.begin());
  }

  if (CFG->Exists("map.sha1")) {
    string CFGValue = CFG->GetString("map.sha1", emptyString);
    if (!MapScriptsSHA1.empty()) {
      string MapValue = ByteArrayToDecString(MapScriptsSHA1);
      MapContentMismatch[3] = CFGValue != MapValue;
    }
    MapScriptsSHA1 = ExtractNumbers(CFGValue, 20);
  } else if (!MapScriptsSHA1.empty()) {
    CFG->SetUint8Vector("map.sha1", MapScriptsSHA1);
  }

  if (MapScriptsSHA1.size() != 20) {
    CFG->SetFailed();
    m_ErrorMessage = "invalid <map.sha1> detected";
  } else {
    copy_n(MapScriptsSHA1.begin(), 20, m_MapScriptsSHA1.begin());
  }

  if (!m_MapData.empty() && HasMismatch()) {
    m_MapContentMismatch = MapContentMismatch;
    Print("[CACHE] error - map content mismatch");
  }

  m_MapSiteURL   = CFG->GetString("map.site", emptyString);
  m_MapShortDesc = CFG->GetString("map.short_desc", emptyString);
  m_MapURL       = CFG->GetString("map.url", emptyString);

  if (CFG->Exists("map.filter_type")) {
    MapFilterType = CFG->GetUint8("map.filter_type", MAPFILTER_TYPE_SCENARIO);
  } else {
    CFG->SetUint8("map.filter_type", MapFilterType);
  }

  // Host to bot map communication (W3HMC)
  m_HMCMode = CFG->GetStringIndex("map.w3hmc.mode", {"disabled", "optional", "required"}, W3HMC_MODE_DISABLED);
  m_HMCTrigger1 = CFG->GetUint8("map.w3hmc.trigger_1", 0);
  m_HMCTrigger2 = CFG->GetUint8("map.w3hmc.trigger_2", 0);
  m_HMCSlot = CFG->GetUint8("map.w3hmc.slot", 1);
  m_HMCPlayerName = CFG->GetString("map.w3hmc.player_name", 1, 15, "[HMC]Aura");

  // CGameConfig overrides
  if (CFG->Exists("map.hosting.game_over.player_count")) {
    m_NumPlayersToStartGameOver = CFG->GetUint8("map.hosting.game_over.player_count", 1);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.game_ready.mode")) {
    m_PlayersReadyMode = CFG->GetStringIndex("map.hosting.game_ready.mode", {"fast", "race", "explicit"}, READY_MODE_EXPECT_RACE);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.net.start_lag.sync_limit")) {
    m_LatencyMaxFrames = CFG->GetUint32("map.net.start_lag.sync_limit", 32);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.net.stop_lag.sync_limit")) {
    m_LatencySafeFrames = CFG->GetUint32("map.net.stop_lag.sync_limit", 8);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.high_ping.kick_ms")) {
    m_AutoKickPing = CFG->GetUint32("map.hosting.high_ping.kick_ms", 300);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.high_ping.warn_ms")) {
    m_WarnHighPing = CFG->GetUint32("map.hosting.high_ping.warn_ms", 200);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.high_ping.safe_ms")) {
    m_SafeHighPing = CFG->GetUint32("map.hosting.high_ping.safe_ms", 150);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.abandoned_lobby.game_expiry_time")) {
    m_LobbyTimeout = CFG->GetUint32("map.hosting.abandoned_lobby.game_expiry_time", 600);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.abandoned_lobby.owner_expiry_time")) {
    m_LobbyOwnerTimeout = CFG->GetUint32("map.hosting.abandoned_lobby.owner_expiry_time", 120);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.game_start.count_down_interval")) {
    m_LobbyCountDownInterval = CFG->GetUint32("map.hosting.game_start.count_down_interval", 500);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.game_start.count_down_ticks")) {
    m_LobbyCountDownStartValue = CFG->GetUint32("map.hosting.game_start.count_down_ticks", 5);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.bot.latency")) {
    m_Latency = CFG->GetUint16("map.bot.latency", 100);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.proxy_reconnect")) {
    m_ProxyReconnect = CFG->GetUint8("map.proxy_reconnect", RECONNECT_ENABLED_GPROXY_BASIC | RECONNECT_ENABLED_GPROXY_EXTENDED);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.ip_filter.flood_handler")) {
    m_UnsafeNameHandler = CFG->GetUint8("map.hosting.ip_filter.flood_handler", ON_IPFLOOD_DENY);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.name_filter.unsafe_handler")) {
    m_UnsafeNameHandler = CFG->GetUint8("map.hosting.name_filter.unsafe_handler", ON_UNSAFE_NAME_DENY);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.auto_start.seconds")) {
    m_AutoStartSeconds = CFG->GetInt64("map.auto_start.seconds", 180);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.auto_start.players")) {
    m_AutoStartPlayers = CFG->GetUint8("map.auto_start.players", 2);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.nicknames.hide_lobby")) {
    m_HideLobbyNames = CFG->GetBool("map.hosting.nicknames.hide_lobby", false);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.nicknames.hide_in_game")) {
    m_HideInGameNames = CFG->GetStringIndex("map.hosting.nicknames.hide_in_game", {"never", "host", "always", "auto"}, HIDE_IGN_AUTO);
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.hosting.log_commands")) {
    m_LogCommands = CFG->GetBool("map.hosting.log_commands", false);
    CFG->FailIfErrorLast();
  }

  if (CFG->Exists("map.options")) {
    MapOptions = CFG->GetUint32("map.options", 0);
    if (MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) MapOptions |= MAPOPT_CUSTOMFORCES;
  } else {
    CFG->SetUint32("map.options", MapOptions);
  }

  m_MapOptions = MapOptions;
  m_MapFlags = CFG->GetUint8("map.flags", MAPFLAG_TEAMSTOGETHER | MAPFLAG_FIXEDTEAMS);
  if (!CFG->Exists("map.flags")) {
    CFG->SetUint8("map.flags", m_MapFlags);
  }

  if (CFG->Exists("map.width")) {
    MapWidth = ExtractNumbers(CFG->GetString("map.width", emptyString), 2);
  } else {
    CFG->SetUint8Vector("map.width", MapWidth);
  }

  if (MapWidth.size() != 2) {
    CFG->SetFailed();
    m_ErrorMessage = "invalid <map.width> detected";
  } else {
    copy_n(MapWidth.begin(), 2, m_MapWidth.begin());
  }

  if (CFG->Exists("map.height")) {
    MapHeight = ExtractNumbers(CFG->GetString("map.height", emptyString), 2);
  } else {
    CFG->SetUint8Vector("map.height", MapHeight);
  }

  if (MapHeight.size() != 2) {
    CFG->SetFailed();
    m_ErrorMessage = "invalid <map.height> detected";
  } else {
    copy_n(MapHeight.begin(), 2, m_MapHeight.begin());
  }

  if (CFG->Exists("map.editor_version")) {
    MapEditorVersion = CFG->GetUint32("map.editor_version", 0);
  } else {
    CFG->SetUint32("map.editor_version", MapEditorVersion);
  }

  m_MapEditorVersion = MapEditorVersion;
  m_MapType = CFG->GetString("map.type", emptyString);
  m_MapMetaDataEnabled = CFG->GetBool("map.meta_data.enabled", m_MapType == "dota" || m_MapType == "evergreen");
  m_MapDefaultHCL = CFG->GetString("map.default_hcl", emptyString);
  if (!CheckIsValidHCL(m_MapDefaultHCL).empty()) {
    Print("[MAP] HCL string [" + m_MapDefaultHCL + "] is not valid.");
    CFG->SetFailed();
  }

  if (CFG->Exists("map.num_disabled")) {
    MapNumDisabled = CFG->GetUint8("map.num_disabled", 0);
  } else {
    CFG->SetUint8("map.num_disabled", MapNumDisabled);
  }

  m_MapNumDisabled = MapNumDisabled;

  if (CFG->Exists("map.num_players")) {
    MapNumPlayers = CFG->GetUint8("map.num_players", 0);
  } else {
    CFG->SetUint8("map.num_players", MapNumPlayers);
  }

  m_MapNumControllers = MapNumPlayers;

  if (CFG->Exists("map.num_teams")) {
    MapNumTeams = CFG->GetUint8("map.num_teams", 0);
  } else {
    CFG->SetUint8("map.num_teams", MapNumTeams);
  }

  m_MapNumTeams = static_cast<uint8_t>(MapNumTeams);

  if (CFG->Exists("map.game_version.min")) {
    MapMinGameVersion = CFG->GetUint8("map.game_version.min", 0);
  } else {
    MapMinGameVersion = 0;
    if (6060 <= MapEditorVersion || Slots.size() > 12 || MapNumPlayers > 12 || MapNumTeams > 12) {
      MapMinGameVersion = 29;
    } else if (6059 <= MapEditorVersion) {
      MapMinGameVersion = 24;
    } else if (6058 <= MapEditorVersion) {
      MapMinGameVersion = 23;
    } else if (6057 <= MapEditorVersion) {
      MapMinGameVersion = 22;
    } else if (6053 <= MapEditorVersion && MapEditorVersion <= 6056) {
      MapMinGameVersion = 22; // not released
    } else if (6050 <= MapEditorVersion && MapEditorVersion <= 6052) {
      MapMinGameVersion = 17 + static_cast<uint8_t>(MapEditorVersion - 6050);
    } else if (6046 <= MapEditorVersion) {
      MapMinGameVersion = 16;
    } else if (6043 <= MapEditorVersion) {
      MapMinGameVersion = 15;
    } else if (6039 <= MapEditorVersion) {
      MapMinGameVersion = 14;
    } else if (6038 <= MapEditorVersion) {
      MapMinGameVersion = 14; // not released
    } else if (6034 <= MapEditorVersion && MapEditorVersion <= 6037) {
      MapMinGameVersion = 10 + static_cast<uint8_t>(MapEditorVersion - 6034);
    } else if (6031 <= MapEditorVersion) {
      MapMinGameVersion = 7;
    }
    CFG->SetUint32("map.game_version.min", MapMinGameVersion);
  }
  m_MapMinGameVersion = MapMinGameVersion;

  if (MapMinGameVersion >= 29) {
    m_MapVersionMaxSlots = static_cast<uint8_t>(MAX_SLOTS_MODERN);
  } else {
    // TODO: Verify. Allegedly, old maps are limited to 12 slots even when hosted in Reforged.
    m_MapVersionMaxSlots = static_cast<uint8_t>(MAX_SLOTS_LEGACY);
  }
  if (m_Aura->m_MaxSlots < m_MapVersionMaxSlots) {
    Print("[MAP] " + ToDecString(m_Aura->m_MaxSlots) + " player limit enforced in modern map (editor version " + to_string(MapEditorVersion) + ")");
    m_MapVersionMaxSlots = m_Aura->m_MaxSlots;
  }

  if (CFG->Exists("map.slot_1")) {
    Slots.clear();

    for (uint32_t Slot = 1; Slot <= m_MapVersionMaxSlots; ++Slot) {
      string SlotString = CFG->GetString("map.slot_" + to_string(Slot), emptyString);

      if (SlotString.empty())
        break;

      std::vector<uint8_t> SlotData = ExtractNumbers(SlotString, 10);
      Slots.emplace_back(SlotData);
    }
  } else if (!Slots.empty()) {
    uint32_t SlotNum = 0;
    for (auto& Slot : Slots) {
      CFG->SetUint8Vector("map.slot_" + to_string(++SlotNum), Slot.GetByteArray());
    }
  }

  m_Slots = Slots;

  // if random races is set force every slot's race to random

  if (!(MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) && (m_MapFlags & MAPFLAG_RANDOMRACES)) {
    Print("[MAP] forcing races to random");

    for (auto& slot : m_Slots)
      slot.SetRace(SLOTRACE_RANDOM);
  }

  m_MapFilterType = MapFilterType;

  // These are per-game flags. Don't automatically set them in the map config.
  m_MapSpeed       = CFG->GetUint8("map.speed", MAPSPEED_FAST);
  m_MapVisibility  = CFG->GetUint8("map.visibility", MAPVIS_DEFAULT);
  m_MapFilterMaker = CFG->GetUint8("map.filter_maker", MAPFILTER_MAKER_USER);
  m_MapFilterSize  = CFG->GetUint8("map.filter_size", MAPFILTER_SIZE_LARGE);

  // Maps supporting observer slots enable them by default.
  if (m_Slots.size() + m_MapNumDisabled < m_MapVersionMaxSlots) {
    m_MapObservers = MAPOBS_ALLOWED;
    m_MapFilterObs = MAPFILTER_OBS_FULL;
  }
  if (CFG->Exists("map.observers")) {
    SetMapObservers(CFG->GetUint8("map.observers", m_MapObservers));
    CFG->FailIfErrorLast();
  }
  if (CFG->Exists("map.filter_obs")) {
    m_MapFilterObs = CFG->GetUint8("map.filter_obs", m_MapFilterObs);
    CFG->FailIfErrorLast();
  }

  if (!CFG->GetSuccess()) {
    m_Valid = false;
    if (m_ErrorMessage.empty()) m_ErrorMessage = "invalid map config file";
    Print("[MAP] " + m_ErrorMessage);
  } else {
    string ErrorMessage = CheckProblems();
    if (!ErrorMessage.empty()) {
      Print("[MAP] " + ErrorMessage);
    } else if (isPartial) {
      CFG->Delete("map.cfg.partial");
    }
  }
}

bool CMap::UnlinkFile()
{
  Print("Deleting " + m_MapServerPath + "...");
  filesystem::path mapLocalPath = m_MapServerPath;
  if (mapLocalPath.is_absolute()) {
    return FileDelete(mapLocalPath);
  }
  filesystem::path resolvedPath =  m_Aura->m_Config->m_MapPath / mapLocalPath;
  return FileDelete(resolvedPath.lexically_normal());
}

string CMap::CheckProblems()
{
  if (!m_Valid) {
    return m_ErrorMessage;
  }

  if (m_ClientMapPath.empty())
  {
    m_Valid = false;
    m_ErrorMessage = "<map.path> not found";
    return m_ErrorMessage;
  }

  if (m_ClientMapPath.length() > 53)
  {
    m_Valid = false;
    m_ErrorMessage = "<map.path> too long";
    return m_ErrorMessage;
  }

  if (m_ClientMapPath.find('/') != string::npos)
    Print(R"(warning - map.path contains forward slashes '/' but it must use Windows style back slashes '\')");

  if (m_MapSize.size() != 4)
  {
    m_Valid = false;
    m_ErrorMessage = "invalid <map.size> detected";
    return m_ErrorMessage;
  }
  else if (!m_MapData.empty() && m_MapData.size() != ByteArrayToUInt32(m_MapSize, false))
  {
    m_Valid = false;
    m_ErrorMessage = "nonmatching <map.size> detected";
    return m_ErrorMessage;
  }

  if (m_MapCRC32.size() != 4)
  {
    m_Valid = false;
    if (m_MapCRC32.empty() && m_MapData.empty()) {
      m_ErrorMessage = "map file not found";
    } else {
      m_ErrorMessage = "invalid <map.crc32> detected";
    }
    return m_ErrorMessage;
  }

  if (m_MapScriptsWeakHash.size() != 4)
  {
    m_Valid = false;
    if (m_MapScriptsWeakHash.empty() && m_MapMPQErrored) {
      m_ErrorMessage = "cannot load map file as MPQ archive";
    } else {
      m_ErrorMessage = "invalid <map.weak_hash> detected";
    }
    return m_ErrorMessage;
  }

  if (m_MapScriptsSHA1.size() != 20)
  {
    m_Valid = false;
    if (m_MapScriptsSHA1.empty() && m_MapMPQErrored) {
      m_ErrorMessage = "cannot load map file as MPQ archive";
    } else {
      m_ErrorMessage = "invalid <map.sha1> detected";
    }
    return m_ErrorMessage;
  }

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
    if (m_MapVersionMaxSlots == MAX_SLOTS_LEGACY) {
      m_ErrorMessage = "map uses too many slots - v1.29+ required";
    } else {
      m_ErrorMessage = "map uses an invalid amount of slots";
    }
    return m_ErrorMessage;
  }

  bitset<MAX_SLOTS_MODERN> usedTeams;
  uint8_t controllerSlotCount = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() > m_MapVersionMaxSlots || slot.GetColor() > m_MapVersionMaxSlots) {
      m_Valid = false;
      if (m_MapVersionMaxSlots == MAX_SLOTS_LEGACY) {
        m_ErrorMessage = "map uses too many players - v1.29+ required";
      } else {
        m_ErrorMessage = "map uses an invalid amount of players";
      }
      return m_ErrorMessage;
    }
    if (slot.GetTeam() == m_MapVersionMaxSlots) {
      continue;
    }
    if (slot.GetTeam() > m_MapNumTeams) {
      // TODO: Or just enforce usedTeams.count() <= m_MapNumTeams?
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

  if (!m_SkipVersionCheck && m_Aura->m_GameVersion < m_MapMinGameVersion) {
    m_Valid = false;
    m_ErrorMessage = "map requires v1." + to_string(m_MapMinGameVersion) + " (using v1." + to_string(m_Aura->m_GameVersion) + ")";
    return m_ErrorMessage;
  }

  if (!m_Valid) {
    return m_ErrorMessage;
  }

  return string();
}

uint32_t CMap::XORRotateLeft(const uint8_t* data, const uint32_t length)
{
  // a big thank you to Strilanc for figuring this out

  uint32_t i   = 0;
  uint32_t Val = 0;

  if (length > 3)
  {
    while (i < length - 3)
    {
      Val = ROTL(Val ^ ((uint32_t)data[i] + (uint32_t)(data[i + 1] << 8) + (uint32_t)(data[i + 2] << 16) + (uint32_t)(data[i + 3] << 24)), 3);
      i += 4;
    }
  }

  while (i < length)
  {
    Val = ROTL(Val ^ data[i], 3);
    ++i;
  }

  return Val;
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
