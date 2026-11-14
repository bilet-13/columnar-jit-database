#pragma once

#include <cstdint>

#include "execution/batch.h"
#include "execution/operator.h"
#include "storage/table.h"

namespace minidb {

// ---------------------------------------------------------------------------
// SeqScan：樹的葉子。把一張表的所有欄位切成 batch 往上送。
//
// 零複製：因為 chunk 大小是 batch 大小的整數倍，一個 batch 永遠落在單一
// chunk 內，所以輸出的指標就是 chunk 記憶體裡的一個位置，沒有搬動任何資料。
//
// 三種執行器共用這一份程式碼，差別只在 batch_size（interpreter 給 1）。
// ---------------------------------------------------------------------------
class SeqScan final : public Operator {
 public:
  SeqScan(const Table& table, std::uint32_t batch_size);

  Batch* Next() override;

 private:
  const Table* table_;
  std::uint32_t batch_size_;
  std::uint32_t num_cols_;
  std::uint32_t chunk_ = 0;    // 目前在第幾個 chunk
  std::uint32_t offset_ = 0;   // 在這個 chunk 內的位置
  Batch out_;
};

}  // namespace minidb
