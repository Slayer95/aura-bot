/*

  Copyright [2024-2025] [Leonardo Julca]

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

#ifndef AURA_UTIL_H_
#define AURA_UTIL_H_

#include "includes.h"
#include "flat_map.h"
#include "hash.h"
#include "parser.h"

#include <fstream>
#include <random>
#include <regex>
#include <filesystem>
#include <functional>

#include <utf8/utf8.h>

#pragma once

#define TO_ARRAY(...) StringArray({__VA_ARGS__})

[[nodiscard]] inline std::string ToDecString(const uint8_t byte)
{
  return std::to_string(static_cast<uint16_t>(byte));
}

[[nodiscard]] inline std::string ToHexString(uint32_t i)
{
  std::string       result;
  std::stringstream SS;
  SS << std::hex << i;
  SS >> result;
  return result;
}

#ifdef _WIN32
[[nodiscard]] inline PLATFORM_STRING_TYPE ToDecStringCPlatform(const size_t value)
{
  PLATFORM_STRING_TYPE platformNumeral;
  std::string numeral = std::to_string(value);
  utf8::utf8to16(numeral.begin(), numeral.end(), std::back_inserter(platformNumeral));
  return platformNumeral;
}
#else
[[nodiscard]] inline PLATFORM_STRING_TYPE ToDecStringCPlatform(const size_t value)
{
  return std::to_string(value);
}
#endif

[[nodiscard]] inline std::string ToDecStringPadded(const int64_t num, const std::string::size_type padding)
{
  std::string numeral = std::to_string(num);
  if (numeral.size() >= padding) return numeral;
  std::string padded = std::string(padding, '0');
  padded.replace(padded.end() - numeral.size(), padded.end(), numeral);
  return padded;
}

[[nodiscard]] inline std::string TrimString(const std::string& str)
{
  if (str.empty()) return std::string();

  size_t firstNonSpace = str.find_first_not_of(" ");
  size_t lastNonSpace = str.find_last_not_of(" ");

  if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos) {
    return str.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
  } else {
    return std::string();
  }
}

[[nodiscard]] inline std::string TrimStringExtended(const std::string& str)
{
  if (str.empty()) return std::string();

  size_t firstNonSpace = str.find_first_not_of(" \r\n");
  size_t lastNonSpace = str.find_last_not_of(" \r\n");

  if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos) {
    return str.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
  } else {
    return std::string();
  }
}

[[nodiscard]] inline std::string RemoveDuplicateWhiteSpace(const std::string& str)
{
  std::string result;
  result.reserve(str.size());

  bool wasSpace = false;
  for (char c : str) {
    bool isSpace = c == ' ';
    if (!(isSpace && wasSpace)) {
      result += c;
    }
    wasSpace = isSpace;
  }

  return result;
}

inline void EllideEmptyElementsInPlace(std::vector<std::string>& list) {
  auto it = list.rbegin();
  while (it != list.rend()) {
    if (it->empty()) {
      it = std::vector<std::string>::reverse_iterator(list.erase((++it).base()));
    } else {
      it++;
    }
  }
}

template <typename T>
[[nodiscard]] inline T SubtractClampZero(const T& minuend, const T& subtrahend) noexcept
{
  if (minuend < subtrahend) return 0;
  return minuend - subtrahend;
}

[[nodiscard]] inline std::string ToFormattedString(const double d, const uint8_t precision = 2)
{
  std::ostringstream out;
  out << std::fixed << std::setprecision(precision) << d;
  return out.str();
}

[[nodiscard]] inline std::string ToFormattedRealm()
{
  return "@@LAN/VPN";
}

[[nodiscard]] inline std::string ToFormattedRealm(const std::string& hostName)
{
  if (hostName.empty()) return ToFormattedRealm();
  return hostName;
}

[[nodiscard]] inline std::string ToFormattedTimeStamp(const int64_t hh, const int64_t mm, const int64_t ss)
{
  if (hh > 0) return ToDecStringPadded(hh, 2) + ":" + ToDecStringPadded(mm, 2) + ":" + ToDecStringPadded(ss, 2);
  return ToDecStringPadded(mm, 2) + ":" + ToDecStringPadded(ss, 2);
}

[[nodiscard]] inline std::string ToDurationString(const int64_t hh, const int64_t mm, const int64_t ss)
{
  std::string result;
  if (hh > 0) result.append(std::to_string(hh) + " h ");
  if (mm > 0) result.append(std::to_string(mm) + " min ");
  if (ss > 0) result.append(std::to_string(ss) + " s ");
  if (result.empty()) return "Now";
  return TrimString(result);
}

[[nodiscard]] inline std::string ToFormattedTimeStamp(const int64_t seconds)
{
  int64_t ss, mm, hh;
  ss = seconds;

  mm = ss / 60;
  ss = ss % 60;
  hh = mm / 60;
  mm = mm % 60;

  return ToFormattedTimeStamp(hh, mm, ss);
}

[[nodiscard]] inline std::string ToDurationString(const int64_t seconds)
{
  int64_t ss, mm, hh;
  ss = seconds;

  mm = ss / 60;
  ss = ss % 60;
  hh = mm / 60;
  mm = mm % 60;

  return ToDurationString(hh, mm, ss);
}

[[nodiscard]] inline std::string ToVersionString(const Version& version)
{
  return ToDecString(version.first) + "." + ToDecString(version.second);
}

[[nodiscard]] inline uint32_t ToVersionFlattened(const Version& version) // MDNS protocol
{
  return 10000u + 100u * (uint32_t)(version.first - 1) + (uint32_t)(version.second);
}

[[nodiscard]] inline uint8_t ToVersionOrdinal(const Version& version) // Aura internal
{
  return (version.first - 1) * 37 + version.second;
}

[[nodiscard]] inline bool GetIsValidVersion(const Version& version)
{
  switch (version.first) {
    case 2: return true;
    case 1: return version.second <= 36;
    default: return false;
  }
}

[[nodiscard]] inline std::string ToOrdinalName(const size_t number)
{
  switch (number % 10) {
    case 1: return std::to_string(number) + "st";
    case 2: return std::to_string(number) + "nd";
    case 3: return std::to_string(number) + "rd";
    default: return std::to_string(number) + "th";
  }
}

inline void WriteUint16(std::vector<uint8_t>& buffer, const uint16_t value, const size_t offset, bool bigEndian = false)
{
  if (!bigEndian) {
    buffer[offset] = static_cast<uint8_t>(value);
    buffer[offset + 1] = static_cast<uint8_t>(value >> 8);
  } else {
    buffer[offset] = static_cast<uint8_t>(value >> 8);
    buffer[offset + 1] = static_cast<uint8_t>(value);
  }
}

inline void WriteUint32(std::vector<uint8_t>& buffer, const uint32_t value, const uint32_t offset, bool bigEndian = false)
{
  if (!bigEndian) {
    buffer[offset] = static_cast<uint8_t>(value);
    buffer[offset + 1] = static_cast<uint8_t>(value >> 8);
    buffer[offset + 2] = static_cast<uint8_t>(value >> 16);
    buffer[offset + 3] = static_cast<uint8_t>(value >> 24);
  } else {
    buffer[offset] = static_cast<uint8_t>(value >> 24);
    buffer[offset + 1] = static_cast<uint8_t>(value >> 16);
    buffer[offset + 2] = static_cast<uint8_t>(value >> 8);
    buffer[offset + 3] = static_cast<uint8_t>(value);
  }
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const uint8_t* a, const size_t size)
{
  return std::vector<uint8_t>(a, a + size);
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const uint8_t c)
{
  return std::vector<uint8_t>{c};
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const uint16_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::vector<uint8_t>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8)};
  else
    return std::vector<uint8_t>{static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const uint32_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::vector<uint8_t>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)};
  else
    return std::vector<uint8_t>{static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const int64_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::vector<uint8_t>{
      static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)/*,
      static_cast<uint8_t>(i >> 32), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 56)*/
    };
  else
    return std::vector<uint8_t>{
      /*static_cast<uint8_t>(i >> 56), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(32),*/
      static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)
    };
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const float i, bool bigEndian)
{
  std::vector<uint8_t> bytes(4, 0);
  if (std::numeric_limits<float>::is_iec559) {
    std::memcpy(bytes.data(), &i, sizeof(float));
    if (bigEndian != GetIsHostBigEndian()) {
      std::reverse(bytes.begin(), bytes.end());
    }
  }
  return bytes;
}

