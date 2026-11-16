#pragma once

#include <cstdint>
#include <string_view>

namespace minidb {

enum class SimdKernel : std::uint8_t { kScalar, kNeon };

constexpr std::string_view ToString(SimdKernel kernel) {
  switch (kernel) {
    case SimdKernel::kScalar:
      return "scalar";
    case SimdKernel::kNeon:
      return "neon";
  }
  return "unknown";
}

}
