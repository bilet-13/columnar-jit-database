#pragma once

#include <cstdint>
#include <vector>

#include "common/types.h"

namespace minidb {

// ---------------------------------------------------------------------------
// Column：一個欄位的所有資料，切成固定大小的 chunk（docs/design.md 7.3）。
//
// 為什麼要切 chunk 而不是用一條長陣列：
//   - v1.1 的平行掃描以 chunk 為工作單位（一個 chunk 一個 task）
//   - v1.4 的 zone map 每個 chunk 存一組 min/max
//   - 不需要一塊幾十 MB 的連續記憶體，成長時也不必整塊搬動
//
// kChunkSize 是 kBatchSize 的整數倍，所以一個 batch 永遠落在單一 chunk 內，
// scan 可以直接把 chunk_data() + offset 交給上層，不必複製。
//
// 注意：Append() 可能讓先前取得的 chunk_data() 指標失效（chunk 內的 vector
// 成長時會搬家）。使用方式是「先把資料全部載入，再開始查詢」，MVP 不支援
// 邊查邊寫。
// ---------------------------------------------------------------------------
class Column {
 public:
  void Append(std::int64_t value);

  // 預留空間的提示，避免載入大表時反覆成長。不改變 num_rows()。
  void Reserve(std::uint64_t num_rows);

  std::uint64_t num_rows() const { return num_rows_; }
  std::uint32_t num_chunks() const { return static_cast<std::uint32_t>(chunks_.size()); }

  // 第 chunk 個 chunk 的連續記憶體起點。給 SIMD / JIT kernel 直接吃。
  // 越界會丟 std::out_of_range。
  const std::int64_t* chunk_data(std::uint32_t chunk) const;

  // 第 chunk 個 chunk 實際有幾列（只有最後一個可能不滿）。
  std::uint32_t chunk_rows(std::uint32_t chunk) const;

  // 隨機存取第 row 列。hash join 把 build 端的值複製出來時用。
  // 越界會丟 std::out_of_range。
  std::int64_t At(std::uint64_t row) const;

 private:
  std::vector<std::vector<std::int64_t>> chunks_;
  std::uint64_t num_rows_ = 0;
};

}  // namespace minidb
