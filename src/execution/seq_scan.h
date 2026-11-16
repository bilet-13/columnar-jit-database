#pragma once

#include <cstdint>

#include "execution/batch.h"
#include "execution/operator.h"
#include "storage/table.h"

namespace minidb {

class SeqScan final : public Operator {
 public:
  SeqScan(const Table& table, std::uint32_t batch_size);

  Batch* Next() override;

 private:
  const Table* table_;
  std::uint32_t batch_size_;
  std::uint32_t num_cols_;
  std::uint32_t chunk_ = 0;
  std::uint32_t offset_ = 0;
  Batch out_;
};

}
