#include "execution/interpreter/filter.h"

#include <stdexcept>
#include <utility>

namespace minidb {

InterpreterFilter::InterpreterFilter(Operator& child, ExprPtr predicate)
    : child_(&child), predicate_(std::move(predicate)) {
  if (predicate_ == nullptr) {
    throw std::invalid_argument("InterpreterFilter: predicate 不可為 null");
  }
}

Batch* InterpreterFilter::Next() {
  while (true) {
    Batch* in = child_->Next();
    if (in == nullptr) {
      return nullptr;  // 下層沒了，我也沒了
    }

    row_.assign(in->num_cols, 0);
    sel_.clear();

    const std::uint32_t active = in->ActiveRows();
    for (std::uint32_t i = 0; i < active; ++i) {
      const std::uint32_t offset = in->RowOffset(i);
      for (std::uint32_t c = 0; c < in->num_cols; ++c) {
        row_[c] = in->cols[c][offset];
      }
      if (predicate_->Evaluate(row_) != 0) {
        sel_.push_back(static_cast<std::uint16_t>(offset));
      }
    }

    if (sel_.empty()) {
      continue;  // 整批被篩光，再要一批
    }

    out_ = *in;
    out_.sel = sel_.data();
    out_.sel_n = static_cast<std::uint32_t>(sel_.size());
    rows_out_ += sel_.size();
    return &out_;
  }
}

}  // namespace minidb
