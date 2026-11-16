#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/catalog.h"
#include "execution/exec_mode.h"
#include "execution/kernels.h"
#include "plan/logical_plan.h"

namespace minidb::testutil {

// ---------------------------------------------------------------------------
// 交叉驗證：同一個計畫用每一種執行器各跑一次，答案必須完全一樣。
//
// 這是整個專案最重要的一條安全網。每加一種執行方式，就多一個獨立的實作在
// 盯著其他實作 —— 而且它們共用 scan、join、aggregate，所以不一致幾乎一定
// 是新加的那個 filter 或 kernel 寫錯了。
//
// Phase 4 只要在 AllVariants() 加一筆 JIT，所有既有的測試就自動變成三方比對。
// ---------------------------------------------------------------------------
struct ExecutorVariant {
  std::string name;
  ExecMode mode;
  SimdKernel kernel;
};

const std::vector<ExecutorVariant>& AllVariants();

struct VariantResult {
  std::string name;
  std::int64_t value;
};

std::vector<VariantResult> RunAllVariants(const LogicalNode& plan, const Catalog& catalog);

}  // namespace minidb::testutil
