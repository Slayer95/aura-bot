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

#ifndef AURA_PJASS_H
#define AURA_PJASS_H

#include "includes.h"
#include "file_util.h"

#ifndef DISABLE_PJASS
#include "../deps/pjass/include/pjass-header.h"

std::pair<bool, std::string> ParseJASS(const std::vector<std::filesystem::path>& filePaths);

inline std::string ExtractFirstJASSError(const std::string& input)
{
  // TODO: Mask file path
  std::string::size_type nlIndex1 = input.find('\n');
  if (nlIndex1 == std::string::npos) return input;
  std::string::size_type nlIndex2 = input.find('\n', nlIndex1 + 1);
  if (nlIndex2 == std::string::npos) return input.substr(nlIndex1 + 1);
  return input.substr(nlIndex1 + 1, nlIndex2 - (nlIndex1 + 1));
}

#endif

#endif // AURA_PJASS_H
