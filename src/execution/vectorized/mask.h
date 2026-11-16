#pragma once

#include <cstdint>

#include "common/types.h"
#include "execution/kernels.h"
#include "plan/expression.h"

namespace minidb {

// ---------------------------------------------------------------------------
// 批次 filter 的三個 kernel（docs/design.md 7.2）
//
// 一個 predicate 的結果先存成 bit mask（一個 row 一個 bit），多個 predicate
// 的 mask 相 AND，最後一次轉成 selection vector。
//
// 為什麼不是「算出第一個 predicate 的 selection vector，再拿它去篩第二個」：
// 那條路第二個 predicate 開始就變成跳著取值（gather），而 ARM NEON 沒有
// 64-bit 的 gather 指令，SIMD 完全用不上。改成每個 predicate 都掃連續記憶體
// 產生 mask，雖然不會提早結束，但每一個 predicate 都吃得到 SIMD。
//
// mask 用 bit 而不是一個 row 一個 byte，是為了讓 AND 一次處理 128 個 row。
// ---------------------------------------------------------------------------

// n 個 row 的 bit mask 要幾個 byte。
constexpr std::uint32_t MaskBytes(std::uint32_t n) { return (n + 7) / 8; }

// selection vector 的輸出緩衝區必須比 batch 多留這麼多空間：NEON 版一次
// 無條件寫 8 個索引、再依實際通過的數量前進，所以最後一組可能多寫幾個。
constexpr std::uint32_t kSelectionSlack = 8;

struct MaskKernels {
  // 把 predicate 套在連續的一整欄上，結果寫成 bit mask。
  void (*compute_mask)(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                       std::uint8_t* mask);

  // acc &= other
  void (*and_mask)(std::uint8_t* acc, const std::uint8_t* other, std::uint32_t bytes);

  // bit mask → 索引陣列。回傳通過的數量。
  // out 必須至少有 n + kSelectionSlack 個元素。
  std::uint32_t (*mask_to_selection)(const std::uint8_t* mask, std::uint32_t n,
                                     std::uint16_t* out);

  // 反方向：上游已經帶了 selection vector 時，先把它轉成 mask 再往下 AND。
  void (*selection_to_mask)(const std::uint16_t* sel, std::uint32_t sel_n, std::uint32_t n,
                            std::uint8_t* mask);
};

// 一次呼叫處理一整個 batch，所以這裡的間接呼叫對每個 row 的成本可以忽略。
const MaskKernels& Kernels(SimdKernel kernel);

namespace scalar {

void ComputeMask(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                 std::uint8_t* mask);
void AndMask(std::uint8_t* acc, const std::uint8_t* other, std::uint32_t bytes);
std::uint32_t MaskToSelection(const std::uint8_t* mask, std::uint32_t n, std::uint16_t* out);
void SelectionToMask(const std::uint16_t* sel, std::uint32_t sel_n, std::uint32_t n,
                     std::uint8_t* mask);

}  // namespace scalar

}  // namespace minidb
