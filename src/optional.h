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
  std::reference_wrapper<const std::optional<T>> source;

public:
  OptReader(const std::optional<T>& nSource);
  ~OptReader();

  void operator>>(T& target) const;
  void operator>>(std::optional<T>& target) const;
};

template<typename T>
class OptWriter {
  std::reference_wrapper<std::optional<T>> target;

public:
  OptWriter(std::optional<T>& nTarget);
  ~OptWriter();

  void operator<<(const T& source);
  void operator<<(const std::optional<T>& source);
};

template<typename T>
OptReader<T> ReadOpt(const std::optional<T>& opt);

template<typename T>
OptWriter<T> WriteOpt(std::optional<T>& value);

#endif // AURA_OPTIONAL_H_
