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

#include "fileutil.h"
#include "util.h"

#include <fstream>
#include <algorithm>
#include <iostream>
#include <bitset>
#include <set>
#include <unordered_set>
#include <optional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#pragma once
#include <windows.h>
#else
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <limits.h>
#endif

#define __STORMLIB_SELF__
#include <StormLib.h>

// unistd.h and limits.h

using namespace std;

bool FileExists(const filesystem::path& file)
{
  return filesystem::exists(file);
}

vector<string> FilesMatch(const filesystem::path& path, const vector<string>& extensionList)
{
  set<string> extensions(extensionList.begin(), extensionList.end());
  vector<string> Files;

#ifdef _WIN32
  WIN32_FIND_DATAA data;
  HANDLE           handle = FindFirstFileA((path.string() + "\\*").c_str(), &data);
  memset(&data, 0, sizeof(WIN32_FIND_DATAA));

  while (handle != INVALID_HANDLE_VALUE) {
    string Name = string(data.cFileName);
    transform(begin(Name), end(Name), begin(Name), ::tolower);
    if (Name != "..") {
      if (extensions.empty() || extensions.find(GetFileExtension(Name)) != extensions.end()) {
        Files.push_back(string(data.cFileName));
      }
    }
    if (FindNextFileA(handle, &data) == FALSE)
      break;
  }

  FindClose(handle);
#else
  DIR* dir = opendir(path.c_str());

  if (dir == nullptr)
    return Files;

  struct dirent* dp = nullptr;

  while ((dp = readdir(dir)) != nullptr) {
    string Name = string(dp->d_name);
    transform(begin(Name), end(Name), begin(Name), ::tolower);
    if (Name != "." && Name != "..") {
      if (extensions.empty() || extensions.find(GetFileExtension(Name)) != extensions.end()) {
        Files.emplace_back(dp->d_name);
      }
    }
  }

  closedir(dir);
#endif

  return Files;
}

string FileRead(const filesystem::path& file, uint32_t start, uint32_t length, int *byteSize)
{
  ifstream IS;
  IS.open(file.c_str(), ios::binary | ios::in);

  if (IS.fail())
  {
    Print("[UTIL] warning - unable to read file part [" + file.string() + "]");
    return string();
  }

  // get length of file

  IS.seekg(0, ios::end);

#pragma warning(push)
#pragma warning(disable: 4244)
  uint32_t FileLength = IS.tellg();
#pragma warning(pop)
  if (byteSize != nullptr) {
    (*byteSize) = FileLength;
  }

  if (start > FileLength)
  {
    IS.close();
    return string();
  }

  IS.seekg(start, ios::beg);

  // read data

  auto Buffer = new char[length];
  IS.read(Buffer, length);
  string BufferString = string(Buffer, IS.gcount());
  IS.close();
  delete[] Buffer;
  return BufferString;
}

string FileRead(const filesystem::path& file, int *byteSize)
{
  ifstream IS;
  IS.open(file.c_str(), ios::binary | ios::in);

  if (IS.fail())
  {
    Print("[UTIL] warning - unable to read file [" + file.string() + "]");
    return string();
  }

  // get length of file

  IS.seekg(0, ios::end);
#pragma warning(push)
#pragma warning(disable: 4244)
  uint32_t FileLength = IS.tellg();
#pragma warning(pop)
  if (byteSize != nullptr) {
    (*byteSize) = FileLength;
  }
  IS.seekg(0, ios::beg);

  // read data

  auto Buffer = new char[FileLength];
  IS.read(Buffer, FileLength);
  string BufferString = string(Buffer, IS.gcount());
  IS.close();
  delete[] Buffer;

  if (BufferString.size() == FileLength)
    return BufferString;
  else
    return string();
}

