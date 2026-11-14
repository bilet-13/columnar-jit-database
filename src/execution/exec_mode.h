#pragma once

#include <cstdint>
#include <string_view>

#include "common/types.h"

namespace minidb {

// ---------------------------------------------------------------------------
// 三種執行器。
//
// 它們共用同一棵 operator 樹，只有 filter 的實作不同（docs/design.md 7.1）：
//   kInterpreter  一次一列，走表達式 AST            ← 基準
//   kVectorized   一次 1024 列，手寫 NEON kernel     ← Phase 2
//   kJit          一次 1024 列，LLVM 編出來的函式    ← Phase 4
//
// 其餘 operator（scan、hash join、aggregate）只有一份程式碼，所以 benchmark
// 是乾淨的對照實驗：唯一的變數就是 filter 怎麼做。
// ---------------------------------------------------------------------------
enum class ExecMode : std::uint8_t { kInterpreter, kVectorized, kJit };

// Interpreter 刻意用 batch = 1，解譯開銷才會誠實地出現在每一列上。
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

}  // namespace minidb
