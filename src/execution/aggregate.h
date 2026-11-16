#pragma once

#include <cstdint>

#include "common/types.h"
#include "execution/batch.h"
#include "execution/kernels.h"
#include "execution/operator.h"
#include "plan/logical_plan.h"

namespace minidb {

// ---------------------------------------------------------------------------
// 聚合 kernel
//
// 累加器用 uint64：C++ 的 signed 整數溢位是未定義行為，UBSan 會直接中止，
// 而三種執行器的加法順序不同，溢位後的結果也可能不一致。unsigned 的 wrap
// 是標準定義好的，而且加法可交換，所以不管怎麼分組累加結果都 bit 級相同
// ——這正是 SIMD 版可以用四個獨立累加器的前提。
//
// 兩個入口的差別是輸入有沒有 selection vector：
//   SumAll  連續記憶體，SIMD 效果最好
//   SumSel  跳著取值（gather），NEON 沒有 64-bit gather，所以只有純量版
//
// COUNT 不需要 kernel：有沒有 selection vector，答案都是一個現成的數字。
// ---------------------------------------------------------------------------
struct SumKernels {
  std::uint64_t (*sum_all)(const std::int64_t* data, std::uint32_t n);
  std::uint64_t (*sum_sel)(const std::int64_t* data, const std::uint16_t* sel, std::uint32_t n);
};

const SumKernels& AggKernels(SimdKernel kernel);

namespace scalar {
std::uint64_t SumAll(const std::int64_t* data, std::uint32_t n);
std::uint64_t SumSel(const std::int64_t* data, const std::uint16_t* sel, std::uint32_t n);
}  // namespace scalar

namespace neon {
std::uint64_t SumAll(const std::int64_t* data, std::uint32_t n);
}  // namespace neon

// ---------------------------------------------------------------------------
// Aggregate：沒有 GROUP BY，所以輸出剛好一個 row 一個 column。
//
// 它是計畫樹的根，第一次 Next() 會把下面整棵樹吸乾。
// ---------------------------------------------------------------------------
class Aggregate final : public Operator {
 public:
  // kCount 忽略 column。
  Aggregate(Operator& child, AggKind agg, ColumnIndex column, SimdKernel kernel);

  Batch* Next() override;

  // 跑完之後的結果；還沒跑就是 0。
  std::int64_t result() const { return result_; }

 private:
  Operator* child_;
  AggKind agg_;
  ColumnIndex column_;
  const SumKernels* kernels_;
  bool done_ = false;
  std::int64_t result_ = 0;
  Batch out_;
};

}  // namespace minidb
