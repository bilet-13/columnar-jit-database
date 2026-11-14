#include "execution/hash_table.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace minidb {
namespace {

std::vector<std::uint32_t> Matches(const HashTable& table, std::int64_t key) {
  std::vector<std::uint32_t> rows;
  table.ForEachMatch(key, [&rows](std::uint32_t row) { rows.push_back(row); });
  return rows;
}

TEST(HashTable, EmptyTableFindsNothing) {
  const HashTable table;
  EXPECT_EQ(table.num_entries(), 0u);
  EXPECT_FALSE(table.Contains(42));
  EXPECT_TRUE(Matches(table, 42).empty());
}

TEST(HashTable, InsertAndFind) {
  HashTable table;
  table.Insert(10, 0);
  table.Insert(20, 1);
  table.Insert(30, 2);

  EXPECT_EQ(table.num_entries(), 3u);
  EXPECT_EQ(Matches(table, 10), std::vector<std::uint32_t>{0});
  EXPECT_EQ(Matches(table, 20), std::vector<std::uint32_t>{1});
  EXPECT_EQ(Matches(table, 30), std::vector<std::uint32_t>{2});
  EXPECT_FALSE(table.Contains(40));
}

// join 可以是多對多，所以 build 端允許重複的 key
TEST(HashTable, DuplicateKeysAllReported) {
  HashTable table;
  table.Insert(7, 0);
  table.Insert(7, 1);
  table.Insert(7, 2);
  table.Insert(8, 3);

  std::vector<std::uint32_t> rows = Matches(table, 7);
  std::sort(rows.begin(), rows.end());
  EXPECT_EQ(rows, (std::vector<std::uint32_t>{0, 1, 2}));
  EXPECT_EQ(table.CountMatches(7), 3u);
  EXPECT_EQ(table.CountMatches(8), 1u);
  EXPECT_EQ(table.CountMatches(9), 0u);
}

// 插到需要重建好幾次，每個 key 都還要找得到
TEST(HashTable, SurvivesGrowth) {
  HashTable table;
  constexpr std::uint32_t kCount = 5000;
  for (std::uint32_t i = 0; i < kCount; ++i) {
    table.Insert(static_cast<std::int64_t>(i) * 3, i);
  }

  EXPECT_EQ(table.num_entries(), kCount);
  EXPECT_GE(table.capacity(), kCount);
  for (std::uint32_t i = 0; i < kCount; ++i) {
    EXPECT_EQ(Matches(table, static_cast<std::int64_t>(i) * 3),
              std::vector<std::uint32_t>{i})
        << "i = " << i;
  }
  EXPECT_FALSE(table.Contains(1));  // 不是 3 的倍數
}

// 負數與極端值也要正常（乘法 hash 會 wrap around，那是定義好的行為）
TEST(HashTable, HandlesNegativeAndExtremeKeys) {
  HashTable table;
  const std::vector<std::int64_t> keys = {0, -1, -1000000, INT64_MIN, INT64_MAX};
  for (std::uint32_t i = 0; i < keys.size(); ++i) {
    table.Insert(keys[i], i);
  }
  for (std::uint32_t i = 0; i < keys.size(); ++i) {
    EXPECT_EQ(Matches(table, keys[i]), std::vector<std::uint32_t>{i});
  }
}

TEST(HashTable, ReserveAvoidsRehashing) {
  HashTable table;
  table.Reserve(1000);
  const std::uint64_t capacity = table.capacity();
  EXPECT_GE(capacity, 1000u);
  for (std::uint32_t i = 0; i < 700; ++i) {
    table.Insert(i, i);
  }
  EXPECT_EQ(table.capacity(), capacity) << "預留過就不該再重建";
}

TEST(HashTable, RejectsSentinelRow) {
  HashTable table;
  EXPECT_THROW(table.Insert(1, HashTable::kEmpty), std::invalid_argument);
}

}  // namespace
}  // namespace minidb
