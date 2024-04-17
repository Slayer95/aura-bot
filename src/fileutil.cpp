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

#include <algorithm>
#include <iostream>
#include <bitset>
#include <set>
#include <optional>
#include <locale>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#pragma once
#include <windows.h>
#define stat _stat
#else
#include <sys/stat.h>
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
  error_code ec;
  return filesystem::exists(file, ec);
}

#ifdef _WIN32
wstring GetFileName(const wstring& inputPath) {
  filesystem::path filePath = inputPath;
  return filePath.filename().wstring();
}

wstring GetFileExtension(const wstring& inputPath) {
  wstring fileName = GetFileName(inputPath);
  size_t extIndex = fileName.find_last_of(L".");
  if (extIndex == wstring::npos) return wstring();
  wstring extension = fileName.substr(extIndex);
  std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](wchar_t c) {
    return std::tolower(c);
  });
  return extension;
}
#else
string GetFileName(const string& inputPath) {
  filesystem::path filePath = inputPath;
  return filePath.filename().string();
}

string GetFileExtension(const string& inputPath) {
  string fileName = GetFileName(inputPath);
  size_t extIndex = fileName.find_last_of(".");
  if (extIndex == string::npos) return string();
  string extension = fileName.substr(extIndex);
  std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}
#endif

string PathToString(const filesystem::path& inputPath) {
#ifdef _WIN32
  if (inputPath.empty()) return string();
  wstring fromWide = inputPath.wstring();
  int size = WideCharToMultiByte(CP_UTF8, 0, &fromWide[0], (int)fromWide.size(), nullptr, 0, nullptr, nullptr);
  string output(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, &fromWide[0], (int)fromWide.size(), &output[0], size, nullptr, nullptr);
  return output;
#else
  return inputPath.string();
#endif
}

vector<filesystem::path> FilesMatch(const filesystem::path& path, const vector<PLATFORM_STRING_TYPE>& extensionList)
{
  set<PLATFORM_STRING_TYPE> extensions(extensionList.begin(), extensionList.end());
  vector<filesystem::path> Files;

#ifdef _WIN32
  WIN32_FIND_DATAW data;
  filesystem::path pattern = path / filesystem::path("*");
  HANDLE           handle = FindFirstFileW(pattern.wstring().c_str(), &data);
  memset(&data, 0, sizeof(WIN32_FIND_DATAW));

  while (handle != INVALID_HANDLE_VALUE) {
    wstring fileName(data.cFileName);
    transform(begin(fileName), end(fileName), begin(fileName), ::tolower);
    if (fileName != L"..") {
      if (extensions.empty() || extensions.find(GetFileExtension(fileName)) != extensions.end()) {
        Files.emplace_back(fileName);
      }
    }
    if (FindNextFileW(handle, &data) == FALSE)
      break;
  }

  FindClose(handle);
#else
  DIR* dir = opendir(path.c_str());

  if (dir == nullptr)
    return Files;

  struct dirent* dp = nullptr;

  while ((dp = readdir(dir)) != nullptr) {
    string fileName = string(dp->d_name);
    transform(begin(fileName), end(fileName), begin(fileName), ::tolower);
    if (fileName != "." && fileName != "..") {
      if (extensions.empty() || extensions.find(GetFileExtension(fileName)) != extensions.end()) {
        Files.emplace_back(dp->d_name);
      }
    }
  }

  closedir(dir);
#endif

  return Files;
}

string FileRead(const filesystem::path& file, size_t start, size_t length, size_t* byteSize)
{
  ifstream IS;
  IS.open(file.native().c_str(), ios::binary | ios::in);

  if (IS.fail())
  {
    Print("[UTIL] warning - unable to read file part [" + PathToString(file) + "]");
    return string();
  }

  // get length of file

  IS.seekg(0, ios::end);

  size_t FileLength = static_cast<long unsigned int>(IS.tellg());
  if (FileLength > 0x18000000) {
    Print("[UTIL] error - refusing to load huge file [" + PathToString(file) + "]");
    return string();
  }
  if (byteSize != nullptr) {
    (*byteSize) = FileLength;
  }
  if (start > FileLength) {
    IS.close();
    return string();
  }

  IS.seekg(start, ios::beg);

  // read data

  auto Buffer = new char[length];
  IS.read(Buffer, length);
  string BufferString = string(Buffer, static_cast<long unsigned int>(IS.gcount()));
  IS.close();
  delete[] Buffer;
  return BufferString;
}

