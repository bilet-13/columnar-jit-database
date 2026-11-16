#include "execution/vectorized/filter.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

#include "execution/interpreter/filter.h"
#include "execution/seq_scan.h"
#include "execution/vectorized/mask.h"
#include "plan/expression.h"

namespace minidb {
namespace {

constexpr ColumnIndex kAmount = 1;

const std::vector<SimdKernel>& AllKernels() {
  static const std::vector<SimdKernel> kernels = {SimdKernel::kScalar, SimdKernel::kNeon};
  return kernels;
}

Table MakeOrders(const std::vector<std::int64_t>& amounts) {
  Table table("orders", Schema({"id", "amount"}));
  table.Reserve(amounts.size());
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

TEST(MaskKernel, NeonKernelsAreCompiledIn) {
#if defined(__ARM_NEON)
  SUCCEED();
#else
  GTEST_SKIP() << "這台機器沒有 NEON，SimdKernel::kNeon 會退回純量版";
#endif
}

TEST(MaskKernel, MaskBytesRoundsUp) {
  EXPECT_EQ(MaskBytes(0), 0u);
  EXPECT_EQ(MaskBytes(1), 1u);
  EXPECT_EQ(MaskBytes(8), 1u);
  EXPECT_EQ(MaskBytes(9), 2u);
  EXPECT_EQ(MaskBytes(kBatchSize), 128u);
}

TEST(MaskKernel, ComputeMaskSetsOneBitPerRow) {
  const std::vector<std::int64_t> column = {200, 50, 300, 10, 400, 0, 500, 1, 600, 2};
  for (const SimdKernel kernel : AllKernels()) {
    std::vector<std::uint8_t> mask(MaskBytes(10), 0xAA);
    Kernels(kernel).compute_mask(Predicate{0, CompareOp::kGt, 100}, column.data(), 10,
                                 mask.data());
    EXPECT_EQ(mask[0], 0b01010101) << ToString(kernel);
    EXPECT_EQ(mask[1], 0b00000001) << ToString(kernel);
  }
}

TEST(MaskKernel, ComputeMaskHandlesEveryOperator) {
  const std::vector<std::int64_t> column = {1, 2, 3};
  struct Case {
    CompareOp op;
    std::uint8_t expected;
  };
  const std::vector<Case> cases = {
      {CompareOp::kEq, 0b010}, {CompareOp::kNe, 0b101}, {CompareOp::kLt, 0b001},
      {CompareOp::kLe, 0b011}, {CompareOp::kGt, 0b100}, {CompareOp::kGe, 0b110},
  };
  for (const SimdKernel kernel : AllKernels()) {
    for (const Case& test_case : cases) {
      std::vector<std::uint8_t> mask(1, 0xFF);
      Kernels(kernel).compute_mask(Predicate{0, test_case.op, 2}, column.data(), 3, mask.data());
      EXPECT_EQ(mask[0] & 0b111, test_case.expected)
          << ToString(kernel) << " " << CompareOpSymbol(test_case.op);
    }
  }
}

TEST(MaskKernel, AndMask) {
  for (const SimdKernel kernel : AllKernels()) {
    std::vector<std::uint8_t> acc = {0b11110000, 0b10101010};
    const std::vector<std::uint8_t> other = {0b01011010, 0b11001100};
    Kernels(kernel).and_mask(acc.data(), other.data(), 2);
    EXPECT_EQ(acc[0], 0b01010000) << ToString(kernel);
    EXPECT_EQ(acc[1], 0b10001000) << ToString(kernel);
  }
}

TEST(MaskKernel, MaskToSelection) {
  for (const SimdKernel kernel : AllKernels()) {
    const std::vector<std::uint8_t> mask = {0b01010101, 0b00000001};
    std::vector<std::uint16_t> selection(10 + kSelectionSlack);
    const std::uint32_t n = Kernels(kernel).mask_to_selection(mask.data(), 10, selection.data());
    ASSERT_EQ(n, 5u) << ToString(kernel);
    const std::vector<std::uint16_t> expected = {0, 2, 4, 6, 8};
    EXPECT_EQ(std::vector<std::uint16_t>(selection.begin(), selection.begin() + n), expected)
        << ToString(kernel);
  }
}

TEST(MaskKernel, MaskToSelectionEmptyAndFull) {
  for (const SimdKernel kernel : AllKernels()) {
    std::vector<std::uint16_t> selection(kBatchSize + kSelectionSlack);

    const std::vector<std::uint8_t> none(MaskBytes(kBatchSize), 0x00);
    EXPECT_EQ(Kernels(kernel).mask_to_selection(none.data(), kBatchSize, selection.data()), 0u)
        << ToString(kernel);

    const std::vector<std::uint8_t> all(MaskBytes(kBatchSize), 0xFF);
    ASSERT_EQ(Kernels(kernel).mask_to_selection(all.data(), kBatchSize, selection.data()),
              kBatchSize)
        << ToString(kernel);
    for (std::uint32_t i = 0; i < kBatchSize; ++i) {
      ASSERT_EQ(selection[i], i) << ToString(kernel) << " i=" << i;
    }
  }
}

TEST(MaskKernel, HandlesTailsThatAreNotAMultipleOfEight) {
  std::mt19937_64 rng(7);
  std::vector<std::int64_t> column(kBatchSize);
  for (std::int64_t& value : column) {
    value = static_cast<std::int64_t>(rng() % 1000);
  }

  for (const std::uint32_t n : {1u, 2u, 7u, 8u, 9u, 15u, 100u, 1000u, kBatchSize}) {
    std::vector<std::uint16_t> expected;
    for (std::uint32_t i = 0; i < n; ++i) {
      if (column[i] > 500) {
        expected.push_back(static_cast<std::uint16_t>(i));
      }
    }

    for (const SimdKernel kernel : AllKernels()) {
      std::vector<std::uint8_t> mask(MaskBytes(kBatchSize), 0xFF);
      std::vector<std::uint16_t> selection(kBatchSize + kSelectionSlack);
      Kernels(kernel).compute_mask(Predicate{0, CompareOp::kGt, 500}, column.data(), n,
                                   mask.data());
      const std::uint32_t selected =
          Kernels(kernel).mask_to_selection(mask.data(), n, selection.data());
      ASSERT_EQ(selected, expected.size()) << ToString(kernel) << " n=" << n;
      EXPECT_EQ(std::vector<std::uint16_t>(selection.begin(), selection.begin() + selected),
                expected)
          << ToString(kernel) << " n=" << n;
    }
  }
}

TEST(MaskKernel, SelectionToMaskIsTheInverse) {
  for (const SimdKernel kernel : AllKernels()) {
    const std::vector<std::uint16_t> selection = {0, 3, 7, 8, 9};
    std::vector<std::uint8_t> mask(MaskBytes(16), 0xFF);
    Kernels(kernel).selection_to_mask(selection.data(), 5, 16, mask.data());

    std::vector<std::uint16_t> round_trip(16 + kSelectionSlack);
    const std::uint32_t n = Kernels(kernel).mask_to_selection(mask.data(), 16, round_trip.data());
    ASSERT_EQ(n, 5u) << ToString(kernel);
    EXPECT_EQ(std::vector<std::uint16_t>(round_trip.begin(), round_trip.begin() + n), selection)
        << ToString(kernel);
  }
}

TEST(VectorizedFilter, KeepsMatchingRows) {
  const Table table = MakeOrders({250, 80, 300, 42, 900});
  for (const SimdKernel kernel : AllKernels()) {
    SeqScan scan(table, kBatchSize);
    VectorizedFilter filter(scan, {Predicate{kAmount, CompareOp::kGt, 100}}, kernel);
    const std::vector<std::int64_t> expected = {250, 300, 900};
    EXPECT_EQ(DrainAmounts(filter), expected) << ToString(kernel);
    EXPECT_EQ(filter.rows_out(), 3u);
  }
}

TEST(VectorizedFilter, ConjunctionAndsTheMasks) {
  const Table table = MakeOrders({50, 150, 500, 850, 950});
  for (const SimdKernel kernel : AllKernels()) {
    SeqScan scan(table, kBatchSize);
    VectorizedFilter filter(scan,
                            {Predicate{kAmount, CompareOp::kGt, 100},
                             Predicate{kAmount, CompareOp::kLt, 900},
                             Predicate{kAmount, CompareOp::kNe, 500}},
                            kernel);
    const std::vector<std::int64_t> expected = {150, 850};
    EXPECT_EQ(DrainAmounts(filter), expected) << ToString(kernel);
  }
}

TEST(VectorizedFilter, NothingMatches) {
  const Table table = MakeOrders({1, 2, 3});
  for (const SimdKernel kernel : AllKernels()) {
    SeqScan scan(table, kBatchSize);
    VectorizedFilter filter(scan, {Predicate{kAmount, CompareOp::kGt, 100}}, kernel);
    EXPECT_EQ(filter.Next(), nullptr) << ToString(kernel);
  }
}

TEST(VectorizedFilter, SkipsFullyFilteredBatches) {
  std::vector<std::int64_t> amounts(kBatchSize * 3, 1);
  amounts[kBatchSize * 2 + 5] = 900;
  const Table table = MakeOrders(amounts);

  for (const SimdKernel kernel : AllKernels()) {
    SeqScan scan(table, kBatchSize);
    VectorizedFilter filter(scan, {Predicate{kAmount, CompareOp::kGt, 100}}, kernel);
    const std::vector<std::int64_t> expected = {900};
    EXPECT_EQ(DrainAmounts(filter), expected) << ToString(kernel);
  }
}

TEST(VectorizedFilter, StackedFiltersCompose) {
  const Table table = MakeOrders({50, 150, 500, 850, 950});
  for (const SimdKernel kernel : AllKernels()) {
    SeqScan scan(table, kBatchSize);
    VectorizedFilter lower(scan, {Predicate{kAmount, CompareOp::kGt, 100}}, kernel);
    VectorizedFilter upper(lower, {Predicate{kAmount, CompareOp::kLt, 900}}, kernel);
    const std::vector<std::int64_t> expected = {150, 500, 850};
    EXPECT_EQ(DrainAmounts(upper), expected) << ToString(kernel);
  }
}

TEST(VectorizedFilter, AgreesWithTheInterpreterOnRandomData) {
  std::mt19937_64 rng(42);
  std::vector<std::int64_t> amounts(kBatchSize * 2 + 377);
  for (std::int64_t& amount : amounts) {
    amount = static_cast<std::int64_t>(rng() % 1000);
  }
  const Table table = MakeOrders(amounts);

  const std::vector<Predicate> predicates = {Predicate{kAmount, CompareOp::kGt, 100},
                                             Predicate{kAmount, CompareOp::kLt, 900}};
  const ExprPtr ast = MakeAnd({MakeColumnCompare(kAmount, CompareOp::kGt, 100),
                               MakeColumnCompare(kAmount, CompareOp::kLt, 900)});

  SeqScan interpreted_scan(table, 1);
  InterpreterFilter interpreted(interpreted_scan, ast);
  const std::vector<std::int64_t> expected = DrainAmounts(interpreted);
  ASSERT_FALSE(expected.empty());

  for (const SimdKernel kernel : AllKernels()) {
    SeqScan scan(table, kBatchSize);
    VectorizedFilter filter(scan, predicates, kernel);
    EXPECT_EQ(DrainAmounts(filter), expected) << ToString(kernel);
  }
}

TEST(VectorizedFilter, RejectsEmptyPredicateList) {
  const Table table = MakeOrders({1});
  SeqScan scan(table, kBatchSize);
  EXPECT_THROW(VectorizedFilter(scan, {}, SimdKernel::kScalar), std::invalid_argument);
}

}
}
