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
#include "net.h"

using namespace std;

//
// GameHost
//

GameHost::GameHost()
 : m_Identifier(0),
   m_EntryKey(0)
{
  memset(&m_Address, 0, sizeof(sockaddr_storage));
}

GameHost::GameHost(const sockaddr_storage& address, uint32_t identifier)
 : m_Address(address),
   m_Identifier(identifier),
   m_EntryKey(0)
{
}

GameHost::GameHost(const sockaddr_storage& address, uint32_t identifier, uint32_t entryKey)
 : m_Address(address),
   m_Identifier(identifier),
   m_EntryKey(entryKey)
{
}

GameHost::GameHost(const string& hexInput)
 : m_Identifier(0),
   m_EntryKey(0)
{
  memset(&m_Address, 0, sizeof(sockaddr_storage));

  string::size_type portStart = hexInput.find(':', 0);
  if (portStart == string::npos) return;
  string::size_type idStart = hexInput.find('#', portStart);
  if (idStart == string::npos) return;
  string rawAddress = hexInput.substr(0, portStart);
  if (rawAddress.length() < 7) return;
  string rawPort = hexInput.substr(portStart + 1, idStart - (portStart + 1));
  if (rawPort.empty()) return;
  string::size_type idEnd = hexInput.find(':', idStart);
  string rawId, rawEntryKey;
  if (idEnd == string::npos) {
    rawId = hexInput.substr(idStart + 1);
  } else {
    rawId = hexInput.substr(idStart + 1, idEnd - (idStart + 1));
    rawEntryKey = hexInput.substr(idEnd + 1);
  }
  if (rawId.empty()) return;
  optional<sockaddr_storage> maybeAddress;
  if (rawAddress[0] == '[' && rawAddress[rawAddress.length() - 1] == ']') {
    maybeAddress = CNet::ParseAddress(rawAddress.substr(1, rawAddress.length() - 2), ACCEPT_IPV4);
  } else {
    maybeAddress = CNet::ParseAddress(rawAddress, ACCEPT_IPV4);
  }
  if (!maybeAddress.has_value()) return;
  optional<uint16_t> maybeGamePort = ParseUint16(rawPort);
  if (!maybeGamePort.has_value()) {
    return;
  }
  uint32_t entryKey = 0;
  optional<uint32_t> maybeId = ParseUint32Hex(rawId);
  if (!maybeId.has_value()) {
    return;
  }
  if (!rawEntryKey.empty()) {
    optional<uint32_t> maybeEntryKey = ParseUint32Hex(rawEntryKey);
    if (!maybeEntryKey.has_value()) {
      return;
    }
    entryKey = maybeEntryKey.value();
  }

  SetAddressPort(&(maybeAddress.value()), *maybeGamePort);

  memcpy(&m_Address, &(maybeAddress.value()), sizeof(sockaddr_storage));
  m_Identifier = *maybeId;
  m_EntryKey = entryKey;
}

GameHost::~GameHost()
{
}

optional<GameHost> GameHost::Parse(const string& hexInput)
{
  optional<GameHost> maybeGameHost;
  maybeGameHost.emplace(hexInput);
  if (maybeGameHost->GetAddress().ss_family == 0) {
    maybeGameHost.reset();
  }
  return maybeGameHost;
}

//
// NetworkGameInfo
//

void NetworkGameInfo::SetAddress(const sockaddr_storage& address)
{
  m_Host.SetAddress(address);
}

void NetworkGameInfo::SetGameType(uint16_t gameType)
{
  m_Info.m_GameType = gameType;
}

void NetworkGameInfo::SetStatus(uint32_t status)
{
  /* open = 4, full = 6, started = 0xe, done = 0xc */
  m_Info.m_GameStatus = status;
}

void NetworkGameInfo::SetGameName(string_view gameName)
{
  m_GameName = gameName;
}

void NetworkGameInfo::SetPassword(string_view passWord)
{
  m_GamePassWord = passWord;
}

bool NetworkGameInfo::SetBNETGameInfo(const string& gameStat)
{
  size_t size = gameStat.size();
  if (size < 10) return false;
  const uint8_t* infoEnd = reinterpret_cast<const uint8_t*>(gameStat.c_str()) + size;

  array<uint8_t, 8> hostCounterRaw;
  hostCounterRaw.fill(0);
  copy_n(gameStat.begin() + 1, 8, hostCounterRaw.begin());
  m_Host.SetIdentifier(ASCIIHexToNum(hostCounterRaw, true));
  
  size_t cursor = 9;
  const uint8_t* encStatStringStart = reinterpret_cast<const uint8_t*>(gameStat.c_str()) + cursor;
  const uint8_t* encStatStringEnd = FindNullDelimiterOrEnd(encStatStringStart, infoEnd);
  string encStatString = GetStringAddressRange(encStatStringStart, encStatStringEnd);
  vector<uint8_t> statData = DecodeStatString(encStatString);
  
  if (statData.size() < 14) {
    //Print("[BNETPROTO] Stat string cannot be read. Encoded: <" + encStatString + ">");
    return false;
  }

  m_Info.m_GameFlags = ByteArrayToUInt32(statData, false, 0);
  m_Info.m_MapWidth = ByteArrayToUInt16(statData, false, 5);
  m_Info.m_MapHeight = ByteArrayToUInt16(statData, false, 7);
  copy_n(statData.begin() + 9, 4, m_Info.m_MapScriptsBlizz.begin());

  size_t cursorStart = 13;
  size_t cursorEnd = FindNullDelimiterOrStart(statData, cursorStart);
  if (cursorEnd == cursorStart) {
    //Print("[BNETPROTO] Failed to read map path");
    return false;
  }
  m_Info.m_MapPath = string(statData.begin() + cursorStart, statData.begin() + cursorEnd);
  //Print("[BNETPROTO] Map path: <" + m_Info.m_MapPath + ">");

  cursorStart = cursorEnd + 1;
  cursorEnd = FindNullDelimiterOrStart(statData, cursorStart);
  if (cursorEnd == cursorStart) {
    //Print("[BNETPROTO] Failed to read host name");
    return false;
  }
  m_HostName = string(statData.begin() + cursorStart, statData.begin() + cursorEnd);
  //Print("[BNETPROTO] Host: <" + m_HostName + ">");
  //Print("[BNETPROTO] Dimensions: " + to_string(m_Info.m_MapWidth) + "x" + to_string(m_Info.m_MapHeight));
  //Print("[BNETPROTO] Blizz Hash: <" + ByteArrayToDecString(m_Info.m_MapScriptsBlizz) + ">");
  return true;
}

string NetworkGameInfo::GetIPString() const
{
  return AddressToStringStrict(GetAddress());
}

string NetworkGameInfo::GetHostDetails() const
{
  return "[" + GetIPString() + "]:" + to_string(GetAddressPort(&(GetAddress()))) + "#" + ToHexString(GetIdentifier()) + ":0"/* + ToHexString(GetEntryKey())*/;
}

string NetworkGameInfo::GetMapClientFileName() const
{
  size_t LastSlash = m_Info.m_MapPath.rfind('\\');
  if (LastSlash == string::npos) {
    return m_Info.m_MapPath;
  }
  return m_Info.m_MapPath.substr(LastSlash + 1);
}