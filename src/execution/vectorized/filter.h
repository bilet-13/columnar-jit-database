#pragma once

#include <cstdint>
#include <vector>

#include "execution/batch.h"
#include "execution/operator.h"
#include "execution/vectorized/mask.h"
#include "plan/expression.h"

namespace minidb {

class VectorizedFilter final : public Operator {
 public:
  VectorizedFilter(Operator& child, std::vector<Predicate> predicates, SimdKernel kernel);

  Batch* Next() override;

 private:
  Operator* child_;
  std::vector<Predicate> predicates_;
  const MaskKernels* kernels_;

  std::vector<std::uint8_t> mask_;
  std::vector<std::uint8_t> scratch_mask_;
  std::vector<std::uint16_t> selection_;
  Batch out_;
};

}
