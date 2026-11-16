
#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>

#include "bench_data.h"
#include "execution/executor.h"
#include "plan/logical_plan.h"
#include "util/queries.h"

namespace minidb {
namespace {

void RunQuery(benchmark::State& state, testutil::QueryId id, ExecMode mode, SimdKernel kernel) {
  const testutil::Dataset& dataset = bench::FullDataset();
  const LogicalNodePtr plan = testutil::MakePlan(id, dataset.catalog, dataset.sizes);

  {
    Executor warmup = Executor::Build(*plan, dataset.catalog, mode, kernel);
    if (warmup.RunScalar() == 0) {
      state.SkipWithError("查詢結果是 0，測資或計畫有問題");
      return;
    }
  }

  for (auto _ : state) {
    Executor executor = Executor::Build(*plan, dataset.catalog, mode, kernel);
    std::int64_t result = executor.RunScalar();
    benchmark::DoNotOptimize(result);
  }

  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(dataset.sizes.num_orders));
}

int RegisterQueryBenchmarks() {
  for (const testutil::QueryId id : testutil::kAllQueries) {
    for (const bench::BenchVariant& variant : bench::Variants()) {
      const std::string name = std::string(testutil::QueryName(id)) + "/" + variant.name;
      benchmark::RegisterBenchmark(name,
                                   [id, variant](benchmark::State& state) {
                                     RunQuery(state, id, variant.mode, variant.kernel);
                                   })
          ->Unit(benchmark::kMillisecond)
          ->MinTime(0.3);
    }
  }
  return 0;
}

const int kRegistered = RegisterQueryBenchmarks();

}
}