bool FileWrite(const filesystem::path& file, uint8_t* data, uint32_t length)
{
  ofstream OS;
  OS.open(file.c_str(), ios::binary);

  if (OS.fail())
  {
    Print("[UTIL] warning - unable to write file [" + file.string() + "]");
    return false;
  }

  // write data

  OS.write(reinterpret_cast<const char*>(data), length);
  OS.close();
  return true;
}

bool FileDelete(const filesystem::path& file)
{
  std::error_code e;
  bool result = std::filesystem::remove(file, e);
  
  if (e) {
    Print("[AURA] Cannot delete " + file.string() + ". " + e.message());
  }

  return result;
}

filesystem::path GetExeDirectory()
{
  static filesystem::path Memoized;
  if (!Memoized.empty())
    return Memoized;

  vector<char> buffer(2048);
#ifdef _WIN32
  DWORD length = 0;
#else
  ssize_t length = 0;
#endif

  do {
    buffer.resize(buffer.size() * 2);
#ifdef _WIN32
    length = GetModuleFileNameA(nullptr, buffer.data(), buffer.size());
#else
    length = readlink("/proc/self/exe", buffer.data(), buffer.size());
#endif
  } while (length >= buffer.size() - 1);

  if (length == 0) {
    throw std::runtime_error("Failed to retrieve the module file name.");
  }
  buffer.resize(length);

  filesystem::path executablePath(buffer.data());
  Memoized = executablePath.parent_path().lexically_normal();
  return Memoized;
}

filesystem::path CaseInsensitiveFileExists(const filesystem::path& path, const string& file)
{
  std::string mutated_file = file;
  const size_t NumberOfCombinations = static_cast<size_t>(std::pow(2, mutated_file.size()));

  for (size_t perm = 0; perm < NumberOfCombinations; ++perm) {
    std::bitset<64> bs(perm);
    std::transform(mutated_file.begin(), mutated_file.end(), mutated_file.begin(), ::tolower);

    for (size_t index = 0; index < bs.size() && index < mutated_file.size(); ++index) {
      if (bs[index])
        mutated_file[index] = static_cast<char>(::toupper(mutated_file[index]));
    }

    filesystem::path testPath = path / filesystem::path(mutated_file);
    if (FileExists(testPath))
      return testPath;
  }

  return "";
}

vector<pair<string, int>> FuzzySearchFiles(const filesystem::path& directory, const vector<string>& baseExtensions, const string& rawPattern)
{
  // If the pattern has a valid extension, restrict the results to files with that extension.
  string rawExtension = GetFileExtension(rawPattern);
  vector<string> extensions;
  if (!rawExtension.empty() && find(baseExtensions.begin(), baseExtensions.end(), rawExtension) != baseExtensions.end()) {
    extensions = vector<string>(1, rawExtension);
  } else {
    extensions = baseExtensions;
  }
  string fuzzyPattern = PreparePatternForFuzzySearch(rawPattern);
  vector<string> folderContents = FilesMatch(directory, extensions);
  unordered_set<string> filesSet(folderContents.begin(), folderContents.end());

  // 1. Try files that start with the given pattern (up to digits).
  //    These have triple weight.
  vector<pair<string, int>> inclusionMatches;
  for (const auto& mapName : filesSet) {
    string cmpName = PreparePatternForFuzzySearch(mapName);
    size_t fuzzyPatternIndex = cmpName.find(fuzzyPattern);
    if (fuzzyPatternIndex == string::npos) {
      continue;
    }
    bool startsWithPattern = true;
    for (uint8_t i = 0; i < fuzzyPatternIndex; ++i) {
      if (!isdigit(mapName[i])) {
        startsWithPattern = false;
        break;
      }
    }
    if (startsWithPattern) {
      inclusionMatches.push_back(make_pair(mapName, mapName.length() - fuzzyPattern.length()));
      if (inclusionMatches.size() >= FUZZY_SEARCH_MAX_RESULTS) {
        break;
      }
    }
  }
  if (inclusionMatches.size() > 0) {
    sort(inclusionMatches.begin(), inclusionMatches.end());
    return inclusionMatches;
  }

  // 2. Try fuzzy searching
  string::size_type maxDistance = FUZZY_SEARCH_MAX_DISTANCE;
  if (fuzzyPattern.size() < maxDistance) {
    maxDistance = fuzzyPattern.size() / 2;
  }

  vector<pair<string, int>> distances;
  for (auto& mapName : filesSet) {
    string cmpName = RemoveNonAlphanumeric(mapName);
    transform(begin(cmpName), end(cmpName), begin(cmpName), ::tolower);
    if ((fuzzyPattern.size() <= cmpName.size() + maxDistance) && (cmpName.size() <= maxDistance + fuzzyPattern.size())) {
      string::size_type distance = GetLevenshteinDistance(fuzzyPattern, cmpName); // source to target
      if (distance <= maxDistance) {
        distances.emplace_back(mapName, distance * 3);
      }
    }
  }

  size_t resultCount = min(FUZZY_SEARCH_MAX_RESULTS, static_cast<int>(distances.size()));
  partial_sort(
    distances.begin(),
    distances.begin() + resultCount,
    distances.end(),
    [](const pair<string, int>& a, const pair<string, int>& b) {
        return a.second < b.second;
    }
  );

  vector<pair<string, int>> fuzzyMatches(distances.begin(), distances.begin() + resultCount);
  return fuzzyMatches;
}

