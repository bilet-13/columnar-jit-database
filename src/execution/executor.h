#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "catalog/catalog.h"
#include "execution/exec_mode.h"
#include "execution/operator.h"
#include "execution/vectorized/mask.h"
#include "plan/logical_plan.h"

namespace minidb {

// ---------------------------------------------------------------------------
// Executor：把邏輯計畫變成一棵 operator 樹，然後跑它。
//
// 三種 ExecMode 共用這份程式碼。差別只有兩處：
//   - batch size（interpreter 1，其餘 1024）
//   - filter 用哪一種實作
// scan、hash join、aggregate 都只有一份，所以 benchmark 比較的確實只是
// filter 的實作方式（docs/design.md 7.1）。
//
// Executor 持有整棵樹的所有節點，所以它活著樹才活著。
// ---------------------------------------------------------------------------
class Executor {
 public:
  // kJit 要等 Phase 4，傳進來會丟 std::logic_error。
  // kernel 只在 kVectorized 下有意義，用來比較純量批次與手寫 NEON。
  static Executor Build(const LogicalNode& plan, const Catalog& catalog, ExecMode mode,
                        SimdKernel kernel = SimdKernel::kNeon);

  Operator& root() { return *root_; }
  std::uint32_t num_output_columns() const { return num_output_columns_; }

  // 跑完整棵樹，回傳所有輸出列。給測試與參考實作比對用。
  std::vector<std::vector<std::int64_t>> RunRows();

  // 聚合計畫的捷徑：跑完回傳那唯一一個數字。
  std::int64_t RunScalar();

 private:
  Operator* BuildNode(const LogicalNode& node, const Catalog& catalog, ExecMode mode,
                      SimdKernel kernel);

  std::vector<std::unique_ptr<Operator>> nodes_;
  Operator* root_ = nullptr;
  std::uint32_t num_output_columns_ = 0;
};

}  // namespace minidb
