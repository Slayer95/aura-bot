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

#ifndef AURA_CONFIG_H_
#define AURA_CONFIG_H_

#include <map>
#include <string>
#include <vector>
#include <set>

//
// CConfig
//

class CConfig
{
private:
  std::map<std::string, std::string> m_CFG;

public:
  CConfig();
  ~CConfig();

  bool Read(const std::string& file);
  bool Exists(const std::string& key);

  std::string GetString(const std::string& key, const std::string& x);
  std::string GetString(const std::string& key, const uint32_t minLength, const uint32_t maxLength, const std::string& x);
  bool GetBool(const std::string& key, bool x);
  int32_t GetInt(const std::string& key, int32_t x);
  float GetFloat(const std::string& key, float x);
  std::vector<std::string> GetList(const std::string& key, char separator, std::vector<std::string> x);
  std::set<std::string> GetSet(const std::string& key, char separator, std::set<std::string> x);
  std::vector<uint8_t> GetUint8Vector(const std::string& key, const uint32_t count, const std::vector<uint8_t>& x);
  std::vector<uint8_t> GetIPv4(const std::string& key, const std::vector<uint8_t>& x);

  void Set(const std::string& key, const std::string& x);
  void SetString(const std::string& key, const std::string& x);
  void SetBool(const std::string& key, const bool& x);
  void SetInt(const std::string& key, const int& x);
  void SetFloat(const std::string& key, const float& x);
  void SetUint8Vector(const std::string& key, const std::vector<std::uint8_t>& x);
  std::vector<uint8_t> Export();

  static std::string ReadString(const std::string& file, const std::string& key);
};

#endif // AURA_CONFIG_H_
