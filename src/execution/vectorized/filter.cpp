#include "execution/vectorized/filter.h"

#include <stdexcept>
#include <utility>

namespace minidb {

VectorizedFilter::VectorizedFilter(Operator& child, std::vector<Predicate> predicates,
                                   SimdKernel kernel)
    : child_(&child),
      predicates_(std::move(predicates)),
      kernels_(&Kernels(kernel)),
      mask_(MaskBytes(kBatchSize)),
      scratch_mask_(MaskBytes(kBatchSize)),
      selection_(kBatchSize + kSelectionSlack) {
  if (predicates_.empty()) {
    throw std::invalid_argument("VectorizedFilter: 至少要有一個 predicate");
  }
}

Batch* VectorizedFilter::Next() {
  while (true) {
    Batch* in = child_->Next();
    if (in == nullptr) {
      return nullptr;
    }
    if (in->n > kBatchSize) {
      throw std::logic_error("VectorizedFilter: batch 超過 kBatchSize");
    }

    const std::uint32_t n = in->n;
    const std::uint32_t bytes = MaskBytes(n);

    // 第一個 predicate 直接產生 mask；其餘的算完再 AND 進來。
    kernels_->compute_mask(predicates_[0], in->cols[predicates_[0].column], n, mask_.data());
    for (std::size_t i = 1; i < predicates_.size(); ++i) {
      kernels_->compute_mask(predicates_[i], in->cols[predicates_[i].column], n,
                             scratch_mask_.data());
      kernels_->and_mask(mask_.data(), scratch_mask_.data(), bytes);
    }

    // 上游如果已經篩過一輪（filter 疊 filter），把它的結果也 AND 進來。
    if (in->sel != nullptr) {
      kernels_->selection_to_mask(in->sel, in->sel_n, n, scratch_mask_.data());
      kernels_->and_mask(mask_.data(), scratch_mask_.data(), bytes);
    }

    const std::uint32_t selected =
        kernels_->mask_to_selection(mask_.data(), n, selection_.data());
    if (selected == 0) {
      continue;  // 整批被篩光，再要一批
    }

    out_ = *in;
    out_.sel = selection_.data();
    out_.sel_n = selected;
    rows_out_ += selected;
    return &out_;
  }
}

}  // namespace minidb
