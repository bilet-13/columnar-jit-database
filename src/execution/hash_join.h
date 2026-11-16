#pragma once

#include <cstdint>
#include <vector>

#include "common/types.h"
#include "execution/batch.h"
#include "execution/hash_table.h"
#include "execution/operator.h"

namespace minidb {

struct HashJoinSpec {
  ColumnIndex probe_key;             // probe 端（左邊）的 join 欄位
  ColumnIndex build_key;             // build 端（右邊）的 join 欄位
  std::uint32_t probe_num_cols = 0;  // probe 端有幾欄
  std::uint32_t build_num_cols = 0;  // build 端有幾欄
  std::uint32_t batch_size = kBatchSize;

  // true 時 probe 分兩輪：先把一整批的探測起點都算出來並預取，再回頭查表。
  // 見 hash_join.cpp 的說明。interpreter 模式用不到（batch 只有一個 row）。
  bool batched_probe = false;
};

// ---------------------------------------------------------------------------
// HashJoin：inner equi-join，分 build 與 probe 兩階段（docs/design.md 7.5）。
//
// build：把右邊（通常是小表）整個吃完，值複製進自己的緩衝區，key 進雜湊表。
//        一定要複製——上層拿到的 Batch 只是借來的指標，下次 Next() 就失效。
// probe：左邊一批一批進來，每列拿 key 去查表，配對到就輸出。
//
// 「第一次被呼叫時把 build 端吸乾」在 pull 模型裡就是一個 while 迴圈，這是
// 當初選 pull 而不選 push 的主要理由。
//
// 輸出直接 materialize 成連續的欄位陣列（左邊的欄位接右邊的欄位），不是
// rowid 配對。上層的 aggregate 與 filter 因此完全不必知道 join 的存在，
// SIMD kernel 可以直接吃。
//
// 一批 probe 資料可能產生比 batch_size 更多的配對（key 重複時），所以配對
// 先全部寫進緩衝區，再分幾次切成 batch 交出去。
// ---------------------------------------------------------------------------
class HashJoin final : public Operator {
 public:
  HashJoin(Operator& probe_child, Operator& build_child, const HashJoinSpec& spec);

  Batch* Next() override;

  std::uint64_t build_rows() const { return build_rows_; }

 private:
  void BuildSide();
  void ProbeOneBatch(const Batch& in);
  void ProbeRowAtATime(const Batch& in);
  void ProbeBatched(const Batch& in);
  void EmitMatch(const Batch& in, std::uint32_t offset, std::uint32_t build_row);

  Operator* probe_child_;
  Operator* build_child_;
  HashJoinSpec spec_;
  std::uint32_t total_cols_;

  bool built_ = false;
  HashTable hash_table_;
  std::vector<std::vector<std::int64_t>> build_cols_;  // build 端複製出來的值
  std::uint64_t build_rows_ = 0;

  std::vector<std::vector<std::int64_t>> out_cols_;  // 這一批 probe 產生的配對
  std::uint64_t out_rows_ = 0;
  std::uint64_t out_pos_ = 0;
  Batch out_;

  std::vector<std::uint64_t> probe_slots_;  // 批次 probe 的探測起點
  std::vector<std::int64_t> probe_keys_;
};

}  // namespace minidb
