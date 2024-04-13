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

#include "aura.h"
#include "gamesetup.h"
#include "gameprotocol.h"
#include "fileutil.h"
#include "command.h"
#include "hash.h"
#include "map.h"

using namespace std;

// CGameExtraOptions

CGameExtraOptions::CGameExtraOptions()
{
}

CGameExtraOptions::CGameExtraOptions(const optional<bool>& nRandomRaces, const optional<bool>& nRandomHeroes, const optional<uint8_t>& nVisibility, const optional<uint8_t>& nObservers)
  : m_RandomRaces(nRandomRaces),
    m_RandomHeroes(nRandomHeroes),
    m_Visibility(nVisibility),
    m_Observers(nObservers)
{
}

bool CGameExtraOptions::ParseMapObservers(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "no" || lower == "no observers" || lower == "no obs" || lower == "sin obs" || lower == "sin observador" || lower == "sin observadores") {
    m_Observers = MAPOBS_NONE;
  } else if (lower == "referee" || lower == "referees" || lower == "arbiter" || lower == "arbitro" || lower == "arbitros" || lower == "árbitros") {
    m_Observers = MAPOBS_REFEREES;
  } else if (lower == "observadores derrotados" || lower == "derrotados" || lower == "obs derrotados" || lower == "obs on defeat" || lower == "observers on defeat" || lower == "on defeat" || lower == "defeat") {
    m_Observers = MAPOBS_ONDEFEAT;
  } else if (lower == "full observers" || lower == "solo observadores") {
    m_Observers = MAPOBS_ALLOWED;
  } else {
    return false;
  }
  return true;
}

bool CGameExtraOptions::ParseMapVisibility(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "no" || lower == "default" || lower == "predeterminado" || lower == "fog" || lower == "fog of war" || lower == "niebla" || lower == "niebla de guerra") {
    m_Visibility = MAPVIS_DEFAULT;
  } else if (lower == "hide terrain" || lower == "hide" || lower == "ocultar terreno" || lower == "ocultar") {
    m_Visibility = MAPVIS_HIDETERRAIN;
  } else if (lower == "explored map" || lower == "map explored" || lower == "explored" || lower == "mapa explorado" || lower == "explorado") {
    m_Visibility = MAPVIS_EXPLORED;
  } else if (lower == "always visible" || lower == "always" || lower == "visible" || lower == "todo visible" || lower == "todo" || lower == "revelar" || lower == "todo revelado") {
    m_Visibility = MAPVIS_ALWAYSVISIBLE;
  } else {
    return false;
  }
  return true;
}

bool CGameExtraOptions::ParseMapRandomRaces(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "random race" || lower == "rr" || lower == "yes" || lower == "random" || lower == "random races") {
    m_RandomRaces = true;
  } else if (lower == "default" || lower == "no" || lower == "predeterminado") {
    m_RandomRaces = false;
  } else {
    return false;
  }
  return true;
}

bool CGameExtraOptions::ParseMapRandomHeroes(const string& s) {
  std::string lower = s;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (lower == "random hero" || lower == "rh" || lower == "yes" || lower == "random" || lower == "random heroes") {
    m_RandomHeroes = true;
  } else if (lower == "default" || lower == "no" || lower == "predeterminado") {
    m_RandomHeroes = false;
  } else {
    return false;
  }
  return true;
}

CGameExtraOptions::~CGameExtraOptions() = default;

//
// CGameSetup
//

CGameSetup::CGameSetup(CAura* nAura, CCommandContext* nCtx, CConfig* nMapCFG)
  : m_Aura(nAura),
    m_Map(nullptr),
    m_Ctx(nCtx),

    m_Attribution(nCtx->GetUserAttribution()),
    m_SearchRawTarget(string()),
    m_SearchType(SEARCH_TYPE_ANY),

    m_AllowPaths(false),
    m_StandardPaths(false),
    m_LuckyMode(false),

    m_IsDownloadable(false),
    m_IsStepDownloading(false),
    m_IsStepDownloaded(false),
    m_MapDownloadSize(0),
    m_DownloadFileStream(nullptr),
    m_DownloadTimeout(m_Aura->m_Net->m_Config->m_DownloadTimeout),
    m_AsyncStep(GAMESETUP_STEP_MAIN),

    m_SkipVersionCheck(false),
    m_IsMapDownloaded(false),

    m_GameIsMirror(false),    
    m_RealmsDisplayMode(GAME_PUBLIC),
    m_CreatedFrom(nullptr),
    m_CreatedFromType(GAMESETUP_ORIGIN_NONE),

    m_MapExtraOptions(nullptr),
    m_MapReadyCallbackAction(MAP_ONREADY_SET_ACTIVE),

    m_DeleteMe(false)
{
  memset(&m_RealmsAddress, 0, sizeof(m_RealmsAddress));
  m_Aura->HoldContext(nCtx);
  m_Map = GetBaseMapFromConfig(nMapCFG, false);
}

CGameSetup::CGameSetup(CAura* nAura, CCommandContext* nCtx, const string nSearchRawTarget, const uint8_t nSearchType, const bool nAllowPaths, const bool nUseStandardPaths, const bool nUseLuckyMode, const bool nSkipVersionCheck)
  : m_Aura(nAura),
    m_Map(nullptr),
    m_Ctx(nCtx),

    m_Attribution(nCtx->GetUserAttribution()),
    m_SearchRawTarget(move(nSearchRawTarget)),
    m_SearchType(nSearchType),

    m_AllowPaths(nAllowPaths),
    m_StandardPaths(nUseStandardPaths),
    m_LuckyMode(nUseLuckyMode),

    m_IsDownloadable(false),
    m_IsStepDownloading(false),
    m_IsStepDownloaded(false),
    m_MapDownloadSize(0),
    m_DownloadFileStream(nullptr),
    m_DownloadTimeout(m_Aura->m_Net->m_Config->m_DownloadTimeout),
    m_AsyncStep(GAMESETUP_STEP_MAIN),

    m_SkipVersionCheck(nSkipVersionCheck),
    m_IsMapDownloaded(false),

    m_GameIsMirror(false),    
    m_RealmsDisplayMode(GAME_PUBLIC),
    m_CreatedFrom(nullptr),
    m_CreatedFromType(GAMESETUP_ORIGIN_NONE),

    m_MapExtraOptions(nullptr),
    m_MapReadyCallbackAction(MAP_ONREADY_SET_ACTIVE),

    m_DeleteMe(false)
    
