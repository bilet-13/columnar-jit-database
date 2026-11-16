#include "execution/seq_scan.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "common/types.h"
#include "execution/exec_mode.h"

namespace minidb {
namespace {

Table MakeOrders(std::uint64_t num_rows) {
  Table table("orders", Schema({"id", "amount"}));
  table.Reserve(num_rows);
  for (std::uint64_t i = 0; i < num_rows; ++i) {
    const std::int64_t row[] = {static_cast<std::int64_t>(i), static_cast<std::int64_t>(i) * 10};
    table.AppendRow(row);
  }
  return table;
}

struct Drained {
  std::vector<std::int64_t> ids;
  std::vector<std::int64_t> amounts;
  std::uint32_t num_batches = 0;
  std::uint32_t max_batch_rows = 0;
};

Drained Drain(Operator& op) {
  Drained drained;
  while (Batch* batch = op.Next()) {
    EXPECT_GT(batch->ActiveRows(), 0u) << "operator 不該回傳空 batch";
    drained.num_batches++;
    drained.max_batch_rows = std::max(drained.max_batch_rows, batch->ActiveRows());
    for (std::uint32_t i = 0; i < batch->ActiveRows(); ++i) {
      drained.ids.push_back(batch->Value(0, i));
      drained.amounts.push_back(batch->Value(1, i));
    }
  }
  return drained;
}

TEST(SeqScan, EmptyTableProducesNothing) {
  const Table table = MakeOrders(0);
  SeqScan scan(table, kBatchSize);
  EXPECT_EQ(scan.Next(), nullptr);
  EXPECT_EQ(scan.rows_out(), 0u);
}

TEST(SeqScan, ReadsEveryRowInOrder) {
  const Table table = MakeOrders(2500);
  SeqScan scan(table, kBatchSize);
  const Drained drained = Drain(scan);

  ASSERT_EQ(drained.ids.size(), 2500u);
  EXPECT_EQ(scan.rows_out(), 2500u);
  for (std::uint64_t i = 0; i < 2500; ++i) {
    ASSERT_EQ(drained.ids[i], static_cast<std::int64_t>(i));
    ASSERT_EQ(drained.amounts[i], static_cast<std::int64_t>(i) * 10);
  }
  EXPECT_EQ(drained.num_batches, 3u);
  EXPECT_EQ(drained.max_batch_rows, kBatchSize);
}

TEST(SeqScan, SingleRowBatches) {
  const Table table = MakeOrders(7);
  SeqScan scan(table, BatchSizeFor(ExecMode::kInterpreter));
  const Drained drained = Drain(scan);

  EXPECT_EQ(drained.ids.size(), 7u);
  EXPECT_EQ(drained.num_batches, 7u);
  EXPECT_EQ(drained.max_batch_rows, 1u);
}

TEST(SeqScan, CrossesChunkBoundaryWithoutLosingRows) {
  const std::uint64_t num_rows = kChunkSize + kBatchSize + 13;
  const Table table = MakeOrders(num_rows);
  ASSERT_EQ(table.column(0).num_chunks(), 2u);

  SeqScan scan(table, kBatchSize);
  const Drained drained = Drain(scan);

  ASSERT_EQ(drained.ids.size(), num_rows);
  EXPECT_LE(drained.max_batch_rows, kBatchSize);
  for (std::uint64_t i = 0; i < num_rows; ++i) {
    ASSERT_EQ(drained.ids[i], static_cast<std::int64_t>(i)) << "i = " << i;
  }
}

TEST(SeqScan, ScanIsZeroCopy) {
  const Table table = MakeOrders(10);
  SeqScan scan(table, kBatchSize);
  Batch* batch = scan.Next();
  ASSERT_NE(batch, nullptr);
  EXPECT_EQ(batch->cols[0], table.column(0).chunk_data(0));
  EXPECT_EQ(batch->cols[1], table.column(1).chunk_data(0));
  EXPECT_EQ(batch->sel, nullptr);
}

TEST(SeqScan, RejectsBadArguments) {
  const Table table = MakeOrders(1);
  EXPECT_THROW(SeqScan(table, 0), std::invalid_argument);

  const Table no_columns("empty", Schema());
  EXPECT_THROW(SeqScan(no_columns, kBatchSize), std::invalid_argument);
}

}
}
