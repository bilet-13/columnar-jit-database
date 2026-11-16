#pragma once

#include <cstdint>

#include "execution/batch.h"

namespace minidb {

class Operator {
 public:
  Operator() = default;
  virtual ~Operator() = default;

  Operator(const Operator&) = delete;
  Operator& operator=(const Operator&) = delete;

  virtual Batch* Next() = 0;

  std::uint64_t rows_out() const { return rows_out_; }

 protected:
  std::uint64_t rows_out_ = 0;
};

}
