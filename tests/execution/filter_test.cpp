#include "execution/interpreter/filter.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

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

std::vector<std::int64_t> DrainAmounts(Operator& op) {
  std::vector<std::int64_t> values;
  while (Batch* batch = op.Next()) {
    EXPECT_GT(batch->ActiveRows(), 0u) << "filter 不該回傳空 batch";
    for (std::uint32_t i = 0; i < batch->ActiveRows(); ++i) {
      values.push_back(batch->Value(kAmount, i));
    }
  }
  return values;
}

TEST(InterpreterFilter, KeepsMatchingRows) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));

  const std::vector<std::int64_t> expected = {250, 300, 900};
  EXPECT_EQ(DrainAmounts(filter), expected);
  EXPECT_EQ(filter.rows_out(), 3u);
}

TEST(InterpreterFilter, OutputCarriesASelectionVector) {
  const Table table = MakeOrders({250, 80, 300});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));

  Batch* batch = filter.Next();
  ASSERT_NE(batch, nullptr);
  ASSERT_NE(batch->sel, nullptr);
  EXPECT_EQ(batch->n, 3u);
  EXPECT_EQ(batch->sel_n, 2u);
  EXPECT_EQ(batch->sel[0], 0u);
  EXPECT_EQ(batch->sel[1], 2u);
  EXPECT_EQ(batch->cols[kAmount], table.column(kAmount).chunk_data(0));
}

TEST(InterpreterFilter, NothingMatches) {
  const Table table = MakeOrders({1, 2, 3});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));

  EXPECT_EQ(filter.Next(), nullptr);
  EXPECT_EQ(filter.rows_out(), 0u);
}

TEST(InterpreterFilter, SkipsFullyFilteredBatches) {
  const Table table = MakeOrders({80, 250, 42, 7, 900, 3});
  SeqScan scan(table, 1);
  InterpreterFilter filter(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));

  const std::vector<std::int64_t> expected = {250, 900};
  EXPECT_EQ(DrainAmounts(filter), expected);
}

TEST(InterpreterFilter, ConjunctionOfThreeConditions) {
  const Table table = MakeOrders({50, 150, 500, 850, 950});
  SeqScan scan(table, kBatchSize);
  const ExprPtr predicate = MakeAnd({MakeColumnCompare(kAmount, CompareOp::kGt, 100),
                                     MakeColumnCompare(kAmount, CompareOp::kLt, 900),
                                     MakeColumnCompare(kAmount, CompareOp::kNe, 500)});
  InterpreterFilter filter(scan, predicate);

  const std::vector<std::int64_t> expected = {150, 850};
  EXPECT_EQ(DrainAmounts(filter), expected);
}

TEST(InterpreterFilter, StackedFiltersCompose) {
  const Table table = MakeOrders({50, 150, 500, 850, 950});
  SeqScan scan(table, kBatchSize);
  InterpreterFilter lower(scan, MakeColumnCompare(kAmount, CompareOp::kGt, 100));
  InterpreterFilter upper(lower, MakeColumnCompare(kAmount, CompareOp::kLt, 900));

  const std::vector<std::int64_t> expected = {150, 500, 850};
  EXPECT_EQ(DrainAmounts(upper), expected);
}

TEST(InterpreterFilter, RejectsNullPredicate) {
  const Table table = MakeOrders({1});
  SeqScan scan(table, kBatchSize);
  EXPECT_THROW(InterpreterFilter(scan, nullptr), std::invalid_argument);
}

}
}
