#pragma once

#include <cstdint>
#include <vector>

#include "execution/batch.h"
#include "execution/operator.h"
#include "plan/expression.h"

namespace minidb {

// ---------------------------------------------------------------------------
// InterpreterFilter：基準版的 filter。
//
// 對每一列做兩件事：把這一列的欄位值收集起來，然後走一次表達式 AST。
// 走樹的過程是一連串虛擬呼叫與 switch，真正有用的工作只是一個整數比較。
// 這個比例就是 Phase 2 的 SIMD 與 Phase 4 的 codegen 要改掉的東西，所以
// 這裡刻意不做任何最佳化 —— 基準必須誠實。
//
// 輸出沿用輸入的欄位指標，只是加上一個 selection vector，所以不複製任何值。
// 如果整批都被篩掉，不能回傳空 batch（上層會以為資料結束），而是回到迴圈
// 開頭再跟下層要一批。
// ---------------------------------------------------------------------------
class InterpreterFilter final : public Operator {
 public:
  InterpreterFilter(Operator& child, ExprPtr predicate);

  Batch* Next() override;

 private:
  Operator* child_;
  ExprPtr predicate_;
  std::vector<std::int64_t> row_;    // 暫存一列，餵給 AST
  std::vector<std::uint16_t> sel_;   // 這一批通過的 offset
  Batch out_;
};

}  // namespace minidb
