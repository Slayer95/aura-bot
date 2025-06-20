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

#ifndef AURA_INCLUDES_H_
#define AURA_INCLUDES_H_

// STL

#include <cmath>
#include <cstring>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>

#include <array>
#include <bitset>
#include <functional>
#include <string>
#include <string_view>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_set>

std::string::size_type constexpr GetStringLength(const char* str)
{
    return *str ? 1 + GetStringLength(str + 1) : 0;
}


#include "constants.h"
#include "forward.h"

#ifdef WIN32
#ifndef _WIN32
#define _WIN32
#endif
#endif

#ifdef _WIN32
#define PLATFORM_STRING_TYPE std::wstring
#define WIDEN(T) L##T
#define PLATFORM_STRING(T) WIDEN(T)
#else
#define PLATFORM_STRING_TYPE std::string
#define PLATFORM_STRING(T) T
#endif

#ifdef min
#undef min
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _MSVC
#define VC_EXTRALEAN
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#if defined(__clang__) || defined(__GNUC__)
#define UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define UNREACHABLE() __assume(0)
#else
#define UNREACHABLE() do {} while (0) // Fallback: does nothing
#endif

#define IGNORE_CASE(T) \
    case T: UNREACHABLE(); break;

#define IGNORE_ENUM_LAST(EnumType) IGNORE_CASE(EnumType::LAST)

#define PRINT_IF(T, U) \
    static_assert(T < LogLevel::LAST, "Use DPRINT_IF for tracing log levels");\
    if (m_Aura->MatchLogLevel(T)) {\
        Print(U); \
    }

#ifdef DEBUG
#define DPRINT_IF(T, U) \
    static_assert(T < LogLevel::LAST, "Invalid tracing log level");\
    static_assert(T >= LogLevel::kTrace, "Use PRINT_IF for regular log levels");\
    if (m_Aura->MatchLogLevel(T)) {\
        Print(U); \
    }
#else
#define DPRINT_IF(T, U)
#endif

#define GAMEVER(T, U) Version((uint8_t)(T), (uint8_t)(U))

#define APP_MAX_TICKS std::numeric_limits<int64_t>::max()
#define APP_MIN_TICKS std::numeric_limits<int64_t>::min()

// time

inline int64_t GetTime()
{
  const std::chrono::steady_clock::time_point time_now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(time_now.time_since_epoch()).count();
}

inline int64_t GetTicks()
{
  const std::chrono::steady_clock::time_point time_now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(time_now.time_since_epoch()).count();
}

inline void LogStream(std::ostream& outStream, const std::string& message, bool details = false)
{
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm timeinfo;
#ifdef _WIN32
  localtime_s(&timeinfo, &now);
#else
  localtime_r(&now, &timeinfo);
#endif
  if (details) {
    outStream << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "] " << message << std::endl;
  } else {
    outStream << "[" << std::put_time(&timeinfo, "%H:%M:%S") << "] " << message << std::endl;
  }
  outStream << std::flush;
}

inline void LogStream(std::ostream& outStream, const char* message, bool details = false)
{
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm timeinfo;
#ifdef _WIN32
  localtime_s(&timeinfo, &now);
#else
  localtime_r(&now, &timeinfo);
#endif
  if (details) {
    outStream << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "] " << message << std::endl;
  } else {
    outStream << "[" << std::put_time(&timeinfo, "%H:%M:%S") << "] " << message << std::endl;
  }
  outStream << std::flush;
}

inline void Print(const std::string& message) // outputs to console
{
  LogStream(std::cout, message);
}

inline void Print(const char* message)
{
  LogStream(std::cout, message);
}

[[nodiscard]] inline std::string ToLowerCase(const std::string& input)
{
  std::string output = input;
  std::transform(std::begin(output), std::end(output), std::begin(output), [](char c) { return static_cast<char>(std::tolower(c)); });
  return output;
}

[[nodiscard]] inline bool IsBase10NaturalOrZero(const std::string& s)
{
  if (s.empty()) return false;
  if (s[0] == '0') return s.length() == 1;

  for (char ch : s) {
    if (!isdigit(ch)) return false;
  }
  return true;
}

[[nodiscard]] inline bool GetIsHostBigEndian()
{
  uint16_t value = 0x0102;
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&value);
  return ptr[0] == 0x01;
}

#endif // AURA_INCLUDES_H_
