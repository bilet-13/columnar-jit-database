// Phase 0 的煙霧 benchmark：只確認 Google Benchmark 接起來了。
// 真正的 benchmark 從 Phase 2（C2.6 query_bench）開始。

#include <benchmark/benchmark.h>

#include <cstdint>

static void BM_Smoke(benchmark::State& state) {
  std::uint64_t acc = 0;
  for (auto _ : state) {
    acc += 1;
    // 告訴編譯器「這個值有用」，否則整個迴圈會被最佳化掉，量到 0ns
    benchmark::DoNotOptimize(acc);
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Smoke);

BENCHMARK_MAIN();
