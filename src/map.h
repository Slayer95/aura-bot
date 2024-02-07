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

#ifndef AURA_MAP_H_
#define AURA_MAP_H_

#define MAPSPEED_SLOW 1
#define MAPSPEED_NORMAL 2
#define MAPSPEED_FAST 3

#define MAPVIS_HIDETERRAIN 1
#define MAPVIS_EXPLORED 2
#define MAPVIS_ALWAYSVISIBLE 3
#define MAPVIS_DEFAULT 4

#define MAPOBS_NONE 1
#define MAPOBS_ONDEFEAT 2
#define MAPOBS_ALLOWED 3
#define MAPOBS_REFEREES 4

#define MAPFLAG_TEAMSTOGETHER 1
#define MAPFLAG_FIXEDTEAMS 2
#define MAPFLAG_UNITSHARE 4
#define MAPFLAG_RANDOMHERO 8
#define MAPFLAG_RANDOMRACES 16

#define MAPOPT_HIDEMINIMAP 1 << 0
#define MAPOPT_MODIFYALLYPRIORITIES 1 << 1
#define MAPOPT_MELEE 1 << 2 // the bot cares about this one...
#define MAPOPT_REVEALTERRAIN 1 << 4
#define MAPOPT_FIXEDPLAYERSETTINGS 1 << 5 // and this one...
#define MAPOPT_CUSTOMFORCES 1 << 6        // and this one, the rest don't affect the bot's logic
#define MAPOPT_CUSTOMTECHTREE 1 << 7
#define MAPOPT_CUSTOMABILITIES 1 << 8
#define MAPOPT_CUSTOMUPGRADES 1 << 9
#define MAPOPT_WATERWAVESONCLIFFSHORES 1 << 11
#define MAPOPT_WATERWAVESONSLOPESHORES 1 << 12

#define MAPFILTER_MAKER_USER 1
#define MAPFILTER_MAKER_BLIZZARD 2

#define MAPFILTER_TYPE_MELEE 1
#define MAPFILTER_TYPE_SCENARIO 2

#define MAPFILTER_SIZE_SMALL 1
#define MAPFILTER_SIZE_MEDIUM 2
#define MAPFILTER_SIZE_LARGE 4

#define MAPFILTER_OBS_FULL 1
#define MAPFILTER_OBS_ONDEATH 2
#define MAPFILTER_OBS_NONE 4

#define MAPGAMETYPE_UNKNOWN0 1 // always set except for saved games?
#define MAPGAMETYPE_BLIZZARD 1 << 3
#define MAPGAMETYPE_MELEE 1 << 5
#define MAPGAMETYPE_SAVEDGAME 1 << 9
#define MAPGAMETYPE_PRIVATEGAME 1 << 11
#define MAPGAMETYPE_MAKERUSER 1 << 13
#define MAPGAMETYPE_MAKERBLIZZARD 1 << 14
#define MAPGAMETYPE_TYPEMELEE 1 << 15
#define MAPGAMETYPE_TYPESCENARIO 1 << 16
#define MAPGAMETYPE_SIZESMALL 1 << 17
#define MAPGAMETYPE_SIZEMEDIUM 1 << 18
#define MAPGAMETYPE_SIZELARGE 1 << 19
#define MAPGAMETYPE_OBSFULL 1 << 20
#define MAPGAMETYPE_OBSONDEATH 1 << 21
#define MAPGAMETYPE_OBSNONE 1 << 22

//
// CMap
//

#include "includes.h"
#include "fileutil.h"
#include "util.h"

#include <algorithm>
#include <iterator>
#include <cctype>
#include <vector>
#include <cpr/cpr.h>

#pragma once

class CAura;
class CGameSlot;
class CConfig;

