#pragma once

#include <cstdint>
#include <vector>

#include "execution/batch.h"
#include "execution/operator.h"
#include "execution/vectorized/mask.h"
#include "plan/expression.h"

namespace minidb {

// ---------------------------------------------------------------------------
// VectorizedFilter：一次處理一整個 batch 的 filter。
//
// 與 InterpreterFilter 的差別只有一件事：它不走表達式 AST。predicate 在組
// 計畫時就被攤平成 (column, op, constant) 的清單，所以這裡沒有虛擬呼叫、
// 沒有一個 row 收集一次欄位值，內層迴圈乾淨到編譯器與 SIMD 都能發揮。
//
// 流程（每個 batch 一次）：
//   第一個 predicate   → mask
//   其餘 predicate     → tmp mask，然後 mask &= tmp
//   mask               → selection vector
//
// 輸出和 InterpreterFilter 一樣：沿用輸入的欄位指標，只換上新的 selection
// vector，不複製任何值。
// ---------------------------------------------------------------------------
class VectorizedFilter final : public Operator {
 public:
  VectorizedFilter(Operator& child, std::vector<Predicate> predicates, SimdKernel kernel);

  Batch* Next() override;

 private:
  Operator* child_;
  std::vector<Predicate> predicates_;
  const MaskKernels* kernels_;

  std::vector<std::uint8_t> mask_;
  std::vector<std::uint8_t> scratch_mask_;
  std::vector<std::uint16_t> selection_;
  Batch out_;
};

}  // namespace minidb
