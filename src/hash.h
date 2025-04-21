#ifndef AURA_HASH_H_
#define AURA_HASH_H_

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <cstdint>

// compile time functions for char*

static constexpr uint64_t HashCode(const char* str, const uint64_t pos, const uint64_t hash)
{
  return str[pos] ? (HashCode(str, pos + 1, 31 * hash + str[pos])) : hash;
}

static constexpr uint64_t HashCode(const char* str)
{
  return HashCode(str, 0, 7);
}

template<typename K, typename V, size_t N>
constexpr std::array<std::pair<K, V>, N> SortPairs(std::array<std::pair<K, V>, N> input) {
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = i + 1; j < N; ++j) {
      if (input[j].first < input[i].first) {
        auto tmp = input[i];
        input[i] = input[j];
        input[j] = tmp;
      }
    }
  }
  return input;
}

// runtime function for std::string

[[nodiscard]] static uint64_t HashCode(const std::string& str)
{
  return HashCode(str.c_str());
}

template<typename K, typename V, size_t N>
[[nodiscard]] static std::array<std::pair<K, V>, N>& SortPairs(std::array<std::pair<K, V>, N>& input) {
  std::sort(std::begin(input), std::end(input), [](const std::pair<K, V>& a, const std::pair<K, V>& b) {
    return a.first < b.first;
  });
  return input;
}

#endif
