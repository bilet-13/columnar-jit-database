#include "execution/hash_table.h"

#include <stdexcept>

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
  const std::uint64_t wanted =
      RoundUpToPowerOfTwo(static_cast<std::uint64_t>(
          static_cast<double>(expected_entries) / kMaxLoadFactor) + 1);
  if (wanted > capacity()) {
    Rehash(wanted);
  }
}

void HashTable::Insert(std::int64_t key, std::uint32_t row) {
  if (row == kEmpty) {
    throw std::invalid_argument("HashTable::Insert: 列號不可等於空值標記");
  }
  if (rows_.empty() ||
      static_cast<double>(num_entries_ + 1) > static_cast<double>(capacity()) * kMaxLoadFactor) {
    Rehash(rows_.empty() ? 16 : capacity() * 2);
  }

  std::uint64_t slot = Hash(key) & mask_;
  while (rows_[slot] != kEmpty) {
    slot = (slot + 1) & mask_;  // 重複的 key 也會佔用新的 slot
  }
  keys_[slot] = key;
  rows_[slot] = row;
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
  std::vector<std::int64_t> old_keys = std::move(keys_);
  std::vector<std::uint32_t> old_rows = std::move(rows_);

  keys_.assign(new_capacity, 0);
  rows_.assign(new_capacity, kEmpty);
  mask_ = new_capacity - 1;
  num_entries_ = 0;

  for (std::size_t i = 0; i < old_rows.size(); ++i) {
    if (old_rows[i] == kEmpty) {
      continue;
    }
    std::uint64_t slot = Hash(old_keys[i]) & mask_;
    while (rows_[slot] != kEmpty) {
      slot = (slot + 1) & mask_;
    }
    keys_[slot] = old_keys[i];
    rows_[slot] = old_rows[i];
    ++num_entries_;
  }
}

}  // namespace minidb
