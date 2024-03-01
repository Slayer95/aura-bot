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

#ifndef AURA_CONFIG_H_
#define AURA_CONFIG_H_

#include "fileutil.h"
#include "socket.h"

#include <map>
#include <string>
#include <vector>
#include <set>
#include <optional>

//
// CConfig
//

class CConfig
{
private:
  bool                               m_ErrorLast;
  bool                               m_CriticalError;
  std::map<std::string, std::string> m_CFG;
  std::set<std::string>              m_ValidKeys;

public:
  CConfig();
  ~CConfig();

  bool Read(const std::filesystem::path& file);
  bool Exists(const std::string& key);
  void Accept(const std::string& key);
  std::vector<std::string> GetInvalidKeys() const;
  inline bool GetErrorLast() const { return m_ErrorLast; };
  inline bool GetSuccess() const { return !m_CriticalError; };
  inline void FailIfErrorLast() {
    if (m_ErrorLast) m_CriticalError = true;
  };
  inline void SetFailed() { m_CriticalError = true; };

  std::string GetString(const std::string& key, const std::string& x);
  std::string GetString(const std::string& key, const uint32_t minLength, const uint32_t maxLength, const std::string& x);
  bool GetBool(const std::string& key, bool x);

  int32_t GetInt(const std::string& key, int32_t x);
  int32_t GetInt32(const std::string& key, int32_t x);
  uint32_t GetUint32(const std::string& key, uint32_t x);
  uint16_t GetUint16(const std::string& key, uint16_t x);
  uint8_t GetUint8(const std::string& key, uint8_t x);

  float GetFloat(const std::string& key, float x);
  uint8_t GetStringIndex(const std::string& key, const std::vector<std::string>& fromList, const uint8_t x);

  std::vector<std::string> GetList(const std::string& key, char separator, const std::vector<std::string> x);
  std::set<std::string> GetSet(const std::string& key, char separator, const std::set<std::string> x);

  std::vector<uint8_t> GetUint8Vector(const std::string& key, const uint32_t count, const std::vector<uint8_t>& x);
  std::vector<uint8_t> GetIPv4(const std::string& key, const std::vector<uint8_t>& x);
  std::set<std::string> GetIPStringSet(const std::string& key, char separator, const std::set<std::string> x);

  std::filesystem::path GetPath(const std::string &key, const std::filesystem::path &x);
  std::filesystem::path GetDirectory(const std::string &key, const std::filesystem::path &x);
  sockaddr_storage GetAddressOfType(const std::string &key, const uint8_t acceptMode, const std::string& x);
  sockaddr_storage GetAddressIPv4(const std::string &key, const std::string& x);
  sockaddr_storage GetAddressIPv6(const std::string &key, const std::string& x);
  sockaddr_storage GetAddress(const std::string &key, const std::string& x);
  std::vector<sockaddr_storage> GetAddressList(const std::string& key, char separator, const std::vector<std::string> x);

  std::optional<bool> GetMaybeBool(const std::string& key);
  std::optional<uint32_t> GetMaybeInt(const std::string& key);
  std::optional<sockaddr_storage> GetMaybeAddressOfType(const std::string& key, const uint8_t acceptMode);
  std::optional<sockaddr_storage> GetMaybeAddressIPv4(const std::string& key);
  std::optional<sockaddr_storage> GetMaybeAddressIPv6(const std::string& key);
  std::optional<sockaddr_storage> GetMaybeAddress(const std::string& key);
  std::optional<std::vector<uint8_t>> GetMaybeIPv4(const std::string& key);
  std::optional<std::filesystem::path> GetMaybePath(const std::string &key);

  void Set(const std::string& key, const std::string& x);
  void SetString(const std::string& key, const std::string& x);
  void SetBool(const std::string& key, const bool& x);
  void SetInt32(const std::string& key, const int32_t& x);
  void SetUint32(const std::string& key, const uint32_t& x);
  void SetUint16(const std::string& key, const uint16_t& x);
  void SetUint8(const std::string& key, const uint8_t& x);
  void SetFloat(const std::string& key, const float& x);
  void SetUint8Vector(const std::string& key, const std::vector<std::uint8_t>& x);
  std::vector<uint8_t> Export() const;

  static std::string ReadString(const std::filesystem::path& file, const std::string& key);
};

#endif // AURA_CONFIG_H_