bool OpenMPQArchive(void** MPQ, const filesystem::path& filePath)
{
  Print("[AURA] loading MPQ file [" + filePath.string() + "]");
  return SFileOpenArchive(filePath.native().c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | STREAM_FLAG_READ_ONLY, MPQ);
}

void CloseMPQArchive(void* MPQ)
{
  SFileCloseArchive(MPQ);
}

bool ExtractMPQFile(void* MPQ, const char* archiveFile, const filesystem::path& outPath)
{
  void* SubFile;
  if (!SFileOpenFileEx(MPQ, archiveFile, 0, &SubFile)) {
    return false;
  }
  const uint32_t FileLength = SFileGetFileSize(SubFile, nullptr);
  bool success = false;

  if (FileLength > 0 && FileLength != 0xFFFFFFFF) {
    auto  SubFileData = new int8_t[FileLength];

#ifdef _WIN32
    unsigned long BytesRead = 0;
#else
    uint32_t BytesRead = 0;
#endif

    if (SFileReadFile(SubFile, SubFileData, FileLength, &BytesRead, nullptr)) {
      if (FileWrite(outPath, reinterpret_cast<uint8_t*>(SubFileData), BytesRead)) {
        Print("[AURA] extracted " + string(archiveFile) + " to [" + outPath.string() + "]");
        success = true;
      } else {
        Print("[AURA] warning - unable to save extracted " + string(archiveFile) + " to [" + outPath.string() + "]");
      }
    } else {
      Print("[AURA] warning - unable to extract " + string(archiveFile) + " from MPQ file");
    }

    delete[] SubFileData;
  }

  SFileCloseFile(SubFile);
  return success;
}

#ifdef _WIN32
optional<string> MaybeReadRegistryKey(const char* keyName)
{
  optional<string> result;
  HKEY hKey;
  DWORD dwType, dwSize;
  char szValue[1024];

  if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Blizzard Entertainment\\Warcraft III", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    dwSize = sizeof(szValue);
    // Query the value of the desired registry entry
    if (RegQueryValueExA(hKey, keyName, nullptr, &dwType, (LPBYTE)szValue, &dwSize) == ERROR_SUCCESS) {
      if (dwType == REG_SZ && 0 < dwSize && dwSize < 1024) {
        string installPath(szValue, dwSize - 1);
        result = installPath;
      }
    }
    // Close the key
    RegCloseKey(hKey);
  }
  return result;
}
#endif
