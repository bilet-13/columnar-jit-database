#include "execution/hash_table.h"

#include <stdexcept>
#include <utility>

namespace minidb {
namespace {

// 負載因子的上限。linear probing 在接近滿的時候探測鏈會變長，0.7 是常見的
// 折衷點。
constexpr double kMaxLoadFactor = 0.7;

std::uint64_t RoundUpToPowerOfTwo(std::uint64_t value) {
  std::uint64_t capacity = 16;
  while (capacity < value) {
    capacity *= 2;
  }
  return capacity;
}

}  // namespace

void HashTable::Reserve(std::uint64_t expected_entries) {
  const std::uint64_t wanted = RoundUpToPowerOfTwo(
      static_cast<std::uint64_t>(static_cast<double>(expected_entries) / kMaxLoadFactor) + 1);
  if (wanted > capacity()) {
    Rehash(wanted);
  }
}

void HashTable::Insert(std::int64_t key, std::uint32_t row) {
  if (row == kEmpty) {
    throw std::invalid_argument("HashTable::Insert: 列號不可等於空值標記");
  }
  if (entries_.empty() ||
      static_cast<double>(num_entries_ + 1) > static_cast<double>(capacity()) * kMaxLoadFactor) {
    Rehash(entries_.empty() ? 16 : capacity() * 2);
  }

  std::uint64_t slot = SlotFor(key);
  while (entries_[slot].row != kEmpty) {
    slot = (slot + 1) & mask_;  // 重複的 key 也會佔用新的 slot
  }
  entries_[slot] = Entry{key, row};
  ++num_entries_;
}

bool HashTable::Contains(std::int64_t key) const {
  bool found = false;
  ForEachMatch(key, [&found](std::uint32_t) { found = true; });
  return found;
}

std::uint32_t HashTable::CountMatches(std::int64_t key) const {
  std::uint32_t count = 0;
  ForEachMatch(key, [&count](std::uint32_t) { ++count; });
  return count;
}

void HashTable::Rehash(std::uint64_t new_capacity) {
  std::vector<Entry> old_entries = std::move(entries_);

  entries_.assign(new_capacity, Entry{});
  mask_ = new_capacity - 1;
  num_entries_ = 0;

  for (const Entry& entry : old_entries) {
    if (entry.row == kEmpty) {
      continue;
    }
    std::uint64_t slot = SlotFor(entry.key);
    while (entries_[slot].row != kEmpty) {
      slot = (slot + 1) & mask_;
    }
    entries_[slot] = entry;
    ++num_entries_;
  }
}

}  // namespace minidb