[[nodiscard]] inline std::vector<uint8_t> CreateByteArray(const double i, bool bigEndian)
{
  std::vector<uint8_t> bytes(8, 0);
  if (std::numeric_limits<double>::is_iec559) {
    std::memcpy(bytes.data(), &i, sizeof(double));
    if (bigEndian != GetIsHostBigEndian()) {
      std::reverse(bytes.begin(), bytes.end());
    }
  }
  return bytes;
}

[[nodiscard]] inline std::array<uint8_t, 1> CreateFixedByteArray(const uint8_t c)
{
  return std::array<uint8_t, 1>{c};
}

[[nodiscard]] inline std::array<uint8_t, 2> CreateFixedByteArray(const uint16_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::array<uint8_t, 2>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8)};
  else
    return std::array<uint8_t, 2>{static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

[[nodiscard]] inline std::array<uint8_t, 4> CreateFixedByteArray(const uint32_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::array<uint8_t, 4>{static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)};
  else
    return std::array<uint8_t, 4>{static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)};
}

[[nodiscard]] inline std::array<uint8_t, 4> CreateFixedByteArray(const int64_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::array<uint8_t, 4>{
      static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24)/*,
      static_cast<uint8_t>(i >> 32), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 56)*/
    };
  else
    return std::array<uint8_t, 4>{
      /*static_cast<uint8_t>(i >> 56), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(32),*/
      static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)
    };
}

[[nodiscard]] inline std::array<uint8_t, 8> CreateFixedByteArray64(const uint64_t i, bool bigEndian)
{
  if (!bigEndian)
    return std::array<uint8_t, 8>{
      static_cast<uint8_t>(i), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 24),
      static_cast<uint8_t>(i >> 32), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 56)
    };
  else
    return std::array<uint8_t, 8>{
      static_cast<uint8_t>(i >> 56), static_cast<uint8_t>(i >> 48), static_cast<uint8_t>(i >> 40), static_cast<uint8_t>(32),
      static_cast<uint8_t>(i >> 24), static_cast<uint8_t>(i >> 16), static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i)
    };
}

[[nodiscard]] inline std::array<uint8_t, 8> CreateFixedByteArray(const double i, bool bigEndian)
{
  std::array<uint8_t, 8> bytes;
  bytes.fill(0);
  if (std::numeric_limits<double>::is_iec559) {
    std::memcpy(bytes.data(), &i, sizeof(double));
    if (bigEndian != GetIsHostBigEndian()) {
      std::reverse(bytes.begin(), bytes.end());
    }
  }
  return bytes;
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 1>>& optArray, const uint8_t c)
{
  std::array<uint8_t, 1> val = CreateFixedByteArray(c);
  optArray.emplace();
  optArray->swap(val);
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 2>>& optArray, const uint16_t i, bool bigEndian)
{
  std::array<uint8_t, 2> val = CreateFixedByteArray(i, bigEndian);
  optArray.emplace();
  optArray->swap(val);
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 4>>& optArray, const uint32_t i, bool bigEndian)
{
  std::array<uint8_t, 4> val = CreateFixedByteArray(i, bigEndian);
  optArray.emplace();
  optArray->swap(val);
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 4>>& optArray, const int64_t i, bool bigEndian)
{
  std::array<uint8_t, 4> val = CreateFixedByteArray(i, bigEndian);
  optArray.emplace();
  optArray->swap(val);
}

inline void EnsureFixedByteArray(std::optional<std::array<uint8_t, 8>>& optArray, const double i, bool bigEndian)
{
  std::array<uint8_t, 8> val = CreateFixedByteArray(i, bigEndian);
  optArray.emplace();
  optArray->swap(val);
}

[[nodiscard]] inline uint16_t ByteArrayToUInt16(const std::vector<uint8_t>& b, bool bigEndian, const size_t start = 0)
{
  if (b.size() < start + 2)
    return 0;

  if (!bigEndian)
    return static_cast<uint16_t>(b[start + 1] << 8 | b[start]);
  else
    return static_cast<uint16_t>(b[start] << 8 | b[start + 1]);
}

[[nodiscard]] inline uint32_t ByteArrayToUInt32(const std::vector<uint8_t>& b, bool bigEndian, const size_t start = 0)
{
  if (b.size() < start + 4)
    return 0;

  if (!bigEndian)
    return static_cast<uint32_t>(b[start + 3] << 24 | b[start + 2] << 16 | b[start + 1] << 8 | b[start]);
  else
    return static_cast<uint32_t>(b[start] << 24 | b[start + 1] << 16 | b[start + 2] << 8 | b[start + 3]);
}

[[nodiscard]] inline uint16_t ByteArrayToUInt16(const std::array<uint8_t, 2>& b, bool bigEndian)
{
  if (!bigEndian)
    return static_cast<uint16_t>(b[1] << 8 | b[0]);
  else
    return static_cast<uint16_t>(b[2] << 8 | b[3]);
}

[[nodiscard]] inline uint32_t ByteArrayToUInt32(const std::array<uint8_t, 4>& b, bool bigEndian)
{
  if (!bigEndian)
    return static_cast<uint32_t>(b[3] << 24 | b[2] << 16 | b[1] << 8 | b[0]);
  else
    return static_cast<uint32_t>(b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]);
}

