#pragma once

#include <string>
#include <vector>

#include "execution/exec_mode.h"
#include "execution/kernels.h"
#include "util/data_gen.h"

namespace minidb::bench {

const testutil::Dataset& FullDataset();

struct BenchVariant {
  std::string name;
  ExecMode mode;
  SimdKernel kernel;
};

const std::vector<BenchVariant>& Variants();

}