{
  memset(&m_RealmsAddress, 0, sizeof(m_RealmsAddress));
  m_Aura->HoldContext(nCtx);
}

std::string CGameSetup::GetInspectName() const
{
  return m_Map->GetConfigName();
}

void CGameSetup::ParseInputLocal()
{
  m_SearchTarget = make_pair("local", m_SearchRawTarget);
}

void CGameSetup::ParseInput()
{
  if (m_StandardPaths) {
    ParseInputLocal();
    return;
  }
  const filesystem::path searchPath = filesystem::path(m_SearchRawTarget);
  if (m_SearchType != SEARCH_TYPE_ANY) {
    ParseInputLocal();
    return;
  }

  std::string lower = m_SearchRawTarget;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  if (lower.length() >= 6 && (lower.substr(0, 6) == "local-" || lower.substr(0, 6) == "local:")) {
    ParseInputLocal();
    return;
  }

  // Custom namespace/protocol
  if (lower.length() >= 8 && (lower.substr(0, 8) == "epicwar-" || lower.substr(0, 8) == "epicwar:")) {
    m_SearchTarget = make_pair("epicwar", MaybeBase10(lower.substr(8)));
    m_IsDownloadable = true;
    return;
  }

  if (lower.length() >= 8 && (lower.substr(0, 8) == "wc3maps-" || lower.substr(0, 8) == "wc3maps:")) {
    m_SearchTarget = make_pair("wc3maps", MaybeBase10(lower.substr(8)));
    m_IsDownloadable = true;
    return;
  }
 

  bool isUri = false;
  if (lower.length() >= 7 && lower.substr(0, 7) == "http://") {
    isUri = true;
    lower = lower.substr(7);
  } else if (lower.length() >= 8 && lower.substr(0, 8) == "https://") {
    isUri = true;
    lower = lower.substr(8);
  }

  if (lower.length() >= 17 && lower.substr(0, 12) == "epicwar.com/") {
    string mapCode = lower.substr(17);
    m_SearchTarget = make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (lower.length() >= 21 && lower.substr(0, 16) == "www.epicwar.com/") {
    string mapCode = lower.substr(21);
    m_SearchTarget = make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (lower.length() >= 16 && lower.substr(0, 12) == "wc3maps.com/") {
    string::size_type mapCodeEnd = lower.find_first_of('/', 16);
    string mapCode = mapCodeEnd == string::npos ? lower.substr(16) : lower.substr(16, mapCodeEnd - 16);
    m_SearchTarget = make_pair("wc3maps", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (lower.length() >= 20 && lower.substr(0, 16) == "www.wc3maps.com/") {
    string::size_type mapCodeEnd = lower.find_first_of('/', 20);
    string mapCode = mapCodeEnd == string::npos ? lower.substr(20) : lower.substr(20, mapCodeEnd - 20);
    m_SearchTarget = make_pair("wc3maps", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (isUri) {
    m_SearchTarget = make_pair("remote", string());
  } else {
    ParseInputLocal();
  }
}


pair<uint8_t, filesystem::path> CGameSetup::SearchInputStandard()
{
  // Error handling: SearchInputStandard() reports the full file path
  // This can be absolute. That's not a problem.
  filesystem::path targetPath = m_SearchTarget.second;
  if (!FileExists(targetPath)) {
    Print("[CLI] File not found: " + PathToString(targetPath));
    if (!targetPath.is_absolute()) {
      try {
        Print("[CLI] (File resolved to: " + PathToString(filesystem::absolute(targetPath)) + ")");
      } catch (...) {}
    }
    return make_pair(MATCH_TYPE_NONE, targetPath);
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP) {
    return make_pair(MATCH_TYPE_MAP, targetPath);
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG) {
    return make_pair(MATCH_TYPE_CONFIG, targetPath);
  }
  if (m_SearchTarget.second.length() < 5) {
    return make_pair(MATCH_TYPE_INVALID, targetPath);
  }

  string targetExt = ParseFileExtension(PathToString(targetPath.filename()));
  if (targetExt == ".w3m" || targetExt == ".w3x") {
    return make_pair(MATCH_TYPE_MAP, targetPath);
  }
  if (targetExt == ".cfg") {
    return make_pair(MATCH_TYPE_CONFIG, targetPath);
  }
  return make_pair(MATCH_TYPE_INVALID, targetPath);
}

pair<uint8_t, filesystem::path> CGameSetup::SearchInputLocalExact()
{
  string fileExtension = ParseFileExtension(m_SearchTarget.second);
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config->m_MapPath / filesystem::path(m_SearchTarget.second)).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config->m_MapPath.parent_path()) {
      return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if ((fileExtension == ".w3m" || fileExtension == ".w3x") && FileExists(testPath)) {
      return make_pair(MATCH_TYPE_MAP, testPath);
    }
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config->m_MapCFGPath / filesystem::path(m_SearchTarget.second)).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config->m_MapCFGPath.parent_path()) {
      return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if ((fileExtension == ".cfg") && FileExists(testPath)) {
      return make_pair(MATCH_TYPE_CONFIG, testPath);
    }
  }
  return make_pair(MATCH_TYPE_NONE, filesystem::path());
}

pair<uint8_t, filesystem::path> CGameSetup::SearchInputLocalTryExtensions()
{
  string fileExtension = ParseFileExtension(m_SearchTarget.second);
  bool hasExtension = fileExtension == ".w3x" || fileExtension == ".w3m" || fileExtension == ".cfg";
  if (hasExtension) {
    return make_pair(MATCH_TYPE_NONE, filesystem::path());
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config->m_MapPath / filesystem::path(m_SearchTarget.second + ".w3x")).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config->m_MapPath.parent_path()) {
      return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if (FileExists(testPath)) {
      return make_pair(MATCH_TYPE_MAP, testPath);
    }
    testPath = (m_Aura->m_Config->m_MapPath / filesystem::path(m_SearchTarget.second + ".w3m")).lexically_normal();
    if (FileExists(testPath)) {
      return make_pair(MATCH_TYPE_MAP, testPath);
    }
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config->m_MapCFGPath / filesystem::path(m_SearchTarget.second + ".cfg")).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config->m_MapCFGPath.parent_path()) {
      return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if (FileExists(testPath)) {
      return make_pair(MATCH_TYPE_CONFIG, testPath);
    }
  }
  return make_pair(MATCH_TYPE_NONE, filesystem::path());
}

pair<uint8_t, filesystem::path> CGameSetup::SearchInputLocalFuzzy(vector<string>& fuzzyMatches)
{
  vector<pair<string, int>> allResults;
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    vector<pair<string, int>> mapResults = FuzzySearchFiles(m_Aura->m_Config->m_MapPath, FILE_EXTENSIONS_MAP, m_SearchTarget.second);
    for (const auto& result : mapResults) {
      allResults.push_back(make_pair(result.first, result.second | 0x80));
    }
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    vector<pair<string, int>> cfgResults = FuzzySearchFiles(m_Aura->m_Config->m_MapCFGPath, FILE_EXTENSIONS_CONFIG, m_SearchTarget.second);
    allResults.insert(allResults.end(), cfgResults.begin(), cfgResults.end());
  }
  if (allResults.empty()) {
    return make_pair(MATCH_TYPE_NONE, filesystem::path());
  }

  size_t resultCount = min(FUZZY_SEARCH_MAX_RESULTS, static_cast<int>(allResults.size()));
  partial_sort(
    allResults.begin(),
    allResults.begin() + resultCount,
    allResults.end(),
    [](const pair<string, int>& a, const pair<string, int>& b) {
        return (a.second &~ 0x80) < (b.second &~ 0x80);
    }
  );

  if (m_LuckyMode || allResults.size() == 1) {
    if ((allResults[0].second & 0x80) == 0) {
      return make_pair(MATCH_TYPE_CONFIG, m_Aura->m_Config->m_MapCFGPath / filesystem::path(allResults[0].first));
    } else {
      return make_pair(MATCH_TYPE_MAP, m_Aura->m_Config->m_MapPath / filesystem::path(allResults[0].first));
    }
  }

  // Return suggestions through passed argument.
  for (uint8_t i = 0; i < resultCount; i++) {
    fuzzyMatches.push_back(allResults[i].first);
  }

  return make_pair(MATCH_TYPE_NONE, filesystem::path());
}

