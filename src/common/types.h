#pragma once

#include <cstdint>

namespace minidb {

enum class TypeId : std::uint8_t {
  kInt64,
};

constexpr std::uint32_t kBatchSize = 1024;

constexpr std::uint32_t kChunkSize = 65536;
static_assert(kChunkSize % kBatchSize == 0,
              "chunk 必須是 batch 的整數倍，否則 batch 會跨 chunk 邊界");

constexpr std::uint32_t kMaxCols = 16;

static_assert(kBatchSize <= 65536, "selection vector 是 uint16_t，batch 不能超過 65536");

using ColumnIndex = std::uint32_t;
using RowId = std::uint64_t;

}
