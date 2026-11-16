#pragma once

#include <array>
#include <cstdint>

#include "common/types.h"

namespace minidb {

struct Batch {
  std::array<const std::int64_t*, kMaxCols> cols{};
  std::uint32_t num_cols = 0;

  std::uint32_t n = 0;

  const std::uint16_t* sel = nullptr;
  std::uint32_t sel_n = 0;

  std::uint32_t ActiveRows() const { return sel == nullptr ? n : sel_n; }

  std::uint32_t RowOffset(std::uint32_t i) const { return sel == nullptr ? i : sel[i]; }

  std::int64_t Value(std::uint32_t col, std::uint32_t i) const {
    return cols[col][RowOffset(i)];
  }

  void ClearSelection() {
    sel = nullptr;
    sel_n = 0;
  }
};

}
