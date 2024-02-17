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

#include "aura.h"
#include "config.h"
#include "includes.h"
#include "util.h"

#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>

using namespace std;

//
// CConfig
//

CConfig::CConfig()
{
}

CConfig::~CConfig() = default;

bool CConfig::Read(const filesystem::path& file)
{
  ifstream in;
  in.open(file.c_str(), ios::in);

  if (in.fail()) {
    Print("[CONFIG] warning - unable to read file [" + file.string() + "]");
    return false;
  }

  Print("[CONFIG] loading file [" + file.string() + "]");

  string Line;

  while (!in.eof())
  {
    getline(in, Line);

    // ignore blank lines and comments

    if (Line.empty() || Line[0] == '#' || Line == "\n")
      continue;

    // remove newlines and partial newlines to help fix issues with Windows formatted config files on Linux systems

    Line.erase(remove(begin(Line), end(Line), '\r'), end(Line));
    Line.erase(remove(begin(Line), end(Line), '\n'), end(Line));

    string::size_type Split = Line.find('=');

    if (Split == string::npos || Split == 0)
      continue;

    string::size_type KeyStart   = Line.find_first_not_of(' ');
    string::size_type KeyEnd     = Line.find_last_not_of(' ', Split - 1) + 1;
    string::size_type ValueStart = Line.find_first_not_of(' ', Split + 1);
    string::size_type ValueEnd   = Line.find_last_not_of(' ') + 1;

    if (ValueStart == string::npos)
      continue;

    m_CFG[Line.substr(KeyStart, KeyEnd - KeyStart)] = Line.substr(ValueStart, ValueEnd - ValueStart);
  }

  in.close();
  return true;
}

bool CConfig::Exists(const string& key)
{
  return m_CFG.find(key) != end(m_CFG);
}

string CConfig::GetString(const string& key, const string& x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  return m_CFG[key];
}

string CConfig::GetString(const string& key, const uint32_t minLength, const uint32_t maxLength, const string& x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  if (m_CFG[key].length() < minLength)
    return x;

  if (m_CFG[key].length() > maxLength)
    return x;

  return m_CFG[key];
}

bool CConfig::GetBool(const string& key, bool x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  if (m_CFG[key] == "0" || m_CFG[key] == "no")
    return false;
  if (m_CFG[key] == "1" || m_CFG[key] == "yes")
    return true;
  return x;
}

int32_t CConfig::GetInt(const string& key, int32_t x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  int32_t Value = x;
  try {
    Value = atoi(m_CFG[key].c_str());
  } catch (...) {}

  return Value;
}

float CConfig::GetFloat(const string& key, float x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  float Value = x;
  try {
    Value = stof(m_CFG[key].c_str());
  } catch (...) {}

  return Value;
}

vector<string> CConfig::GetList(const string& key, char separator, vector<string> x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  vector<string> Output;
  stringstream ss(m_CFG[key]);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.length() > 0) {
      Output.push_back(element);
    }
  }
  return Output;
}

set<string> CConfig::GetSet(const string& key, char separator, set<string> x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  set<string> Output;
  stringstream ss(m_CFG[key]);
  while (ss.good()) {
    string element;
    getline(ss, element, separator);
    if (element.length() > 0) {
      Output.insert(element);
    }
  }
  return Output;
}

vector<uint8_t> CConfig::GetUint8Vector(const string& key, uint32_t count, const std::vector<uint8_t> &x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  vector<uint8_t> Output = ExtractNumbers(m_CFG[key], count);
  if (Output.size() != count)
    return x;

  return Output;
}

vector<uint8_t> CConfig::GetIPv4(const string& key, const vector<uint8_t> &x) {
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  vector<uint8_t> Output = ExtractIPv4(m_CFG[key]);
  if (Output.empty())
    return x;

  return Output;
}

