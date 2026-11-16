#pragma once

#include <string>
#include <vector>

#include "execution/exec_mode.h"
#include "execution/kernels.h"
#include "util/data_gen.h"

namespace minidb::bench {

// ---------------------------------------------------------------------------
// 整個 benchmark 程式共用一份測資。
//
// 產生 100 萬筆資料要幾十毫秒，那是測資的成本、不是查詢的成本，所以只做一次。
// 固定 seed，所以不同次執行、不同 phase 量到的數字可以直接比較
// （docs/design.md 8.1 / 8.2）。
// ---------------------------------------------------------------------------
const testutil::Dataset& FullDataset();

// 要量的執行器組合。
//
// 跟 tests/e2e/cross_check.h 的清單長得很像但用途不同：那邊決定「要驗證哪些
// 實作」，這邊決定「要量哪些實作」。把 vectorized 拆成純量與 NEON 兩筆，是
// 為了分離「批次賺多少」與「手寫 SIMD 再賺多少」。
struct BenchVariant {
  std::string name;
  ExecMode mode;
  SimdKernel kernel;
};

const std::vector<BenchVariant>& Variants();

}  // namespace minidb::bench
