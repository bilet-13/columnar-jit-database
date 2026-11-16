#pragma once

#include <cstdint>

#include "plan/expression.h"

namespace minidb {

// 把運算子變成編譯期常數，內層迴圈裡就沒有 switch。
//
// 兩種 kernel 都用得到：純量版整個迴圈靠它，NEON 版用它處理尾巴那幾個
// 不滿一組的 row。
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

// 六個運算子的 switch 只做一次（每個 batch 一次），之後呼叫的就是特化過的
// 迴圈。Fn 是一個 template<CompareOp> 的 functor。
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

}  // namespace minidb
