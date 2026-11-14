#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "common/types.h"
#include "execution/exec_mode.h"
#include "execution/executor.h"
#include "util/data_gen.h"
#include "util/queries.h"
#include "util/reference.h"

namespace minidb {
namespace {

using testutil::Dataset;
using testutil::DatasetSizes;
using testutil::kAllQueries;
using testutil::MakeDataset;
using testutil::MakePlan;
using testutil::QueryId;
using testutil::QueryName;
using testutil::ReferenceMatchingRows;
using testutil::ReferenceResult;

std::int64_t RunQuery(const Dataset& dataset, QueryId id, ExecMode mode) {
  const LogicalNodePtr plan = MakePlan(id, dataset.catalog, dataset.sizes);
  Executor executor = Executor::Build(*plan, dataset.catalog, mode);
  return executor.RunScalar();
}

// ---------------------------------------------------------------------------
// Phase 1 的完成標準：Q1–Q5 用 interpreter 跑出來的結果與參考實作完全一致。
// ---------------------------------------------------------------------------
TEST(EndToEnd, InterpreterMatchesReference) {
  const Dataset dataset = MakeDataset(testutil::SmallSizes());
  for (const QueryId id : kAllQueries) {
    EXPECT_EQ(RunQuery(dataset, id, ExecMode::kInterpreter), ReferenceResult(id, dataset))
        << QueryName(id);
  }
}

// 跨 chunk 邊界、而且每個 operator 都要處理好幾千個 batch
TEST(EndToEnd, InterpreterMatchesReferenceAcrossChunks) {
  DatasetSizes sizes = testutil::SmallSizes();
  sizes.num_orders = kChunkSize + 1000;  // 兩個 chunk
  const Dataset dataset = MakeDataset(sizes);
  ASSERT_EQ(dataset.catalog.GetTable("orders").column(0).num_chunks(), 2u);

  for (const QueryId id : kAllQueries) {
    EXPECT_EQ(RunQuery(dataset, id, ExecMode::kInterpreter), ReferenceResult(id, dataset))
        << QueryName(id);
  }
}

// 固定 seed：同樣的尺寸跑兩次，資料與結果都必須一模一樣。
// benchmark 的數字要能互相比較就靠這個。
TEST(EndToEnd, DatasetIsDeterministic) {
  const Dataset a = MakeDataset(testutil::SmallSizes());
  const Dataset b = MakeDataset(testutil::SmallSizes());
  EXPECT_EQ(a.order_amount, b.order_amount);
  EXPECT_EQ(a.order_cust_id, b.order_cust_id);
  EXPECT_EQ(a.city_pop, b.city_pop);
  for (const QueryId id : kAllQueries) {
    EXPECT_EQ(RunQuery(a, id, ExecMode::kInterpreter), RunQuery(b, id, ExecMode::kInterpreter))
        << QueryName(id);
  }
}

// 測資的選擇率要符合 docs/design.md 8.3 的設計，不然 Q2 / Q3 的對照就沒意義。
TEST(EndToEnd, SelectivityMatchesTheDesign) {
  const Dataset dataset = MakeDataset(testutil::SmallSizes());
  const double total = static_cast<double>(dataset.sizes.num_orders);

  const double q2 = static_cast<double>(ReferenceMatchingRows(QueryId::kQ2, dataset)) / total;
  const double q3 = static_cast<double>(ReferenceMatchingRows(QueryId::kQ3, dataset)) / total;

  EXPECT_NEAR(q2, 0.90, 0.03) << "Q2 應該是高選擇率";
  EXPECT_NEAR(q3, 0.10, 0.03) << "Q3 應該是低選擇率";
}

// 每個查詢至少要留下一些列，否則測試等於什麼都沒驗
TEST(EndToEnd, EveryQueryMatchesSomething) {
  const Dataset dataset = MakeDataset(testutil::SmallSizes());
  for (const QueryId id : kAllQueries) {
    EXPECT_GT(ReferenceMatchingRows(id, dataset), 0u) << QueryName(id);
    EXPECT_GT(RunQuery(dataset, id, ExecMode::kInterpreter), 0) << QueryName(id);
  }
}

// Q3 的門檻比 Q2 高，所以結果一定比較小；Q5 經過 join 與第二個 filter，
// 一定不超過只有 amount 條件的量。這些不變量跟具體數字無關。
TEST(EndToEnd, ResultsAreOrderedAsExpected) {
  const Dataset dataset = MakeDataset(testutil::SmallSizes());
  const std::int64_t q1 = RunQuery(dataset, QueryId::kQ1, ExecMode::kInterpreter);
  const std::int64_t q2 = RunQuery(dataset, QueryId::kQ2, ExecMode::kInterpreter);
  const std::int64_t q3 = RunQuery(dataset, QueryId::kQ3, ExecMode::kInterpreter);
  const std::int64_t q5 = RunQuery(dataset, QueryId::kQ5, ExecMode::kInterpreter);

  EXPECT_LT(q2, q1);
  EXPECT_LT(q3, q2);
  EXPECT_LT(q5, q1);
}

}  // namespace
}  // namespace minidb