[[nodiscard]] inline uint16_t ByteArrayToUInt16(const uint8_t* b, bool bigEndian)
{
  if (!bigEndian)
    return static_cast<uint16_t>(b[1] << 8 | b[0]);
  else
    return static_cast<uint16_t>(b[2] << 8 | b[3]);
}

[[nodiscard]] inline uint32_t ByteArrayToUInt32(const uint8_t* b, bool bigEndian)
{
  if (!bigEndian)
    return static_cast<uint32_t>(b[3] << 24 | b[2] << 16 | b[1] << 8 | b[0]);
  else
    return static_cast<uint32_t>(b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]);
}

[[nodiscard]] inline std::string ByteArrayToDecString(const std::vector<uint8_t>& b)
{
  if (b.empty())
    return std::string();

  std::string result = std::to_string(b[0]);

  for (auto i = cbegin(b) + 1; i != cend(b); ++i)
    result += " " + std::to_string(*i);

  return result;
}

[[nodiscard]] inline std::string ByteArrayToDecString(const uint8_t* start, const size_t size)
{
  if (size == 0)
    return std::string();

  std::string result = std::to_string(start[0]);
  for (size_t i = 1; i < size; ++i) {
    result += " " + std::to_string(start[i]);
  }

  return result;
}

template <size_t SIZE>
[[nodiscard]] inline std::string ByteArrayToDecString(const std::array<uint8_t, SIZE>& b)
{
  std::string result = std::to_string(b[0]);

  for (auto i = cbegin(b) + 1; i != cend(b); ++i)
    result += " " + std::to_string(*i);

  return result;
}

[[nodiscard]] inline std::string ByteArrayToHexString(const std::vector<uint8_t>& b)
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

[[nodiscard]] inline std::string ByteArrayToHexString(const uint8_t* start, const size_t size)
{
  if (size == 0)
    return std::string();

  std::string result = ToHexString(start[0]);
  for (size_t i = 1; i < size; ++i) {
    if (start[i] < 0x10) 
      result += " 0" + ToHexString(start[i]);
    else
      result += " " + ToHexString(start[i]);
  }

  return result;
}

