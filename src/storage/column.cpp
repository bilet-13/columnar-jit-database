#include "storage/column.h"

#include <stdexcept>

namespace minidb {

void Column::Append(std::int64_t value) {
  if (chunks_.empty() || chunks_.back().size() == kChunkSize) {
    chunks_.emplace_back();
  }
  // kChunkSize 是 2 的次方，所以 vector 自然的倍數成長剛好停在 kChunkSize，
  // 不會多配置。
  chunks_.back().push_back(value);
  ++num_rows_;
}

void Column::Reserve(std::uint64_t num_rows) {
  const std::size_t needed_chunks =
      static_cast<std::size_t>((num_rows + kChunkSize - 1) / kChunkSize);
  chunks_.reserve(needed_chunks);
  if (needed_chunks > 1 && !chunks_.empty()) {
    chunks_.back().reserve(kChunkSize);
  }
}

const std::int64_t* Column::chunk_data(std::uint32_t chunk) const {
  if (chunk >= num_chunks()) {
    throw std::out_of_range("Column::chunk_data: chunk 索引越界");
  }
  return chunks_[chunk].data();
}

std::uint32_t Column::chunk_rows(std::uint32_t chunk) const {
  if (chunk >= num_chunks()) {
    throw std::out_of_range("Column::chunk_rows: chunk 索引越界");
  }
  return static_cast<std::uint32_t>(chunks_[chunk].size());
}

std::int64_t Column::At(std::uint64_t row) const {
  if (row >= num_rows_) {
    throw std::out_of_range("Column::At: 列索引越界");
  }
  return chunks_[row / kChunkSize][row % kChunkSize];
}

}  // namespace minidb
