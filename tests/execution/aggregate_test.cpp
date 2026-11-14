#include "execution/aggregate.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "execution/interpreter/filter.h"
#include "execution/seq_scan.h"
#include "plan/expression.h"

namespace minidb {
namespace {

constexpr ColumnIndex kAmount = 1;

Table MakeOrders(const std::vector<std::int64_t>& amounts) {
  Table table("orders", Schema({"id", "amount"}));
  for (std::size_t i = 0; i < amounts.size(); ++i) {
    const std::int64_t row[] = {static_cast<std::int64_t>(i), amounts[i]};
    table.AppendRow(row);
  }
  return table;
}

// ---------------------------------------------------------------------------
// Kernel
// ---------------------------------------------------------------------------

TEST(SumKernel, SumAll) {
  const std::vector<std::int64_t> data = {250, 80, 300, 42, 900};
  EXPECT_EQ(SumAll(data.data(), 5), 1572u);
  EXPECT_EQ(SumAll(data.data(), 0), 0u);
}

TEST(SumKernel, SumSel) {
  const std::vector<std::int64_t> data = {250, 80, 300, 42, 900};
  const std::vector<std::uint16_t> sel = {0, 2, 4};
  EXPECT_EQ(SumSel(data.data(), sel.data(), 3), 1450u);
  EXPECT_EQ(SumSel(data.data(), sel.data(), 0), 0u);
}

// 累加器用 unsigned，所以溢位是定義好的 wrap，UBSan 不會中止，而且不管加法
// 順序如何，三種執行器的結果都會 bit 級相同。
TEST(SumKernel, OverflowWrapsInsteadOfTrapping) {
  const std::vector<std::int64_t> data = {INT64_MAX, INT64_MAX};
  const std::uint64_t sum = SumAll(data.data(), 2);
  EXPECT_EQ(static_cast<std::int64_t>(sum), -2);
}

// ---------------------------------------------------------------------------
// Operator
// ---------------------------------------------------------------------------

TEST(Aggregate, SumOverWholeTable) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  SeqScan scan(table, kBatchSize);
  Aggregate sum(scan, AggKind::kSum, kAmount);

  Batch* batch = sum.Next();
  ASSERT_NE(batch, nullptr);
  EXPECT_EQ(batch->num_cols, 1u);
  EXPECT_EQ(batch->n, 1u);
  EXPECT_EQ(batch->Value(0, 0), 1572);
  EXPECT_EQ(sum.result(), 1572);
  // 聚合只輸出一列
  EXPECT_EQ(sum.Next(), nullptr);
}

// filter 之後走的是 SumSel 這條路
TEST(Aggregate, SumRespectsSelectionVector) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));
  Aggregate sum(filter, AggKind::kSum, kAmount);

  ASSERT_NE(sum.Next(), nullptr);
  EXPECT_EQ(sum.result(), 1450);  // 250 + 300 + 900
}

TEST(Aggregate, SumAcrossManyBatches) {
  std::vector<std::int64_t> amounts(2500, 2);
  const Table table = MakeOrders(amounts);
  SeqScan scan(table, kBatchSize);
  Aggregate sum(scan, AggKind::kSum, kAmount);

  ASSERT_NE(sum.Next(), nullptr);
  EXPECT_EQ(sum.result(), 5000);
}

TEST(Aggregate, Count) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  SeqScan scan(table, kBatchSize);
  Aggregate count(scan, AggKind::kCount, 0);

  ASSERT_NE(count.Next(), nullptr);
  EXPECT_EQ(count.result(), 5);
}

TEST(Aggregate, CountRespectsSelectionVector) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));
  Aggregate count(filter, AggKind::kCount, 0);

  ASSERT_NE(count.Next(), nullptr);
  EXPECT_EQ(count.result(), 3);
}

// 空輸入仍然要輸出一列（SUM 回 0、COUNT 回 0），不是回 nullptr
TEST(Aggregate, EmptyInputStillProducesOneRow) {
  const Table table = MakeOrders({});
  SeqScan scan(table, kBatchSize);
  Aggregate sum(scan, AggKind::kSum, kAmount);

  Batch* batch = sum.Next();
  ASSERT_NE(batch, nullptr);
  EXPECT_EQ(batch->Value(0, 0), 0);
  EXPECT_EQ(sum.Next(), nullptr);
}

TEST(Aggregate, EverythingFilteredOutGivesZero) {
  const Table table = MakeOrders({1, 2, 3});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));
  Aggregate sum(filter, AggKind::kSum, kAmount);

  ASSERT_NE(sum.Next(), nullptr);
  EXPECT_EQ(sum.result(), 0);
}

}  // namespace
}  // namespace minidb
