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

#ifndef AURA_OSUTIL_H_
#define AURA_OSUTIL_H_

#include "fileutil.h"

#ifdef _WIN32
std::optional<std::wstring> MaybeReadRegistry(const wchar_t* mainKey, const wchar_t* subKey);
std::optional<std::filesystem::path> MaybeReadRegistryPath(const wchar_t* mainKey, const wchar_t* subKey);
bool DeleteUserRegistryKey(const wchar_t* subKey);
bool CreateUserRegistryKey(const wchar_t* subKey, const wchar_t* valueName, const wchar_t* value);
std::optional<std::string> GetUserMultiPlayerName();
#endif

std::filesystem::path GetExePath();
std::filesystem::path GetExeDirectory();
PLATFORM_STRING_TYPE ReadPersistentPathEnvironment();
void SetPersistentPathEnvironment(const PLATFORM_STRING_TYPE&);
bool GetIsDirectoryInPath(const std::filesystem::path& nPath);
void AddDirectoryToPath(const std::filesystem::path& nPath);
void EnsureDirectoryInPath(const std::filesystem::path& nPath);

#endif // AURA_FILEUTIL_H_
