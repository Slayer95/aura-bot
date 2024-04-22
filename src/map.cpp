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
    m_MapMPQLoaded(false),
    m_MapMPQErrored(false),
    m_SkipVersionCheck(skipVersionCheck)
{
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

string CMap::GetMapFileName() const
{
  size_t LastSlash = m_ClientMapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_ClientMapPath;
  }
  return m_ClientMapPath.substr(LastSlash + 1);
}

bool CMap::IsObserverSlot(const CGameSlot* slot) const
{
  if (slot->GetPID() != 0 || slot->GetDownloadStatus() != 255) {
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
  m_MapVisibility = nMapObservers;
  return true;
}

bool CMap::SetMapVisibility(const uint8_t nMapVisibility)
{
  m_MapVisibility = nMapVisibility;
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
  if (m_MapFlags & MAPOPT_FIXEDPLAYERSETTINGS) {
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

  m_MapServerPath = CFG->GetString("map_localpath", emptyString);
  m_MapData.clear();

  bool IsPartial = CFG->GetBool("cfg_partial", false);
  bool ignoreMPQ = m_MapServerPath.empty() || (!IsPartial && m_Aura->m_Config->m_CFGCacheRevalidateAlgorithm == CACHE_REVALIDATION_NEVER);

  size_t RawMapSize = 0;
  if (IsPartial || m_Aura->m_Net->m_Config->m_AllowTransfers != MAP_TRANSFERS_NEVER) {
    if (m_MapServerPath.empty()) {
      return;
    }
    filesystem::path mapServerPath(m_MapServerPath);
    if (mapServerPath.filename() == mapServerPath) {
      m_MapData = FileRead(m_Aura->m_Config->m_MapPath / mapServerPath, &RawMapSize);
    } else {
      m_MapData = FileRead(m_MapServerPath, &RawMapSize);
    }
    if (IsPartial && m_MapData.empty()) {
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

  optional<int64_t> CachedModifiedTime = CFG->GetMaybeInt64("map_localmtime");
  optional<int64_t> FileModifiedTime;
  filesystem::path MapMPQFilePath(m_MapServerPath);

  if (!ignoreMPQ) {
    if (MapMPQFilePath.filename() == MapMPQFilePath) {
      MapMPQFilePath = m_Aura->m_Config->m_MapPath / MapMPQFilePath;
    } else {
      MapMPQFilePath = m_MapServerPath;
    }    
    FileModifiedTime = GetMaybeModifiedTime(MapMPQFilePath);
    ignoreMPQ = (
      !IsPartial && m_Aura->m_Config->m_CFGCacheRevalidateAlgorithm == CACHE_REVALIDATION_MODIFIED && (
        !FileModifiedTime.has_value() || (
          CachedModifiedTime.has_value() && FileModifiedTime.has_value() &&
          FileModifiedTime.value() <= CachedModifiedTime.value()
        )
      )
    );
  }
  if (FileModifiedTime.has_value()) {
    if (!CachedModifiedTime.has_value() || FileModifiedTime.value() != CachedModifiedTime.value()) {
      CFG->SetInt64("map_localmtime", FileModifiedTime.value());
      CFG->SetIsModified();
    }
  }

  uint32_t mapLocale = CFG->GetUint32("map_locale", 0);
  SFileSetLocale(mapLocale);

  void* MapMPQ;
  if (!ignoreMPQ) {
    // load the map MPQ file
    m_MapMPQLoaded = OpenMPQArchive(&MapMPQ, MapMPQFilePath);
    if (!m_MapMPQLoaded) {
      m_MapMPQErrored = true;
#ifdef _WIN32
      uint32_t errorCode = (uint32_t)GetLastError();
      string errorCodeString = (
        errorCode == 2 ? "Map not found" : (
        errorCode == 11 ? "File is corrupted." : (
        (errorCode == 3 || errorCode == 15) ? "Config error: <bot.maps_path> is not a valid directory" : (
        (errorCode == 32 || errorCode == 33) ? "File is currently opened by another process." : (
        "Error code " + to_string(errorCode)
        ))))
      );
#else
      int32_t errorCode = static_cast<int32_t>(GetLastError());
      string errorCodeString = "Error code " + to_string(errorCode);
#endif
      Print("[MAP] warning - unable to load MPQ file [" + PathToString(MapMPQFilePath) + "] - " + errorCodeString);
    }
  }

  // try to calculate map_size, map_info, map_crc, map_sha1

  std::vector<uint8_t> MapSize, MapCRC32, MapHash, MapSHA1;

  if (!m_MapData.empty()) {
    m_Aura->m_SHA->Reset();

    // calculate map_info (this is actually the CRC32)

    MapCRC32 = CreateByteArray(m_Aura->m_CRC->CalculateCRC((uint8_t*)m_MapData.c_str(), m_MapData.size()), false);
    if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
      Print("[MAP] calculated <map_info = " + ByteArrayToDecString(MapCRC32) + ">");
    }

    // calculate map_crc (this is a misnomer) and map_sha1
    // a big thank you to Strilanc for figuring the map_crc algorithm out

    filesystem::path commonPath = m_Aura->m_Config->m_JASSPath / filesystem::path("common-" + to_string(m_Aura->m_GameVersion) +".j");
    string CommonJ = FileRead(commonPath, nullptr);

    if (CommonJ.empty())
      Print("[MAP] unable to calculate map_crc/sha1 - unable to read file [" + PathToString(commonPath) + "]");
    else
    {
      filesystem::path blizzardPath = m_Aura->m_Config->m_JASSPath / filesystem::path("blizzard-" + to_string(m_Aura->m_GameVersion) +".j");
      string BlizzardJ = FileRead(blizzardPath, nullptr);

      if (BlizzardJ.empty())
        Print("[MAP] unable to calculate map_crc/sha1 - unable to read file [" + PathToString(blizzardPath) + "]");
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
                Print("[MAP] overriding default common.j with map copy while calculating map_crc/sha1");
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
                Print("[MAP] overriding default blizzard.j with map copy while calculating map_crc/sha1");
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
            Print(R"([MAP] couldn't find war3map.j or scripts\war3map.j in MPQ file, calculated map_crc/sha1 is probably wrong)");

          MapHash = CreateByteArray(Val, false);
          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
            Print("[MAP] calculated <map_crc = " + ByteArrayToDecString(MapHash) + ">");
          }

          m_Aura->m_SHA->Final();
          uint8_t SHA1[20];
          memset(SHA1, 0, sizeof(uint8_t) * 20);
          m_Aura->m_SHA->GetHash(SHA1);
          MapSHA1 = CreateByteArray(SHA1, 20);
          if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
            Print("[MAP] calculated <map_sha1 = " + ByteArrayToDecString(MapSHA1) + ">");
          }
        }
        else
          Print("[MAP] skipping map_crc/sha1 calculation - map archive not loaded");
      }
    }
  }
  else
    Print("[MAP] no map data available, using config file for <map_size>, <map_info>, <map_crc>, <map_sha1>");

  // try to calculate map_width, map_height, map_slot<x>, map_numplayers, map_numteams, map_filtertype

  std::vector<uint8_t> MapWidth;
  std::vector<uint8_t> MapHeight;
  uint32_t             MapEditorVersion = 0;
  uint32_t             MapOptions    = 0;
  uint8_t              MapNumPlayers = 0;
  uint8_t              MapFilterType = MAPFILTER_TYPE_SCENARIO;
  uint8_t              MapNumTeams   = 0;
  uint8_t              MapMinGameVersion = 0;
  vector<CGameSlot>    Slots;

  if (IsPartial && m_MapMPQLoaded) {
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
            uint8_t ClosedSlots = 0;

            for (uint32_t i = 0; i < RawMapNumPlayers; ++i)
            {
              CGameSlot Slot(0, SLOTPROG_RST, SLOTSTATUS_OPEN, SLOTCOMP_NO, 0, 1, SLOTRACE_RANDOM);
              uint32_t  Color, Status, Race;
              ISS.read(reinterpret_cast<char*>(&Color), 4); // colour
              Slot.SetColor(static_cast<uint8_t>(Color));
              ISS.read(reinterpret_cast<char*>(&Status), 4); // status

              if (Status == 1 || (Status == 2 && !(RawMapFlags & MAPOPT_FIXEDPLAYERSETTINGS))) {
                // WC3 ignores computer slots defined in WorldEdit if Fixed Player Settings is disabled.
                Slot.SetSlotStatus(SLOTSTATUS_OPEN);
              } else if (Status == 2) {
                Slot.SetSlotStatus(SLOTSTATUS_OCCUPIED);
                Slot.SetComputer(SLOTCOMP_YES);
                Slot.SetComputerType(SLOTCOMP_NORMAL);
              } else {
                Slot.SetSlotStatus(SLOTSTATUS_CLOSED);
                ++ClosedSlots;
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
                Print("[MAP] calculated <map_options = " + to_string(MapOptions) + ">");
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
              MapNumPlayers = static_cast<uint8_t>(RawMapNumPlayers) - ClosedSlots;

              if (MapOptions & MAPOPT_MELEE) {
                Print("[MAP] found melee map");
                MapFilterType = MAPFILTER_TYPE_MELEE;
              }

              if (!(MapOptions & MAPOPT_FIXEDPLAYERSETTINGS)) {
                // make races selectable

                for (auto& Slot : Slots)
                  (Slot).SetRace(SLOTRACE_RANDOM | SLOTRACE_SELECTABLE);
              }

              uint32_t SlotNum = 1;

              if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                Print("[MAP] calculated <map_width = " + ByteArrayToDecString(MapWidth) + ">");
                Print("[MAP] calculated <map_height = " + ByteArrayToDecString(MapHeight) + ">");
                Print("[MAP] calculated <map_numplayers = " + to_string(MapNumPlayers) + ">");
                Print("[MAP] calculated <map_numteams = " + to_string(MapNumTeams) + ">");
              }

              for (auto& Slot : Slots) {
                if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
                  Print("[MAP] calculated <map_slot" + to_string(SlotNum) + " = " + ByteArrayToDecString((Slot).GetProtocolArray()) + ">");
                }
                ++SlotNum;
              }
            } else {
              Print("[MAP] unable to calculate <map_slotN>, <map_numplayers>, <map_numteams> - unable to extract war3map.w3i from map file");
              MapNumPlayers = 0;
              MapNumTeams = 0;
            }
          }
        }
        else
          Print("[MAP] unable to calculate <map_options>, <map_width>, <map_height>, <map_slotN>, <map_numplayers>, <map_numteams> - unable to extract war3map.w3i from map file");

        delete[] SubFileData;
      }

      SFileCloseFile(SubFile);
    } else {
      Print("[MAP] unable to calculate <map_options>, <map_width>, <map_height>, <map_slotN>, <map_numplayers>, <map_numteams> - couldn't find war3map.w3i in map file");
    }
  } else {
    if (!IsPartial) {
      //This is debug log
      if (m_Aura->MatchLogLevel(LOG_LEVEL_TRACE)) {
        Print("[MAP] using mapcfg for <map_options>, <map_width>, <map_height>, <map_slotN>, <map_numplayers>, <map_numteams>");
      }
    } else if (!m_MapMPQLoaded) {
      Print("[MAP] unable to calculate <map_options>, <map_width>, <map_height>, <map_slotN>, <map_numplayers>, <map_numteams> - map archive not loaded");
    }
  }

  // close the map MPQ

  if (m_MapMPQLoaded)
    SFileCloseArchive(MapMPQ);

  m_ClientMapPath = CFG->GetString("map_path", emptyString);
  vector<uint8_t> MapContentMismatch = vector<uint8_t>(4, 0);

  if (CFG->Exists("map_size")) {
    string CFGValue = CFG->GetString("map_size", emptyString);
    if (RawMapSize != 0) {
      string MapValue = ByteArrayToDecString(CreateByteArray(static_cast<uint32_t>(RawMapSize), false));
      MapContentMismatch[0] = CFGValue != MapValue;
    }
    MapSize = ExtractNumbers(CFGValue, 4);
  } else if (RawMapSize != 0) {
    MapSize = CreateByteArray(static_cast<uint32_t>(RawMapSize), false);
    CFG->SetUint8Vector("map_size", MapSize);
  }

  m_MapSize = MapSize;

  if (CFG->Exists("map_info")) {
    string CFGValue = CFG->GetString("map_info", emptyString);
    if (!MapCRC32.empty()) {
      string MapValue = ByteArrayToDecString(MapCRC32);
      MapContentMismatch[1] = CFGValue != MapValue;
    }
    MapCRC32 = ExtractNumbers(CFGValue, 4);
  } else if (!MapCRC32.empty()) {
    CFG->SetUint8Vector("map_info", MapCRC32);
  }

  m_MapCRC32 = MapCRC32;

  if (CFG->Exists("map_crc")) {
    string CFGValue = CFG->GetString("map_crc", emptyString);
    if (!MapHash.empty()) {
      string MapValue = ByteArrayToDecString(MapHash);
      MapContentMismatch[2] = CFGValue != MapValue;
    }
    MapHash = ExtractNumbers(CFGValue, 4);
  } else if (!MapHash.empty()) {
    CFG->SetUint8Vector("map_crc", MapHash);
  }

  m_MapHash = MapHash;

  if (CFG->Exists("map_sha1")) {
    string CFGValue = CFG->GetString("map_sha1", emptyString);
    if (!MapSHA1.empty()) {
      string MapValue = ByteArrayToDecString(MapSHA1);
      MapContentMismatch[3] = CFGValue != MapValue;
    }
    MapSHA1 = ExtractNumbers(CFGValue, 20);
  } else if (!MapSHA1.empty()) {
    CFG->SetUint8Vector("map_sha1", MapSHA1);
  }

  m_MapSHA1 = MapSHA1;

  if (!m_MapData.empty()) {
    m_MapContentMismatch = MapContentMismatch;
    if (HasMismatch()) {
      Print("[CACHE] error - map content mismatch");
    }
  }

  m_MapSiteURL   = CFG->GetString("map_site", emptyString);
  m_MapShortDesc = CFG->GetString("map_shortdesc", emptyString);
  m_MapURL       = CFG->GetString("map_url", emptyString);

  if (CFG->Exists("map_filter_type")) {
    MapFilterType = CFG->GetUint8("map_filter_type", MAPFILTER_TYPE_SCENARIO);
  } else {
    CFG->SetUint8("map_filter_type", MapFilterType);
  }

  m_MapFilterType = MapFilterType;

  // These are per-game flags. Don't automatically set them in the map config.
  m_MapSpeed       = CFG->GetUint8("map_speed", MAPSPEED_FAST);
  m_MapVisibility  = CFG->GetUint8("map_visibility", MAPVIS_DEFAULT);
  m_MapObservers   = CFG->GetUint8("map_observers", MAPOBS_ALLOWED);
  m_MapFilterMaker = CFG->GetUint8("map_filter_maker", MAPFILTER_MAKER_USER);
  m_MapFilterSize  = CFG->GetUint8("map_filter_size", MAPFILTER_SIZE_LARGE);
  m_MapFilterObs   = CFG->GetUint8("map_filter_obs", MAPFILTER_OBS_NONE);

  if (CFG->Exists("map_options")) {
    MapOptions = CFG->GetUint32("map_options", 0);
    if (MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) MapOptions |= MAPOPT_CUSTOMFORCES;
  } else {
    CFG->SetUint32("map_options", MapOptions);
  }

  m_MapOptions = MapOptions;
  m_MapFlags = CFG->GetUint8("map_flags", MAPFLAG_TEAMSTOGETHER | MAPFLAG_FIXEDTEAMS);
  if (!CFG->Exists("map_flags")) {
    CFG->SetUint8("map_flags", m_MapFlags);
  }

  if (CFG->Exists("map_width")) {
    MapWidth = ExtractNumbers(CFG->GetString("map_width", emptyString), 2);
  } else {
    CFG->SetUint8Vector("map_width", MapWidth);
  }

  m_MapWidth = MapWidth;

  if (CFG->Exists("map_height")) {
    MapHeight = ExtractNumbers(CFG->GetString("map_height", emptyString), 2);
  } else {
    CFG->SetUint8Vector("map_height", MapHeight);
  }

  if (CFG->Exists("map_editorversion")) {
    MapEditorVersion = CFG->GetUint32("map_editorversion", 0);
  } else {
    CFG->SetUint32("map_editorversion", MapEditorVersion);
  }
  m_MapHeight        = MapHeight;
  m_MapEditorVersion = MapEditorVersion;
  m_MapType       = CFG->GetString("map_type", emptyString);
  m_MapDefaultHCL = CFG->GetString("map_defaulthcl", emptyString);

  if (CFG->Exists("map_numplayers")) {
    MapNumPlayers = CFG->GetUint8("map_numplayers", 0);
  } else {
    CFG->SetUint8("map_numplayers", MapNumPlayers);
  }

  m_MapNumControllers = static_cast<uint8_t>(MapNumPlayers);

  if (CFG->Exists("map_numteams")) {
    MapNumTeams = CFG->GetUint8("map_numteams", 0);
  } else {
    CFG->SetUint8("map_numteams", MapNumTeams);
  }

  m_MapNumTeams = static_cast<uint8_t>(MapNumTeams);

  if (CFG->Exists("map_slot1")) {
    Slots.clear();

    for (uint32_t Slot = 1; Slot <= m_Aura->m_MaxSlots; ++Slot) {
      string SlotString = CFG->GetString("map_slot" + to_string(Slot), emptyString);

      if (SlotString.empty())
        break;

      std::vector<uint8_t> SlotData = ExtractNumbers(SlotString, 9);
      Slots.emplace_back(SlotData);
    }
  } else if (!Slots.empty()) {
    uint32_t SlotNum = 0;
    for (auto& Slot : Slots) {
      CFG->SetUint8Vector("map_slot" + to_string(++SlotNum), Slot.GetProtocolArray());
    }
  }

  m_Slots = Slots;

  if (CFG->Exists("map_gameversion_min")) {
    MapMinGameVersion = CFG->GetUint8("map_gameversion_min", 0);
  } else {
    MapMinGameVersion = 0;
    if (6060 <= MapEditorVersion  || Slots.size() > 12 || MapNumPlayers > 12 || MapNumTeams > 12) {
      MapMinGameVersion = 29;
    } else if (6059 <= MapEditorVersion) {
      MapMinGameVersion = 24;
    } else if (6058 <= MapEditorVersion) {
      MapMinGameVersion = 23;
    } else if (6057 <= MapEditorVersion) {
      MapMinGameVersion = 22;
    } else if (6053 <= MapEditorVersion && MapEditorVersion <= 6056) {
      MapMinGameVersion = 22; // Not released
    } else if (6050 <= MapEditorVersion && MapEditorVersion <= 6052) {
      MapMinGameVersion = 17 + static_cast<uint8_t>(MapEditorVersion - 6050);
    } else if (6046 <= MapEditorVersion) {
      MapMinGameVersion = 16;
    } else if (6043 <= MapEditorVersion) {
      MapMinGameVersion = 15;
    } else if (6039 <= MapEditorVersion) {
      MapMinGameVersion = 14;
    } else if (6038 <= MapEditorVersion) {
      MapMinGameVersion = 14; // Not released
    } else if (6034 <= MapEditorVersion && MapEditorVersion <= 6037) {
      MapMinGameVersion = 10 + static_cast<uint8_t>(MapEditorVersion - 6034);
    } else if (6031 <= MapEditorVersion) {
      MapMinGameVersion = 7;
    }
    CFG->SetUint32("map_gameversion_min", MapMinGameVersion);
  }
  m_MapMinGameVersion = MapMinGameVersion;

  // if random races is set force every slot's race to random

  if (!(MapOptions & MAPOPT_FIXEDPLAYERSETTINGS) && (m_MapFlags & MAPFLAG_RANDOMRACES)) {
    Print("[MAP] forcing races to random");

    for (auto& slot : m_Slots)
      slot.SetRace(SLOTRACE_RANDOM);
  }

  // force melee maps to have observer slots enabled by default

  if (m_MapFilterType & MAPFILTER_TYPE_MELEE && m_MapObservers == MAPOBS_NONE)
    m_MapObservers = MAPOBS_ALLOWED;

  string ErrorMessage = CheckProblems();

  if (!ErrorMessage.empty()) {
    Print("[MAP] " + ErrorMessage);
  } else if (IsPartial) {
    CFG->Delete("cfg_partial");
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
  if (m_ClientMapPath.empty())
  {
    m_Valid = false;
    return "map_path not found";
  }

  if (m_ClientMapPath.length() > 53)
  {
    m_Valid = false;
    return "map_path too long";
  }

  if (m_ClientMapPath.find('/') != string::npos)
    Print(R"(warning - map_path contains forward slashes '/' but it must use Windows style back slashes '\')");

  if (m_MapSize.size() != 4)
  {
    m_Valid = false;
    return "invalid map_size detected";
  }
  else if (!m_MapData.empty() && m_MapData.size() != ByteArrayToUInt32(m_MapSize, false))
  {
    m_Valid = false;
    return "nonmatching map_size detected";
  }

  if (m_MapCRC32.size() != 4)
  {
    m_Valid = false;
    if (m_MapCRC32.empty() && m_MapData.empty()) {
      return "map file not found";
    } else {
      return "invalid map_info detected";
    }
  }

  if (m_MapHash.size() != 4)
  {
    m_Valid = false;
    if (m_MapHash.empty() && m_MapMPQErrored) {
      return "cannot load map file as MPQ archive";
    } else {
      return "invalid map_crc detected";
    }
  }

  if (m_MapSHA1.size() != 20)
  {
    m_Valid = false;
    if (m_MapSHA1.empty() && m_MapMPQErrored) {
      return "cannot load map file as MPQ archive";
    } else {
      return "invalid map_sha1 detected";
    }
  }

  if (m_MapSpeed != MAPSPEED_SLOW && m_MapSpeed != MAPSPEED_NORMAL && m_MapSpeed != MAPSPEED_FAST)
  {
    m_Valid = false;
    return "invalid map_speed detected";
  }

  if (m_MapVisibility != MAPVIS_HIDETERRAIN && m_MapVisibility != MAPVIS_EXPLORED && m_MapVisibility != MAPVIS_ALWAYSVISIBLE && m_MapVisibility != MAPVIS_DEFAULT)
  {
    m_Valid = false;
    return "invalid map_visibility detected";
  }

  if (m_MapObservers != MAPOBS_NONE && m_MapObservers != MAPOBS_ONDEFEAT && m_MapObservers != MAPOBS_ALLOWED && m_MapObservers != MAPOBS_REFEREES && m_MapObservers != MAPOBS_ONANY)
  {
    m_Valid = false;
    return "invalid map_observers detected";
  }

  if (m_MapWidth.size() != 2)
  {
    m_Valid = false;
    return "invalid map_width detected";
  }

  if (m_MapHeight.size() != 2)
  {
    m_Valid = false;
    return "invalid map_height detected";
  }

  if (m_MapNumControllers < 2 || m_MapNumControllers > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    return "invalid map_numplayers detected";
  }

  if (m_MapNumTeams < 2 || m_MapNumTeams > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    return "invalid map_numteams detected";
  }

  if (m_Slots.size() < 2 || m_Slots.size() > MAX_SLOTS_MODERN)
  {
    m_Valid = false;
    return "invalid map_slot<x> detected";
  }

  if (m_MapNumControllers > m_Aura->m_MaxSlots || m_MapNumTeams > m_Aura->m_MaxSlots || m_Slots.size() > m_Aura->m_MaxSlots) {
    m_Valid = false;
    return "map uses more than " + to_string(m_Aura->m_MaxSlots) + " players - v1.29+ required";
  }

  bitset<MAX_SLOTS_MODERN> usedTeams;
  uint8_t controllerSlotCount = 0;
  for (const auto& slot : m_Slots) {
    if (slot.GetTeam() > m_Aura->m_MaxSlots || slot.GetColor() > m_Aura->m_MaxSlots) {
      m_Valid = false;
      return "map uses more than " + to_string(m_Aura->m_MaxSlots) + " players - v1.29+ required";
    }
    if (slot.GetTeam() == m_Aura->m_MaxSlots) {
      continue;
    }
    if (slot.GetTeam() > m_MapNumTeams) {
      // TODO: Or just enforce usedTeams.count() <= m_MapNumTeams?
      m_Valid = false;
      return "invalid map_slot<x> detected";
    }
    usedTeams.set(slot.GetTeam());
    ++controllerSlotCount;
  }
  if (controllerSlotCount != m_MapNumControllers) {
    m_Valid = false;
    return "invalid map_slot<x> detected"; 
  }
  if ((m_MapOptions & MAPOPT_CUSTOMFORCES) && usedTeams.count() <= 1) {
    m_Valid = false;
    return "invalid map_slot<x> detected";
  }

  if (!m_SkipVersionCheck && m_Aura->m_GameVersion < m_MapMinGameVersion) {
    m_Valid = false;
    return "map requires v1." + to_string(m_MapMinGameVersion) + " (using v1." + to_string(m_Aura->m_GameVersion) + ")";
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
