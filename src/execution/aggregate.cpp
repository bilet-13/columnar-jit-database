#include "execution/aggregate.h"

#include <stdexcept>

namespace minidb {

namespace scalar {

std::uint64_t SumAll(const std::int64_t* data, std::uint32_t n) {
  std::uint64_t acc = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    acc += static_cast<std::uint64_t>(data[i]);
  }
  return acc;
}

std::uint64_t SumSel(const std::int64_t* data, const std::uint16_t* sel, std::uint32_t n) {
  std::uint64_t acc = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    acc += static_cast<std::uint64_t>(data[sel[i]]);
  }
  return acc;
}

}

namespace {

constexpr SumKernels kScalarSum = {&scalar::SumAll, &scalar::SumSel};
constexpr SumKernels kNeonSum = {&neon::SumAll, &scalar::SumSel};

}

const SumKernels& AggKernels(SimdKernel kernel) {
  switch (kernel) {
    case SimdKernel::kScalar:
      return kScalarSum;
    case SimdKernel::kNeon:
      return kNeonSum;
  }
  return kScalarSum;
}

Aggregate::Aggregate(Operator& child, AggKind agg, ColumnIndex column, SimdKernel kernel)
    : child_(&child), agg_(agg), column_(column), kernels_(&AggKernels(kernel)) {}

Batch* Aggregate::Next() {
  if (done_) {
    return nullptr;
  }
  done_ = true;

  std::uint64_t acc = 0;
  while (Batch* in = child_->Next()) {
    if (agg_ == AggKind::kCount) {
      acc += in->ActiveRows();
      continue;
    }
    if (column_ >= in->num_cols) {
      throw std::out_of_range("Aggregate: 欄位索引超出輸入的寬度");
    }
    acc += in->sel == nullptr ? kernels_->sum_all(in->cols[column_], in->n)
                              : kernels_->sum_sel(in->cols[column_], in->sel, in->sel_n);
  }

  result_ = static_cast<std::int64_t>(acc);
  out_.cols[0] = &result_;
  out_.num_cols = 1;
  out_.n = 1;
  out_.ClearSelection();
  rows_out_ = 1;
  return &out_;
}

}
