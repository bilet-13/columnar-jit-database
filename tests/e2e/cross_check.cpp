#include "e2e/cross_check.h"

#include "execution/executor.h"

namespace minidb::testutil {

const std::vector<ExecutorVariant>& AllVariants() {
  static const std::vector<ExecutorVariant> variants = {
      {"interpreter", ExecMode::kInterpreter, SimdKernel::kScalar},
      {"vectorized/scalar", ExecMode::kVectorized, SimdKernel::kScalar},
      {"vectorized/neon", ExecMode::kVectorized, SimdKernel::kNeon},
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

}
