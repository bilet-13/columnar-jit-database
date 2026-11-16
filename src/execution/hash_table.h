#pragma once

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace minidb {

class HashTable {
 public:
  static constexpr std::uint32_t kEmpty = std::numeric_limits<std::uint32_t>::max();

  struct Entry {
    std::int64_t key = 0;
    std::uint32_t row = kEmpty;
  };

  void Reserve(std::uint64_t expected_entries);

  void Insert(std::int64_t key, std::uint32_t row);

  std::uint64_t num_entries() const { return num_entries_; }
  std::uint64_t capacity() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

  std::uint64_t SlotFor(std::int64_t key) const { return Hash(key) & mask_; }

  void Prefetch(std::uint64_t slot) const {
    __builtin_prefetch(&entries_[slot]);
  }

  template <typename Fn>
  void ForEachMatchFrom(std::int64_t key, std::uint64_t slot, Fn&& fn) const {
    if (entries_.empty()) {
      return;
    }
    while (entries_[slot].row != kEmpty) {
      if (entries_[slot].key == key) {
        fn(entries_[slot].row);
      }
      slot = (slot + 1) & mask_;
    }
  }

  template <typename Fn>
  void ForEachMatch(std::int64_t key, Fn&& fn) const {
    if (entries_.empty()) {
      return;
    }
    ForEachMatchFrom(key, SlotFor(key), std::forward<Fn>(fn));
  }

  bool Contains(std::int64_t key) const;
  std::uint32_t CountMatches(std::int64_t key) const;

 private:
  static std::uint64_t Hash(std::int64_t key) {
    std::uint64_t x = static_cast<std::uint64_t>(key);
    x *= 0x9E3779B97F4A7C15ULL;
    return x ^ (x >> 29);
  }

  void Rehash(std::uint64_t new_capacity);

  std::vector<Entry> entries_;
  std::uint64_t mask_ = 0;
  std::uint64_t num_entries_ = 0;
};

}
