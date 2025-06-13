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

#include "json.h"
#include "parser.h"

using namespace std;

namespace JSONAPI
{
  optional<string> GetMaybeString(const nlohmann::json& data)
  {
    optional<string> result;
    if (data.is_discarded() || data.type() != nlohmann::json::value_t::string) {
      return result;
    }
    result.emplace();
    data.get_to(*result);
    return result;
  }

  optional<vector<string>> GetMaybeStringArray(const nlohmann::json& data)
  {
    optional<vector<string>> result;
    if (data.is_discarded() || data.type() != nlohmann::json::value_t::array) {
      return result;
    }
    size_t size = data.size();
    vector<string> entries;
    entries.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      nlohmann::json j = data.at(i);
      if (j.type() != nlohmann::json::value_t::string) {
        return result;
      }
      entries.push_back(string());
      j.get_to(entries.back());
    }
    result = entries;
    return result;
  }

  optional<string> ParseString(const string& encoded)
  {
    nlohmann::json data = nlohmann::json::parse(encoded, nullptr, false, false);
    return JSONAPI::GetMaybeString(data);
  }

  optional<vector<string>> ParseStringArray(const string& encoded)
  {
    nlohmann::json data = nlohmann::json::parse(encoded, nullptr, false, false);
    return JSONAPI::GetMaybeStringArray(data);
  }

  // The parser will use number_unsigned_t for all positive integers (including 0) if they fit into 64 bits.
  // It will use number_integer_t for negative integers if they fit into 64 bits.
  // In both cases, it falls back to double otherwise.

  optional<vector<uint64_t>> ParseUnsignedIntArray(const string& encoded)
  {
    optional<vector<uint64_t>> result;
    nlohmann::json data = nlohmann::json::parse(encoded, nullptr, false, false);
    if (data.is_discarded() || data.type() != nlohmann::json::value_t::array) {
      return result;
    }
    size_t size = data.size();
    vector<uint64_t> entries;
    entries.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      nlohmann::json j = data.at(i);
      uint64_t element = 0;
      if (j.is_number_unsigned()) {
        j.get_to(element);
      } else if (j.is_string()) {
        string numeral;
        j.get_to(numeral);
        optional<uint64_t> maybeInt = ParseUint64(numeral);
        if (!maybeInt.has_value()) {
          return result;
        }
        element = *maybeInt;
      } else {
        return result;
      }
      entries.push_back(element);
    }
    result = entries;
    return result;
  }

  optional<vector<int64_t>> ParseSignedIntArray(const string& encoded)
  {
    optional<vector<int64_t>> result;
    nlohmann::json data = nlohmann::json::parse(encoded, nullptr, false, false);
    if (data.is_discarded() || data.type() != nlohmann::json::value_t::array) {
      return result;
    }
    size_t size = data.size();
    vector<int64_t> entries;
    entries.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      nlohmann::json j = data.at(i);
      int64_t element = 0;
      if (j.is_number_unsigned()) {
        uint64_t unsignedValue = 0u;
        j.get_to(unsignedValue);
        if (unsignedValue > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
          return result;
        }
        element = static_cast<int64_t>(unsignedValue);
      } else if (j.is_number_integer()) {
        j.get_to(element);
      } else if (j.is_string()) {
        string numeral;
        j.get_to(numeral);
        optional<int64_t> maybeInt = ParseInt64(numeral);
        if (!maybeInt.has_value()) {
          return result;
        }
        element = *maybeInt;
      } else {
        return result;
      }
      entries.push_back(element);
    }
    result = entries;
    return result;
  }

  optional<vector<double>> ParseDoubleArray(const string& encoded)
  {
    optional<vector<double>> result;
    nlohmann::json data = nlohmann::json::parse(encoded, nullptr, false, false);
    if (data.is_discarded() || data.type() != nlohmann::json::value_t::array) {
      return result;
    }
    size_t size = data.size();
    vector<double> entries;
    entries.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      nlohmann::json j = data.at(i);
      double element = 0.;
      if (j.is_number()) {
        j.get_to(element);
      } else if (j.is_string()) {
        string numeral;
        j.get_to(numeral);
        optional<double> maybeDouble = ParseDouble(numeral);
        if (!maybeDouble.has_value()) {
          return result;
        }
        element = *maybeDouble;
      } else {
        return result;
      }
      entries.push_back(element);
    }
    result = entries;
    return result;
  }
};
