
#include <benchmark/benchmark.h>

#include <cstdint>

static void BM_Smoke(benchmark::State& state) {
  std::uint64_t acc = 0;
  for (auto _ : state) {
    acc += 1;
    benchmark::DoNotOptimize(acc);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Smoke);

BENCHMARK_MAIN();