string FileRead(const filesystem::path& file, size_t* byteSize)
{
  ifstream IS;
  IS.open(file.native().c_str(), ios::binary | ios::in);

  if (IS.fail())
  {
    Print("[UTIL] warning - unable to read file [" + PathToString(file) + "]");
    return string();
  }

  // get length of file

  IS.seekg(0, ios::end);
  size_t FileLength = static_cast<long unsigned int>(IS.tellg());
  if (FileLength > 0x18000000) {
    Print("[UTIL] error - refusing to load huge file [" + PathToString(file) + "]");
    return string();
  }
  if (byteSize != nullptr) {
    (*byteSize) = FileLength;
  }
  IS.seekg(0, ios::beg);

  // read data

  auto Buffer = new char[FileLength];
  IS.read(Buffer, FileLength);
  string BufferString = string(Buffer, static_cast<long unsigned int>(IS.gcount()));
  IS.close();
  delete[] Buffer;

  if (BufferString.size() == FileLength)
    return BufferString;
  else
    return string();
}

bool FileWrite(const filesystem::path& file, const uint8_t* data, size_t length)
{
  ofstream OS;
  OS.open(file.native().c_str(), ios::binary);

  if (OS.fail())
  {
    Print("[UTIL] warning - unable to write file [" + PathToString(file) + "]");
    return false;
  }

  // write data

  OS.write(reinterpret_cast<const char*>(data), length);
  OS.close();
  return true;
}

bool FileDelete(const filesystem::path& file)
{
  error_code e;
  bool result = filesystem::remove(file, e);
  
  if (e) {
    Print("[AURA] Cannot delete " + PathToString(file) + ". " + e.message());
  }

  return result;
}

optional<int64_t> GetMaybeModifiedTime(const filesystem::path& file)
{
  optional<int64_t> result;
  
  try {
    filesystem::file_time_type lastModifiedTime = filesystem::last_write_time(file);
    result = chrono::duration_cast<chrono::seconds>(lastModifiedTime.time_since_epoch()).count();
  } catch (...) {
  }
  return result;
}

filesystem::path GetExeDirectory()
{
  static filesystem::path Memoized;
  if (!Memoized.empty())
    return Memoized;

#ifdef _WIN32
  vector<wchar_t> buffer(2048);
  DWORD length = 0;
#else
  vector<char> buffer(2048);
  vector<char>::size_type length = 0;
#endif

  do {
    buffer.resize(buffer.size() * 2);
#ifdef _WIN32
    length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
#else
    length = readlink("/proc/self/exe", buffer.data(), buffer.size());
#endif
  } while ((buffer.size() <= 0xFFFF) && (buffer.size() - 1 <= static_cast<size_t>(length)));

  if (length == 0) {
    Print("Failed to retrieve Aura's directory.");
    return Memoized;
  }
  buffer.resize(length);

  filesystem::path executablePath(buffer.data());
  filesystem::path cwd;
  try {
    cwd = filesystem::current_path();
  } catch (...) {}
  if (!cwd.empty()) {
    NormalizeDirectory(cwd);
    cwd = cwd.parent_path();
  }

  bool cwdIsAncestor = cwd.empty();
  if (!cwdIsAncestor) {
    try {
      filesystem::path exeAncestor = executablePath;
      filesystem::path exeRoot = exeAncestor.root_path();
      while (exeAncestor != exeRoot) {
        exeAncestor = exeAncestor.parent_path();
        if (exeAncestor == cwd) {
          cwdIsAncestor = true;
          break;
        }
      }
    } catch (...) {}
  }

  if (cwdIsAncestor) {    
    Memoized = executablePath.parent_path().lexically_relative(cwd);
  } else {
    Memoized = executablePath.parent_path();
  }

  NormalizeDirectory(Memoized);
  return Memoized;
}

filesystem::path CaseInsensitiveFileExists(const filesystem::path& path, const string& file)
{
  std::string mutated_file = file;
  const size_t NumberOfCombinations = static_cast<size_t>(1) << mutated_file.size();

  for (size_t perm = 0; perm < NumberOfCombinations; ++perm) {
    std::bitset<64> bs(perm);
    std::transform(mutated_file.begin(), mutated_file.end(), mutated_file.begin(), ::tolower);

    for (size_t index = 0; index < bs.size() && index < mutated_file.size(); ++index) {
      if (bs[index]) {
        mutated_file[index] = static_cast<char>(::toupper(mutated_file[index]));
      }
    }

    filesystem::path testPath = path / filesystem::path(mutated_file);
    if (FileExists(testPath))
      return testPath;
  }

  return "";
}

