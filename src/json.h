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

#ifndef AURA_JSON_H
#define AURA_JSON_H

#include "includes.h"

#ifdef DISABLE_DPP
#include <nlohmann/json.hpp>
#else
#include <dpp/nlohmann/json.hpp>
#endif

namespace JSONAPI
{
  [[nodiscard]] std::optional<std::string> GetMaybeString(const nlohmann::json& data);
  [[nodiscard]] std::optional<std::vector<std::string>> GetMaybeStringArray(const nlohmann::json& data);

  [[nodiscard]] std::optional<std::string> ParseString(const std::string& encoded);
  [[nodiscard]] std::optional<std::vector<std::string>> ParseStringArray(const std::string& encoded);
  [[nodiscard]] std::optional<std::vector<uint64_t>> ParseUnsignedIntArray(const std::string& encoded);
  [[nodiscard]] std::optional<std::vector<int64_t>> ParseSignedIntArray(const std::string& encoded);
  [[nodiscard]] std::optional<std::vector<double>> ParseDoubleArray(const std::string& encoded);
};

#endif // AURA_JSON_H
