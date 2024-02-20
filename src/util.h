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

#ifndef AURA_UTIL_H_
#define AURA_UTIL_H_

#include <string>
#include <cstdint>
#include <vector>
#include <sstream>
#include <iomanip>
#include <regex>
#include <filesystem>

#undef min

#pragma once

inline std::string ToHexString(uint32_t i)
{
  std::string       result;
  std::stringstream SS;
  SS << std::hex << i;
  SS >> result;
  return result;
}

inline std::string ToFormattedString(const double d, const uint8_t precision = 2)
{
  std::ostringstream out;
  out << std::fixed << std::setprecision(precision) << d;
  return out.str();
}

inline std::vector<uint8_t> CreateByteArray(const uint8_t* a, const int32_t size)
{
  if (size < 1)
    return std::vector<uint8_t>();

  return std::vector<uint8_t>(a, a + size);
}

inline std::vector<uint8_t> CreateByteArray(const uint8_t c)
{
  return std::vector<uint8_t>{c};
}

inline std::vector<uint8_t> CreateByteArray(const uint16_t i, bool reverse)
{
  if (!reverse)
    return std::vector<uint8_t>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8)};
  else
    return std::vector<uint8_t>{static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

inline std::vector<uint8_t> CreateByteArray(const uint32_t i, bool reverse)
{
  if (!reverse)
    return std::vector<uint8_t>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)};
  else
    return std::vector<uint8_t>{static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

inline std::vector<uint8_t> CreateByteArray(const int64_t i, bool reverse)
{
  if (!reverse)
    return std::vector<uint8_t>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)};
  else
    return std::vector<uint8_t>{static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

inline uint16_t ByteArrayToUInt16(const std::vector<uint8_t>& b, bool reverse, const uint32_t start = 0)
{
  if (b.size() < start + 2)
    return 0;

  if (!reverse)
    return static_cast<uint16_t>(b[start + 1] << 8 | b[start]);
  else
    return static_cast<uint16_t>(b[start] << 8 | b[start + 1]);
}

inline uint32_t ByteArrayToUInt32(const std::vector<uint8_t>& b, bool reverse, const uint32_t start = 0)
{
  if (b.size() < start + 4)
    return 0;

  if (!reverse)
    return static_cast<uint32_t>(b[start + 3] << 24 | b[start + 2] << 16 | b[start + 1] << 8 | b[start]);
  else
    return static_cast<uint32_t>(b[start] << 24 | b[start + 1] << 16 | b[start + 2] << 8 | b[start + 3]);
}

inline std::string ByteArrayToDecString(const std::vector<uint8_t>& b)
{
  if (b.empty())
    return std::string();

  std::string result = std::to_string(b[0]);

  for (auto i = cbegin(b) + 1; i != cend(b); ++i)
    result += " " + std::to_string(*i);

  return result;
}

inline std::string ByteArrayToHexString(const std::vector<uint8_t>& b)
{
  if (b.empty())
    return std::string();

  std::string result = ToHexString(b[0]);

  for (auto i = cbegin(b) + 1; i != cend(b); ++i)
  {
    if (*i < 0x10)
      result += " 0" + ToHexString(*i);
    else
      result += " " + ToHexString(*i);
  }

  return result;
}

inline void AppendByteArray(std::vector<uint8_t>& b, const std::vector<uint8_t>& append)
{
  b.insert(end(b), begin(append), end(append));
}

inline void AppendByteArrayFast(std::vector<uint8_t>& b, const std::vector<uint8_t>& append)
{
  b.insert(end(b), begin(append), end(append));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint8_t* a, const int32_t size)
{
  AppendByteArray(b, CreateByteArray(a, size));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const std::string& append, bool terminator = true)
{
  // append the std::string plus a null terminator

  b.insert(end(b), begin(append), end(append));

  if (terminator)
    b.push_back(0);
}

inline void AppendByteArrayFast(std::vector<uint8_t>& b, const std::string& append, bool terminator = true)
{
  // append the std::string plus a null terminator

  b.insert(end(b), begin(append), end(append));

  if (terminator)
    b.push_back(0);
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint16_t i, bool reverse)
{
  AppendByteArray(b, CreateByteArray(i, reverse));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint32_t i, bool reverse)
{
  AppendByteArray(b, CreateByteArray(i, reverse));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const int64_t i, bool reverse)
{
  AppendByteArray(b, CreateByteArray(i, reverse));
}

inline std::vector<uint8_t> ExtractCString(const std::vector<uint8_t>& b, const uint32_t start)
{
  // start searching the byte array at position 'start' for the first null value
  // if found, return the subarray from 'start' to the null value but not including the null value

  if (start < b.size())
  {
    for (uint32_t i = start; i < b.size(); ++i)
    {
      if (b[i] == 0)
        return std::vector<uint8_t>(begin(b) + start, begin(b) + i);
    }

    // no null value found, return the rest of the byte array

    return std::vector<uint8_t>(begin(b) + start, end(b));
  }

  return std::vector<uint8_t>();
}

inline uint8_t ExtractHex(const std::vector<uint8_t>& b, const uint32_t start, bool reverse)
{
  // consider the byte array to contain a 2 character ASCII encoded hex value at b[start] and b[start + 1] e.g. "FF"
  // extract it as a single decoded byte

  if (start + 1 < b.size())
  {
    uint8_t c = 0;
    std::string temp = std::string(begin(b) + start, begin(b) + start + 2);

    if (reverse)
      temp = std::string(temp.rend(), temp.rbegin());

    std::stringstream SS;
    SS << temp;
    SS >> std::hex >> c;
    return c;
  }

  return 0;
}

inline std::vector<uint8_t> ExtractNumbers(const std::string& s, const uint32_t count)
{
  // consider the std::string to contain a bytearray in dec-text form, e.g. "52 99 128 1"

  std::vector<uint8_t> result;
  uint32_t             c;
  std::stringstream    SS;
  SS << s;

  for (uint32_t i = 0; i < count; ++i)
  {
    if (SS.eof())
      break;

    SS >> c;

    if (SS.fail() || c > 0xFF)
      break;

    result.push_back(static_cast<uint8_t>(c));
  }

  return result;
}

inline std::vector<uint8_t> ExtractHexNumbers(const std::string& s)
{
  // consider the std::string to contain a bytearray in hex-text form, e.g. "4e 17 b7 e6"

  std::vector<uint8_t> result;
  uint32_t             c;
  std::stringstream    SS;
  SS << s;

  while (!SS.eof())
  {
    SS >> std::hex >> c;
    if (c > 0xFF)
      break;

    result.push_back(static_cast<uint8_t>(c));
  }

  return result;
}

inline std::vector<uint8_t> ExtractIPv4(const std::string& s)
{
  std::vector<uint8_t> Output;
  std::stringstream ss(s);
  while (ss.good()) {
    std::string element;
    std::getline(ss, element, '.');
    if (element.empty())
      break;

    uint32_t parsedElement = 0;
    try {
      parsedElement = std::stoi(element);
    } catch (...) {
      break;
    }
    if (parsedElement < 0 || parsedElement > 0xFF)
      break;

    Output.push_back(static_cast<uint8_t>(parsedElement));
  }

  if (Output.size() != 4)
    Output.clear();

  return Output;
}

inline std::string TrimString(const std::string& str) {
  size_t firstNonSpace = str.find_first_not_of(" ");
  size_t lastNonSpace = str.find_last_not_of(" ");

  if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos) {
    return str.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
  } else {
    return std::string();
  }
}

