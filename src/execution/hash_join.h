#pragma once

#include <cstdint>
#include <vector>

#include "common/types.h"
#include "execution/batch.h"
#include "execution/hash_table.h"
#include "execution/operator.h"

namespace minidb {

struct HashJoinSpec {
  ColumnIndex probe_key;
  ColumnIndex build_key;
  std::uint32_t probe_num_cols = 0;
  std::uint32_t build_num_cols = 0;
  std::uint32_t batch_size = kBatchSize;

  bool batched_probe = false;
};

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
  std::vector<std::vector<std::int64_t>> build_cols_;
  std::uint64_t build_rows_ = 0;

  std::vector<std::vector<std::int64_t>> out_cols_;
  std::uint64_t out_rows_ = 0;
  std::uint64_t out_pos_ = 0;
  Batch out_;

  std::vector<std::uint64_t> probe_slots_;
  std::vector<std::int64_t> probe_keys_;
};

}
