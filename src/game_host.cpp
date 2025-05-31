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

#include "game_host.h"
#include "game.h"
#include "game_user.h"

using namespace std;

//
// GameHost
//

GameHost::GameHost()
 : m_GameType(0),
   m_GameStatus(0),
   m_HostCounter(0),
   m_MapFlags(0),
   m_MapWidth(0),
   m_MapHeight(0)
{
  memset(&m_HostAddress, 0, sizeof(sockaddr_storage));
  m_MapScriptsBlizz.fill(0);
}

GameHost::~GameHost()
{
}

void GameHost::SetGameType(uint16_t gameType)
{
  m_GameType = gameType;
}

void GameHost::SetAddress(const sockaddr_storage& address)
{
  m_HostAddress = address;
}

void GameHost::SetStatus(uint32_t status)
{
  /* open = 4, full = 6, started = 0xe, done = 0xc */
  m_GameStatus = status;
}

void GameHost::SetGameName(string_view gameName)
{
  m_GameName = gameName;
}

void GameHost::SetPassword(string_view passWord)
{
  m_GamePassWord = passWord;
}

bool GameHost::SetBNETGameInfo(const string& gameInfo)
{
  m_GameInfo = gameInfo;

  size_t size = gameInfo.size();
  if (size < 10) return false;
  const uint8_t* infoEnd = reinterpret_cast<const uint8_t*>(gameInfo.c_str()) + size;

  array<uint8_t, 8> hostCounterRaw;
  hostCounterRaw.fill(0);
  copy_n(gameInfo.begin() + 1, 8, hostCounterRaw.begin());
  m_HostCounter = ASCIIHexToNum(hostCounterRaw, true);
  
  size_t cursor = 9;
  const uint8_t* encStatStringStart = reinterpret_cast<const uint8_t*>(gameInfo.c_str()) + cursor;
  const uint8_t* encStatStringEnd = FindNullDelimiterOrEnd(encStatStringStart, infoEnd);
  string encStatString = GetStringAddressRange(encStatStringStart, encStatStringEnd);
  vector<uint8_t> statData = DecodeStatString(encStatString);
  
  if (statData.size() < 14) {
    //Print("[BNETPROTO] Stat string cannot be read. Encoded: <" + encStatString + ">");
    return false;
  }

  m_MapFlags = ByteArrayToUInt32(statData, false, 0);
  m_MapWidth = ByteArrayToUInt16(statData, false, 5);
  m_MapHeight = ByteArrayToUInt16(statData, false, 7);
  copy_n(statData.begin() + 9, 4, m_MapScriptsBlizz.begin());

  size_t cursorStart = 13;
  size_t cursorEnd = FindNullDelimiterOrStart(statData, cursorStart);
  if (cursorEnd == cursorStart) {
    //Print("[BNETPROTO] Failed to read map path");
    return false;
  }
  m_MapPath = string(statData.begin() + cursorStart, statData.begin() + cursorEnd);
  //Print("[BNETPROTO] Map path: <" + m_MapPath + ">");

  cursorStart = cursorEnd + 1;
  cursorEnd = FindNullDelimiterOrStart(statData, cursorStart);
  if (cursorEnd == cursorStart) {
    //Print("[BNETPROTO] Failed to read host name");
    return false;
  }
  m_HostName = string(statData.begin() + cursorStart, statData.begin() + cursorEnd);
  //Print("[BNETPROTO] Host: <" + m_HostName + ">");
  //Print("[BNETPROTO] Dimensions: " + to_string(m_MapWidth) + "x" + to_string(m_MapHeight));
  //Print("[BNETPROTO] Blizz Hash: <" + ByteArrayToDecString(m_MapScriptsBlizz) + ">");
  return true;
}

string GameHost::GetIPString() const
{
  return AddressToStringStrict(m_HostAddress);
}

string GameHost::GetHostDetails() const
{
  return "[" + GetIPString() + "]:" + to_string(GetAddressPort(&m_HostAddress)) + "#" + ToHexString(GetHostCounter()) + ":0"/* + ToHexString(GetEntryKey())*/;
}

string GameHost::GetMapClientFileName() const
{
  size_t LastSlash = m_MapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_MapPath;
  }
  return m_MapPath.substr(LastSlash + 1);
}