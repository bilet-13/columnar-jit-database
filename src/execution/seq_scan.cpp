#include "execution/seq_scan.h"

#include <algorithm>
#include <stdexcept>

namespace minidb {

SeqScan::SeqScan(const Table& table, std::uint32_t batch_size)
    : table_(&table), batch_size_(batch_size), num_cols_(table.num_columns()) {
  if (batch_size_ == 0) {
    throw std::invalid_argument("SeqScan: batch size 不可為 0");
  }
  if (num_cols_ == 0) {
    throw std::invalid_argument("SeqScan: 表沒有任何欄位");
  }
  if (num_cols_ > kMaxCols) {
    throw std::invalid_argument("SeqScan: 欄位數超過 kMaxCols");
  }
}

Batch* SeqScan::Next() {
  const Column& first = table_->column(0);

  while (chunk_ < first.num_chunks()) {
    const std::uint32_t rows_in_chunk = first.chunk_rows(chunk_);
    if (offset_ >= rows_in_chunk) {
      ++chunk_;
      offset_ = 0;
      continue;
    }

    const std::uint32_t n = std::min(batch_size_, rows_in_chunk - offset_);
    for (std::uint32_t c = 0; c < num_cols_; ++c) {
      out_.cols[c] = table_->column(c).chunk_data(chunk_) + offset_;
    }
    out_.num_cols = num_cols_;
    out_.n = n;
    out_.ClearSelection();

    offset_ += n;
    rows_out_ += n;
    return &out_;
  }

  return nullptr;
}

}