class CMap
{
public:
  CAura* m_Aura;

private:
  std::vector<uint8_t>   m_MapSHA1;   // config value: map sha1 (20 bytes)
  std::vector<uint8_t>   m_MapSize;   // config value: map size (4 bytes)
  std::vector<uint8_t>   m_MapInfo;   // config value: map info (4 bytes) -> this is the real CRC
  std::vector<uint8_t>   m_MapCRC;    // config value: map crc (4 bytes) -> this is not the real CRC, it's the "xoro" value
  std::vector<uint8_t>   m_MapWidth;  // config value: map width (2 bytes)
  std::vector<uint8_t>   m_MapHeight; // config value: map height (2 bytes)
  std::vector<CGameSlot> m_Slots;
  std::string            m_CFGFile;
  std::string            m_MapPath;       // config value: map path
  std::string            m_MapType;       // config value: map type (for stats class)
  std::string            m_MapDefaultHCL; // config value: map default HCL to use
  std::string            m_MapLocalPath;  // config value: map local path
  std::string            m_MapURL;
  std::string            m_MapSiteURL;
  std::string            m_MapShortDesc;
  std::string            m_MapData;       // the map data itself, for sending the map to players
  uint32_t               m_MapOptions;
  uint8_t                m_MapNumPlayers; // config value: max map number of players
  uint8_t                m_MapNumTeams;   // config value: max map number of teams
  uint8_t                m_MapSpeed;
  uint8_t                m_MapVisibility;
  uint8_t                m_MapObservers;
  uint8_t                m_MapFlags;
  uint8_t                m_MapFilterMaker;
  uint8_t                m_MapFilterType;
  uint8_t                m_MapFilterSize;
  uint8_t                m_MapFilterObs;
  std::vector<uint8_t>   m_MapContentMismatch;
  bool                   m_Valid;

public:
  CMap(CAura* nAura, CConfig* CFG, const std::string& nCFGFile);
  ~CMap();

  inline bool                   GetValid() const { return m_Valid; }
  inline bool                   GetValidLinkedMap() const { return m_MapContentMismatch[0] == 0 && m_MapContentMismatch[1] == 0 && m_MapContentMismatch[2] == 0 && m_MapContentMismatch[3] == 0; }
  inline std::string            GetCFGFile() const { return m_CFGFile; }
  inline std::string            GetMapPath() const { return m_MapPath; }
  inline std::vector<uint8_t>   GetMapSize() const { return m_MapSize; }
  inline std::vector<uint8_t>   GetMapInfo() const { return m_MapInfo; }
  inline std::vector<uint8_t>   GetMapCRC() const { return m_MapCRC; }
  inline std::vector<uint8_t>   GetMapSHA1() const { return m_MapSHA1; }
  std::string                   GetMapURL() const { return m_MapURL; }
  std::string                   GetMapSiteURL() const { return m_MapSiteURL; }
  std::string                   GetMapShortDesc() const { return m_MapShortDesc; }
  inline uint8_t                GetMapSpeed() const { return m_MapSpeed; }
  inline uint8_t                GetMapVisibility() const { return m_MapVisibility; }
  inline uint8_t                GetMapObservers() const { return m_MapObservers; }
  inline uint8_t                GetMapFlags() { return m_MapFlags; }
  std::vector<uint8_t>          GetMapGameFlags() const;
  uint32_t                      GetMapGameType() const;
  inline uint32_t               GetMapOptions() const { return m_MapOptions; }
  uint8_t                       GetMapLayoutStyle() const;
  inline std::vector<uint8_t>   GetMapWidth() const { return m_MapWidth; }
  inline std::vector<uint8_t>   GetMapHeight() const { return m_MapHeight; }
  inline std::string            GetMapType() const { return m_MapType; }
  inline std::string            GetMapDefaultHCL() const { return m_MapDefaultHCL; }
  inline std::string            GetMapLocalPath() const { return m_MapLocalPath; }
  inline std::string*           GetMapData() { return &m_MapData; }
  inline uint8_t                GetMapNumPlayers() const { return m_MapNumPlayers; }
  inline uint8_t                GetMapNumTeams() const { return m_MapNumTeams; }
  inline std::vector<CGameSlot> GetSlots() const { return m_Slots; }
  void                          AddMapFlags(uint32_t ExtraFlags) { m_MapFlags |= ExtraFlags; }
  void                          ClearMapData() { m_MapData.clear(); }
  inline void                   SetMapSiteURL(const std::string& s) { m_MapSiteURL = s; }

  void Load(CConfig* CFG, const std::string& nCFGFile);
  bool DeleteFile();
  const char* CheckValid();
  uint32_t XORRotateLeft(uint8_t* data, uint32_t length);
  uint8_t GetLobbyRace(const CGameSlot* slot);
};

inline std::vector<std::pair<std::string, int>> ExtractEpicWarMaps(const std::string &s, const int maxCount) {
  std::regex pattern(R"(<a href="/maps/(\d+)/"><b>([^<\n]+)</b></a>)");
  std::vector<std::pair<std::string, int>> Output;

  std::sregex_iterator iter(s.begin(), s.end(), pattern);
  std::sregex_iterator end;

  int count = 0;
  while (iter != end && count < maxCount) {
    std::smatch match = *iter;
    Output.emplace_back(std::make_pair(match[2], std::stoi(match[1])));
    ++iter;
    ++count;
  }

  return Output;
}