filesystem::path CConfig::GetPath(const string &key, const filesystem::path &x) {
  if (m_CFG.find(key) == end(m_CFG))
    return x;

  filesystem::path value = m_CFG[key];
  if (value.is_absolute()) return value;
  return filesystem::path(GetExeDirectory() / value).lexically_normal();
}

filesystem::path CConfig::GetDirectory(const string &key, const filesystem::path &x) {
  if (m_CFG.find(key) == end(m_CFG))
    return x.empty() ? GetExeDirectory() : x;

  filesystem::path value = m_CFG[key];
  if (value.is_absolute()) return value;
  return filesystem::path(GetExeDirectory() / value).lexically_normal();
}

optional<bool> CConfig::GetMaybeBool(const string& key)
{
  optional<bool> result;

  if (m_CFG.find(key) == end(m_CFG))
    return result;

  if (m_CFG[key] == "0" || m_CFG[key] == "no")
    result = false;
  if (m_CFG[key] == "1" || m_CFG[key] == "yes")
    result = true;

  return result;
}

optional<uint32_t> CConfig::GetMaybeInt(const string& key)
{
  optional<uint32_t> result;

  if (m_CFG.find(key) == end(m_CFG))
    return result;

  try {
    result = atoi(m_CFG[key].c_str());
  } catch (...) {}

  return result;
}

optional<filesystem::path> CConfig::GetMaybePath(const string &key) {
  optional<filesystem::path> result;

  if (m_CFG.find(key) == end(m_CFG))
    return result;

  result = m_CFG[key];
  if (result.value().is_absolute()) {
    return result;
  }
  result = filesystem::path(GetExeDirectory() / result.value()).lexically_normal();
  return result;
}

void CConfig::Set(const string& key, const string& x)
{
  m_CFG[key] = x;
}

void CConfig::SetString(const string& key, const string& x)
{
  m_CFG[key] = x;
}

void CConfig::SetBool(const string& key, const bool& x)
{
  m_CFG[key] = x ? "1" : "0";
}

void CConfig::SetInt(const string& key, const int& x)
{
  m_CFG[key] = to_string(x);
}

void CConfig::SetFloat(const string& key, const float& x)
{
  m_CFG[key] = to_string(x);
}

void CConfig::SetUint8Vector(const string& key, const std::vector<std::uint8_t> &x)
{
  m_CFG[key] = ByteArrayToDecString(x);
}

std::vector<uint8_t> CConfig::Export()
{
  std::ostringstream SS;
  for (auto it = m_CFG.begin(); it != m_CFG.end(); ++it) {
    SS << (it->first + " =" + it->second + "\n");
  }

  std::string str = SS.str();
  std::vector<uint8_t> bytes(str.begin(), str.end());
  return bytes;
}

std::string CConfig::ReadString(const std::filesystem::path& file, const std::string& key)
{
  std::string Output;
  ifstream in;
  in.open(file.c_str(), ios::in);

  if (in.fail())
    return Output;

  string Line;

  while (!in.eof()) {
    getline(in, Line);

    // ignore blank lines and comments

    if (Line.empty() || Line[0] == '#' || Line == "\n")
      continue;

    // remove newlines and partial newlines to help fix issues with Windows formatted config files on Linux systems

    Line.erase(remove(begin(Line), end(Line), '\r'), end(Line));
    Line.erase(remove(begin(Line), end(Line), '\n'), end(Line));

    string::size_type Split = Line.find('=');

    if (Split == string::npos || Split == 0)
      continue;

    string::size_type KeyStart   = Line.find_first_not_of(' ');
    string::size_type KeyEnd     = Line.find_last_not_of(' ', Split - 1) + 1;
    string::size_type ValueStart = Line.find_first_not_of(' ', Split + 1);
    string::size_type ValueEnd   = Line.find_last_not_of(' ') + 1;

    if (ValueStart == string::npos)
      continue;

    if (Line.substr(KeyStart, KeyEnd - KeyStart) == key) {
      Output = Line.substr(ValueStart, ValueEnd - ValueStart);
      break;
    }
  }

  in.close();
  return Output;
}