template <size_t SIZE>
[[nodiscard]] inline std::string ByteArrayToHexString(const std::array<uint8_t, SIZE>& b)
{
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

[[nodiscard]] inline std::string ReverseByteArrayToDecString(const std::vector<uint8_t>& b)
{
  if (b.empty())
    return std::string();

  std::string result = std::to_string(b.back());

  for (auto i = b.crbegin() + 1; i != b.crend(); ++i)
    result += " " + std::to_string(*i);

  return result;
}

[[nodiscard]] inline std::string ReverseByteArrayToDecString(const uint8_t* start, const size_t size)
{
  if (size == 0)
    return std::string();

  size_t i = size - 1;
  std::string result = std::to_string(start[i]);

  while (i--) {
    result += " " + std::to_string(start[i]);
  }

  return result;
}

template <size_t SIZE>
[[nodiscard]] inline std::string ReverseByteArrayToDecString(const std::array<uint8_t, SIZE>& b)
{
  std::string result = std::to_string(b.back());

  for (auto i = b.crbegin() + 1; i != b.crend(); ++i)
    result += " " + std::to_string(*i);

  return result;
}

[[nodiscard]] inline std::string ReverseByteArrayToHexString(const std::vector<uint8_t>& b)
{
  if (b.empty())
    return std::string();

  std::string result = ToHexString(b.back());

  for (auto i = b.crbegin() + 1; i != b.crend(); ++i)
  {
    if (*i < 0x10)
      result += " 0" + ToHexString(*i);
    else
      result += " " + ToHexString(*i);
  }

  return result;
}

template <size_t SIZE>
[[nodiscard]] inline std::string ReverseByteArrayToHexString(const std::array<uint8_t, SIZE>& b)
{
  std::string result = ToHexString(b.back());

  for (auto i = b.crbegin() + 1; i != b.crend(); ++i)
  {
    if (*i < 0x10)
      result += " 0" + ToHexString(*i);
    else
      result += " " + ToHexString(*i);
  }

  return result;
}

inline void AppendByteArrayFast(std::vector<uint8_t>& b, const std::vector<uint8_t>& append)
{
  b.insert(end(b), begin(append), end(append));
}

template <size_t SIZE>
inline void AppendByteArrayFast(std::vector<uint8_t>& b, const std::array<uint8_t, SIZE>& append)
{
  b.insert(end(b), begin(append), end(append));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint8_t* a, const size_t size)
{
  size_t cursor = b.size();
  b.resize(cursor + size);
  std::copy_n(a, size, b.data() + cursor);
}

inline void AppendByteArrayString(std::vector<uint8_t>& b, std::string_view append, bool terminator = true)
{
  // append the std::string plus a null terminator

  b.insert(end(b), begin(append), end(append));

  if (terminator)
    b.push_back(0);
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint16_t i, bool bigEndian)
{
  size_t offset = b.size();
  b.resize(offset + 2);
  uint8_t* cursor = b.data() + offset;
  if (bigEndian) {
    cursor[0] = static_cast<uint8_t>(i >> 8);
    cursor[1] = static_cast<uint8_t>(i);
  } else {
    cursor[0] = static_cast<uint8_t>(i);
    cursor[1] = static_cast<uint8_t>(i >> 8);
  }
}

inline void AppendByteArray(std::vector<uint8_t>& b, const uint32_t i, bool bigEndian)
{
  size_t offset = b.size();
  b.resize(offset + 4);
  uint8_t* cursor = b.data() + offset;
  if (bigEndian) {
    cursor[0] = static_cast<uint8_t>(i >> 24);
    cursor[1] = static_cast<uint8_t>(i >> 16);
    cursor[2] = static_cast<uint8_t>(i >> 8);
    cursor[3] = static_cast<uint8_t>(i);
  } else {
    cursor[0] = static_cast<uint8_t>(i);
    cursor[1] = static_cast<uint8_t>(i >> 8);
    cursor[2] = static_cast<uint8_t>(i >> 16);
    cursor[3] = static_cast<uint8_t>(i >> 24);
  }
}

inline void AppendByteArray(std::vector<uint8_t>& b, const int64_t i, bool bigEndian)
{
  AppendByteArrayFast(b, CreateByteArray(i, bigEndian));
}

inline void AppendByteArray(std::vector<uint8_t>& b, const float i, bool bigEndian)
{
  size_t offset = b.size();
  b.resize(offset + 4);
  if constexpr (std::numeric_limits<float>::is_iec559 && sizeof(float) == 4) {
    uint8_t* cursor = b.data() + offset;
    std::memcpy(cursor, &i, 4);
    if (bigEndian != GetIsHostBigEndian()) {
      std::reverse(cursor, cursor + 4);
    }
  } else {
  }
}

inline void AppendByteArray(std::vector<uint8_t>& b, const double i, bool bigEndian)
{
  size_t offset = b.size();
  b.resize(offset + 8);
  if constexpr (std::numeric_limits<double>::is_iec559 && sizeof(double) == 8) {
    uint8_t* cursor = b.data() + offset;
    std::memcpy(cursor, &i, 8);
    if (bigEndian != GetIsHostBigEndian()) {
      std::reverse(cursor, cursor + 8);
    }
  }
}

inline void AppendSwapString(std::string& fromString, std::string& toString)
{
  if (toString.empty()) {
    fromString.swap(toString);
  } else {
    toString.append(fromString);
    fromString.clear();
  }
}

inline void AppendProtoBufferFromLengthDelimitedS2S(std::vector<uint8_t>& b, const std::string& key, const std::string& value)
{
  size_t thisSize = key.size() + value.size() + 4;
  b.reserve(b.size() + thisSize + 2);
  b.push_back(0x1A);
  b.push_back((uint8_t)thisSize);
  b.push_back(0x0A);
  b.push_back((uint8_t)key.size());
  AppendByteArrayString(b, key, false);
  b.push_back(0x12);
  b.push_back((uint8_t)value.size());
  AppendByteArrayString(b, value, false);
}

inline void AppendProtoBufferFromLengthDelimitedS2C(std::vector<uint8_t>& b, const std::string& key, const uint8_t value)
{
  size_t thisSize = key.size() + 5;
  b.reserve(b.size() + thisSize + 2);
  b.push_back(0x1A);
  b.push_back((uint8_t)thisSize);
  b.push_back(0x0A);
  b.push_back((uint8_t)key.size());
  AppendByteArrayString(b, key, false);
  b.push_back(0x12);
  b.push_back(1);
  b.push_back(value);
}

[[nodiscard]] inline bool IsAllZeroes(const uint8_t* start, const uint8_t* end)
{
  const uint8_t* needle = start;
  while (needle < end) {
    if (*needle != 0) {
      return false;
    }
    ++needle;
  }
  return true;
}

[[nodiscard]] inline size_t FindNullDelimiterOrStart(const std::vector<uint8_t>& b, const size_t start)
{
  size_t end = b.size();
  if (start >= end) return start;
  for (size_t i = start; i < end; ++i) {
    if (b[i] == 0) {
      return i;
    }
  }
  return start;
}

[[nodiscard]] inline const uint8_t* FindNullDelimiterOrStart(const uint8_t* start, const uint8_t* end)
{
  const uint8_t* needle = start;
  while (needle < end) {
    if (*needle == 0) {
      return needle;
    }
    ++needle;
  }
  return start;
}

[[nodiscard]] inline size_t FindNullDelimiterOrEnd(const std::vector<uint8_t>& b, const size_t start)
{
  // start searching the byte array at position 'start' for the first null value
  // if found, return the subarray from 'start' to the null value but not including the null value

  size_t end = b.size();
  if (start >= end) return end;
  for (size_t i = start; i < end; ++i) {
    if (b[i] == 0) {
      return i;
    }
  }
  return end;
}

[[nodiscard]] inline const uint8_t* FindNullDelimiterOrEnd(const uint8_t* start, const uint8_t* end)
{
  const uint8_t* needle = start;
  while (needle < end) {
    if (*needle == 0) {
      return needle;
    }
    ++needle;
  }
  return end;
}

[[nodiscard]] inline std::string GetStringAddressRange(const uint8_t* start, const uint8_t* end)
{
  if (end == start) return std::string();
  return std::string(reinterpret_cast<const char*>(start), end - start);
}

[[nodiscard]] inline std::string GetStringAddressRange(const std::vector<uint8_t>& b, const size_t start, const size_t end)
{
  if (end == start) return std::string();
  return std::string(reinterpret_cast<const char*>(b.data() + start), end - start);
}

[[nodiscard]] inline std::vector<uint8_t> ExtractCString(const std::vector<uint8_t>& b, const size_t start)
{
  // start searching the byte array at position 'start' for the first null value
  // if found, return the subarray from 'start' to the null value but not including the null value

  if (start < b.size())
  {
    for (size_t i = start; i < b.size(); ++i)
    {
      if (b[i] == 0)
        return std::vector<uint8_t>(begin(b) + start, begin(b) + i);
    }

    // no null value found, return the rest of the byte array

    return std::vector<uint8_t>(begin(b) + start, end(b));
  }

  return std::vector<uint8_t>();
}

[[nodiscard]] inline uint8_t ExtractHex(const std::vector<uint8_t>& b, const size_t start, bool bigEndian)
{
  // consider the byte array to contain a 2 character ASCII encoded hex value at b[start] and b[start + 1] e.g. "FF"
  // extract it as a single decoded byte

  if (start + 1 < b.size())
  {
    uint8_t c = 0;
    std::string temp = std::string(begin(b) + start, begin(b) + start + 2);

    if (bigEndian)
      temp = std::string(temp.rend(), temp.rbegin());

    std::stringstream SS;
    SS << temp;
    SS >> std::hex >> c;
    return c;
  }

  return 0;
}

[[nodiscard]] inline std::vector<uint8_t> ExtractNumbers(const std::string& s, const uint32_t maxCount)
{
  // consider the std::string to contain a bytearray in dec-text form, e.g. "52 99 128 1"

  std::vector<uint8_t> result;
  uint32_t             c;
  std::stringstream    SS;
  SS << s;

  for (uint32_t i = 0; i < maxCount; ++i)
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

[[nodiscard]] inline std::vector<uint8_t> ExtractHexNumbers(const std::string& s)
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

[[nodiscard]] inline std::vector<uint8_t> ExtractIPv4(const std::string& s)
{
  std::vector<uint8_t> Output;
  std::stringstream ss(s);
  while (ss.good()) {
    std::string element;
    std::getline(ss, element, '.');
    if (element.empty())
      break;

    int32_t parsedElement = 0;
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

[[nodiscard]] inline std::string ToUpperCase(const std::string& input)
{
  std::string output = input;
  std::transform(std::begin(output), std::end(output), std::begin(output), [](char c) { return static_cast<char>(std::toupper(c)); });
  return output;
}

[[nodiscard]] inline std::vector<uint8_t> SplitNumeral(const std::string& input)
{
  std::vector<uint8_t> result;
  result.reserve(input.size());
  for (const auto& c : input) {
    if (isdigit(c)) {
      result.push_back(c - 0x30);
    } else {
      return std::vector<uint8_t>();
    }
  }
  return result;
}

[[nodiscard]] inline std::vector<std::string> SplitArgs(const std::string& s, const uint8_t expectedCount)
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

[[nodiscard]] inline std::vector<std::string> SplitArgs(const std::string& s, const uint8_t minCount, const uint8_t maxCount)
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

[[nodiscard]] inline std::vector<uint32_t> SplitNumericArgs(const std::string& s, const uint8_t expectedCount)
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

[[nodiscard]] inline std::vector<uint32_t> SplitNumericArgs(const std::string& s, const uint8_t minCount, const uint8_t maxCount)
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

[[nodiscard]] inline bool ValidateLength(const std::vector<uint8_t>& content)
{
  // verify that bytes 3 and 4 (indices 2 and 3) of the content array describe the length

  size_t size = content.size();
  if (size >= 4 && size <= 0xFFFF) {
    return ByteArrayToUInt16(content, false, 2) == static_cast<uint16_t>(size);
  }
  return false;
}

[[nodiscard]] inline std::string AddPathSeparator(const std::string& path)
{
  if (path.empty())
    return std::string();

#ifdef _WIN32
  const char Separator = '\\';
#else
  const char Separator = '/';
#endif

  if (*(end(path) - 1) == Separator)
    return path;
  else
    return path + std::string(1, Separator);
}

[[nodiscard]] inline std::vector<uint8_t> EncodeStatString(const std::vector<uint8_t>& data)
{
  uint8_t b = 0, mask = 1;
  size_t i = 0, j = 0, w = 1;
  size_t len = data.size();
  size_t fullBlockCount = len / 7u;
  size_t fullBlockLen = fullBlockCount * 7u;
  size_t outSize = fullBlockCount * 8u;
  if (fullBlockLen < len) {
    outSize += (len % 7u) + 1u;
  }
  std::vector<uint8_t> result;
  result.resize(outSize);

  for (; i < fullBlockLen; i += 7, w++) {
    mask = 1;
    for (j = 0; j < 7; j++, w++) {
      b = data[i + j];
      if (b % 2 == 0) {
        result[w] = b + 1;
      } else {
        result[w] = b;
        mask |= (1u << (j + 1u));
      }
    }
    result[w - 8] = mask;
  }

  if (fullBlockLen < len) {
    mask = 1;
    for (i = fullBlockLen; i < len; i++, w++) {
      b = data[i];
      if (b % 2 == 0) {
        result[w] = b + 1;
      } else {
        result[w] = b;
        mask |= (1u << ((i % 7u) + 1u));
      }
    }
    result[fullBlockCount * 8] = mask;
  }


  return result;
}

template <typename T>
[[nodiscard]] inline std::vector<uint8_t> DecodeStatString(const T& data)
{
  uint8_t mask = 1u;
  uint8_t bitOrder = 0u;
  size_t i = 0, j = 0;
  size_t len = data.size();
  size_t fullBlockCount = len / 8u;
  size_t fullBlockLen = fullBlockCount * 8u;
  size_t outSize = fullBlockCount * 7u;
  if (fullBlockLen < len) {
    outSize += (len % 8u) - 1u;
  }
  std::vector<uint8_t> result;
  result.reserve(outSize);

  for (i = 0; i < fullBlockLen; i += 8) {
    mask = (uint8_t)data[i];
    for (j = 1; j < 8; j++) {
      bitOrder = (uint8_t)j;
      if (((mask >> bitOrder) & 0x1) == 0) {
        result.push_back((uint8_t)data[i + j] - 1);
      } else {
        result.push_back((uint8_t)data[i + j]);
      }
    }
  }

  if (fullBlockLen < len) {
    mask = (uint8_t)data[fullBlockLen];
    for (i = fullBlockLen + 1; i < len; i++) {
      bitOrder = (uint8_t)i % 8;
      if (((mask >> bitOrder) & 0x1) == 0) {
        result.push_back((uint8_t)data[i] - 1);
      } else {
        result.push_back((uint8_t)data[i]);
      }
    }
  }

  return result;
}

[[nodiscard]] inline std::vector<std::string> Tokenize(const std::string& s, const char delim)
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

[[nodiscard]] inline std::string::size_type GetLevenshteinDistance(const std::string& s1, const std::string& s2) {
  std::string::size_type m = s1.length();
  std::string::size_type n = s2.length();

  // Create a 2D vector to store the distances
  std::vector<std::vector<std::string::size_type>> dp(m + 1, std::vector<std::string::size_type>(n + 1, 0));

  for (std::string::size_type i = 0; i <= m; ++i) {
    for (std::string::size_type j = 0; j <= n; ++j) {
      if (i == 0) {
          dp[i][j] = j;
      } else if (j == 0) {
          dp[i][j] = i;
      } else if (s1[i - 1] == s2[j - 1]) {
          dp[i][j] = dp[i - 1][j - 1];
      } else {
        std::string::size_type cost = (s1[i - 1] == s2[j - 1]) ? 0 : (isdigit(s1[i - 1]) || isdigit(s2[j - 1])) ? 3 : 1;
        dp[i][j] = std::min({ dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost });
      }
    }
  }

  return dp[m][n];
}

[[nodiscard]] inline std::string::size_type GetLevenshteinDistanceForSearch(const std::string& s1, const std::string& s2, const std::string::size_type bestDistance) {
  std::string::size_type m = s1.length();
  std::string::size_type n = s2.length();

  if (m > n + bestDistance) {
    return m - n;
  }

  if (n > m + bestDistance) {
    return n - m;
  }

  // Create a 2D vector to store the distances
  std::vector<std::vector<std::string::size_type>> dp(m + 1, std::vector<std::string::size_type>(n + 1, 0));

  for (std::string::size_type i = 0; i <= m; ++i) {
    for (std::string::size_type j = 0; j <= n; ++j) {
      if (i == 0) {
          dp[i][j] = j;
      } else if (j == 0) {
          dp[i][j] = i;
      } else if (s1[i - 1] == s2[j - 1]) {
          dp[i][j] = dp[i - 1][j - 1];
      } else {
        std::string::size_type cost = (s1[i - 1] == s2[j - 1]) ? 0 : (isdigit(s1[i - 1]) || isdigit(s2[j - 1])) ? 3 : 1;
        dp[i][j] = std::min({ dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost });
      }
    }
  }

  return dp[m][n];
}

[[nodiscard]] inline std::string CheckIsValidHCLStandard(const std::string& s) {
  std::string HCLChars = HCL_CHARSET_STANDARD;
  if (s.find_first_not_of(HCLChars) != std::string::npos) {
    return "[" + s + "] is not a standard HCL string.";
  }
  return std::string();
}

[[nodiscard]] inline std::string CheckIsValidHCLSmall(const std::string& s) {
  std::string HCLChars = HCL_CHARSET_SMALL;
  if (s.find_first_not_of(HCLChars) != std::string::npos) {
    return "[" + s + "] is not a short HCL string.";
  }
  return std::string();
}

[[nodiscard]] inline std::string DurationLeftToString(int64_t remainingSeconds) {
  if (remainingSeconds < 0)
    remainingSeconds = 0;
  int64_t remainingMinutes = remainingSeconds / 60;
  remainingSeconds = remainingSeconds % 60;
  if (remainingMinutes == 0) {
    return std::to_string(remainingSeconds) + " seconds";
  } else if (remainingSeconds == 0) {
    return std::to_string(remainingMinutes) + " minutes";
  } else {
    return std::to_string(remainingMinutes) + " min " + std::to_string(remainingSeconds) + "s";
  }
}

[[nodiscard]] inline std::string FourCCToString(uint32_t fourCC) {
  std::string result;
  result.reserve(4);
  result += static_cast<char>(static_cast<uint8_t>(fourCC >> 24));
  result += static_cast<char>(static_cast<uint8_t>(fourCC >> 16));
  result += static_cast<char>(static_cast<uint8_t>(fourCC >> 8));
  result += static_cast<char>(static_cast<uint8_t>(fourCC));
  return result;
}

[[nodiscard]] inline std::string RemoveNonAlphanumeric(const std::string& s) {
    std::regex nonAlphanumeric("[^a-zA-Z0-9]");
    return std::regex_replace(s, nonAlphanumeric, "");
}

[[nodiscard]] inline std::string RemoveNonAlphanumericNorHyphen(const std::string& s) {
    std::regex nonAlphanumericNorHyphen("[^a-zA-Z0-9-]");
    return std::regex_replace(s, nonAlphanumericNorHyphen, "");
}

[[nodiscard]] inline bool IsValidMapName(const std::string& s) {
  if (!s.length()) return false;
  if (s[0] == '.') return false;
  std::regex invalidChars("[^a-zA-Z0-9_ ().~-]");
  std::regex validExtensions("\\.w3(m|x)$");
  return !std::regex_search(s, invalidChars) && std::regex_search(s, validExtensions);
}

[[nodiscard]] inline bool IsValidCFGName(const std::string& s) {
  if (!s.length()) return false;
  if (s[0] == '.') return false;
  std::regex invalidChars("[^a-zA-Z0-9_ ().~-]");
  std::regex validExtensions("\\.ini$");
  return !std::regex_search(s, invalidChars) && std::regex_search(s, validExtensions);
}

[[nodiscard]] inline std::string TrimTrailingSlash(const std::string s) {
  if (s.empty()) return s;
  if (s[s.length() - 1] == '/') return s.substr(0, s.length() - 1);
  return s;
}

[[nodiscard]] inline std::string MaybeBase10(const std::string s) {
  return IsBase10NaturalOrZero(s) ? s : std::string();
}

template<typename Container>
[[nodiscard]] inline std::string JoinStrings(const Container& list, const std::string connector, const bool trailingConnector) {
  using T = typename Container::value_type;
  static_assert(std::is_same_v<T, std::string> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>, "Container must contain std::string or uint16_t or uint32_t");

  std::string results;
  for (const auto& element : list) {
    if constexpr (std::is_same_v<T, std::string>) {
      results += element + connector;
    } else {
      results += std::to_string(element) + connector;
    }
  }

  if (!trailingConnector && !list.empty()) {
    results.erase(results.length() - connector.length());
  }

  return results;
}

template<typename Container>
[[nodiscard]] inline std::string JoinStrings(const Container& list, const bool trailingComma) {
  return JoinStrings(list, ", ", trailingComma);
}

[[nodiscard]] inline std::string IPv4ToString(const std::array<uint8_t, 4> ip) {
  return ToDecString(ip[0]) + "." + ToDecString(ip[1]) + "." + ToDecString(ip[2]) + "." + ToDecString(ip[3]);
}

[[nodiscard]] inline bool SplitIPAddressAndPortOrDefault(const std::string& input, const uint16_t defaultPort, std::string& ip, uint16_t& port)
{
  size_t colonPos = input.rfind(':');
  
  if (colonPos == std::string::npos) {
    ip = input;
    port = defaultPort;
    return true;
  }

  if (input.find(']') == std::string::npos) {
    ip = input.substr(0, colonPos);
    int32_t parsedPort = 0;
    try {
      parsedPort = std::stoi(input.substr(colonPos + 1));
    } catch (...) {
      return false;
    }
    if (parsedPort < 0 || 0xFFFF < parsedPort) {
      return false;
    }
    port = static_cast<uint16_t>(parsedPort);
    return true;
  }

  // This is an IPv6 literal, expect format: [IPv6]:PORT
  size_t startBracket = input.find('[');
  size_t endBracket = input.find(']');
  if (startBracket == std::string::npos || endBracket == std::string::npos || endBracket < startBracket) {
    return false;
  }

  ip = input.substr(startBracket + 1, endBracket - startBracket - 1);
  if (colonPos > endBracket) {
    int32_t parsedPort = 0;
    try {
      parsedPort = std::stoi(input.substr(colonPos + 1));
    } catch (...) {
      return false;
    }
    if (parsedPort < 0 || 0xFFFF < parsedPort) {
      return false;
    }
    port = static_cast<uint16_t>(parsedPort);
  } else {
    port = defaultPort;
  }

  return true;
}

template <size_t N>
constexpr std::array<std::string, N> StringArray(const char* const (&strings)[N]) {
  std::array<std::string, N> arr;
  for (size_t i = 0; i < N; ++i) {
    arr[i] = strings[i];
  }
  return arr;
}

[[nodiscard]] inline std::string EncodeURIComponent(const std::string& s) {
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

[[nodiscard]] inline std::string DecodeURIComponent(const std::string& encoded) {
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

[[nodiscard]] inline bool CaseInsensitiveEquals(const std::string& nameOne, const std::string& nameTwo) {
  std::string lowerOne = nameOne;
  std::string lowerTwo = nameTwo;
  std::transform(std::begin(lowerOne), std::end(lowerOne), std::begin(lowerOne), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::transform(std::begin(lowerTwo), std::end(lowerTwo), std::begin(lowerTwo), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return lowerOne == lowerTwo;
}

[[nodiscard]] inline bool FileNameEquals(const std::string& nameOne, const std::string& nameTwo) {
#ifndef _WIN32
  return nameOne == nameTwo;
#else
  return CaseInsensitiveEquals(nameOne, nameTwo);
#endif
}

[[nodiscard]] inline bool HasNullOrBreak(const std::string& unsafeInput) {
  for (const auto& c : unsafeInput) {
    if (c == '\0' || c == '\n' || c == '\r' || c == '\f') {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline bool PathHasNullBytes(const std::filesystem::path& filePath) {
  for (const auto& c : filePath.native()) {
    if (c == '\0') {
      return true;
    }
  }
  return false;
}

[[nodiscard]] inline bool IsASCII(const std::string& unsafeInput) {
  for (const auto& c : unsafeInput) {
    if ((c & 0x80) != 0) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline uint32_t ASCIIHexToNum(const std::array<uint8_t, 8>& data, bool reverse)
{
  std::string ascii;
  if (reverse) {
    ascii = std::string(data.rbegin(), data.rend());
  } else {
    ascii = std::string(data.begin(), data.end());
  }
  uint32_t result = 0;
  for (size_t j = 0; j < 4; j++) {
    unsigned int c;
    std::stringstream SS;
    SS << ascii.substr(j * 2, 2);
    SS >> std::hex >> c;
    result |= c << (24 - j * 8);
  }
  return result;
}

[[nodiscard]] inline std::array<uint8_t, 8> NumToASCIIHex(uint32_t num, bool reverse)
{
  std::array<uint8_t, 8> result;
  result.fill(0x30);

  for (size_t j = 0; j < 4; j++) {
    uint8_t c;
    if (reverse) {
      c = static_cast<uint8_t>(num >> (24 - j * 8));
    } else {
      c = static_cast<uint8_t>(num >> (j * 8));
    }
    std::string fragment = ToHexString(c);
    if (c > 0xF) {
      result[7 - (j * 2)] = fragment[0];
      result[6 - (j * 2)] = fragment[1];
    } else {
      result[6 - (j * 2)] = fragment[0];
    }
  }
  return result;
}

[[nodiscard]] inline std::string PreparePatternForFuzzySearch(const std::string& rawPattern)
{
  std::string pattern = rawPattern;
  std::transform(std::begin(pattern), std::end(pattern), std::begin(pattern), [](unsigned char c) {
    if (c == static_cast<char>(0x20)) return static_cast<char>(0x2d);
    return static_cast<char>(std::tolower(c));
  });
  return RemoveNonAlphanumericNorHyphen(pattern);
}

[[nodiscard]] inline std::string PrepareMapPatternForFuzzySearch(const std::string& rawPattern)
{
  std::string pattern = rawPattern;
  std::transform(std::begin(pattern), std::end(pattern), std::begin(pattern), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::string extension = ParseFileExtension(pattern);
  if (extension == ".w3x" || extension == ".w3m" || extension == ".ini") {
    pattern = pattern.substr(0, pattern.length() - extension.length());
  }
  return RemoveNonAlphanumeric(pattern);
}

[[nodiscard]] inline std::vector<std::string> ReadChatTemplate(const std::filesystem::path& filePath) {
  std::ifstream in;
  in.open(filePath.native().c_str(), std::ios::in);
  std::vector<std::string> fileContents;
  if (!in.fail()) {
    while (!in.eof()) {
      std::string line;
      std::getline(in, line);
      if (line.empty()) {
        if (!in.eof())
          fileContents.push_back(" ");
      } else {
        fileContents.push_back(line);
      }
    }
    in.close();
  }
  return fileContents;
}

[[nodiscard]] inline std::string GetNormalizedAlias(const std::string& alias)
{
  if (alias.empty()) return alias;

  std::string result;
  if (!utf8::is_valid(alias.begin(), alias.end())) {
    return result;
  }

  std::vector<unsigned short> utf16line;
  utf8::utf8to16(alias.begin(), alias.end(), std::back_inserter(utf16line));
  // Note that MSVC 2019 doesn't fully support unicode string literals.
  for (const auto& c : utf16line) {
    switch (c) {
      case 32: case 39: case 45: case 95: // whitespace ( ), single quote ('), hyphen (-), underscore (_)
        break;
      case 224: case 225: case 226: case 227: case 228: case 229: // à á â ã ä å
        result += 'a'; break;
      case 231: // ç
        result += 'c'; break;
      case 232: case 233: case 234: case 235: // è é ê ë
        result += 'e'; break;
      case 236: case 237: case 238: case 239: // ì í î ï
        result += 'i'; break;
      case 241: // ñ
        result += 'n'; break;
      case 242: case 243: case 244: case 245: case 246: case 248: // ò ó ô õ ö ø
        result += 'o'; break;
      case 249: case 250: case 251: case 252: // ù ú û ü
        result += 'u'; break;
      case 253: case 255: // ý ÿ
        result += 'y'; break;
      default:
        if (c <= 0x7f) {
          // Single-byte UTF-8 encoding for ASCII characters
          result += static_cast<char>(c & 0x7F);
          continue;
        } else if (c <= 0x7FF) {
          // Two-byte UTF-8 encoding
          result += static_cast<char>(0xC0 | ((c >> 6) & 0x1F));
          result += static_cast<char>(0x80 | (c & 0x3F));
        } else {
          // Three-byte UTF-8 encoding
          result += static_cast<char>(0xE0 | ((c >> 12) & 0x0F));
          result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
          result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
  }
  return result;
}

inline void NormalizeDirectory(std::filesystem::path& filePath)
{
  if (filePath.empty()) return;
  filePath += filePath.preferred_separator;
  filePath = filePath.lexically_normal();
}

[[nodiscard]] inline bool FindNextMissingElementBack(uint8_t& element, std::vector<uint8_t> counters)
{
  if (element == 0) return false;
  do {
    --element;
  } while (counters[element] != 0 && element > 0);
  return counters[element] == 0;
}

[[nodiscard]] inline std::optional<uint32_t> ToUint32(const std::string& input)
{
  std::optional<uint32_t> container = std::nullopt;
  if (input.empty()) return container;

  try {
    int64_t Value = std::stol(input);
    if (Value < 0 || 0xFFFFFFFF < Value) {
      return container;
    }
    container = static_cast<uint32_t>(Value);
  } catch (...) {}

  return container;
}

[[nodiscard]] inline std::optional<int32_t> ToInt32(const std::string& input)
{
  return ParseInt32(input, true);
}

[[nodiscard]] inline std::optional<double> ToDouble(const std::string& input)
{
  return ParseDouble(input);
}

[[nodiscard]] inline std::pair<std::string, std::string> SplitAddress(const std::string& fqName)
{
  std::string::size_type atSignPos = fqName.find('@');
  if (atSignPos == std::string::npos) {
    return std::make_pair(fqName, std::string());
  }
  return std::make_pair(
    TrimString(fqName.substr(0, atSignPos)),
    TrimString(fqName.substr(atSignPos + 1))
  );
}

[[nodiscard]] inline bool CheckTargetGameSyntax(const std::string& rawInput)
{
  if (rawInput.empty()) {
    return false;
  }
  std::string inputGame = ToLowerCase(rawInput);
  if (inputGame == "lobby" || inputGame == "game#lobby") {
    return true;
  }
  if (inputGame == "oldest" || inputGame == "game#oldest") {
    return true;
  }
  if (inputGame == "newest" || inputGame == "latest" || inputGame == "game#newest" || inputGame == "game#latest") {
    return true;
  }
  if (inputGame == "lobby#oldest") {
    return true;
  }
  if (inputGame == "lobby#newest") {
    return true;
  }
  if (inputGame.substr(0, 5) == "game#") {
    inputGame = inputGame.substr(5);
  } else if (inputGame.substr(0, 6) == "lobby#") {
    inputGame = inputGame.substr(6);
  }

  try {
    long long value = std::stoll(inputGame);
    return value >= 0;
  } catch (...) {
    return false;
  }
}

inline bool ReplaceText(std::string& input, const std::string& fragment, const std::string& replacement)
{
  std::string::size_type matchIndex = input.find(fragment);
  if (matchIndex == std::string::npos) return false;
  input.replace(matchIndex, fragment.size(), replacement);
  return true;
}

[[nodiscard]] inline std::optional<size_t> CountTemplateFixedChars(const std::string& input)
{
  size_t size = 0;
  size_t pos = 0;
  size_t start = 0;

  while ((start = input.find('{', pos)) != std::string::npos) {
    size += start - pos;

    size_t end = input.find('}', start);
    if (end == std::string::npos || end == start + 1) {
      return std::nullopt;
    }
    pos = end + 1;
  }

  size += input.size() - pos;
  return size;
}

[[nodiscard]] inline std::multiset<std::string> GetTemplateTokens(const std::string& input)
{
  std::multiset<std::string> tokens;
  size_t pos = 0;
  size_t start = 0;

  while ((start = input.find('{', pos)) != std::string::npos) {
    size_t end = input.find('}', start);
    if (end == std::string::npos || end == start + 1) {
      return std::multiset<std::string>();
    }
    tokens.insert(input.substr(start + 1, end - start - 1));
    pos = end + 1;
  }

  return tokens;
}

[[nodiscard]] inline std::string ReplaceTemplate(const std::string& input, const FlatMap<int64_t, bool>* boolCache, const FlatMap<int64_t, std::string>* textCache, const FlatMap<int64_t, std::function<bool()>>* boolFuncsMap, const FlatMap<int64_t, std::function<std::string()>>* textFuncsMap, bool tolerant = false)
{
  std::string result;
  std::string::size_type pos = 0;
  std::string::size_type start = 0;
  const bool* boolCacheMatch = nullptr;
  const std::string* textCacheMatch = nullptr;
  const std::function<bool()>* boolFuncMatch = nullptr;
  const std::function<std::string()>* textFuncMatch = nullptr;

  while ((start = input.find('{', pos)) != std::string::npos) {
    result.append(input, pos, start - pos);

    size_t end = input.find('}', start);
    if (end == std::string::npos || end == start + 1) {
      if (!tolerant) return std::string();
      result.append(input, start, input.size() - start);
      return result;
    }

    std::string token = input.substr(start + 1, end - start - 1);
    bool isCondition = token[0] == '?' || token[0] == '!';
    if (isCondition) {
      token = token.substr(1);
    }
    int64_t cacheKey = HashCode(token);

    if (isCondition) {
      bool checkResult = false;
      if (boolCache != nullptr && ((boolCacheMatch = boolCache->find(cacheKey)) != nullptr)) {
        checkResult = *boolCacheMatch;
      } else if (boolFuncsMap != nullptr && ((boolFuncMatch = boolFuncsMap->find(cacheKey)) != nullptr)) {
        bool value = (*boolFuncMatch)();
        checkResult = value;
      } else {
        if (!tolerant) return std::string();
        result.append("{").append(token).append("}");
      }
      if (checkResult == (token[0] == '?')) {
        pos = end + 1;
      } else {
        pos = input.find('\n', pos);
      }
    } else {
      if (textCache != nullptr && ((textCacheMatch = textCache->find(cacheKey)) != nullptr)) {
        result.append(*textCacheMatch);
      } else if (textFuncsMap != nullptr && ((textFuncMatch = textFuncsMap->find(cacheKey)) != nullptr)) {
        std::string value = (*textFuncMatch)();
        result.append(value);
      } else {
        if (!tolerant) return std::string();
        result.append("{").append(token).append("}");
      }
      pos = end + 1;
    }
  }

  result.append(input, pos, std::string::npos);
  return result;
}

[[nodiscard]] inline std::string ReplaceTemplate(const std::string& input, std::unordered_map<int64_t, bool>* boolCache, std::unordered_map<int64_t, std::string>* textCache, const FlatMap<int64_t, std::function<bool()>>* boolFuncsMap, const FlatMap<int64_t, std::function<std::string()>>* textFuncsMap, bool tolerant = false)
{
  std::string result;
  std::string::size_type pos = 0;
  std::string::size_type start = 0;
  std::unordered_map<int64_t, bool>::iterator boolCacheMatch;
  std::unordered_map<int64_t, std::string>::iterator textCacheMatch;
  const std::function<bool()>* boolFuncMatch = nullptr;
  const std::function<std::string()>* textFuncMatch = nullptr;

  while ((start = input.find('{', pos)) != std::string::npos) {
    result.append(input, pos, start - pos);

    size_t end = input.find('}', start);
    if (end == std::string::npos || end == start + 1) {
      if (!tolerant) return std::string();
      result.append(input, start, input.size() - start);
      return result;
    }

    std::string token = input.substr(start + 1, end - start - 1);
    bool isCondition = token[0] == '?' || token[0] == '!';
    if (isCondition) {
      token = token.substr(1);
    }
    int64_t cacheKey = HashCode(token);

    if (isCondition) {
      bool checkResult = false;
      if (boolCache != nullptr && ((boolCacheMatch = boolCache->find(cacheKey)) != boolCache->end())) {
        checkResult = boolCacheMatch->second;
      } else if (boolFuncsMap != nullptr && ((boolFuncMatch = boolFuncsMap->find(cacheKey)) != nullptr)) {
        bool value = (*boolFuncMatch)();
        if (boolCache != nullptr) {
          (*boolCache)[cacheKey] = value;
        }
        checkResult = value;
      } else {
        if (!tolerant) return std::string();
        result.append("{").append(token).append("}");
      }
      if (checkResult == (token[0] == '?')) {
        pos = end + 1;
      } else {
        pos = input.find('\n', pos);
      }
    } else {
      if (textCache != nullptr && ((textCacheMatch = textCache->find(cacheKey)) != textCache->end())) {
        result.append(textCacheMatch->second);
      } else if (textFuncsMap != nullptr && ((textFuncMatch = textFuncsMap->find(cacheKey)) != nullptr)) {
        std::string value = (*textFuncMatch)();
        if (textCache != nullptr) {
          (*textCache)[cacheKey] = value;
        }
        result.append(value);
      } else {
        if (!tolerant) return std::string();
        result.append("{").append(token).append("}");
      }
      pos = end + 1;
    }
  }

  result.append(input, pos, std::string::npos);
  return result;
}

[[nodiscard]] inline float LinearInterpolation(const float x, const float x1, const float x2, const float y1, const float y2)
{
  float y = y1 + (x - x1) * (y2 - y1) / (x2 - x1);
  return y;
}

/*
[[nodiscard]] inline float HyperbolicInterpolation(const float x, const float x1, const float x2, const float y1, const float y2)
{
  float y = y1 + (x - x1) * (y2 - y1) / (x2 - x1);
  return y;
}

[[nodiscard]] inline float ExponentialInterpolation(const float x, const float x1, const float x2, const float y1, const float y2)
{
  float y = y1 + (x - x1) * (y2 - y1) / (x2 - x1);
  return y;
}
*/

[[nodiscard]] inline uint32_t GetRandomUInt32()
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dis;
  return dis(gen);
}

#endif // AURA_UTIL_H_