inline std::string GetEpicWarSuggestions(std::string & pattern, int maxCount)
{
    std::string SearchUri = "https://www.epicwar.com/maps/search/?go=1&n=" + EncodeURIComponent(pattern) + "&a=&c=0&p=0&pf=0&roc=0&tft=0&order=desc&sort=downloads&page=1";
    Print("[AURA] Downloading <" + SearchUri + ">...");
    std::string Suggestions;
    auto response = cpr::Get(cpr::Url{SearchUri});
    if (response.status_code != 200) {
      return Suggestions;
    }

    std::vector<std::pair<std::string, int>> MatchingMaps = ExtractEpicWarMaps(response.text, maxCount);

    for (const auto& element : MatchingMaps) {
      Suggestions += element.first + " (epicwar-" + std::to_string(element.second) + "), ";
    }

    Suggestions = Suggestions.substr(0, Suggestions.length() - 2);
    return Suggestions;
}

inline int ParseMapObservers(const std::string s, bool & errored) {
  int result = MAPOBS_NONE;
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "no" || lower == "no observers" || lower == "no obs" || lower == "sin obs" || lower == "sin observador" || lower == "sin observadores") {
    result = MAPOBS_NONE;
  } else if (lower == "referee" || lower == "referees" || lower == "arbiter" || lower == "arbitro" || lower == "arbitros" || lower == "árbitros") {
    result = MAPOBS_REFEREES;
  } else if (lower == "observadores derrotados" || lower == "derrotados" || lower == "obs derrotados" || lower == "obs on defeat" || lower == "observers on defeat" || lower == "on defeat" || lower == "defeat") {
    result = MAPOBS_ONDEFEAT;
  } else if (lower == "full observers" || lower == "solo observadores") {
    result = MAPOBS_ALLOWED;
  } else {
    errored = true;
    return result;
  }
  errored = false;
  return result;
}

inline int ParseMapVisibility(const std::string s, bool & errored) {
  int result = MAPVIS_DEFAULT;
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "no" || lower == "default" || lower == "predeterminado" || lower == "fog" || lower == "fog of war" || lower == "niebla" || lower == "niebla de guerra") {
    result = MAPVIS_DEFAULT;
  } else if (lower == "hide terrain" || lower == "hide" || lower == "ocultar terreno" || lower == "ocultar") {
    result = MAPVIS_HIDETERRAIN;
  } else if (lower == "map explored" || lower == "explored" || lower == "mapa explorado" || lower == "explorado") {
    result = MAPVIS_EXPLORED;
  } else if (lower == "always visible" || lower == "always" || lower == "visible" || lower == "todo visible" || lower == "todo" || lower == "revelar" || lower == "todo revelado") {
    result = MAPVIS_ALWAYSVISIBLE;
  } else {
    errored = true;
    return result;
  }
  errored = false;
  return result;
}

inline int ParseMapRandomHero(const std::string s, bool & errored) {
  int result = 0;
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "random hero" || lower == "rh" || lower == "yes" || lower == "heroe aleatorio" || lower == "aleatorio" || lower == "héroe aleatorio") {
    result = MAPFLAG_RANDOMHERO;
  } else if (lower == "default" || lower == "no" || lower == "predeterminado") {
    result = 0;
  } else {
    errored = true;
    return result;
  }
  errored = false;
  return result;
}

inline std::pair<std::string, std::string> ParseMapId(const std::string s, const bool forceExactPath) {
  if (forceExactPath) return make_pair("local", s);

  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  // Custom namespace/protocol
  if (lower.substr(0, 8) == "epicwar-" || lower.substr(0, 8) == "epicwar:") {
    return make_pair("epicwar", MaybeBase10(lower.substr(8, lower.length())));
  }
  if (lower.substr(0, 6) == "local-" || lower.substr(0, 6) == "local:") {
    return make_pair("local", s);
  }
  

  bool isUri = false;
  if (lower.substr(0, 7) == "http://") {
    isUri = true;
    lower = lower.substr(7, lower.length());
  } else if (lower.substr(0, 8) == "https://") {
    isUri = true;
    lower = lower.substr(8, lower.length());
  }

  if (lower.substr(0, 12) == "epicwar.com/") {
    std::string mapCode = lower.substr(17, lower.length());
    return make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
  }
  if (lower.substr(0, 16) == "www.epicwar.com/") {
    std::string mapCode = lower.substr(21, lower.length());
    return make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
  }
  if (isUri) {
    return make_pair("remote", std::string());
  } else {
    return make_pair("local", s);
  }
}

