#pragma once

#include <cstdint>
#include <string_view>

#include "common/types.h"

namespace minidb {

enum class ExecMode : std::uint8_t { kInterpreter, kVectorized, kJit };

constexpr std::uint32_t BatchSizeFor(ExecMode mode) {
  return mode == ExecMode::kInterpreter ? 1u : kBatchSize;
}

constexpr std::string_view ToString(ExecMode mode) {
  switch (mode) {
    case ExecMode::kInterpreter:
      return "interpreter";
    case ExecMode::kVectorized:
      return "vectorized";
    case ExecMode::kJit:
      return "jit";
  }
  return "unknown";
}

}
