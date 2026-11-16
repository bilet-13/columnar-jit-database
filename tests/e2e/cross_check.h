#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/catalog.h"
#include "execution/exec_mode.h"
#include "execution/kernels.h"
#include "plan/logical_plan.h"

namespace minidb::testutil {

struct ExecutorVariant {
  std::string name;
  ExecMode mode;
  SimdKernel kernel;
};

const std::vector<ExecutorVariant>& AllVariants();

struct VariantResult {
  std::string name;
  std::int64_t value;
};

std::vector<VariantResult> RunAllVariants(const LogicalNode& plan, const Catalog& catalog);

}
