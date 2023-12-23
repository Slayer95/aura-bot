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

void CConfig::Read(const string& file)
{
  ifstream in;
  in.open(file.c_str());

  if (in.fail())
    Print("[CONFIG] warning - unable to read file [" + file + "]");
  else
  {
    Print("[CONFIG] loading file [" + file + "]");
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

      if (Split == string::npos)
        continue;

      string::size_type KeyStart   = Line.find_first_not_of(' ');
      string::size_type KeyEnd     = Line.find(' ', KeyStart);
      string::size_type ValueStart = Line.find_first_not_of(' ', Split + 1);
      string::size_type ValueEnd   = Line.size();

      if (ValueStart != string::npos)
        m_CFG[Line.substr(KeyStart, KeyEnd - KeyStart)] = Line.substr(ValueStart, ValueEnd - ValueStart);
    }

    in.close();
  }
}

bool CConfig::Exists(const string& key)
{
  return m_CFG.find(key) != end(m_CFG);
}

int32_t CConfig::GetInt(const string& key, int32_t x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;
  else
    return atoi(m_CFG[key].c_str());
}

string CConfig::GetString(const string& key, const string& x)
{
  if (m_CFG.find(key) == end(m_CFG))
    return x;
  else
    return m_CFG[key];
}

void CConfig::Set(const string& key, const string& x)
{
  m_CFG[key] = x;
}

void CConfig::SetString(const string& key, const string& x)
{
  m_CFG[key] = x;
}

void CConfig::SetInt(const string& key, const int& x)
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
    SS << (it->first + "=" + it->second + "\n");
  }

  std::string str = SS.str();
  std::vector<uint8_t> bytes(str.begin(), str.end());
  return bytes;
}
