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

#ifndef AURA_GAME_HOST_H_
#define AURA_GAME_HOST_H_

#include "socket.h"
#include "includes.h"
#include "list.h"
#include "protocol/game_protocol.h"

//
// GameHost
//

struct GameHost
{
  uint16_t                  m_GameType;
  uint32_t                  m_GameStatus;
  uint32_t                  m_HostCounter;
  uint32_t                  m_MapFlags;
  uint16_t                  m_MapWidth;
  uint16_t                  m_MapHeight;
  std::array<uint8_t, 4>    m_MapScriptsBlizz;
  sockaddr_storage          m_HostAddress;
  std::string               m_GameName;
  std::string               m_GamePassWord;
  std::string               m_GameInfo; // WIP
  std::string               m_MapPath;
  std::string               m_HostName;

  GameHost();
	~GameHost();

  void SetGameType(uint16_t gameType);
  void SetAddress(const sockaddr_storage& address);
  void SetStatus(uint32_t status);
  void SetGameName(std::string_view gameName);
  void SetPassword(std::string_view passWord);
  bool SetBNETGameInfo(const std::string& gameInfo);

  [[nodiscard]] std::string GetIPString() const;
  [[nodiscard]] inline uint32_t GetHostCounter() const { return m_HostCounter; };
  [[nodiscard]] inline uint32_t GetMapFlags() const { return m_MapFlags; };
  [[nodiscard]] inline std::string_view GetGameName() const { return m_GameName; };
  [[nodiscard]] inline std::string_view GetHostName() const { return m_HostName; };
  [[nodiscard]] std::string GetHostDetails() const;
  [[nodiscard]] inline bool GetIsGProxy() const { return m_MapWidth == m_MapHeight && m_MapHeight == 1984; }
  [[nodiscard]] inline const std::array<uint8_t, 4>& GetMapScriptsBlizz() const { return m_MapScriptsBlizz; }
  [[nodiscard]] inline std::string_view GetMapClientPath() const { return m_MapPath; };
  [[nodiscard]] std::string GetMapClientFileName() const;
};

#endif // AURA_GAME_HOST_H_
