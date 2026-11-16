#pragma once

#include <cstdint>

#include "common/types.h"
#include "execution/batch.h"
#include "execution/kernels.h"
#include "execution/operator.h"
#include "plan/logical_plan.h"

namespace minidb {

struct SumKernels {
  std::uint64_t (*sum_all)(const std::int64_t* data, std::uint32_t n);
  std::uint64_t (*sum_sel)(const std::int64_t* data, const std::uint16_t* sel, std::uint32_t n);
};

const SumKernels& AggKernels(SimdKernel kernel);

namespace scalar {
std::uint64_t SumAll(const std::int64_t* data, std::uint32_t n);
std::uint64_t SumSel(const std::int64_t* data, const std::uint16_t* sel, std::uint32_t n);
}

namespace neon {
std::uint64_t SumAll(const std::int64_t* data, std::uint32_t n);
}

class Aggregate final : public Operator {
 public:
  Aggregate(Operator& child, AggKind agg, ColumnIndex column, SimdKernel kernel);

  Batch* Next() override;

  std::int64_t result() const { return result_; }

 private:
  Operator* child_;
  AggKind agg_;
  ColumnIndex column_;
  const SumKernels* kernels_;
  bool done_ = false;
  std::int64_t result_ = 0;
  Batch out_;
};

}