inline uint8_t DownloadRemoteMap(const std::string & siteId, const std::string & siteUri, std::string & downloadUri, std::string & downloadFilename, std::string & searchFilename, std::filesystem::path & mapRootPath, std::unordered_multiset<std::string> & forbiddenFileNames) {
  // Proceed with the download
  Print("[AURA] Downloading <" + siteUri + ">...");
  if (siteId == "epicwar") {
    auto response = cpr::Get(cpr::Url{siteUri});
    if (response.status_code != 200) return 5;
    
    size_t DownloadUriStartIndex = response.text.find("<a href=\"/maps/download/");
    if (DownloadUriStartIndex == std::string::npos) return 5;
    size_t DownloadUriEndIndex = response.text.find("\"", DownloadUriStartIndex + 24);
    if (DownloadUriEndIndex == std::string::npos) return 5;
    downloadUri = "https://epicwar.com" + response.text.substr(DownloadUriStartIndex + 9, (DownloadUriEndIndex) - (DownloadUriStartIndex + 9));
    size_t LastSlashIndex = downloadUri.rfind("/");
    if (LastSlashIndex == std::string::npos) return 5;
    std::string encodedName = downloadUri.substr(LastSlashIndex + 1, downloadUri.length());
    downloadFilename = DecodeURIComponent(encodedName);
  } else {
    return 5;
  }

  if (downloadFilename.empty() || downloadFilename[0] == '.' || downloadFilename[0] == '-' || downloadFilename.length() > 80){
    return 2;
  }

  std::string InvalidChars = "/\\\0\"'*?:|<>;,";
  if (downloadFilename.find_first_of(InvalidChars) != std::string::npos) {
    return 2;
  }

  size_t ExtensionIndex = downloadFilename.find_last_of(".");
  std::string DownloadFileExt;
  if (ExtensionIndex == std::string::npos) {
    return 2;
  } else {
    DownloadFileExt = downloadFilename.substr(ExtensionIndex, downloadFilename.length());
    std::transform(std::begin(DownloadFileExt), std::end(DownloadFileExt), std::begin(DownloadFileExt), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (DownloadFileExt != ".w3m" && DownloadFileExt != ".w3x") {
      return 2;
    }
    std::string downloadFileNameNoExt = downloadFilename.substr(0, ExtensionIndex);
    downloadFilename = downloadFileNameNoExt + DownloadFileExt; // case-sensitive name, but lowercase extension

    // CON, PRN, AUX, NUL, etc. are case-insensitive
    std::transform(std::begin(downloadFileNameNoExt), std::end(downloadFileNameNoExt), std::begin(downloadFileNameNoExt), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (downloadFileNameNoExt == "con" || downloadFileNameNoExt == "prn" || downloadFileNameNoExt == "aux" || downloadFileNameNoExt == "nul") {
      return 2;
    }
    if (downloadFileNameNoExt.length() == 4 && (downloadFileNameNoExt.substr(0, 3) == "com" || downloadFileNameNoExt.substr(0, 3) == "lpt")) {
      return 2;
    }
  }

  searchFilename = downloadFilename;
  // TODO: Further sanitize downloadFilename

  std::string MapSuffix;
  bool FoundAvailableSuffix = false;
  for (int i = 0; i < 10; i++) {
    if (i != 0) {
      MapSuffix = "~" + std::to_string(i);
    }
    std::filesystem::path FileNameFragment = downloadFilename.substr(0, downloadFilename.length() - 4) + MapSuffix + DownloadFileExt;
    if (FileExists(mapRootPath / FileNameFragment)) {
      // Map already exists.
      // I'd rather directly open the file with wx flags to avoid racing conditions,
      // but there is no standard C++ way to do this, and cstdio isn't very helpful.
      continue;
    }
    if (forbiddenFileNames.find(downloadFilename.substr(0, downloadFilename.length() - 4) + MapSuffix + DownloadFileExt) != forbiddenFileNames.end()) {
      // Map already hosted.
      continue;
    }
    FoundAvailableSuffix = true;
    break;
  }
  if (!FoundAvailableSuffix) {
    return 3;
  }
  Print("[AURA] Downloading <" + downloadUri + "> as [" + downloadFilename + "]...");
  downloadFilename = downloadFilename.substr(0, downloadFilename.length() - 4) + MapSuffix + DownloadFileExt;
  std::filesystem::path DestinationPath = mapRootPath / std::filesystem::path(downloadFilename);
  std::ofstream MapFile(DestinationPath.string(), std::ios_base::out | std::ios_base::binary);
  if (!MapFile.is_open()) {
    MapFile.close();
    return 4;
  }
  cpr::Response MapResponse = cpr::Download(MapFile, cpr::Url{downloadUri}, cpr::Header{{"user-agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:109.0) Gecko/20100101 Firefox/115.0"}});
  MapFile.close();
  if (MapResponse.status_code != 200) {
    return 1;
  }

  return 0;
}

#endif // AURA_MAP_H_
