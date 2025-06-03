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

#ifndef AURA_OPTIONAL_H_
#define AURA_OPTIONAL_H_

#include "includes.h"

template<typename T>
class OptReader {
  const std::optional<T>& source;

public:
  void operator>>(T& target) const
  {
    if (source.has_value()) target = *source;
  }

  void operator>>(std::optional<T>& target) const
  {
    if (source.has_value()) target = *source;
  }

  OptReader(const std::optional<T>& nSource)
   : source(nSource)
  {
  };

  ~OptReader() = default;
};

template<typename T>
class OptWriter {
  std::optional<T>& target;

public:
  void operator<<(T& source)
  {
    target = source;
  }

  void operator<<(const std::optional<T>& source)
  {
    target = *source;
  }

  OptWriter(std::optional<T>& nTarget)
   : target(nTarget)
  {
  };

  ~OptWriter() = default;
};

template<typename T>
OptReader<T> ReadOpt(const std::optional<T>& opt) {
  return {opt};
}

template<typename T>
OptWriter<T> WriteOpt(std::optional<T>& value) {
  return {value};
}

#endif // AURA_OPTIONAL_H_
