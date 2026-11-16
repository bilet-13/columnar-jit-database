#pragma once

#include <cstdint>
#include <vector>

#include "common/types.h"

namespace minidb {

class Column {
 public:
  void Append(std::int64_t value);

  void Reserve(std::uint64_t num_rows);

  std::uint64_t num_rows() const { return num_rows_; }
  std::uint32_t num_chunks() const { return static_cast<std::uint32_t>(chunks_.size()); }

  const std::int64_t* chunk_data(std::uint32_t chunk) const;

  std::uint32_t chunk_rows(std::uint32_t chunk) const;

  std::int64_t At(std::uint64_t row) const;

 private:
  std::vector<std::vector<std::int64_t>> chunks_;
  std::uint64_t num_rows_ = 0;
};

}
