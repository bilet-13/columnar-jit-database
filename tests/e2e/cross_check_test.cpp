#include "e2e/cross_check.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "common/types.h"
#include "util/data_gen.h"
#include "util/queries.h"
#include "util/reference.h"

namespace minidb {
namespace {

using testutil::AllVariants;
using testutil::Dataset;
using testutil::DatasetSizes;
using testutil::kAllQueries;
using testutil::MakeDataset;
using testutil::MakePlan;
using testutil::QueryId;
using testutil::QueryName;
using testutil::ReferenceResult;
using testutil::RunAllVariants;
using testutil::VariantResult;

void ExpectAllVariantsAgree(const Dataset& dataset, QueryId id) {
  const LogicalNodePtr plan = MakePlan(id, dataset.catalog, dataset.sizes);
  const std::vector<VariantResult> results = RunAllVariants(*plan, dataset.catalog);
  ASSERT_FALSE(results.empty());

  const std::int64_t expected = ReferenceResult(id, dataset);
  for (const VariantResult& result : results) {
    EXPECT_EQ(result.value, expected) << QueryName(id) << " / " << result.name;
  }
}

TEST(CrossCheck, EveryExecutorAgreesOnQ1ToQ5) {
  const Dataset dataset = MakeDataset(testutil::SmallSizes());
  for (const QueryId id : kAllQueries) {
    ExpectAllVariantsAgree(dataset, id);
  }
}

TEST(CrossCheck, EveryExecutorAgreesAcrossChunks) {
  DatasetSizes sizes = testutil::SmallSizes();
  sizes.num_orders = kChunkSize + 1000;
  const Dataset dataset = MakeDataset(sizes);
  for (const QueryId id : kAllQueries) {
    ExpectAllVariantsAgree(dataset, id);
  }
}

TEST(CrossCheck, EveryExecutorAgreesAtAwkwardRowCounts) {
  for (const std::uint64_t num_orders : {std::uint64_t{1}, std::uint64_t{7}, std::uint64_t{8},
                                         std::uint64_t{9}, std::uint64_t{kBatchSize - 1},
                                         std::uint64_t{kBatchSize},
                                         std::uint64_t{kBatchSize + 1},
                                         std::uint64_t{kBatchSize * 2 + 3}}) {
    DatasetSizes sizes = testutil::SmallSizes();
    sizes.num_orders = num_orders;
    const Dataset dataset = MakeDataset(sizes);
    for (const QueryId id : kAllQueries) {
      ExpectAllVariantsAgree(dataset, id);
    }
  }
}

TEST(CrossCheck, VariantListCoversEveryMode) {
  bool has_interpreter = false;
  bool has_vectorized_scalar = false;
  bool has_vectorized_neon = false;
  for (const testutil::ExecutorVariant& variant : AllVariants()) {
    if (variant.mode == ExecMode::kInterpreter) {
      has_interpreter = true;
    }
    if (variant.mode == ExecMode::kVectorized && variant.kernel == SimdKernel::kScalar) {
      has_vectorized_scalar = true;
    }
    if (variant.mode == ExecMode::kVectorized && variant.kernel == SimdKernel::kNeon) {
      has_vectorized_neon = true;
    }
  }
  EXPECT_TRUE(has_interpreter);
  EXPECT_TRUE(has_vectorized_scalar);
  EXPECT_TRUE(has_vectorized_neon);
}

}
}