inline std::vector<std::string> SplitArgs(const std::string& s, const uint8_t expectedCount)
{
  uint8_t parsedCount = 0;
  std::stringstream SS(s);
  std::string NextItem;
  std::vector<std::string> Output;
  do {
    std::getline(SS, NextItem, ',');
    if (SS.fail()) {
      break;
    }
    Output.push_back(TrimString(NextItem));
    ++parsedCount;
  } while (!SS.eof() && parsedCount < expectedCount);

  if (parsedCount != expectedCount)
    Output.clear();

  return Output;
}

inline std::vector<std::string> SplitArgs(const std::string& s, const uint8_t minCount, const uint8_t maxCount)
{
  uint8_t parsedCount = 0;
  std::stringstream SS(s);
  std::string NextItem;
  std::vector<std::string> Output;
  do {
    std::getline(SS, NextItem, ',');
    if (SS.fail()) {
      break;
    }
    Output.push_back(TrimString(NextItem));
    ++parsedCount;
  } while (!SS.eof() && parsedCount < maxCount);

  if (!(minCount <= parsedCount && parsedCount <= maxCount))
    Output.clear();

  return Output;
}

inline std::vector<uint32_t> SplitNumericArgs(const std::string& s, const uint8_t expectedCount)
{
  uint8_t parsedCount = 0;
  std::stringstream SS(s);
  uint32_t NextItem;
  std::string NextString;
  std::vector<uint32_t> Output;
  do {
    bool elemOkay = true;
    std::getline(SS, NextString, ',');
    if (SS.fail()) {
      break;
    }
    try {
      NextItem = std::stol(TrimString(NextString));
    } catch (...) {
      elemOkay = false;
    }
    if (!elemOkay) {
      Output.clear();
      break;
    }
    Output.push_back(NextItem);
    ++parsedCount;
  } while (!SS.eof() && parsedCount < expectedCount);

  if (parsedCount != expectedCount)
    Output.clear();

  return Output;
}

