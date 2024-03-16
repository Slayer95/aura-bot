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
#ifndef DISABLE_CPR
#include <cpr/cpr.h>
#endif

#include "aura.h"
#include "command.h"
#include "map.h"
#include "realm.h"

#define SEARCH_TYPE_ONLY_MAP 1
#define SEARCH_TYPE_ONLY_CONFIG 2
#define SEARCH_TYPE_ONLY_FILE 3
#define SEARCH_TYPE_ANY 7

#define MATCH_TYPE_NONE 0
#define MATCH_TYPE_MAP 1
#define MATCH_TYPE_CONFIG 2
#define MATCH_TYPE_INVALID 4
#define MATCH_TYPE_FORBIDDEN 8

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

class CAura;
class CCommandContext;
class CMap;
class CRealm;

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

#ifndef DISABLE_CPR
inline std::vector<std::string> GetEpicWarSuggestions(std::string & pattern, uint8_t maxCount)
{
  std::vector<std::string> suggestions;
  std::string searchUri = "https://www.epicwar.com/maps/search/?go=1&n=" + EncodeURIComponent(pattern) + "&a=&c=0&p=0&pf=0&roc=0&tft=0&order=desc&sort=downloads&page=1";
  Print("[AURA] Looking up suggestions...");
  Print("[AURA] GET <" + searchUri + ">");
  auto response = cpr::Get(cpr::Url{searchUri});
  if (response.status_code != 200) {
    return suggestions;
  }

  std::vector<std::pair<std::string, int>> matchingMaps = ExtractEpicWarMaps(response.text, maxCount);

  for (const auto& element : matchingMaps) {
    suggestions.push_back(element.first + " (epicwar-" + std::to_string(element.second) + ")");
  }
  return suggestions;
}
#endif

//
// CGameExtraOptions
//

class CGameExtraOptions
{
public:
  CGameExtraOptions();
  CGameExtraOptions(const std::optional<bool>& nRandomRaces, const std::optional<bool>& nRandomHeroes, const std::optional<uint8_t>& nVisibility, const std::optional<uint8_t>& nObservers);
  ~CGameExtraOptions();

  std::optional<bool>         m_RandomRaces;
  std::optional<bool>         m_RandomHeroes;
  std::optional<uint8_t>      m_Visibility;
  std::optional<uint8_t>      m_Observers;

  bool ParseMapObservers(const std::string& s);
  bool ParseMapVisibility(const std::string& s);
  bool ParseMapRandomRaces(const std::string& s);
  bool ParseMapRandomHeroes(const std::string& s);
};

//
// CGameSetup
//

class CGameSetup
{
public:
  CAura*                                          m_Aura;
  CMap*                                           m_Map;
  CCommandContext*                                m_Ctx;

  std::string                                     m_Attribution;
  std::string                                     m_SearchRawTarget;
  uint8_t                                         m_SearchType;
  bool                                            m_StandardPaths;
  bool                                            m_LuckyMode;
  std::pair<std::string, std::string>             m_SearchTarget;

  bool                                            m_IsDownloadable;
  bool                                            m_IsDownloaded;
  std::string                                     m_BaseDownloadFileName;
  std::string                                     m_MapDownloadUri;
  uint32_t                                        m_MapDownloadSize;
  std::string                                     m_MapSiteUri;
  std::filesystem::path                           m_DownloadFilePath;

  std::string                                     m_GameName;
  std::pair<std::string, std::string>             m_GameOwner;
  std::optional<uint32_t>                         m_GameIdentifier;
  std::optional<uint32_t>                         m_GameChannelKey;
  bool                                            m_GameIsMirror;
  uint8_t                                         m_RealmsDisplayMode;
  sockaddr_storage                                m_RealmsAddress;
  std::set<std::string>                           m_RealmsExcluded;

  std::string                                     m_CreatorName;
  CRealm*                                         m_CreatorRealm;

  CGameSetup(CAura* nAura, CCommandContext* nCtx, CConfig* mapCFG);
  CGameSetup(CAura* nAura, CCommandContext* nCtx, const std::string nSearchRawTarget, const uint8_t nSearchType, const bool nUseStandardPaths, const bool nUseLuckyMode);
  ~CGameSetup();

  std::string GetInspectName() const;

  void ParseInput();
  std::pair<uint8_t, std::filesystem::path> SearchInputStandard();
  std::pair<uint8_t, std::filesystem::path> SearchInputLocalExact();
  std::pair<uint8_t, std::filesystem::path> SearchInputLocalTryExtensions();
  std::pair<uint8_t, std::filesystem::path> SearchInputLocalFuzzy(std::vector<std::string>& fuzzyMatches);
#ifndef DISABLE_CPR
  void SearchInputRemoteFuzzy(std::vector<std::string>& fuzzyMatches);
#endif
  std::pair<uint8_t, std::filesystem::path> SearchInputLocal(std::vector<std::string>& fuzzyMatches);
  std::pair<uint8_t, std::filesystem::path> SearchInput();
  CMap* GetBaseMapFromConfig(CConfig* mapCFG, const bool silent);
  CMap* GetBaseMapFromConfigFile(const std::filesystem::path& filePath, const bool silent);
  CMap* GetBaseMapFromMapFile(const std::filesystem::path& filePath, const bool silent);
  CMap* GetBaseMapFromMapFileOrCache(const std::filesystem::path& mapPath, const bool silent);
  bool ApplyMapModifiers(CGameExtraOptions* extraOptions);
  uint8_t ResolveRemoteMap();
  void SetDownloadFilePath(std::filesystem::path&& filePath);
#ifndef DISABLE_CPR
  uint32_t RunDownload();
#endif
  bool LoadMap();
  bool SetActive();
  bool RunHost();

  inline bool GetIsMirror() const { return m_GameIsMirror; }

  bool SetMirrorSource(const sockaddr_storage& nSourceAddress, const uint32_t nGameIdentifier);
  bool SetMirrorSource(const std::string& nInput);
  void AddIgnoredRealm(const CRealm* nRealm);
  void RemoveIgnoredRealm(const CRealm* nRealm);
  void SetDisplayMode(const uint8_t nDisplayMode);
  void SetOwner(const std::string& nOwner, const CRealm* nRealm);
  void SetCreator(const std::string& nCreator, CRealm* nRealm);
  void SetName(const std::string& nName);
  void SetContext(CCommandContext* nCtx);
};

#endif // AURA_GAMESETUP_H_
