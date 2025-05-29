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

#ifndef AURA_FLAT_MAP_H
#define AURA_FLAT_MAP_H

#include <vector>
#include <utility>
#include <algorithm>
#include <stdexcept>
#include <initializer_list>

template<typename K, typename V>
class FlatMap {
public:
  using KVPair = std::pair<K, V>;

  FlatMap() = default;

  static bool comp(const KVPair& a, const KVPair& b) {
    return a.first < b.first;
  }

  explicit FlatMap(size_t capacity)
  {
    data_.reserve(capacity);
  }

  explicit FlatMap(std::vector<KVPair>&& items)
  {
    if (!std::is_sorted(items.begin(), items.end(), &comp)) {
      std::sort(items.begin(), items.end(), &comp);
    }
    data_ = std::move(items);
  }

  template<size_t N>
  explicit FlatMap(const std::array<KVPair, N>& arr)
   : FlatMap(N)
  {
    data_.insert(data_.end(), arr.begin(), arr.end());
  }

  void reserve(size_t n)
  {
    data_.reserve(n);
  }

  void insert(const K& key, const V& value)
  {
    auto it = std::lower_bound(data_.begin(), data_.end(), key,
      [](const KVPair& pair, const K& val) {
        return pair.first < val;
      });

    if (it != data_.end() && it->first == key) {
      it->second = value;
    } else {
      data_.insert(it, KVPair{key, value});
    }
  }

  const V* find(const K& key) const
  {
    auto it = std::lower_bound(data_.begin(), data_.end(), key,
      [](const KVPair& pair, const K& val) {
        return pair.first < val;
      });

    return (it != data_.end() && it->first == key) ? &it->second : nullptr;
  }

  V& operator[](const K& key)
  {
    auto it = std::lower_bound(data_.begin(), data_.end(), key,
      [](const KVPair& pair, const K& val) {
        return pair.first < val;
      });

    if (it == data_.end() || it->first != key) {
      it = data_.insert(it, KVPair{key, V{}});
    }
    return it->second;
  }

  size_t size() const noexcept { return data_.size(); }
  bool empty() const noexcept { return data_.empty(); }
  const std::vector<KVPair>& get() const noexcept { return data_; }

private:
  std::vector<KVPair> data_;
};

#endif // AURA_FLAT_MAP_H