inline std::vector<uint32_t> SplitNumericArgs(const std::string& s, const uint8_t minCount, const uint8_t maxCount)
{
  uint8_t parsedCount = 0;
  std::stringstream SS(s);
  uint32_t NextItem;
  std::string NextString;
  std::vector<uint32_t> Output;
  do {
    bool elemOkay = true;
    std::getline(SS, NextString, ',');
    if (SS.fail()) {
      break;
    }
    try {
      NextItem = std::stol(TrimString(NextString));
    } catch (...) {
      elemOkay = false;
    }
    if (!elemOkay) {
      Output.clear();
      break;
    }
    Output.push_back(NextItem);
    ++parsedCount;
  } while (!SS.eof() && parsedCount < maxCount);

  if (!(minCount <= parsedCount && parsedCount <= maxCount))
    Output.clear();

  return Output;
}

inline void AssignLength(std::vector<uint8_t>& content)
{
  // insert the actual length of the content array into bytes 3 and 4 (indices 2 and 3)

  const uint16_t Size = static_cast<uint16_t>(content.size());

  content[2] = static_cast<uint8_t>(Size);
  content[3] = static_cast<uint8_t>(Size >> 8);
}

inline std::string AddPathSeparator(const std::string& path)
{
  if (path.empty())
    return std::string();

#ifdef WIN32
  const char Separator = '\\';
#else
  const char Separator = '/';
#endif

  if (*(end(path) - 1) == Separator)
    return path;
  else
    return path + std::string(1, Separator);
}

inline std::vector<uint8_t> EncodeStatString(std::vector<uint8_t>& data)
{
  std::vector<uint8_t> Result;
  uint8_t              Mask = 1;

  for (uint32_t i = 0; i < data.size(); ++i)
  {
    if ((data[i] % 2) == 0)
      Result.push_back(data[i] + 1);
    else
    {
      Result.push_back(data[i]);
      Mask |= 1 << ((i % 7) + 1);
    }

    if (i % 7 == 6 || i == data.size() - 1)
    {
      Result.insert(end(Result) - 1 - (i % 7), Mask);
      Mask = 1;
    }
  }

  return Result;
}

inline std::vector<uint8_t> DecodeStatString(const std::vector<uint8_t>& data)
{
  uint8_t              Mask = 1;
  std::vector<uint8_t> Result;

  for (uint32_t i = 0; i < data.size(); ++i)
  {
    if ((i % 8) == 0)
      Mask = data[i];
    else
    {
      if ((Mask & (1 << (i % 8))) == 0)
        Result.push_back(data[i] - 1);
      else
        Result.push_back(data[i]);
    }
  }

  return Result;
}

inline std::vector<std::string> Tokenize(const std::string& s, const char delim)
{
  std::vector<std::string> Tokens;
  std::string              Token;

  for (auto i = begin(s); i != end(s); ++i)
  {
    if (*i == delim)
    {
      if (Token.empty())
        continue;

      Tokens.push_back(Token);
      Token.clear();
    }
    else
      Token += *i;
  }

  if (!Token.empty())
    Tokens.push_back(Token);

  return Tokens;
}

