#include "e2e/cross_check.h"

#include "execution/executor.h"

namespace minidb::testutil {

const std::vector<ExecutorVariant>& AllVariants() {
  static const std::vector<ExecutorVariant> variants = {
      {"interpreter", ExecMode::kInterpreter, SimdKernel::kScalar},
      // 批次但沒有手寫 SIMD：用來分離「批次」與「SIMD」各自的貢獻
      {"vectorized/scalar", ExecMode::kVectorized, SimdKernel::kScalar},
      {"vectorized/neon", ExecMode::kVectorized, SimdKernel::kNeon},
      // Phase 4 會在這裡加上 {"jit", ExecMode::kJit, SimdKernel::kNeon}
  };
  return variants;
}

std::vector<VariantResult> RunAllVariants(const LogicalNode& plan, const Catalog& catalog) {
  std::vector<VariantResult> results;
  results.reserve(AllVariants().size());
  for (const ExecutorVariant& variant : AllVariants()) {
    Executor executor = Executor::Build(plan, catalog, variant.mode, variant.kernel);
    results.push_back(VariantResult{variant.name, executor.RunScalar()});
  }
  return results;
}

}  // namespace minidb::testutil
