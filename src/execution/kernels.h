#pragma once

#include <cstdint>
#include <string_view>

namespace minidb {

// ---------------------------------------------------------------------------
// 一個 kernel 要用純量還是手寫 SIMD。
//
// 這不是 ExecMode：ExecMode 決定「filter 走 AST、走批次、還是走編譯出來的
// 機器碼」，而這個開關只決定批次路徑裡的 kernel 怎麼實作。保留純量版是為了
// 量出「批次本身賺多少」與「手寫 NEON 再賺多少」這兩個數字 —— 兩者混在一起
// 的話，SIMD 的功勞會被高估。
// ---------------------------------------------------------------------------
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

}  // namespace minidb
