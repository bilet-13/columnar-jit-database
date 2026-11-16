#pragma once

#include <cstdint>

#include "plan/expression.h"

namespace minidb {

template <CompareOp kOp>
constexpr bool CompareStatic(std::int64_t lhs, std::int64_t rhs) {
  if constexpr (kOp == CompareOp::kEq) {
    return lhs == rhs;
  } else if constexpr (kOp == CompareOp::kNe) {
    return lhs != rhs;
  } else if constexpr (kOp == CompareOp::kLt) {
    return lhs < rhs;
  } else if constexpr (kOp == CompareOp::kLe) {
    return lhs <= rhs;
  } else if constexpr (kOp == CompareOp::kGt) {
    return lhs > rhs;
  } else {
    return lhs >= rhs;
  }
}

template <typename Fn>
auto DispatchCompareOp(CompareOp op, Fn&& fn) {
  switch (op) {
    case CompareOp::kEq:
      return fn.template operator()<CompareOp::kEq>();
    case CompareOp::kNe:
      return fn.template operator()<CompareOp::kNe>();
    case CompareOp::kLt:
      return fn.template operator()<CompareOp::kLt>();
    case CompareOp::kLe:
      return fn.template operator()<CompareOp::kLe>();
    case CompareOp::kGt:
      return fn.template operator()<CompareOp::kGt>();
    case CompareOp::kGe:
      return fn.template operator()<CompareOp::kGe>();
  }
  return fn.template operator()<CompareOp::kEq>();
}

}
