/*

  Copyright [2025] [Leonardo Julca]

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

#include "game_stat.h"
#include "util.h"

using namespace std;

// GameStat

GameStat::GameStat()
: m_IsValid(false),
  m_GameFlags(0),
  m_MapWidth(0),
  m_MapHeight(0)
{
  m_MapScriptsBlizzHash.fill(0);
}

GameStat::GameStat(uint32_t gameFlags, uint16_t mapWidth, uint16_t mapHeight, const string& mapPath, const string& hostName, const array<uint8_t, 4>& mapBlizzHash, const optional<array<uint8_t, 20>>& mapSHA1)
: m_IsValid(true),
  m_GameFlags(gameFlags),
  m_MapWidth(mapWidth),
  m_MapHeight(mapHeight),
  m_MapPath(mapPath),
  m_HostName(hostName)
{
  m_MapScriptsBlizzHash.fill(0);
  copy_n(mapBlizzHash.begin(), 4, m_MapScriptsBlizzHash.begin());

  if (mapSHA1.has_value()) {
    m_MapScriptsSHA1.emplace();
    copy_n(mapSHA1->begin(), 4, m_MapScriptsSHA1->begin());
  }
}

GameStat::GameStat(const uint8_t* ptr, size_t size)
: m_IsValid(false),
  m_GameFlags(0),
  m_MapWidth(0),
  m_MapHeight(0)
{
  const uint8_t* dataEnd = ptr + size;
  const uint8_t* cursorStart = ptr;
  const uint8_t* cursorEnd = cursorStart;

  m_MapScriptsBlizzHash.fill(0);

  cursorEnd = cursorStart + 4;
  if (cursorEnd > dataEnd) return;
  m_GameFlags = ByteArrayToUInt32(cursorStart, false);
  cursorStart = cursorEnd + 1;

  cursorEnd = cursorStart + 2;
  if (cursorEnd > dataEnd) return;
  m_MapWidth = ByteArrayToUInt16(cursorStart, false);
  cursorStart = cursorEnd;

  cursorEnd = cursorStart + 2;
  if (cursorEnd > dataEnd) return;
  m_MapHeight = ByteArrayToUInt16(cursorStart, false);
  cursorStart = cursorEnd;

  cursorEnd = cursorStart + 4;
  if (cursorEnd > dataEnd) return;
  copy_n(cursorStart, 4, m_MapScriptsBlizzHash.begin());
  cursorStart = cursorEnd;

  cursorEnd = FindNullDelimiterOrStart(cursorStart, dataEnd);
  if (cursorEnd == cursorStart) return;

  m_MapPath = GetStringAddressRange(cursorStart, cursorEnd);

  cursorStart = cursorEnd + 1;
  cursorEnd = FindNullDelimiterOrStart(cursorStart, dataEnd);
  if (cursorEnd == cursorStart) return;

  m_HostName = GetStringAddressRange(cursorStart, cursorEnd);

  cursorStart = cursorEnd + 2;
  cursorEnd = cursorStart + 20;

  // Since v1.23, savefiles for LAN games store an all-zeroes SHA1
  if (cursorEnd <= dataEnd && !IsAllZeroes(cursorStart, cursorEnd)) {
    m_MapScriptsSHA1.emplace();
    copy_n(cursorStart, 20, m_MapScriptsSHA1->begin());
  }

  m_IsValid = true;
}

GameStat::~GameStat()
{
}

vector<uint8_t> GameStat::Encode() const
{
  vector<uint8_t> encoded;
  encoded.reserve(13 + m_MapPath.size() + 1 + m_HostName.size() + 2 + (m_MapScriptsSHA1.has_value() ? 20 : 0));
  AppendByteArray(encoded, m_GameFlags, false);
  encoded.push_back(0);
  AppendByteArray(encoded, m_MapWidth, false);
  AppendByteArray(encoded, m_MapHeight, false);
  AppendByteArrayFast(encoded, m_MapScriptsBlizzHash);
  AppendByteArrayString(encoded, m_MapPath, true);
  AppendByteArrayString(encoded, m_HostName, true);
  encoded.push_back(0);
  if (m_MapScriptsSHA1.has_value()) {
    AppendByteArrayFast(encoded, *m_MapScriptsSHA1);
  }
  return EncodeStatString(encoded);
}

string GameStat::GetMapClientFileName() const
{
  size_t LastSlash = m_MapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_MapPath;
  }
  return m_MapPath.substr(LastSlash + 1);
}