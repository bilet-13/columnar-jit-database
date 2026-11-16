#pragma once

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace minidb {

// ---------------------------------------------------------------------------
// HashTable：hash join 的 build 端用的雜湊表（docs/design.md 7.5）。
//
// Open addressing + linear probing：兩個 key 算到同一個位置時，往後找下一個
// 空格（另一種流派是在每個位置掛鏈結串列）。選它的理由是探測時走的是連續
// 記憶體，對 cache 友善得多。
//
// key 與列號存在同一個陣列的同一個 entry 裡，而不是兩個平行陣列。probe 時
// 要同時用到這兩個值，放在一起只碰一條 cache line，分開放就是兩次。
//
// 刻意允許重複的 key，因為 join 可以是多對多。做法是同一個 key 佔用多個
// slot；探測時從 hash 位置往前走，遇到相同 key 就算一個配對，走到空格才停。
// 所以 ForEachMatch 可能回報多次。
// ---------------------------------------------------------------------------
class HashTable {
 public:
  static constexpr std::uint32_t kEmpty = std::numeric_limits<std::uint32_t>::max();

  struct Entry {
    std::int64_t key = 0;
    std::uint32_t row = kEmpty;  // kEmpty 代表這個 slot 是空的
  };

  // 預留容量，避免插入過程中反覆重建。
  void Reserve(std::uint64_t expected_entries);

  void Insert(std::int64_t key, std::uint32_t row);

  std::uint64_t num_entries() const { return num_entries_; }
  std::uint64_t capacity() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

  // 探測的起點。批次 probe 會先把一整批的起點都算出來，再回頭查表。
  std::uint64_t SlotFor(std::int64_t key) const { return Hash(key) & mask_; }

  // 告訴 CPU「等一下會用到這條 cache line」。批次 probe 靠它把記憶體延遲
  // 藏在其他 row 的計算後面。
  void Prefetch(std::uint64_t slot) const {
    __builtin_prefetch(&entries_[slot]);
  }

  // 從指定的起點開始探測。對每個 key 相同的 row 呼叫一次 fn(row)。
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
  // 乘法 hash（黃金比例常數），避開除法，而且會把低位的規律打散。
  static std::uint64_t Hash(std::int64_t key) {
    std::uint64_t x = static_cast<std::uint64_t>(key);
    x *= 0x9E3779B97F4A7C15ULL;
    return x ^ (x >> 29);
  }

  void Rehash(std::uint64_t new_capacity);

  std::vector<Entry> entries_;
  std::uint64_t mask_ = 0;  // capacity - 1，capacity 一律是 2 的次方
  std::uint64_t num_entries_ = 0;
};

}  // namespace minidb
