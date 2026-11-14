#pragma once

#include <array>
#include <cstdint>

#include "common/types.h"

namespace minidb {

// ---------------------------------------------------------------------------
// Batch：operator 之間一次傳遞的一批資料（docs/design.md 7.2）。
//
// cols[c] 指向第 c 個輸出欄位在這一批的第一列，後面是 n 個連續的 int64。
// 「連續」是重點：SIMD kernel 與 JIT 編出來的函式都直接吃這段記憶體。
// Batch 不持有資料，只是一組指標 —— scan 指向 columnstore 的 chunk，
// hash join 指向它自己 materialize 出來的緩衝區。
//
// selection vector（sel）是「通過 filter 的列在這一批裡的 offset 清單」。
// nullptr 代表全部有效。用索引陣列而不是 bitmask，是因為 NEON 的比較結果
// 轉成索引陣列之後，下游可以直接跳著取值。
//
// 為了把複雜度關住，規定：只有 filter 會產生 sel，而且只有它的直接上層
// （aggregate 或 hash join）需要處理。其他 operator 只會看到 sel == nullptr。
// ---------------------------------------------------------------------------
struct Batch {
  std::array<const std::int64_t*, kMaxCols> cols{};
  std::uint32_t num_cols = 0;

  // cols 裡有幾列（不管 sel）。
  std::uint32_t n = 0;

  const std::uint16_t* sel = nullptr;
  std::uint32_t sel_n = 0;

  // 實際有效的列數。
  std::uint32_t ActiveRows() const { return sel == nullptr ? n : sel_n; }

  // 第 i 個有效列在 cols 裡的位置。
  std::uint32_t RowOffset(std::uint32_t i) const { return sel == nullptr ? i : sel[i]; }

  // 第 i 個有效列的第 col 欄的值。
  std::int64_t Value(std::uint32_t col, std::uint32_t i) const {
    return cols[col][RowOffset(i)];
  }

  void ClearSelection() {
    sel = nullptr;
    sel_n = 0;
  }
};

}  // namespace minidb
