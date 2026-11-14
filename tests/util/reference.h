#pragma once

#include <cstdint>

#include "util/data_gen.h"
#include "util/queries.h"

namespace minidb::testutil {

// ---------------------------------------------------------------------------
// 參考實作：用最笨最直白的迴圈算出正確答案。
//
// 它刻意不使用任何引擎的程式碼——不經過 operator、不經過 batch、不經過
// 雜湊表，直接讀 Dataset 裡的原始 vector。這樣它不可能跟引擎犯同一個錯，
// 才有當標準答案的資格。慢沒關係。
//
// v1.3 加了 SQL parser 之後，這個角色會換成 SQLite。
// ---------------------------------------------------------------------------
std::int64_t ReferenceResult(QueryId id, const Dataset& dataset);

// 通過 filter 的列數，用來確認測資的選擇率符合設計（Q2 約 90%、Q3 約 10%）。
std::uint64_t ReferenceMatchingRows(QueryId id, const Dataset& dataset);

}  // namespace minidb::testutil
