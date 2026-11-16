#include "execution/vectorized/mask.h"

namespace minidb {
namespace {

constexpr MaskKernels kScalarKernels = {
    &scalar::ComputeMask,
    &scalar::AndMask,
    &scalar::MaskToSelection,
    &scalar::SelectionToMask,
};

constexpr MaskKernels kNeonKernels = {
    &neon::ComputeMask,
    &neon::AndMask,
    &neon::MaskToSelection,
    // selection vector → mask 是 scatter（跳著寫），NEON 沒有對應的指令，
    // 所以這一格刻意用純量版。而且它只在 filter 疊 filter 時才會被呼到。
    &scalar::SelectionToMask,
};

}  // namespace

const MaskKernels& Kernels(SimdKernel kernel) {
  switch (kernel) {
    case SimdKernel::kScalar:
      return kScalarKernels;
    case SimdKernel::kNeon:
      return kNeonKernels;
  }
  return kScalarKernels;
}

}  // namespace minidb
