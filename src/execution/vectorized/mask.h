#pragma once

#include <cstdint>

#include "common/types.h"
#include "execution/kernels.h"
#include "plan/expression.h"

namespace minidb {

constexpr std::uint32_t MaskBytes(std::uint32_t n) { return (n + 7) / 8; }

constexpr std::uint32_t kSelectionSlack = 8;

struct MaskKernels {
  void (*compute_mask)(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                       std::uint8_t* mask);

  void (*and_mask)(std::uint8_t* acc, const std::uint8_t* other, std::uint32_t bytes);

  std::uint32_t (*mask_to_selection)(const std::uint8_t* mask, std::uint32_t n,
                                     std::uint16_t* out);

  void (*selection_to_mask)(const std::uint16_t* sel, std::uint32_t sel_n, std::uint32_t n,
                            std::uint8_t* mask);
};

const MaskKernels& Kernels(SimdKernel kernel);

namespace scalar {

void ComputeMask(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                 std::uint8_t* mask);
void AndMask(std::uint8_t* acc, const std::uint8_t* other, std::uint32_t bytes);
std::uint32_t MaskToSelection(const std::uint8_t* mask, std::uint32_t n, std::uint16_t* out);
void SelectionToMask(const std::uint16_t* sel, std::uint32_t sel_n, std::uint32_t n,
                     std::uint8_t* mask);

}

namespace neon {

void ComputeMask(const Predicate& predicate, const std::int64_t* column, std::uint32_t n,
                 std::uint8_t* mask);
void AndMask(std::uint8_t* acc, const std::uint8_t* other, std::uint32_t bytes);
std::uint32_t MaskToSelection(const std::uint8_t* mask, std::uint32_t n, std::uint16_t* out);

}

}
