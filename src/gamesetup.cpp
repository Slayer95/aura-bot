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
  int result = 0;
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
  int result = 0;
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
    m_StandardPaths(false),
    m_LuckyMode(false),

    m_IsDownloadable(false),
    m_IsDownloaded(false),

    m_GameIsMirror(false),    
    m_RealmsDisplayMode(GAME_PUBLIC),
    m_CreatorRealm(nullptr)
    
{
  memset(&m_RealmsAddress, 0, sizeof(m_RealmsAddress));
  m_Map = GetBaseMapFromConfig(nMapCFG, false);
}

CGameSetup::CGameSetup(CAura* nAura, CCommandContext* nCtx, const string nSearchRawTarget, const uint8_t nSearchType, const bool nUseStandardPaths, const bool nUseLuckyMode)
  : m_Aura(nAura),
    m_Map(nullptr),
    m_Ctx(nCtx),

    m_Attribution(nCtx->GetUserAttribution()),
    m_SearchRawTarget(move(nSearchRawTarget)),
    m_SearchType(nSearchType),
    m_StandardPaths(nUseStandardPaths),
    m_LuckyMode(nUseLuckyMode),

    m_IsDownloadable(false),
    m_IsDownloaded(false),

    m_GameIsMirror(false),    
    m_RealmsDisplayMode(GAME_PUBLIC),
    m_CreatorRealm(nullptr)
    
{
  memset(&m_RealmsAddress, 0, sizeof(m_RealmsAddress));
}

std::string CGameSetup::GetInspectName() const
{
  return m_Map->GetConfigName();
}

