#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "catalog/catalog.h"
#include "execution/exec_mode.h"
#include "execution/operator.h"
#include "execution/vectorized/mask.h"
#include "plan/logical_plan.h"

namespace minidb {

class Executor {
 public:
  static Executor Build(const LogicalNode& plan, const Catalog& catalog, ExecMode mode,
                        SimdKernel kernel = SimdKernel::kNeon);

  Operator& root() { return *root_; }
  std::uint32_t num_output_columns() const { return num_output_columns_; }

  std::vector<std::vector<std::int64_t>> RunRows();

  std::int64_t RunScalar();

 private:
  Operator* BuildNode(const LogicalNode& node, const Catalog& catalog, ExecMode mode,
                      SimdKernel kernel);

  std::vector<std::unique_ptr<Operator>> nodes_;
  Operator* root_ = nullptr;
  std::uint32_t num_output_columns_ = 0;
};

}
