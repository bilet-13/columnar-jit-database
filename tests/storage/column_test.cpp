#include "storage/column.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "common/types.h"
#include "storage/table.h"

namespace minidb {
namespace {

Column MakeColumn(std::uint64_t num_rows) {
  Column column;
  column.Reserve(num_rows);
  for (std::uint64_t i = 0; i < num_rows; ++i) {
    column.Append(static_cast<std::int64_t>(i) * 10);
  }
  return column;
}

TEST(Column, EmptyColumnHasNoChunks) {
  const Column column;
  EXPECT_EQ(column.num_rows(), 0u);
  EXPECT_EQ(column.num_chunks(), 0u);
  EXPECT_THROW(column.At(0), std::out_of_range);
  EXPECT_THROW(column.chunk_data(0), std::out_of_range);
}

TEST(Column, SingleRow) {
  const Column column = MakeColumn(1);
  EXPECT_EQ(column.num_rows(), 1u);
  EXPECT_EQ(column.num_chunks(), 1u);
  EXPECT_EQ(column.chunk_rows(0), 1u);
  EXPECT_EQ(column.At(0), 0);
}

// ---------------------------------------------------------------------------
// chunk 邊界是這個類別唯一容易寫錯的地方，三個關鍵列數各測一次。
// ---------------------------------------------------------------------------

TEST(Column, OneRowBeforeChunkBoundary) {
  const Column column = MakeColumn(kChunkSize - 1);
  EXPECT_EQ(column.num_chunks(), 1u);
  EXPECT_EQ(column.chunk_rows(0), kChunkSize - 1);
}

TEST(Column, ExactlyOneFullChunk) {
  const Column column = MakeColumn(kChunkSize);
  EXPECT_EQ(column.num_chunks(), 1u);
  EXPECT_EQ(column.chunk_rows(0), kChunkSize);
  EXPECT_THROW(column.chunk_data(1), std::out_of_range);
}

TEST(Column, OneRowAfterChunkBoundary) {
  const Column column = MakeColumn(kChunkSize + 1);
  ASSERT_EQ(column.num_chunks(), 2u);
  EXPECT_EQ(column.chunk_rows(0), kChunkSize);
  EXPECT_EQ(column.chunk_rows(1), 1u);
  // 跨過邊界的那兩列
  EXPECT_EQ(column.At(kChunkSize - 1), static_cast<std::int64_t>(kChunkSize - 1) * 10);
  EXPECT_EQ(column.At(kChunkSize), static_cast<std::int64_t>(kChunkSize) * 10);
}

TEST(Column, AtMatchesAppendOrderAcrossChunks) {
  const std::uint64_t num_rows = kChunkSize * 2 + 123;
  const Column column = MakeColumn(num_rows);
  ASSERT_EQ(column.num_rows(), num_rows);
  ASSERT_EQ(column.num_chunks(), 3u);
  for (std::uint64_t row : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{kChunkSize - 1},
                            std::uint64_t{kChunkSize}, std::uint64_t{kChunkSize * 2},
                            num_rows - 1}) {
    EXPECT_EQ(column.At(row), static_cast<std::int64_t>(row) * 10) << "row = " << row;
  }
  EXPECT_THROW(column.At(num_rows), std::out_of_range);
}

// 走訪所有 chunk 的連續記憶體，應該剛好重建整個欄位——scan 之後就是這樣讀資料的。
TEST(Column, WalkingChunksReproducesWholeColumn) {
  const std::uint64_t num_rows = kChunkSize + kBatchSize + 7;
  const Column column = MakeColumn(num_rows);

  std::vector<std::int64_t> seen;
  seen.reserve(num_rows);
  for (std::uint32_t chunk = 0; chunk < column.num_chunks(); ++chunk) {
    const std::int64_t* data = column.chunk_data(chunk);
    const std::uint32_t rows = column.chunk_rows(chunk);
    seen.insert(seen.end(), data, data + rows);
  }

  ASSERT_EQ(seen.size(), num_rows);
  for (std::uint64_t i = 0; i < num_rows; ++i) {
    ASSERT_EQ(seen[i], static_cast<std::int64_t>(i) * 10) << "i = " << i;
  }
}

// 一個 batch 永遠不跨 chunk 邊界，這是 scan 能零複製的前提。
TEST(Column, BatchNeverStraddlesChunkBoundary) {
  static_assert(kChunkSize % kBatchSize == 0);
  const Column column = MakeColumn(kChunkSize * 2);
  for (std::uint32_t chunk = 0; chunk < column.num_chunks(); ++chunk) {
    EXPECT_EQ(column.chunk_rows(chunk) % kBatchSize, 0u);
  }
}

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------

Table MakeOrders() {
  return Table("orders", Schema({"id", "cust_id", "amount"}));
}

TEST(Table, SchemaIsQualifiedWithTableName) {
  const Table orders = MakeOrders();
  EXPECT_EQ(orders.name(), "orders");
  ASSERT_EQ(orders.num_columns(), 3u);
  EXPECT_EQ(orders.schema().column_name(0), "orders.id");
  EXPECT_EQ(orders.schema().column_name(2), "orders.amount");
  EXPECT_EQ(orders.schema().ResolveColumnIndex("amount"), 2u);
}

TEST(Table, AppendRowKeepsColumnsInSync) {
  Table orders = MakeOrders();
  const std::int64_t row0[] = {1, 7, 250};
  const std::int64_t row1[] = {2, 3, 80};
  orders.AppendRow(row0);
  orders.AppendRow(row1);

  EXPECT_EQ(orders.num_rows(), 2u);
  for (std::uint32_t i = 0; i < orders.num_columns(); ++i) {
    EXPECT_EQ(orders.column(i).num_rows(), 2u);
  }
  EXPECT_EQ(orders.column(2).At(0), 250);
  EXPECT_EQ(orders.column(2).At(1), 80);
}

TEST(Table, AppendRowRejectsWrongWidth) {
  Table orders = MakeOrders();
  const std::int64_t too_short[] = {1, 2};
  EXPECT_THROW(orders.AppendRow(too_short), std::invalid_argument);
  EXPECT_EQ(orders.num_rows(), 0u);
}

TEST(Table, ColumnIndexOutOfRangeThrows) {
  Table orders = MakeOrders();
  EXPECT_THROW(orders.column(3), std::out_of_range);
}

}  // namespace
}  // namespace minidb