void CGameSetup::ParseInput()
{
  if (m_StandardPaths || m_SearchType != SEARCH_TYPE_ANY) {
    m_SearchTarget = make_pair("local", m_SearchRawTarget);
    return;
  }

  std::string lower = m_SearchRawTarget;
  std::transform(std::begin(lower), std::end(lower), std::begin(lower), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  // Custom namespace/protocol
  if (lower.substr(0, 8) == "epicwar-" || lower.substr(0, 8) == "epicwar:") {
    m_SearchTarget = make_pair("epicwar", MaybeBase10(lower.substr(8)));
    m_IsDownloadable = true;
    return;
  }
  if (lower.substr(0, 6) == "local-" || lower.substr(0, 6) == "local:") {
    m_SearchTarget = make_pair("local", m_SearchRawTarget);
    return;
  }
 

  bool isUri = false;
  if (lower.substr(0, 7) == "http://") {
    isUri = true;
    lower = lower.substr(7);
  } else if (lower.substr(0, 8) == "https://") {
    isUri = true;
    lower = lower.substr(8);
  }

  if (lower.substr(0, 12) == "epicwar.com/") {
    std::string mapCode = lower.substr(17);
    m_SearchTarget = make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (lower.substr(0, 16) == "www.epicwar.com/") {
    std::string mapCode = lower.substr(21);
    m_SearchTarget = make_pair("epicwar", MaybeBase10(TrimTrailingSlash(mapCode)));
    m_IsDownloadable = true;
    return;
  }
  if (isUri) {
    m_SearchTarget = make_pair("remote", std::string());
  } else {
    m_SearchTarget = make_pair("local", m_SearchRawTarget);
  }
}


pair<uint8_t, filesystem::path> CGameSetup::SearchInputStandard()
{
  // Error handling: SearchInputStandard() reports the full file path
  // This can be absolute. That's not a problem.
  filesystem::path targetPath = m_SearchTarget.second;
  if (!FileExists(targetPath)) {
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

  string targetExt = targetPath.filename().string().substr(m_SearchTarget.second.length() - 4, 4);
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
  string fileExtension = GetFileExtension(m_SearchTarget.second);
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config->m_MapPath / filesystem::path(m_SearchTarget.second)).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config->m_MapPath) {
      return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());
    }
    if ((fileExtension == ".w3m" || fileExtension == ".w3x") && FileExists(testPath)) {
      return make_pair(MATCH_TYPE_MAP, testPath);
    }
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config->m_MapCFGPath / filesystem::path(m_SearchTarget.second)).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config->m_MapCFGPath) {
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
  string fileExtension = GetFileExtension(m_SearchTarget.second);
  bool hasExtension = fileExtension == ".w3x" || fileExtension == ".w3m" || fileExtension == ".cfg";
  if (hasExtension) {
    return make_pair(MATCH_TYPE_NONE, filesystem::path());
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    filesystem::path testPath = (m_Aura->m_Config->m_MapPath / filesystem::path(m_SearchTarget.second + ".w3x")).lexically_normal();
    if (testPath.parent_path() != m_Aura->m_Config->m_MapPath) {
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
    if (testPath.parent_path() != m_Aura->m_Config->m_MapCFGPath) {
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
  size_t leastDistance = 255;
  string closestMatch;

  vector<pair<string, int>> allResults;
  if (m_SearchType == SEARCH_TYPE_ONLY_MAP || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    vector<pair<string, int>> mapResults = FuzzySearchFiles(m_Aura->m_Config->m_MapPath, {".w3x", ".w3m"}, m_SearchTarget.second);
    for (const auto& result : mapResults) {
      allResults.push_back(make_pair(result.first, result.second | 0x80));
    }
  }
  if (m_SearchType == SEARCH_TYPE_ONLY_CONFIG || m_SearchType == SEARCH_TYPE_ONLY_FILE || m_SearchType == SEARCH_TYPE_ANY) {
    vector<pair<string, int>> cfgResults = FuzzySearchFiles(m_Aura->m_Config->m_MapCFGPath, {".cfg"}, m_SearchTarget.second);
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
      return make_pair(MATCH_TYPE_CONFIG, m_Aura->m_Config->m_MapCFGPath / filesystem::path(closestMatch));
    } else {
      return make_pair(MATCH_TYPE_MAP, m_Aura->m_Config->m_MapPath / filesystem::path(closestMatch));
    }
  }

  // Return suggestions through passed argument.
  for (uint8_t i = 0; i < resultCount; i++) {
    fuzzyMatches.push_back(allResults[i].first);
  }

  return make_pair(MATCH_TYPE_NONE, filesystem::path());
}

void CGameSetup::SearchInputRemoteFuzzy(vector<string>& fuzzyMatches) {
  if (fuzzyMatches.size() >= 5) return;
  vector<string> remoteSuggestions = GetEpicWarSuggestions(m_SearchTarget.second, 5 - static_cast<uint8_t>(fuzzyMatches.size()));
  // Return suggestions through passed argument.
  fuzzyMatches.insert(end(fuzzyMatches), begin(remoteSuggestions), end(remoteSuggestions));
}

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
    // Find config or map path
    vector<string> fuzzyMatches;
    pair<uint8_t, filesystem::path> result = SearchInputLocal(fuzzyMatches);
    if (result.first == MATCH_TYPE_MAP || result.first == MATCH_TYPE_CONFIG) {
      return result;
    }

    if (m_Aura->m_Config->m_MapSearchShowSuggestions) {
      if (m_Aura->m_Games.empty()) {
        // Synchronous download, only if there are no ongoing games.
        SearchInputRemoteFuzzy(fuzzyMatches);
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
  if (PathHasNullBytes(resolvedCFGPath) || resolvedCFGPath.parent_path() != m_Aura->m_Config->m_MapCFGPath) {
    return make_pair(MATCH_TYPE_FORBIDDEN, filesystem::path());
  }
  if (FileExists(resolvedCFGPath)) {
    return make_pair(MATCH_TYPE_CONFIG, resolvedCFGPath);
  }
  return make_pair(MATCH_TYPE_NONE, resolvedCFGPath);
}

CMap* CGameSetup::GetBaseMapFromConfig(CConfig* mapCFG, const bool silent)
{
  CMap* map = new CMap(m_Aura, mapCFG);
  if (!map) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map config", CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }
  const char* errorMessage = map->CheckValid();
  if (errorMessage) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map config: " + string(errorMessage), CHAT_SEND_SOURCE_ALL);
    delete map;
    return nullptr;
  }
  return map;
}

CMap* CGameSetup::GetBaseMapFromConfigFile(const filesystem::path& filePath, const bool silent)
{
  CConfig MapCFG;
  if (!MapCFG.Read(filePath)) {
    if (!silent) m_Ctx->ErrorReply("Map config file [" + filePath.filename().string() + "] not found.", CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }
  return GetBaseMapFromConfig(&MapCFG, silent);
}

CMap* CGameSetup::GetBaseMapFromMapFile(const filesystem::path& filePath, const bool silent)
{
  bool isInMapsFolder = filePath.parent_path() == m_Aura->m_Config->m_MapPath;
  string fileName = filePath.filename().string();
  string baseFileName;
  if (fileName.length() > 6 && fileName[fileName.length() - 6] == '~' && isdigit(fileName[fileName.length() - 5])) {
    baseFileName = fileName.substr(0, fileName.length() - 6) + fileName.substr(fileName.length() - 4);
  } else {
    baseFileName = fileName;
  }

  CConfig MapCFG;
  MapCFG.SetBool("cfg_partial", true);
  MapCFG.Set("map_path", R"(Maps\Download\)" + baseFileName);
  MapCFG.Set("map_localpath", isInMapsFolder ? fileName : filePath.string());

  if (m_IsDownloaded) {
    MapCFG.Set("map_site", m_MapSiteUri);
    MapCFG.Set("map_url", m_MapDownloadUri);
    MapCFG.Set("downloaded_by", m_Attribution);
  }

  if (fileName.find("DotA") != string::npos) {
    MapCFG.Set("map_type", "dota");
  }

  CMap* baseMap = new CMap(m_Aura, &MapCFG);
  if (!baseMap) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map.", CHAT_SEND_SOURCE_ALL);
    return nullptr;
  }
  const char* errorMessage = baseMap->CheckValid();
  if (errorMessage) {
    if (!silent) m_Ctx->ErrorReply("Failed to load map: " + string(errorMessage), CHAT_SEND_SOURCE_ALL);
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
    filesystem::path resolvedCFGPath = (m_Aura->m_Config->m_MapCFGPath / filesystem::path(resolvedCFGName)).lexically_normal();

    vector<uint8_t> bytes = MapCFG.Export();
    FileWrite(resolvedCFGPath, bytes.data(), bytes.size());
    m_Aura->m_CachedMaps[fileName] = resolvedCFGName;
    Print("[AURA] Cached map config for [" + fileName + "] as [" + resolvedCFGPath.string() + "]");
  }

  if (!silent) m_Ctx->SendReply("Map file loaded OK [" + fileName + "]", CHAT_SEND_SOURCE_ALL | CHAT_LOG_CONSOLE);
  return baseMap;
}

CMap* CGameSetup::GetBaseMapFromMapFileOrCache(const filesystem::path& mapPath, const bool silent)
{
  string fileName = mapPath.filename().string();
  if (m_Aura->m_Config->m_EnableCFGCache && m_Aura->m_CachedMaps.find(fileName) != m_Aura->m_CachedMaps.end()) {
    string cfgName = m_Aura->m_CachedMaps[fileName];
    CMap* cachedResult = GetBaseMapFromConfigFile(cfgName, true);
    if (cachedResult == nullptr || cachedResult->GetMapLocalPath() != fileName) {
      // Oops! CFG file is gone!
      m_Aura->m_CachedMaps.erase(fileName);
    } else {
      Print("[DEBUG] Retrieving cached config " + cfgName + " -> " + fileName);
      return cachedResult;
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

uint8_t CGameSetup::ResolveRemoteMap()
{
  string downloadUri, downloadFileName;
  uint64_t SearchTargetType = HashCode(m_SearchTarget.first);

  switch (SearchTargetType) {
    case HashCode("epicwar"): {
      m_MapSiteUri = "https://www.epicwar.com/maps/" + m_SearchTarget.second;
      Print("Downloading <" + m_MapSiteUri + ">");
      auto response = cpr::Get(cpr::Url{m_MapSiteUri}, cpr::Timeout{m_Aura->m_Net->m_Config->m_DownloadTimeout});
      if (response.status_code != 200) return RESOLUTION_ERR;
      
      size_t DownloadUriStartIndex = response.text.find("<a href=\"/maps/download/");
      if (DownloadUriStartIndex == string::npos) return RESOLUTION_ERR;
      size_t DownloadUriEndIndex = response.text.find("\"", DownloadUriStartIndex + 24);
      if (DownloadUriEndIndex == string::npos) return RESOLUTION_ERR;
      downloadUri = "https://epicwar.com" + response.text.substr(DownloadUriStartIndex + 9, (DownloadUriEndIndex) - (DownloadUriStartIndex + 9));
      size_t LastSlashIndex = downloadUri.rfind("/");
      if (LastSlashIndex == string::npos) return RESOLUTION_ERR;
      string encodedName = downloadUri.substr(LastSlashIndex + 1);
      downloadFileName = DecodeURIComponent(encodedName);
      break;
    }

    default: {
      Print("Unsupported remote domain: " + m_SearchTarget.first);
      return RESOLUTION_ERR;
    }
  }

  if (downloadFileName.empty() || downloadFileName[0] == '.' || downloadFileName[0] == '-' || downloadFileName.length() > 80){
    return RESOLUTION_BAD_NAME;
  }

  std::string InvalidChars = "/\\\0\"*?:|<>;,";
  if (downloadFileName.find_first_of(InvalidChars) != string::npos) {
    return RESOLUTION_BAD_NAME;
  }

  string fileExtension = GetFileExtension(downloadFileName);
  if (fileExtension != ".w3m" && fileExtension != ".w3x") {
    return RESOLUTION_BAD_NAME;
  }

  string downloadFileNameNoExt = downloadFileName.substr(0, downloadFileName.length() - fileExtension.length());
  downloadFileName = downloadFileNameNoExt + fileExtension; // case-sensitive name, but lowercase extension

  // CON, PRN, AUX, NUL, etc. are case-insensitive
  transform(begin(downloadFileNameNoExt), end(downloadFileNameNoExt), begin(downloadFileNameNoExt), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (downloadFileNameNoExt == "con" || downloadFileNameNoExt == "prn" || downloadFileNameNoExt == "aux" || downloadFileNameNoExt == "nul") {
    return RESOLUTION_BAD_NAME;
  }
  if (downloadFileNameNoExt.length() == 4 && (downloadFileNameNoExt.substr(0, 3) == "com" || downloadFileNameNoExt.substr(0, 3) == "lpt")) {
    return RESOLUTION_BAD_NAME;
  }

  m_BaseDownloadFileName = downloadFileName;
  m_MapDownloadUri = downloadUri;

  return RESOLUTION_OK;
}

void CGameSetup::SetDownloadFilePath(filesystem::path&& filePath)
{
  m_DownloadFilePath = move(filePath);
}

bool CGameSetup::RunDownload()
{
  if (m_SearchTarget.first != "epicwar") {
    Print("Error!! trying to download from " + m_SearchTarget.first + "!!");
    return false;
  }
  if (!m_Aura->m_Net->m_Config->m_AllowDownloads) {
    m_Ctx->ErrorReply("Map downloads are not allowed.", CHAT_SEND_SOURCE_ALL);
    return false;
  }
  if (!m_Aura->m_Games.empty()/* && m_Aura->m_Net->m_Config->m_HasBufferBloat*/) {
    // Since the download is synchronous, it will lag active games even without bufferbloat.
    m_Ctx->ErrorReply(string("Currently hosting ") + to_string(m_Aura->m_Games.size()) + " game(s). Downloads are disabled to prevent high latency.", CHAT_SEND_SOURCE_ALL);
    return false;
  }

  string fileNameFragmentPost = GetFileExtension(m_BaseDownloadFileName);
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
    break;
  }
  if (!nameSuccess) {
    m_Ctx->ErrorReply("Download failed - duplicate map name.", CHAT_SEND_SOURCE_ALL);
    return false;
  }
  Print("[AURA] Downloading <" + m_MapDownloadUri + "> as [" + m_DownloadFilePath.filename().string() + "]...");
  std::ofstream mapFile(m_DownloadFilePath.string(), std::ios_base::out | std::ios_base::binary);
  if (!mapFile.is_open()) {
    mapFile.close();
    m_Ctx->ErrorReply("Download failed - unable to write to disk.", CHAT_SEND_SOURCE_ALL);
    return false;
  }
  cpr::Response MapResponse = cpr::Download(mapFile, cpr::Url{m_MapDownloadUri}, cpr::Header{{"user-agent", "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:109.0) Gecko/20100101 Firefox/115.0"}});
  mapFile.close();
  if (MapResponse.status_code != 200) {
    m_Ctx->ErrorReply("Map not found in EpicWar.", CHAT_SEND_SOURCE_ALL);
    return false;
  }

  m_IsDownloaded = true;
  return true;
}

bool CGameSetup::LoadMap()
{
  if (m_Map) return true;

  ParseInput();
  pair<uint8_t, std::filesystem::path> searchResult = SearchInput();
  if (searchResult.first == MATCH_TYPE_FORBIDDEN) {
    return false;
  }
  if (searchResult.first == MATCH_TYPE_INVALID) {
    // Exclusive to standard paths mode.
    m_Ctx->ErrorReply("Invalid file extension for [" + searchResult.second.filename().string() + "]. Please use --filetype");
    return false;
  }
  if (searchResult.first != MATCH_TYPE_MAP && searchResult.first != MATCH_TYPE_CONFIG) {
    if (m_SearchType != SEARCH_TYPE_ANY || !m_IsDownloadable || ResolveRemoteMap() != RESOLUTION_OK) {
      return false;
    }
    if (RunDownload()) {
      searchResult = SearchInput();
      if (searchResult.first != MATCH_TYPE_MAP && searchResult.first != MATCH_TYPE_CONFIG) {
        return false;
      }
    }
  }
  if (searchResult.first == MATCH_TYPE_CONFIG) {
    m_Map = GetBaseMapFromConfigFile(searchResult.second, false);
  } else {
    m_Map = GetBaseMapFromMapFileOrCache(searchResult.second, false);
  }
  return true;
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

bool CGameSetup::SetMirrorSource(const sockaddr_storage& nSourceAddress, const uint32_t nGameIdentifier, const uint32_t nGameChannelKey)
{
  m_GameIsMirror = true;
  m_GameIdentifier = nGameIdentifier;
  m_GameChannelKey = nGameChannelKey;
  memcpy(&m_RealmsAddress, &nSourceAddress, sizeof(sockaddr_storage));
  return true;
}

bool CGameSetup::SetMirrorSource(const string& nInput)
{
  size_t portStart = nInput.find(":", 0);
  if (portStart == string::npos) return false;
  size_t idStart = nInput.find("#", portStart);
  if (idStart == string::npos) return false;
  size_t keyStart = nInput.find(":", idStart);
  if (keyStart == string::npos) return false;
  string rawAddress = nInput.substr(0, portStart);
  if (rawAddress.length() < 7) return false;
  string rawPort = nInput.substr(portStart + 1, idStart - (portStart + 1));
  if (rawPort.empty()) return false;
  string rawId = nInput.substr(idStart + 1, keyStart - (idStart + 1));
  if (rawId.empty()) return false;
  string rawKey = nInput.substr(keyStart + 1);
  if (rawKey.empty()) return false;
  optional<sockaddr_storage> maybeAddress = CNet::ParseAddress(rawAddress, ACCEPT_IPV4);
  if (!maybeAddress.has_value()) return false;
  uint16_t gamePort = 0;
  uint32_t gameId = 0;
  uint32_t gameKey = 0;
  try {
    uint32_t value = stoul(rawPort);
    if (value <= 0 || value > 0xFF) return false;
    gamePort = static_cast<uint16_t>(value);
  } catch (...) {
    return false;
  }
  try {
    uint64_t value = stoul(rawId);
    if (value < 0 || value > 0xFFFFFFFF) return false;
    gameId = static_cast<uint32_t>(value);
  } catch (...) {
    return false;
  }
  try {
    uint64_t value = stoul(rawKey);
    if (value < 0 || value > 0xFFFFFFFF) return false;
    gameKey = static_cast<uint32_t>(value);
  } catch (...) {
    return false;
  }
  SetAddressPort(&(maybeAddress.value()), gamePort);
  return SetMirrorSource(maybeAddress.value(), gameId, gameKey);
}

void CGameSetup::AddIgnoredRealm(const string& nRealm)
{
  m_RealmsExcluded.insert(nRealm);
}

void CGameSetup::AddIgnoredRealm(const CRealm* nRealm)
{
  m_RealmsExcluded.insert(nRealm->GetServer());
}

void CGameSetup::RemoveIgnoredRealm(const CRealm* nRealm)
{
  m_RealmsExcluded.erase(nRealm->GetServer());
}
void CGameSetup::RemoveIgnoredRealm(const string& nRealm)
{
  m_RealmsExcluded.erase(nRealm);
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
  m_CreatorName = nCreator;
  m_CreatorRealm = nRealm;
}

void CGameSetup::SetName(const string& nName)
{
  m_GameName = nName;
}

void CGameSetup::SetContext(CCommandContext* nCtx)
{
  m_Ctx = nCtx;
}

CGameSetup::~CGameSetup() = default;
