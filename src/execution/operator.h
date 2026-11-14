#pragma once

#include <cstdint>

#include "execution/batch.h"

namespace minidb {

// ---------------------------------------------------------------------------
// Operator：計畫樹上的一個節點。整個執行引擎的介面只有一個函式。
//
// Pull-based（docs/design.md 7.1）：上層呼叫下層的 Next() 要資料，資料順著
// 回傳值往上流。整棵樹就是一個巨大的迭代器，沒人呼叫 Next() 時引擎靜止。
//
// 選 pull 而不是 push，主要是因為 hash join 有兩個下層、而且必須先把 build
// 端整個吃完才能開始 probe —— 在 pull 裡那就是「第一次被呼叫時用 while 把
// build 端吸乾」，幾行就解決。
//
// 回傳的 Batch* 只在下次呼叫 Next() 之前有效，而且 Batch 裡的指標是借來的。
// 需要留住值的 operator（hash join 的 build 端）必須自己複製。
//
// 回傳 nullptr 代表資料結束。「這一批全被篩掉」不能用空 batch 表示，否則
// 上層會誤判結束，所以 filter 會自己回到迴圈開頭再要一批。
// ---------------------------------------------------------------------------
class Operator {
 public:
  Operator() = default;
  virtual ~Operator() = default;

  Operator(const Operator&) = delete;
  Operator& operator=(const Operator&) = delete;

  virtual Batch* Next() = 0;

  // 這個節點實際輸出了幾列。Phase 3 的 explain 用它跟估計值對照。
  std::uint64_t rows_out() const { return rows_out_; }

 protected:
  std::uint64_t rows_out_ = 0;
};

}  // namespace minidb
