// NEON 版的 SUM（docs/design.md 7.2）。
//
// 關鍵不只是「一個指令加兩個數」，而是用四個互相獨立的累加器。單一累加器
// 的話每次加法都要等前一次的結果，整個迴圈被加法的延遲綁住；四個累加器讓
// CPU 可以同時跑四條加法鏈，最後再合起來。
//
// 這樣做會改變加總的順序，但累加器是 unsigned，加法可交換且 wrap 有定義，
// 所以結果跟純量版 bit 級相同 —— 這是 aggregate.h 選 uint64 的另一個好處。

#include <cstdint>

#include "execution/aggregate.h"

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace minidb {
namespace neon {

#if defined(__ARM_NEON)

std::uint64_t SumAll(const std::int64_t* data, std::uint32_t n) {
  uint64x2_t acc0 = vdupq_n_u64(0);
  uint64x2_t acc1 = vdupq_n_u64(0);
  uint64x2_t acc2 = vdupq_n_u64(0);
  uint64x2_t acc3 = vdupq_n_u64(0);

  std::uint32_t i = 0;
  for (; i + 8 <= n; i += 8) {
    acc0 = vaddq_u64(acc0, vreinterpretq_u64_s64(vld1q_s64(data + i)));
    acc1 = vaddq_u64(acc1, vreinterpretq_u64_s64(vld1q_s64(data + i + 2)));
    acc2 = vaddq_u64(acc2, vreinterpretq_u64_s64(vld1q_s64(data + i + 4)));
    acc3 = vaddq_u64(acc3, vreinterpretq_u64_s64(vld1q_s64(data + i + 6)));
  }

  std::uint64_t acc = vaddvq_u64(vaddq_u64(vaddq_u64(acc0, acc1), vaddq_u64(acc2, acc3)));
  for (; i < n; ++i) {
    acc += static_cast<std::uint64_t>(data[i]);
  }
  return acc;
}

#else  // 不是 ARM：退回純量版

std::uint64_t SumAll(const std::int64_t* data, std::uint32_t n) {
  return scalar::SumAll(data, n);
}

#endif

}  // namespace neon
}  // namespace minidb
