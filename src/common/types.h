#pragma once

#include <cstdint>

namespace minidb {

// ---------------------------------------------------------------------------
// 型別
//
// MVP 只有 INT64 一種型別（docs/design.md 第 3 節）。保留 enum 是為了之後
// v1.4 加 DOUBLE / STRING 時，介面不用整個翻掉。
// ---------------------------------------------------------------------------
enum class TypeId : std::uint8_t {
  kInt64,
};

// ---------------------------------------------------------------------------
// 全專案常數（docs/design.md 7.2 / 7.3）
// ---------------------------------------------------------------------------

// Vectorized / JIT 執行器一次處理的列數。Interpreter 用 1。
constexpr std::uint32_t kBatchSize = 1024;

// Columnstore 每個 chunk 的列數。
//
// 刻意取成 kBatchSize 的整數倍（64 倍），這樣一個 batch 永遠不會跨越 chunk
// 邊界，scan 就能直接把 chunk 裡的連續記憶體切片交給上層，零複製。
constexpr std::uint32_t kChunkSize = 65536;
static_assert(kChunkSize % kBatchSize == 0,
              "chunk 必須是 batch 的整數倍，否則 batch 會跨 chunk 邊界");

// 一個 Batch 最多能帶幾個欄位。3 表 join 全欄位輸出也只有 8 欄，16 夠用。
constexpr std::uint32_t kMaxCols = 16;

// selection vector 用 uint16_t 存 batch 內的 offset，所以 batch 不能超過
// uint16_t 能表示的範圍。
static_assert(kBatchSize <= 65536, "selection vector 是 uint16_t，batch 不能超過 65536");

// ---------------------------------------------------------------------------
// 常用別名，讓介面讀起來比較清楚
// ---------------------------------------------------------------------------
using ColumnIndex = std::uint32_t;  // 欄位在 schema 裡的位置
using RowId = std::uint64_t;        // 一張表裡的第幾列

}  // namespace minidb
