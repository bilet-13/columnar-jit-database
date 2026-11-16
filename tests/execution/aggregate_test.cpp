#include "execution/aggregate.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
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

const std::vector<SimdKernel>& AllKernels() {
  static const std::vector<SimdKernel> kernels = {SimdKernel::kScalar, SimdKernel::kNeon};
  return kernels;
}

TEST(SumKernel, SumAll) {
  const std::vector<std::int64_t> data = {250, 80, 300, 42, 900};
  for (const SimdKernel kernel : AllKernels()) {
    EXPECT_EQ(AggKernels(kernel).sum_all(data.data(), 5), 1572u) << ToString(kernel);
    EXPECT_EQ(AggKernels(kernel).sum_all(data.data(), 0), 0u) << ToString(kernel);
  }
}

TEST(SumKernel, SumSel) {
  const std::vector<std::int64_t> data = {250, 80, 300, 42, 900};
  const std::vector<std::uint16_t> sel = {0, 2, 4};
  for (const SimdKernel kernel : AllKernels()) {
    EXPECT_EQ(AggKernels(kernel).sum_sel(data.data(), sel.data(), 3), 1450u) << ToString(kernel);
    EXPECT_EQ(AggKernels(kernel).sum_sel(data.data(), sel.data(), 0), 0u) << ToString(kernel);
  }
}

// 累加器用 unsigned，所以溢位是定義好的 wrap，UBSan 不會中止。
// 加法可交換，所以四個獨立累加器的 NEON 版跟逐個相加的純量版結果一致。
TEST(SumKernel, OverflowWrapsInsteadOfTrapping) {
  const std::vector<std::int64_t> data = {INT64_MAX, INT64_MAX};
  for (const SimdKernel kernel : AllKernels()) {
    const std::uint64_t sum = AggKernels(kernel).sum_all(data.data(), 2);
    EXPECT_EQ(static_cast<std::int64_t>(sum), -2) << ToString(kernel);
  }
}

// NEON 版一次吃 8 個，所以每個不是 8 的倍數的長度都要跟純量版對得上
TEST(SumKernel, NeonAgreesWithScalarAtEveryLength) {
  std::mt19937_64 rng(11);
  std::vector<std::int64_t> data(300);
  for (std::int64_t& value : data) {
    value = static_cast<std::int64_t>(rng() % 1000);
  }
  for (std::uint32_t n = 0; n <= data.size(); ++n) {
    EXPECT_EQ(neon::SumAll(data.data(), n), scalar::SumAll(data.data(), n)) << "n = " << n;
  }
}

// 即使溢位，兩個版本的分組方式不同也必須得到同一個答案
TEST(SumKernel, NeonAgreesWithScalarOnOverflow) {
  std::vector<std::int64_t> data(37, INT64_MAX / 3);
  EXPECT_EQ(neon::SumAll(data.data(), data.size()), scalar::SumAll(data.data(), data.size()));
}

// ---------------------------------------------------------------------------
// Operator
// ---------------------------------------------------------------------------

TEST(Aggregate, SumOverWholeTable) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  SeqScan scan(table, kBatchSize);
  Aggregate sum(scan, AggKind::kSum, kAmount, SimdKernel::kNeon);

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
  Aggregate sum(filter, AggKind::kSum, kAmount, SimdKernel::kNeon);

  ASSERT_NE(sum.Next(), nullptr);
  EXPECT_EQ(sum.result(), 1450);  // 250 + 300 + 900
}

TEST(Aggregate, SumAcrossManyBatches) {
  std::vector<std::int64_t> amounts(2500, 2);
  const Table table = MakeOrders(amounts);
  SeqScan scan(table, kBatchSize);
  Aggregate sum(scan, AggKind::kSum, kAmount, SimdKernel::kNeon);

  ASSERT_NE(sum.Next(), nullptr);
  EXPECT_EQ(sum.result(), 5000);
}

TEST(Aggregate, Count) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  SeqScan scan(table, kBatchSize);
  Aggregate count(scan, AggKind::kCount, 0, SimdKernel::kNeon);

  ASSERT_NE(count.Next(), nullptr);
  EXPECT_EQ(count.result(), 5);
}

TEST(Aggregate, CountRespectsSelectionVector) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));
  Aggregate count(filter, AggKind::kCount, 0, SimdKernel::kNeon);

  ASSERT_NE(count.Next(), nullptr);
  EXPECT_EQ(count.result(), 3);
}

// 空輸入仍然要輸出一列（SUM 回 0、COUNT 回 0），不是回 nullptr
TEST(Aggregate, EmptyInputStillProducesOneRow) {
  const Table table = MakeOrders({});
  SeqScan scan(table, kBatchSize);
  Aggregate sum(scan, AggKind::kSum, kAmount, SimdKernel::kNeon);

  Batch* batch = sum.Next();
  ASSERT_NE(batch, nullptr);
  EXPECT_EQ(batch->Value(0, 0), 0);
  EXPECT_EQ(sum.Next(), nullptr);
}

TEST(Aggregate, EverythingFilteredOutGivesZero) {
  const Table table = MakeOrders({1, 2, 3});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));
  Aggregate sum(filter, AggKind::kSum, kAmount, SimdKernel::kNeon);

  ASSERT_NE(sum.Next(), nullptr);
  EXPECT_EQ(sum.result(), 0);
}

}  // namespace
}  // namespace minidb
