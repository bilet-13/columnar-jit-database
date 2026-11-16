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
    &scalar::SelectionToMask,
};

}

const MaskKernels& Kernels(SimdKernel kernel) {
  switch (kernel) {
    case SimdKernel::kScalar:
      return kScalarKernels;
    case SimdKernel::kNeon:
      return kNeonKernels;
  }
  return kScalarKernels;
}

}