#ifndef DISABLE_CPR
void CGameSetup::SearchInputRemoteFuzzy(vector<string>& fuzzyMatches) {
  if (fuzzyMatches.size() >= 5) return;
  vector<string> remoteSuggestions = GetMapRepositorySuggestions(m_SearchTarget.second, 5 - static_cast<uint8_t>(fuzzyMatches.size()));
  // Return suggestions through passed argument.
  fuzzyMatches.insert(end(fuzzyMatches), begin(remoteSuggestions), end(remoteSuggestions));
}
#endif

pair<uint8_t, filesystem::path> CGameSetup::SearchInputLocal(vector<string>& fuzzyMatches)
{
  // 1. Try exact match
  pair<uint8_t, filesystem::path> exactResult = SearchInputLocalExact();
  if (exactResult.first == MATCH_TYPE_MAP || exactResult.first == MATCH_TYPE_CONFIG) {
    return exactResult;
  }
  // 2. If no extension, try adding extensions: w3x, w3m, cfg
  pair<uint8_t, filesystem::path> plusExtensionResult = SearchInputLocalTryExtensions();
  if (plusExtensionResult.first == MATCH_TYPE_MAP || plusExtensionResult.first == MATCH_TYPE_CONFIG) {
    return plusExtensionResult;
  }
  // 3. Fuzzy search
  if (!m_Aura->m_Config->m_StrictSearch) {
    pair<uint8_t, filesystem::path> fuzzyResult = SearchInputLocalFuzzy(fuzzyMatches);
    if (fuzzyResult.first == MATCH_TYPE_MAP || fuzzyResult.first == MATCH_TYPE_CONFIG) {
      return fuzzyResult;
    }
  }
  return make_pair(MATCH_TYPE_NONE, filesystem::path());
}

pair<uint8_t, filesystem::path> CGameSetup::SearchInput()
{
  if (m_StandardPaths) {
    return SearchInputStandard();
  }

  if (m_SearchTarget.first == "remote") {
    // "remote" means unsupported URL.
    // Supported URLs specify the domain in m_SearchTarget.first, such as "epicwar".
    return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());
  }

  if (m_SearchTarget.first == "local") {
    filesystem::path testSearchPath = filesystem::path(m_SearchTarget.second);
    if (testSearchPath != testSearchPath.filename()) {
      if (m_AllowPaths) {
        // Search target has slashes. Treat as standard path.
        return SearchInputStandard();
      } else {
        // Search target has slashes. Protect against arbitrary directory traversal.
        return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());        
      }
    }

    // Find config or map path
    vector<string> fuzzyMatches;
    pair<uint8_t, filesystem::path> result = SearchInputLocal(fuzzyMatches);
    if (result.first == MATCH_TYPE_MAP || result.first == MATCH_TYPE_CONFIG) {
      return result;
    }

    if (m_Aura->m_Config->m_MapSearchShowSuggestions) {
      if (m_Aura->m_Games.empty()) {
        // Synchronous download, only if there are no ongoing games.
#ifndef DISABLE_CPR
        SearchInputRemoteFuzzy(fuzzyMatches);
#endif
      }
      if (!fuzzyMatches.empty()) {
        m_Ctx->ErrorReply("Suggestions: " + JoinVector(fuzzyMatches, false), CHAT_SEND_SOURCE_ALL);
      }
    }

    return make_pair(MATCH_TYPE_NONE, filesystem::path());
  }

  // Input corresponds to a namespace, such as epicwar.
  // Gotta find a matching config file.
  string resolvedCFGName = m_SearchTarget.first + "-" + m_SearchTarget.second + ".cfg";
  filesystem::path resolvedCFGPath = (m_Aura->m_Config->m_MapCFGPath / filesystem::path(resolvedCFGName)).lexically_normal();
  if (PathHasNullBytes(resolvedCFGPath) || resolvedCFGPath.parent_path() != m_Aura->m_Config->m_MapCFGPath.parent_path()) {
    return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());
  }
  if (FileExists(resolvedCFGPath)) {
    return make_pair(MATCH_TYPE_CONFIG, resolvedCFGPath);
  }
  return make_pair(MATCH_TYPE_NONE, resolvedCFGPath);
}

