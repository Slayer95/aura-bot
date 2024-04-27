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

#ifndef AURA_FILEUTIL_H_
#define AURA_FILEUTIL_H_

#define FUZZY_SEARCH_MAX_RESULTS 5
#define FUZZY_SEARCH_MAX_DISTANCE 10

#ifdef _WIN32
#define PLATFORM_STRING_TYPE std::wstring
#else
#define PLATFORM_STRING_TYPE std::string
#endif

#include "includes.h"
#include <vector>
#include <set>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <cstdio>
#include <optional>
#include <ctime>

bool FileExists(const std::filesystem::path& file);
PLATFORM_STRING_TYPE GetFileName(const PLATFORM_STRING_TYPE& inputPath);
PLATFORM_STRING_TYPE GetFileExtension(const PLATFORM_STRING_TYPE& inputPath);
std::string PathToString(const std::filesystem::path& file);
std::vector<std::filesystem::path> FilesMatch(const std::filesystem::path& path, const std::vector<PLATFORM_STRING_TYPE>& extensionList);
std::string FileRead(const std::filesystem::path& file, size_t start, size_t length, size_t* byteSize);
std::string FileRead(const std::filesystem::path& file, size_t* byteSize);
bool FileWrite(const std::filesystem::path& file, const uint8_t* data, size_t length);
bool FileDelete(const std::filesystem::path& File);
std::optional<int64_t> GetMaybeModifiedTime(const std::filesystem::path& file);
std::filesystem::path GetExePath();
std::filesystem::path GetExeDirectory();
std::filesystem::path CaseInsensitiveFileExists(const std::filesystem::path& path, const std::string& file);
std::vector<std::pair<std::string, int>> FuzzySearchFiles(const std::filesystem::path& directory, const std::vector<PLATFORM_STRING_TYPE>& baseExtensions, const std::string& rawPattern);
bool OpenMPQArchive(void** MPQ, const std::filesystem::path& filePath);
void CloseMPQArchive(void* MPQ);
bool ExtractMPQFile(void* MPQ, const char* archivedFile, const std::filesystem::path& outPath);
#ifdef _WIN32
std::optional<std::wstring> MaybeReadRegistry(const wchar_t* mainKey, const wchar_t* subKey);
std::optional<std::filesystem::path> MaybeReadRegistryPath(const wchar_t* mainKey, const wchar_t* subKey);
bool DeleteUserRegistryKey(const wchar_t* subKey);
bool CreateUserRegistryKey(const wchar_t* subKey, const wchar_t* valueName, const wchar_t* value);
std::optional<std::string> GetUserMultiPlayerName();
#endif

#endif // AURA_FILEUTIL_H_
