#include <cstring>

#include "execution/vectorized/mask.h"

namespace minidb {
namespace scalar {
namespace {

// 把運算子變成編譯期常數，內層迴圈裡就沒有 switch 了。
// 這很重要：這個版本是「只有批次、沒有手寫 SIMD」的對照組，如果每個 row 都
// 還要判斷一次要做哪種比較，量到的就不只是批次的效果。
template <CompareOp kOp>
constexpr bool CompareStatic(std::int64_t lhs, std::int64_t rhs) {
  if constexpr (kOp == CompareOp::kEq) {
    return lhs == rhs;
  } else if constexpr (kOp == CompareOp::kNe) {
    return lhs != rhs;
  } else if constexpr (kOp == CompareOp::kLt) {
    return lhs < rhs;
  } else if constexpr (kOp == CompareOp::kLe) {
    return lhs <= rhs;
  } else if constexpr (kOp == CompareOp::kGt) {
    return lhs > rhs;
  } else {
    return lhs >= rhs;
  }
}

template <CompareOp kOp>
void ComputeMaskImpl(const std::int64_t* column, std::uint32_t n, std::int64_t constant,
                     std::uint8_t* mask) {
  std::memset(mask, 0, MaskBytes(n));
  for (std::uint32_t i = 0; i < n; ++i) {
    if (CompareStatic<kOp>(column[i], constant)) {
      mask[i >> 3] |= static_cast<std::uint8_t>(1u << (i & 7));
    }
  }
}

}  // namespace

void ComputeMask(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                 std::uint8_t* mask) {
  switch (predicate.op) {
    case CompareOp::kEq:
      return ComputeMaskImpl<CompareOp::kEq>(column, n, predicate.constant, mask);
    case CompareOp::kNe:
      return ComputeMaskImpl<CompareOp::kNe>(column, n, predicate.constant, mask);
    case CompareOp::kLt:
      return ComputeMaskImpl<CompareOp::kLt>(column, n, predicate.constant, mask);
    case CompareOp::kLe:
      return ComputeMaskImpl<CompareOp::kLe>(column, n, predicate.constant, mask);
    case CompareOp::kGt:
      return ComputeMaskImpl<CompareOp::kGt>(column, n, predicate.constant, mask);
    case CompareOp::kGe:
      return ComputeMaskImpl<CompareOp::kGe>(column, n, predicate.constant, mask);
  }
}

void AndMask(std::uint8_t* acc, const std::uint8_t* other, std::uint32_t bytes) {
  for (std::uint32_t i = 0; i < bytes; ++i) {
    acc[i] = static_cast<std::uint8_t>(acc[i] & other[i]);
  }
}

std::uint32_t MaskToSelection(const std::uint8_t* mask, std::uint32_t n, std::uint16_t* out) {
  std::uint32_t written = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    if ((mask[i >> 3] >> (i & 7)) & 1u) {
      out[written++] = static_cast<std::uint16_t>(i);
    }
  }
  return written;
}

void SelectionToMask(const std::uint16_t* sel, std::uint32_t sel_n, std::uint32_t n,
                     std::uint8_t* mask) {
  std::memset(mask, 0, MaskBytes(n));
  for (std::uint32_t i = 0; i < sel_n; ++i) {
    const std::uint16_t row = sel[i];
    mask[row >> 3] |= static_cast<std::uint8_t>(1u << (row & 7));
  }
}

}  // namespace scalar

namespace {
constexpr MaskKernels kScalarKernels = {
    &scalar::ComputeMask,
    &scalar::AndMask,
    &scalar::MaskToSelection,
    &scalar::SelectionToMask,
};
}  // namespace

const MaskKernels& Kernels(SimdKernel kernel) {
  switch (kernel) {
    case SimdKernel::kScalar:
      return kScalarKernels;
  }
  return kScalarKernels;
}

}  // namespace minidb
