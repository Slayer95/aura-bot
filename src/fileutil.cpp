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

#include "fileutil.h"

#include <fstream>
#include <algorithm>
#include <iostream>
#include <bitset>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#pragma once
#include <windows.h>
#else
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <limits.h>
#endif

// unistd.h and limits.h

using namespace std;

bool FileExists(const filesystem::path& file)
{
  return filesystem::exists(file);
}

vector<string> FilesMatch(const filesystem::path& path, const string& pattern)
{
  vector<string> Files;

#ifdef WIN32
  WIN32_FIND_DATAA data;
  HANDLE           handle = FindFirstFileA((path.string() + "\\*").c_str(), &data);
  memset(&data, 0, sizeof(WIN32_FIND_DATAA));

  while (handle != INVALID_HANDLE_VALUE)
  {
    string Name = string(data.cFileName);
    transform(begin(Name), end(Name), begin(Name), ::tolower);

    if (Name == pattern)
    {
      Files.push_back(string(data.cFileName));
      break;
    }

    if (Name.find(pattern) != string::npos && Name != "..")
      Files.push_back(string(data.cFileName));

    if (FindNextFileA(handle, &data) == FALSE)
      break;
  }

  FindClose(handle);
#else
  DIR* dir = opendir(path.c_str());

  if (dir == nullptr)
    return Files;

  struct dirent* dp = nullptr;

  while ((dp = readdir(dir)) != nullptr)
  {
    string Name = string(dp->d_name);
    transform(begin(Name), end(Name), begin(Name), ::tolower);

    if (Name == pattern)
    {
      Files.emplace_back(dp->d_name);
      break;
    }

    if (Name.find(pattern) != string::npos && Name != "." && Name != "..")
      Files.emplace_back(dp->d_name);
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
  uint32_t FileLength = IS.tellg();
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
  uint32_t FileLength = IS.tellg();
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
#ifdef WIN32
  DWORD length = 0;
#else
  ssize_t length = 0;
#endif

  do {
    buffer.resize(buffer.size() * 2);
#ifdef WIN32
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
  const size_t NumberOfCombinations = std::pow(2, mutated_file.size());

  for (size_t perm = 0; perm < NumberOfCombinations; ++perm) {
    std::bitset<64> bs(perm);
    std::transform(mutated_file.begin(), mutated_file.end(), mutated_file.begin(), ::tolower);

    for (size_t index = 0; index < bs.size() && index < mutated_file.size(); ++index) {
      if (bs[index])
        mutated_file[index] = ::toupper(mutated_file[index]);
    }

    filesystem::path testPath = path / filesystem::path(mutated_file);
    if (FileExists(testPath))
      return testPath;
  }

  return "";
}
