#include "execution/batch.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "execution/exec_mode.h"

namespace minidb {
namespace {

TEST(Batch, WithoutSelectionEveryRowIsActive) {
  const std::vector<std::int64_t> amount = {250, 80, 300, 42, 900};
  Batch batch;
  batch.cols[0] = amount.data();
  batch.num_cols = 1;
  batch.n = 5;

  EXPECT_EQ(batch.ActiveRows(), 5u);
  EXPECT_EQ(batch.RowOffset(3), 3u);
  EXPECT_EQ(batch.Value(0, 3), 42);
}

TEST(Batch, SelectionVectorSkipsRows) {
  const std::vector<std::int64_t> amount = {250, 80, 300, 42, 900};
  const std::vector<std::uint16_t> sel = {0, 2, 4};  // 通過 amount > 100 的列
  Batch batch;
  batch.cols[0] = amount.data();
  batch.num_cols = 1;
  batch.n = 5;
  batch.sel = sel.data();
  batch.sel_n = 3;

  EXPECT_EQ(batch.ActiveRows(), 3u);
  EXPECT_EQ(batch.RowOffset(1), 2u);
  EXPECT_EQ(batch.Value(0, 0), 250);
  EXPECT_EQ(batch.Value(0, 1), 300);
  EXPECT_EQ(batch.Value(0, 2), 900);
}

TEST(Batch, ClearSelectionRestoresAllRows) {
  const std::vector<std::int64_t> amount = {1, 2, 3};
  const std::vector<std::uint16_t> sel = {1};
  Batch batch;
  batch.cols[0] = amount.data();
  batch.num_cols = 1;
  batch.n = 3;
  batch.sel = sel.data();
  batch.sel_n = 1;
  ASSERT_EQ(batch.ActiveRows(), 1u);

  batch.ClearSelection();
  EXPECT_EQ(batch.ActiveRows(), 3u);
  EXPECT_EQ(batch.Value(0, 2), 3);
}

// Interpreter 刻意用 batch = 1，解譯開銷才會出現在每一列上
TEST(ExecMode, InterpreterUsesSingleRowBatches) {
  EXPECT_EQ(BatchSizeFor(ExecMode::kInterpreter), 1u);
  EXPECT_EQ(BatchSizeFor(ExecMode::kVectorized), kBatchSize);
  EXPECT_EQ(BatchSizeFor(ExecMode::kJit), kBatchSize);
}

TEST(ExecMode, Names) {
  EXPECT_EQ(ToString(ExecMode::kInterpreter), "interpreter");
  EXPECT_EQ(ToString(ExecMode::kVectorized), "vectorized");
  EXPECT_EQ(ToString(ExecMode::kJit), "jit");
}

}  // namespace
}  // namespace minidb
