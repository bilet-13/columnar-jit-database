#include "bench_data.h"

namespace minidb::bench {

const testutil::Dataset& FullDataset() {
  static const testutil::Dataset dataset = testutil::MakeDataset(testutil::DatasetSizes{});
  return dataset;
}

const std::vector<BenchVariant>& Variants() {
  static const std::vector<BenchVariant> variants = {
      {"interpreter", ExecMode::kInterpreter, SimdKernel::kScalar},
      {"batch_scalar", ExecMode::kVectorized, SimdKernel::kScalar},
      {"batch_neon", ExecMode::kVectorized, SimdKernel::kNeon},
  };
  return variants;
}

}
