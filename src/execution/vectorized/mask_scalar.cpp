#include <cstring>

#include "execution/vectorized/compare_static.h"
#include "execution/vectorized/mask.h"

namespace minidb {
namespace scalar {
namespace {

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

}  // namespace

void ComputeMask(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                 std::uint8_t* mask) {
  DispatchCompareOp(predicate.op, ComputeMaskDispatch{column, n, predicate.constant, mask});
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
      out[written] = static_cast<std::uint16_t>(i);
      ++written;
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
}  // namespace minidb