inline uint32_t GetLevenshteinDistance(const std::string& s1, const std::string& s2) {
  int m = s1.length();
  int n = s2.length();

  // Create a 2D vector to store the distances
  std::vector<std::vector<uint32_t>> dp(m + 1, std::vector<uint32_t>(n + 1, 0));

  for (int i = 0; i <= m; ++i) {
    for (int j = 0; j <= n; ++j) {
      if (i == 0) {
          dp[i][j] = j;
      } else if (j == 0) {
          dp[i][j] = i;
      } else if (s1[i - 1] == s2[j - 1]) {
          dp[i][j] = dp[i - 1][j - 1];
      } else {
        int cost = (s1[i - 1] == s2[j - 1]) ? 0 : (isdigit(s1[i - 1]) || isdigit(s2[j - 1])) ? 3 : 1;
        dp[i][j] = std::min({ dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost });
      }
    }
  }

  return dp[m][n];
}

inline std::string RemoveNonAlphanumeric(const std::string& s) {
    std::regex nonAlphanumeric("[^a-zA-Z0-9]");
    return std::regex_replace(s, nonAlphanumeric, "");
}

inline bool IsValidMapName(const std::string& s) {
  if (!s.length()) return false;
  if (s[0] == '.') return false;
  std::regex invalidChars("[^a-zA-Z0-9_ ().~-]");
  std::regex validExtensions("\\.w3(m|x)$");
  return !std::regex_search(s, invalidChars) && std::regex_search(s, validExtensions);
}

inline bool IsValidCFGName(const std::string& s) {
  if (!s.length()) return false;
  if (s[0] == '.') return false;
  std::regex invalidChars("[^a-zA-Z0-9_ ().~-]");
  std::regex validExtensions("\\.cfg$");
  return !std::regex_search(s, invalidChars) && std::regex_search(s, validExtensions);
}

inline std::string TrimTrailingSlash(const std::string s) {
  if (s.empty()) return s;
  if (s[s.length() - 1] == '/') return s.substr(0, s.length() - 1);
  return s;
}

inline bool IsBase10Number(const std::string& s) {
  if (s.empty()) return false;
  if (s[0] == '0') return s.length() == 1;

  for (char ch : s) {
    if (!isdigit(ch)) return false;
  }
  return true;
}

inline std::string MaybeBase10(const std::string s) {
  return IsBase10Number(s) ? s : std::string();
}

inline std::string JoinVector(const std::vector<std::string> list, const std::string connector, const bool trailingConnector) {
  std::string Results;
  for (const auto& element : list)
    Results += element + connector;
  if (!trailingConnector) Results = Results.substr(0, Results.length() - 2);
  return Results;
}

inline std::string JoinVector(const std::vector<std::string> list, const bool trailingComma) {
  return JoinVector(list, ", ", trailingComma);
}

inline std::string IPv4ToString(const std::vector<uint8_t> ip) {
  if (ip.size() != 4) {
    return std::string();
  }
  return std::to_string(static_cast<int>(ip[0])) + "." + std::to_string(static_cast<int>(ip[1])) + "." + std::to_string(static_cast<int>(ip[2])) + "." + std::to_string(static_cast<int>(ip[3]));
}

inline std::string EncodeURIComponent(const std::string & s) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (char c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        escaped << c;
    } else if (c == ' ') {
        escaped << '+';
    } else {
        escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
    }
  }

  return escaped.str();
}

inline std::string DecodeURIComponent(const std::string & encoded) {
  std::ostringstream decoded;

  for (std::size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] == '%' && i + 2 < encoded.size() &&
      std::isxdigit(encoded[i + 1]) && std::isxdigit(encoded[i + 2])) {
      int hexValue;
      std::istringstream hexStream(encoded.substr(i + 1, 2));
      hexStream >> std::hex >> hexValue;
      decoded << static_cast<char>(hexValue);
      i += 2;
    } else if (encoded[i] == '+') {
      decoded << ' ';
    } else {
      decoded << encoded[i];
    }
  }

  return decoded.str();
}

inline std::string GetFileName(const std::string & inputPath) {
  std::filesystem::path filePath = inputPath;
  return filePath.filename().string();
}

inline bool PathHasNullBytes(const std::filesystem::path & filePath) {
  for (const auto& c : filePath.native()) {
    if (c == '\0') {
      return true; // Found a null byte
    }
  }
  return false; // No null bytes found
}

#endif // AURA_UTIL_H_
