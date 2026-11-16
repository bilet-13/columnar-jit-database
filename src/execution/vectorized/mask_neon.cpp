
#include <array>
#include <cstdint>

#include "execution/vectorized/compare_static.h"
#include "execution/vectorized/mask.h"

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace minidb {
namespace neon {

#if defined(__ARM_NEON)

namespace {

struct IndexTable {
  std::array<std::array<std::uint8_t, 8>, 256> indices{};
  std::array<std::uint8_t, 256> counts{};
};

constexpr IndexTable MakeIndexTable() {
  IndexTable table{};
  for (std::uint32_t bits = 0; bits < 256; ++bits) {
    std::uint8_t written = 0;
    for (std::uint8_t bit = 0; bit < 8; ++bit) {
      if ((bits >> bit) & 1u) {
        table.indices[bits][written] = bit;
        ++written;
      }
    }
    table.counts[bits] = written;
  }
  return table;
}

constexpr IndexTable kIndexTable = MakeIndexTable();

template <CompareOp kOp>
inline uint64x2_t CompareLanes(int64x2_t values, int64x2_t constant) {
  if constexpr (kOp == CompareOp::kEq) {
    return vceqq_s64(values, constant);
  } else if constexpr (kOp == CompareOp::kNe) {
    return vreinterpretq_u64_u32(vmvnq_u32(vreinterpretq_u32_u64(vceqq_s64(values, constant))));
  } else if constexpr (kOp == CompareOp::kLt) {
    return vcltq_s64(values, constant);
  } else if constexpr (kOp == CompareOp::kLe) {
    return vcleq_s64(values, constant);
  } else if constexpr (kOp == CompareOp::kGt) {
    return vcgtq_s64(values, constant);
  } else {
    return vcgeq_s64(values, constant);
  }
}

template <CompareOp kOp>
void ComputeMaskImpl(const std::int64_t* column, std::uint32_t n, std::int64_t constant,
                     std::uint8_t* mask) {
  const int64x2_t broadcast = vdupq_n_s64(constant);
  static constexpr std::uint16_t kBitWeights[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const uint16x8_t weights = vld1q_u16(kBitWeights);

  std::uint32_t i = 0;
  for (; i + 8 <= n; i += 8) {
    const uint64x2_t m0 = CompareLanes<kOp>(vld1q_s64(column + i), broadcast);
    const uint64x2_t m1 = CompareLanes<kOp>(vld1q_s64(column + i + 2), broadcast);
    const uint64x2_t m2 = CompareLanes<kOp>(vld1q_s64(column + i + 4), broadcast);
    const uint64x2_t m3 = CompareLanes<kOp>(vld1q_s64(column + i + 6), broadcast);

    const uint32x4_t low = vcombine_u32(vmovn_u64(m0), vmovn_u64(m1));
    const uint32x4_t high = vcombine_u32(vmovn_u64(m2), vmovn_u64(m3));
    const uint16x8_t lanes = vcombine_u16(vmovn_u32(low), vmovn_u32(high));

    mask[i >> 3] = static_cast<std::uint8_t>(vaddvq_u16(vandq_u16(lanes, weights)));
  }

  if (i < n) {
    std::uint8_t tail = 0;
    for (std::uint32_t row = i; row < n; ++row) {
      if (CompareStatic<kOp>(column[row], constant)) {
        tail = static_cast<std::uint8_t>(tail | (1u << (row & 7)));
      }
    }
    mask[i >> 3] = tail;
  }
}

struct ComputeMaskDispatch {
  const std::int64_t* column;
  std::uint32_t n;
  std::int64_t constant;
  std::uint8_t* mask;

  template <CompareOp kOp>
  int operator()() const {
    ComputeMaskImpl<kOp>(column, n, constant, mask);
    return 0;
  }
};

}

void ComputeMask(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                 std::uint8_t* mask) {
  DispatchCompareOp(predicate.op,
                    ComputeMaskDispatch{column, n, predicate.constant, mask});
}

void AndMask(std::uint8_t* acc, const std::uint8_t* other, std::uint32_t bytes) {
  std::uint32_t i = 0;
  for (; i + 16 <= bytes; i += 16) {
    vst1q_u8(acc + i, vandq_u8(vld1q_u8(acc + i), vld1q_u8(other + i)));
  }
  for (; i < bytes; ++i) {
    acc[i] = static_cast<std::uint8_t>(acc[i] & other[i]);
  }
}

std::uint32_t MaskToSelection(const std::uint8_t* mask, std::uint32_t n, std::uint16_t* out) {
  std::uint32_t written = 0;
  const std::uint32_t full_bytes = n >> 3;

  for (std::uint32_t byte = 0; byte < full_bytes; ++byte) {
    const std::uint8_t bits = mask[byte];
    const uint8x8_t packed = vld1_u8(kIndexTable.indices[bits].data());
    const uint16x8_t indices =
        vaddq_u16(vmovl_u8(packed), vdupq_n_u16(static_cast<std::uint16_t>(byte * 8)));
    vst1q_u16(out + written, indices);
    written += kIndexTable.counts[bits];
  }

  for (std::uint32_t row = full_bytes << 3; row < n; ++row) {
    if ((mask[row >> 3] >> (row & 7)) & 1u) {
      out[written] = static_cast<std::uint16_t>(row);
      ++written;
    }
  }
  return written;
}

#else

void ComputeMask(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                 std::uint8_t* mask) {
  scalar::ComputeMask(predicate, column, n, mask);
}

void AndMask(std::uint8_t* acc, const std::uint8_t* other, std::uint32_t bytes) {
  scalar::AndMask(acc, other, bytes);
}

std::uint32_t MaskToSelection(const std::uint8_t* mask, std::uint32_t n, std::uint16_t* out) {
  return scalar::MaskToSelection(mask, n, out);
}

#endif

}
}