CMap* CGameSetup::GetBaseMapFromConfig(CConfig* mapCFG, const bool silent)
{
  CMap* map = new CMap(m_Aura, mapCFG, m_SkipVersionCheck);
  if (!map) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map config", CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }
  string errorMessage = map->CheckProblems();
  if (!errorMessage.empty()) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map config: " + errorMessage, CHAT_SEND_SOURCE_ALL);
    delete map;
    return nullptr;
  }
  return map;
}

CMap* CGameSetup::GetBaseMapFromConfigFile(const filesystem::path& filePath, const bool isCache, const bool silent)
{
  CConfig MapCFG;
  if (!MapCFG.Read(filePath)) {
    if (!silent) m_Ctx->ErrorReply("Map config file [" + PathToString(filePath.filename()) + "] not found.", CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }
  CMap* map = GetBaseMapFromConfig(&MapCFG, silent);
  if (isCache) {
    if (map->HasMismatch()) return nullptr;
    if (MapCFG.GetIsModified()) {
      vector<uint8_t> bytes = MapCFG.Export();
      FileWrite(filePath, bytes.data(), bytes.size());
      Print("[AURA] Updated map cache for [" + PathToString(filePath.filename()) + "] as [" + PathToString(filePath) + "]");
    }
  }
  return map;
}

CMap* CGameSetup::GetBaseMapFromMapFile(const filesystem::path& filePath, const bool silent)
{
  bool isInMapsFolder = filePath.parent_path() == m_Aura->m_Config->m_MapPath.parent_path();
  string fileName = PathToString(filePath.filename());
  if (fileName.empty()) return nullptr;
  string baseFileName;
  if (fileName.length() > 6 && fileName[fileName.length() - 6] == '~' && isdigit(fileName[fileName.length() - 5])) {
    baseFileName = fileName.substr(0, fileName.length() - 6) + fileName.substr(fileName.length() - 4);
  } else {
    baseFileName = fileName;
  }

  CConfig MapCFG;
  MapCFG.SetBool("cfg_partial", true);
  MapCFG.Set("map_path", R"(Maps\Download\)" + baseFileName);
  string localPath = isInMapsFolder ? fileName : PathToString(filePath);
  MapCFG.Set("map_localpath", localPath);

  if (m_IsMapDownloaded) {
    MapCFG.Set("map_site", m_MapSiteUri);
    MapCFG.Set("map_url", m_MapDownloadUri);
    MapCFG.Set("downloaded_by", m_Attribution);
  }

  if (fileName.find("DotA") != string::npos) {
    MapCFG.Set("map_type", "dota");
  }

  CMap* baseMap = new CMap(m_Aura, &MapCFG, m_SkipVersionCheck);
  if (!baseMap) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map.", CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }
  string errorMessage = baseMap->CheckProblems();
  if (!errorMessage.empty()) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map: " + errorMessage, CHAT_SEND_SOURCE_ALL);
    delete baseMap;
    return nullptr;
  }

  if (m_Aura->m_Config->m_EnableCFGCache && isInMapsFolder) {
    string resolvedCFGName;
    if (m_SearchTarget.first == "local") {
      resolvedCFGName = m_SearchTarget.first + "-" + fileName + ".cfg";
    } else {
      resolvedCFGName = m_SearchTarget.first + "-" + m_SearchTarget.second + ".cfg";
    }
    filesystem::path resolvedCFGPath = (m_Aura->m_Config->m_MapCachePath / filesystem::path(resolvedCFGName)).lexically_normal();

    vector<uint8_t> bytes = MapCFG.Export();
    FileWrite(resolvedCFGPath, bytes.data(), bytes.size());
    m_Aura->m_CachedMaps[fileName] = resolvedCFGName;
    Print("[AURA] Cached map config for [" + fileName + "] as [" + PathToString(resolvedCFGPath) + "]");
  }

  if (!silent) m_Ctx->SendReply("Map file loaded OK [" + fileName + "]", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
  return baseMap;
}

CMap* CGameSetup::GetBaseMapFromMapFileOrCache(const filesystem::path& mapPath, const bool silent)
{
  string fileName = PathToString(mapPath.filename());
  if (fileName.empty()) return nullptr;
  if (m_Aura->m_Config->m_EnableCFGCache) {
    bool cacheSuccess = false;
    if (m_Aura->m_CachedMaps.find(fileName) != m_Aura->m_CachedMaps.end()) {
      string cfgName = m_Aura->m_CachedMaps[fileName];
      filesystem::path cfgPath = m_Aura->m_Config->m_MapCachePath / filesystem::path(cfgName);
      CMap* cachedResult = GetBaseMapFromConfigFile(cfgPath, true, true);
      if (cachedResult && cachedResult->GetMapLocalPath() == fileName) {
        cacheSuccess = true;
      }
      if (cacheSuccess) {
        if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
          Print("[AURA] Cache success");
        }
        if (!silent) m_Ctx->SendReply("Map file loaded OK [" + fileName + "]", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
        return cachedResult;
      } else {
        delete cachedResult;
        m_Aura->m_CachedMaps.erase(fileName);
      }
    }
    if (m_Aura->MatchLogLevel(LOG_LEVEL_DEBUG)) {
      Print("[AURA] Cache miss");
    }
  }
  return GetBaseMapFromMapFile(mapPath, silent);
}

bool CGameSetup::ApplyMapModifiers(CGameExtraOptions* extraOptions)
{
  bool failed = false;
  if (extraOptions->m_RandomRaces.has_value()) {
    if (!m_Map->SetRandomRaces(extraOptions->m_RandomRaces.value()))
      failed = true;
  }
  if (extraOptions->m_RandomHeroes.has_value()) {
    if (!m_Map->SetRandomHeroes(extraOptions->m_RandomHeroes.value()))
      failed = true;
  }
  if (extraOptions->m_Visibility.has_value()) {
    if (!m_Map->SetMapVisibility(extraOptions->m_Visibility.value()))
      failed = true;
  }
  if (extraOptions->m_Observers.has_value()) {
    if (!m_Map->SetMapObservers(extraOptions->m_Observers.value()))
      failed = true;
  }
  return !failed;
}

#ifndef DISABLE_CPR
uint32_t CGameSetup::ResolveMapRepositoryTask()
{
  if (m_Aura->m_Config->m_MapRepositories.find(m_SearchTarget.first) == m_Aura->m_Config->m_MapRepositories.end()) {
    m_ErrorMessage = "Downloads from  " + m_SearchTarget.first + " are disabled.";
    return RESOLUTION_ERR;
  }

  string downloadUri, downloadFileName;
  uint64_t SearchTargetType = HashCode(m_SearchTarget.first);

  switch (SearchTargetType) {
    case HashCode("epicwar"): {
      m_MapSiteUri = "https://www.epicwar.com/maps/" + m_SearchTarget.second;
      Print("[NET] GET <" + m_MapSiteUri + ">");
      auto response = cpr::Get(cpr::Url{m_MapSiteUri}, cpr::Timeout{m_DownloadTimeout});
      if (response.status_code == 0) {
        m_ErrorMessage = "Failed to access " + m_SearchTarget.first + " repository (connectivity error).";
        return RESOLUTION_ERR;
      }
      if (response.status_code != 200) {
        m_ErrorMessage = "Failed to access repository (status code " + to_string(response.status_code) + ").";
        return RESOLUTION_ERR;
      }
      Print("[AURA] resolved " + m_SearchTarget.first + " entry in " + to_string(static_cast<float>(response.elapsed * 1000)) + " ms");
      
      size_t downloadUriStartIndex = response.text.find("<a href=\"/maps/download/");
      if (downloadUriStartIndex == string::npos) return RESOLUTION_ERR;
      size_t downloadUriEndIndex = response.text.find("\"", downloadUriStartIndex + 24);
      if (downloadUriEndIndex == string::npos) {
        m_ErrorMessage = "Malformed API response";
        return RESOLUTION_ERR;
      }
      downloadUri = "https://epicwar.com" + response.text.substr(downloadUriStartIndex + 9, (downloadUriEndIndex) - (downloadUriStartIndex + 9));
      size_t lastSlashIndex = downloadUri.rfind("/");
      if (lastSlashIndex == string::npos) {
        m_ErrorMessage = "Malformed download URI";
        return RESOLUTION_ERR;
      }
      string encodedName = downloadUri.substr(lastSlashIndex + 1);
      downloadFileName = DecodeURIComponent(encodedName);
      break;
    }

    case HashCode("wc3maps"): {
      m_MapSiteUri = "https://www.wc3maps.com/api/download/" + m_SearchTarget.second;
      Print("[NET] GET <" + m_MapSiteUri + ">");
      auto response = cpr::Get(cpr::Url{m_MapSiteUri}, cpr::Timeout{m_DownloadTimeout}, cpr::Redirect{0, false, false, cpr::PostRedirectFlags::POST_ALL});
      if (response.status_code == 0) {
        m_ErrorMessage = "Remote host unavailable (status code " + to_string(response.status_code) + ").";
        return RESOLUTION_ERR;
      }
      if (response.status_code < 300 || 399 < response.status_code) {
        m_ErrorMessage = "Failed to access repository (status code " + to_string(response.status_code) + ").";
        return RESOLUTION_ERR;
      }
      Print("[AURA] Resolved " + m_SearchTarget.first + " entry in " + to_string(static_cast<float>(response.elapsed * 1000)) + " ms");
      downloadUri = response.header["location"];
      size_t lastSlashIndex = downloadUri.rfind("/");
      if (lastSlashIndex == string::npos) {
        m_ErrorMessage = "Malformed download URI.";
        return RESOLUTION_ERR;
      }
      downloadFileName = downloadUri.substr(lastSlashIndex + 1);
      downloadUri = downloadUri.substr(0, lastSlashIndex + 1) + EncodeURIComponent(downloadFileName);
      break;
    }

    default: {
      m_ErrorMessage = "Unsupported remote domain: " + m_SearchTarget.first;
      return RESOLUTION_ERR;
    }
  }

  if (downloadFileName.empty() || downloadFileName[0] == '.' || downloadFileName[0] == '-' || downloadFileName.length() > 80) {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }

  std::string InvalidChars = "/\\\0\"*?:|<>;,";
  if (downloadFileName.find_first_of(InvalidChars) != string::npos) {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }

  string fileExtension = ParseFileExtension(downloadFileName);
  if (fileExtension != ".w3m" && fileExtension != ".w3x") {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }

  string downloadFileNameNoExt = downloadFileName.substr(0, downloadFileName.length() - fileExtension.length());
  downloadFileName = downloadFileNameNoExt + fileExtension; // case-sensitive name, but lowercase extension

  // CON, PRN, AUX, NUL, etc. are case-insensitive
  transform(begin(downloadFileNameNoExt), end(downloadFileNameNoExt), begin(downloadFileNameNoExt), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (downloadFileNameNoExt == "con" || downloadFileNameNoExt == "prn" || downloadFileNameNoExt == "aux" || downloadFileNameNoExt == "nul") {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }
  if (downloadFileNameNoExt.length() == 4 && (downloadFileNameNoExt.substr(0, 3) == "com" || downloadFileNameNoExt.substr(0, 3) == "lpt")) {
    m_ErrorMessage = "Invalid map file.";
    return RESOLUTION_BAD_NAME;
  }

  m_IsStepDownloaded = true;
  m_BaseDownloadFileName = downloadFileName;
  m_MapDownloadUri = downloadUri;

  return RESOLUTION_OK;
}

void CGameSetup::RunResolveMapRepository()
{
  m_IsStepDownloading = true;
  m_AsyncStep = GAMESETUP_STEP_RESOLUTION;
  m_DownloadFuture = async(launch::async, &::CGameSetup::ResolveMapRepositoryTask, this);
}

uint32_t CGameSetup::RunResolveMapRepositorySync()
{
  m_IsStepDownloading = true;
  uint32_t result = ResolveMapRepositoryTask();
  m_IsStepDownloading = false;
  if (!m_IsStepDownloaded) {
    m_Ctx->ErrorReply(m_ErrorMessage, CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return 0;
  }
  m_IsStepDownloading = false;
  m_IsStepDownloaded = false;
  return result;
}

void CGameSetup::OnResolveMapSuccess()
{
  if (PrepareDownloadMap()) {
    RunDownloadMap();
  } else {
    OnLoadMapError();
  }  
}

void CGameSetup::SetDownloadFilePath(filesystem::path&& filePath)
{
  m_DownloadFilePath = move(filePath);
}

bool CGameSetup::PrepareDownloadMap()
{
  if (m_SearchTarget.first != "epicwar" && m_SearchTarget.first != "wc3maps") {
    Print("Error!! trying to download from " + m_SearchTarget.first + "!!");
    return false;
  }
  if (!m_Aura->m_Net->m_Config->m_AllowDownloads) {
    m_Ctx->ErrorReply("Map downloads are not allowed.", CHAT_SEND_SOURCE_ALL);
    return false;
  }

  string fileNameFragmentPost = ParseFileExtension(m_BaseDownloadFileName);
  string fileNameFragmentPre = m_BaseDownloadFileName.substr(0, m_BaseDownloadFileName.length() - fileNameFragmentPost.length());
  bool nameSuccess = false;
  string mapSuffix;
  for (uint8_t i = 0; i < 10; ++i) {
    if (i != 0) {
      mapSuffix = "~" + to_string(i);
    }
    string testFileName = fileNameFragmentPre + mapSuffix + fileNameFragmentPost;
    filesystem::path testFilePath = m_Aura->m_Config->m_MapPath / filesystem::path(testFileName);
    if (FileExists(testFilePath)) {
      // Map already exists.
      // I'd rather directly open the file with wx flags to avoid racing conditions,
      // but there is no standard C++ way to do this, and cstdio isn't very helpful.
      continue;
    }
    if (m_Aura->m_BusyMaps.find(testFileName) != m_Aura->m_BusyMaps.end()) {
      // Map already hosted.
      continue;
    }
    SetDownloadFilePath(move(testFilePath));
    nameSuccess = true;
    break;
  }
  if (!nameSuccess) {
    m_Ctx->ErrorReply("Download failed - duplicate map name [" + m_BaseDownloadFileName + "].", CHAT_SEND_SOURCE_ALL);
    return false;
  }
  Print("[NET] GET <" + m_MapDownloadUri + "> as [" + PathToString(m_DownloadFilePath.filename()) + "]...");
  m_DownloadFileStream = new ofstream(m_DownloadFilePath.native().c_str(), std::ios_base::out | std::ios_base::binary);
  return true;
}

uint32_t CGameSetup::DownloadMapTask()
{
  if (!m_DownloadFileStream) return 0;
  if (!m_DownloadFileStream->is_open()) {
    m_DownloadFileStream->close();
    delete m_DownloadFileStream;
    m_DownloadFileStream = nullptr;
    m_ErrorMessage = "Download failed - unable to write to disk.";
    return 0;
  }
  cpr::Response response = cpr::Download(
    *m_DownloadFileStream,
    cpr::Url{m_MapDownloadUri},
    cpr::Header{{"user-agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:109.0) Gecko/20100101 Firefox/115.0"}},
    cpr::Timeout{m_DownloadTimeout}
  );
  m_DownloadFileStream->close();
  delete m_DownloadFileStream;
  m_DownloadFileStream = nullptr;
  if (response.status_code == 0) {
    m_ErrorMessage = "Failed to access " + m_SearchTarget.first + " repository (connectivity error).";
    FileDelete(m_DownloadFilePath);
    return 0;
  }
  if (response.status_code != 200) {
    m_ErrorMessage = "Map not found in " + m_SearchTarget.first + " repository (code " + to_string(response.status_code) + ").";
    FileDelete(m_DownloadFilePath);
    return 0;
  }
  const bool timedOut = cpr::ErrorCode::OPERATION_TIMEDOUT == response.error.code;
  if (timedOut) {
    m_ErrorMessage = "Map download took too long.";
    FileDelete(m_DownloadFilePath);
    return 0;
  }
  Print("[AURA] download task completed in " + to_string(static_cast<float>(response.elapsed * 1000)) + " ms");
  // Signals completion.
  m_IsStepDownloaded = true;

  return static_cast<uint32_t>(response.downloaded_bytes);
}

void CGameSetup::RunDownloadMap()
{
  m_IsStepDownloading = true;
  m_AsyncStep = GAMESETUP_STEP_DOWNLOAD;
  m_DownloadFuture = async(launch::async, &::CGameSetup::DownloadMapTask, this);
}

uint32_t CGameSetup::RunDownloadMapSync()
{
  m_IsStepDownloading = true;
  uint32_t byteSize = DownloadMapTask();
  m_IsStepDownloading = false;
  if (!m_IsStepDownloaded) {
    m_Ctx->ErrorReply(m_ErrorMessage, CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    return 0;
  }
  m_IsStepDownloaded = false;
  return byteSize;
}

vector<string> CGameSetup::GetMapRepositorySuggestions(const string& pattern, const uint8_t maxCount)
{
  vector<string> suggestions;
  string searchUri = "https://www.epicwar.com/maps/search/?go=1&n=" + EncodeURIComponent(pattern) + "&a=&c=0&p=0&pf=0&roc=0&tft=0&order=desc&sort=downloads&page=1";
  Print("[AURA] Looking up suggestions...");
  Print("[AURA] GET <" + searchUri + ">");
  auto response = cpr::Get(cpr::Url{searchUri});
  if (response.status_code != 200) {
    return suggestions;
  }

  vector<pair<string, int>> matchingMaps = ExtractEpicWarMaps(response.text, maxCount);
  for (const auto& element : matchingMaps) {
    suggestions.push_back(element.first + " (epicwar-" + to_string(element.second) + ")");
  }
  return suggestions;
}

#endif

void CGameSetup::LoadMap()
{
  if (m_Map) {
    OnLoadMapSuccess();
    return;
  }

  ParseInput();
  pair<uint8_t, std::filesystem::path> searchResult = SearchInput();
  if (searchResult.first == MATCH_TYPE_FORBIDDEN) {
    OnLoadMapError();
    return;
  }
  if (searchResult.first == MATCH_TYPE_INVALID) {
    // Exclusive to standard paths mode.
    m_Ctx->ErrorReply("Invalid file extension for [" + PathToString(searchResult.second.filename()) + "]. Please use --search-type");
    OnLoadMapError();
    return;
  }
  if (searchResult.first != MATCH_TYPE_MAP && searchResult.first != MATCH_TYPE_CONFIG) {
    if (m_SearchType != SEARCH_TYPE_ANY || !m_IsDownloadable) {
      OnLoadMapError();
      return;
    }
    if (m_Aura->m_Config->m_EnableCFGCache) {
      filesystem::path cachePath = m_Aura->m_Config->m_MapCachePath / filesystem::path(m_SearchTarget.first + "-" + m_SearchTarget.second + ".cfg");
      m_Map = GetBaseMapFromConfigFile(cachePath, true, true);
      if (m_Map) {
        OnLoadMapSuccess();
        return;
      }
    }
#ifndef DISABLE_CPR
    RunResolveMapRepository();
    return;
#else
    OnLoadMapError();
    return;
#endif
  }
  if (searchResult.first == MATCH_TYPE_CONFIG) {
    m_Map = GetBaseMapFromConfigFile(searchResult.second, false, false);
  } else {
    m_Map = GetBaseMapFromMapFileOrCache(searchResult.second, false);
  }
  if (m_Map) {
    OnLoadMapSuccess();
  } else {
    OnLoadMapError();
  }
//
}

void CGameSetup::OnLoadMapSuccess()
{
  if (m_Ctx->GetPartiallyDestroyed()) {
    m_DeleteMe = true;
    return;
  }
  if (m_MapExtraOptions) {
    if (!ApplyMapModifiers(m_MapExtraOptions)) {
      m_Ctx->ErrorReply("Invalid map options. Map has fixed player settings.", CHAT_SEND_SOURCE_ALL);
      m_DeleteMe = true;
      ResetExtraOptions();
      return;
    }
    ResetExtraOptions();
  }

  if (m_MapReadyCallbackAction == MAP_ONREADY_HOST) {
    if (m_Aura->m_CurrentLobby)  {
      m_Ctx->ErrorReply("Already hosting a game.", CHAT_SEND_SOURCE_ALL);
    } else {
      SetName(m_MapReadyCallbackData);
      CRealm* sourceRealm = m_Ctx->GetSourceRealm();
      SetOwner(m_Ctx->GetSender(), sourceRealm);
      if (sourceRealm) {
        SetCreator(m_Ctx->GetSender(), sourceRealm);
      } else if (m_Ctx->GetSourceIRC()) {
        SetCreator(m_Ctx->GetSender(), m_Ctx->GetSourceIRC());
      }
      RunHost();
    }
  }
}

void CGameSetup::OnLoadMapError()
{
  if (m_Ctx->GetPartiallyDestroyed()) {
    m_DeleteMe = true;
    return;
  }
  m_Ctx->ErrorReply("Map not found", CHAT_SEND_SOURCE_ALL);
  m_DeleteMe = true;
}

#ifndef DISABLE_CPR
void CGameSetup::OnDownloadMapSuccess()
{
  if (!m_Aura) return;
  m_IsMapDownloaded = true;
  m_Map = GetBaseMapFromMapFileOrCache(m_DownloadFilePath, false);
  if (m_Map) {
    OnLoadMapSuccess();
  } else {
    OnLoadMapError();
  }
}

void CGameSetup::OnFetchSuggestionsEnd()
{
}
#endif

bool CGameSetup::LoadMapSync()
{
  if (m_Map) return true;

  ParseInput();
  pair<uint8_t, std::filesystem::path> searchResult = SearchInput();
  if (searchResult.first == MATCH_TYPE_FORBIDDEN) {
    return false;
  }
  if (searchResult.first == MATCH_TYPE_INVALID) {
    // Exclusive to standard paths mode.
    m_Ctx->ErrorReply("Invalid file extension for [" + PathToString(searchResult.second.filename()) + "]. Please use --search-type");
    return false;
  }
  if (searchResult.first != MATCH_TYPE_MAP && searchResult.first != MATCH_TYPE_CONFIG) {
    if (m_SearchType != SEARCH_TYPE_ANY || !m_IsDownloadable) {
      return false;
    }
    if (m_Aura->m_Config->m_EnableCFGCache) {
      filesystem::path cachePath = m_Aura->m_Config->m_MapCachePath / filesystem::path(m_SearchTarget.first + "-" + m_SearchTarget.second + ".cfg");
      m_Map = GetBaseMapFromConfigFile(cachePath, true, true);
      if (m_Map) return true;
    }
#ifndef DISABLE_CPR
    if (RunResolveMapRepositorySync() != RESOLUTION_OK) {
      Print("[AURA] Failed to resolve remote map.");
      return false;
    }
    if (!PrepareDownloadMap()) {
      return false;
    }
    uint32_t downloadSize = RunDownloadMapSync();
    if (downloadSize == 0) {
      Print("[AURA] Failed to download map.");
      return false;
    }
    m_Map = GetBaseMapFromMapFileOrCache(m_DownloadFilePath, false);
    return true;
#else
    return false;
#endif
  }
  if (searchResult.first == MATCH_TYPE_CONFIG) {
    m_Map = GetBaseMapFromConfigFile(searchResult.second, false, false);
  } else {
    m_Map = GetBaseMapFromMapFileOrCache(searchResult.second, false);
  }
  return m_Map != nullptr;
}

bool CGameSetup::SetActive()
{
  if (m_Aura->m_GameSetup) {
    delete m_Aura->m_GameSetup;
  }
  m_Aura->m_GameSetup = this;
  return true;
}

bool CGameSetup::RunHost()
{
  return m_Aura->CreateGame(this);
}

bool CGameSetup::SetMirrorSource(const sockaddr_storage& nSourceAddress, const uint32_t nGameIdentifier)
{
  m_GameIsMirror = true;
  m_GameIdentifier = nGameIdentifier;
  memcpy(&m_RealmsAddress, &nSourceAddress, sizeof(sockaddr_storage));
  return true;
}

bool CGameSetup::SetMirrorSource(const string& nInput)
{
  size_t portStart = nInput.find(":", 0);
  if (portStart == string::npos) return false;
  size_t idStart = nInput.find("#", portStart);
  if (idStart == string::npos) return false;
  string rawAddress = nInput.substr(0, portStart);
  if (rawAddress.length() < 7) return false;
  string rawPort = nInput.substr(portStart + 1, idStart - (portStart + 1));
  if (rawPort.empty()) return false;
  string rawId = nInput.substr(idStart + 1);
  if (rawId.empty()) return false;
  optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(rawAddress, ACCEPT_IPV4);
  if (!maybeAddress.has_value()) return false;
  uint16_t gamePort = 0;
  uint32_t gameId = 0;
  try {
    int64_t value = stol(rawPort);
    if (value <= 0 || value > 0xFFFF) return false;
    gamePort = static_cast<uint16_t>(value);
  } catch (...) {
    return false;
  }
  try {
    int64_t value = stol(rawId);
    if (value < 0 || value > 0xFFFFFFFF) return false;
    gameId = static_cast<uint32_t>(value);
  } catch (...) {
    return false;
  }
  SetAddressPort(&(maybeAddress.value()), gamePort);
  return SetMirrorSource(maybeAddress.value(), gameId);
}

void CGameSetup::AddIgnoredRealm(const CRealm* nRealm)
{
  m_RealmsExcluded.insert(nRealm->GetServer());
}

void CGameSetup::RemoveIgnoredRealm(const CRealm* nRealm)
{
  m_RealmsExcluded.erase(nRealm->GetServer());
}

void CGameSetup::SetDisplayMode(const uint8_t nDisplayMode)
{
  m_RealmsDisplayMode = nDisplayMode;
}

void CGameSetup::SetOwner(const string& nOwner, const CRealm* nRealm)
{
  if (nRealm == nullptr) {
    m_GameOwner = make_pair(nOwner, string());
  } else {
    m_GameOwner = make_pair(nOwner, nRealm->GetServer());
  }
}

void CGameSetup::SetCreator(const string& nCreator, CRealm* nRealm)
{
  m_CreatedBy = nCreator;
  m_CreatedFrom = reinterpret_cast<void*>(nRealm);
  m_CreatedFromType = GAMESETUP_ORIGIN_REALM;
}

void CGameSetup::SetCreator(const string& nCreator, CIRC* nIRC)
{
  m_CreatedBy = nCreator;
  m_CreatedFrom = reinterpret_cast<void*>(nIRC);
  m_CreatedFromType = GAMESETUP_ORIGIN_IRC;
}

void CGameSetup::RemoveCreator()
{
  m_CreatedBy.clear();
  m_CreatedFrom = nullptr;
  m_CreatedFromType = GAMESETUP_ORIGIN_INVALID;
}

bool CGameSetup::MatchesCreatedFrom(const uint8_t fromType, const void* fromThing) const
{
  if (m_CreatedFromType != fromType) return false;
  switch (fromType) {
    case GAMESETUP_ORIGIN_REALM:
      return reinterpret_cast<const CRealm*>(m_CreatedFrom) == reinterpret_cast<const CRealm*>(fromThing);
    case GAMESETUP_ORIGIN_IRC:
      return reinterpret_cast<const CIRC*>(m_CreatedFrom) == reinterpret_cast<const CIRC*>(fromThing);
  }
  return false;
}

bool CGameSetup::Update()
{
#ifndef DISABLE_CPR
  if (!m_IsStepDownloading) return m_DeleteMe;
  auto status = m_DownloadFuture.wait_for(chrono::seconds(0));
  if (status != future_status::ready) return m_DeleteMe;
  const bool success = m_IsStepDownloaded;
  const uint8_t finishedStep = m_AsyncStep;
  m_IsStepDownloading = false;
  m_IsStepDownloaded = false;
  m_AsyncStep = GAMESETUP_STEP_MAIN;
  if (!success && finishedStep != GAMESETUP_STEP_SUGGESTIONS) {
    m_Ctx->ErrorReply(m_ErrorMessage, CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
    m_DeleteMe = true;
    return m_DeleteMe;
  }
  switch (finishedStep) {
    case GAMESETUP_STEP_RESOLUTION:
      OnResolveMapSuccess();
      break;
    case GAMESETUP_STEP_DOWNLOAD:
      OnDownloadMapSuccess();
      break;
    case GAMESETUP_STEP_SUGGESTIONS:
      OnFetchSuggestionsEnd();
      break;
    default:
      Print("[AURA] error - unhandled async task completion");
  }
#endif
  return m_DeleteMe;
}

void CGameSetup::ResetExtraOptions()
{
  if (m_MapExtraOptions) {
    delete m_MapExtraOptions;
    m_MapExtraOptions = nullptr;
  }
}

CGameSetup::~CGameSetup()
{
  ResetExtraOptions();
  if (m_Aura) {
    m_Aura->UnholdContext(m_Ctx);
  } else {
    delete m_Ctx;
  }
  m_Ctx = nullptr;
  m_CreatedFrom = nullptr;
  m_Aura = nullptr;
  m_Map = nullptr;
}
