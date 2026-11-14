#include "execution/hash_join.h"

#include <algorithm>
#include <stdexcept>

namespace minidb {

HashJoin::HashJoin(Operator& probe_child, Operator& build_child, const HashJoinSpec& spec)
    : probe_child_(&probe_child),
      build_child_(&build_child),
      spec_(spec),
      total_cols_(spec.probe_num_cols + spec.build_num_cols) {
  if (spec_.batch_size == 0) {
    throw std::invalid_argument("HashJoin: batch size 不可為 0");
  }
  if (spec_.probe_num_cols == 0 || spec_.build_num_cols == 0) {
    throw std::invalid_argument("HashJoin: 兩邊都必須至少有一個欄位");
  }
  if (total_cols_ > kMaxCols) {
    throw std::invalid_argument("HashJoin: 輸出欄位數超過 kMaxCols");
  }
  if (spec_.probe_key >= spec_.probe_num_cols || spec_.build_key >= spec_.build_num_cols) {
    throw std::invalid_argument("HashJoin: join 欄位索引越界");
  }

  build_cols_.assign(spec_.build_num_cols, {});
  out_cols_.assign(total_cols_, {});
}

void HashJoin::BuildSide() {
  while (Batch* in = build_child_->Next()) {
    const std::uint32_t active = in->ActiveRows();
    for (std::uint32_t i = 0; i < active; ++i) {
      const std::uint32_t offset = in->RowOffset(i);
      const std::uint32_t row = static_cast<std::uint32_t>(build_rows_);
      // 值必須複製：上層拿到的 Batch 指標只在下次 Next() 之前有效
      for (std::uint32_t c = 0; c < spec_.build_num_cols; ++c) {
        build_cols_[c].push_back(in->cols[c][offset]);
      }
      hash_table_.Insert(in->cols[spec_.build_key][offset], row);
      ++build_rows_;
    }
  }
}

void HashJoin::ProbeOneBatch(const Batch& in) {
  for (std::vector<std::int64_t>& column : out_cols_) {
    column.clear();
  }

  const std::uint32_t active = in.ActiveRows();
  for (std::uint32_t i = 0; i < active; ++i) {
    const std::uint32_t offset = in.RowOffset(i);
    const std::int64_t key = in.cols[spec_.probe_key][offset];
    hash_table_.ForEachMatch(key, [&](std::uint32_t build_row) {
      for (std::uint32_t c = 0; c < spec_.probe_num_cols; ++c) {
        out_cols_[c].push_back(in.cols[c][offset]);
      }
      for (std::uint32_t c = 0; c < spec_.build_num_cols; ++c) {
        out_cols_[spec_.probe_num_cols + c].push_back(build_cols_[c][build_row]);
      }
    });
  }

  out_rows_ = out_cols_[0].size();
  out_pos_ = 0;
}

Batch* HashJoin::Next() {
  if (!built_) {
    BuildSide();
    built_ = true;
  }

  while (true) {
    if (out_pos_ < out_rows_) {
      const std::uint32_t n = static_cast<std::uint32_t>(
          std::min<std::uint64_t>(spec_.batch_size, out_rows_ - out_pos_));
      for (std::uint32_t c = 0; c < total_cols_; ++c) {
        out_.cols[c] = out_cols_[c].data() + out_pos_;
      }
      out_.num_cols = total_cols_;
      out_.n = n;
      out_.ClearSelection();
      out_pos_ += n;
      rows_out_ += n;
      return &out_;
    }

    Batch* in = probe_child_->Next();
    if (in == nullptr) {
      return nullptr;
    }
    ProbeOneBatch(*in);
  }
}

}  // namespace minidb