vector<pair<string, int>> FuzzySearchFiles(const filesystem::path& directory, const vector<PLATFORM_STRING_TYPE>& baseExtensions, const string& rawPattern)
{
  // If the pattern has a valid extension, restrict the results to files with that extension.
#ifdef _WIN32
  wstring rawExtension = filesystem::path(rawPattern).wstring();
#else
  string rawExtension = filesystem::path(rawPattern).string();
#endif
  vector<PLATFORM_STRING_TYPE> extensions;
  if (!rawExtension.empty() && find(baseExtensions.begin(), baseExtensions.end(), rawExtension) != baseExtensions.end()) {
    extensions = vector<PLATFORM_STRING_TYPE>(1, rawExtension);
  } else {
    extensions = baseExtensions;
  }
  string fuzzyPattern = PreparePatternForFuzzySearch(rawPattern);
  vector<filesystem::path> folderContents = FilesMatch(directory, extensions);
  set<filesystem::path> filesSet(folderContents.begin(), folderContents.end());

  // 1. Try files that start with the given pattern (up to digits).
  //    These have triple weight.
  vector<pair<string, int>> inclusionMatches;
  for (const auto& mapName : filesSet) {
    string mapString = PathToString(mapName);
    if (mapName.empty()) continue;
    string cmpName = PreparePatternForFuzzySearch(mapString);
    size_t fuzzyPatternIndex = cmpName.find(fuzzyPattern);
    if (fuzzyPatternIndex == string::npos) {
      continue;
    }
    bool startsWithPattern = true;
    for (uint8_t i = 0; i < fuzzyPatternIndex; ++i) {
      if (!isdigit(mapString[i])) {
        startsWithPattern = false;
        break;
      }
    }
    if (startsWithPattern) {
      inclusionMatches.push_back(make_pair(mapString, static_cast<int>(mapString.length() - fuzzyPattern.length())));
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
  for (const auto& mapName : filesSet) {
    string mapString = PathToString(mapName);
    if (mapString.empty()) continue;
    string cmpName = RemoveNonAlphanumeric(mapString);
    transform(begin(cmpName), end(cmpName), begin(cmpName), ::tolower);
    if ((fuzzyPattern.size() <= cmpName.size() + maxDistance) && (cmpName.size() <= maxDistance + fuzzyPattern.size())) {
      string::size_type distance = GetLevenshteinDistance(fuzzyPattern, cmpName); // source to target
      if (distance <= maxDistance) {
        distances.emplace_back(mapString, static_cast<int>(distance * 3));
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
  uint32_t locale = SFileGetLocale();
  if (locale == 0) {
    Print("[AURA] loading MPQ file [" + PathToString(filePath) + "]");
  } else {
    Print("[AURA] loading MPQ file [" + PathToString(filePath) + "] using locale " + to_string(locale));
  }
  return SFileOpenArchive(filePath.native().c_str(), 0, MPQ_OPEN_FORCE_MPQ_V1 | STREAM_FLAG_READ_ONLY, MPQ);
}

void CloseMPQArchive(void* MPQ)
{
  SFileCloseArchive(MPQ);
}

bool ExtractMPQFile(void* MPQ, const char* archiveFile, const filesystem::path& outPath)
{
  SFileSetLocale(0);
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
        Print("[AURA] extracted " + string(archiveFile) + " to [" + PathToString(outPath) + "]");
        success = true;
      } else {
        Print("[AURA] warning - unable to save extracted " + string(archiveFile) + " to [" + PathToString(outPath) + "]");
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
optional<filesystem::path> MaybeReadPathFromRegistry(const wchar_t* name)
{
  optional<filesystem::path> result;
  HKEY hKey;
  LPCWSTR registryPath = L"SOFTWARE\\Blizzard Entertainment\\Warcraft III";
  LPCWSTR keyName = name;
  WCHAR buffer[1024];

  if (RegOpenKeyExW(HKEY_CURRENT_USER, registryPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD bufferSize = sizeof(buffer);
    DWORD valueType;
    // Query the value of the desired registry entry
    if (RegQueryValueExW(hKey, keyName, nullptr, &valueType, reinterpret_cast<BYTE*>(buffer), &bufferSize) == ERROR_SUCCESS) {
      if (valueType == REG_SZ && 0 < bufferSize && bufferSize < 1024) {
        buffer[bufferSize / sizeof(WCHAR)] = L'\0';
        filesystem::path installPath = wstring(buffer);
        result = installPath;
      }
    }
    // Close the key
    RegCloseKey(hKey);
  }
  return result;
}
#endif
