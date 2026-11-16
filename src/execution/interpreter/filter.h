#pragma once

#include <cstdint>
#include <vector>

#include "execution/batch.h"
#include "execution/operator.h"
#include "plan/expression.h"

namespace minidb {

class InterpreterFilter final : public Operator {
 public:
  InterpreterFilter(Operator& child, ExprPtr predicate);

  Batch* Next() override;

 private:
  Operator* child_;
  ExprPtr predicate_;
  std::vector<std::int64_t> row_;
  std::vector<std::uint16_t> sel_;
  Batch out_;
};

}
