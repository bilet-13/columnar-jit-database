#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "catalog/catalog.h"
#include "plan/logical_plan.h"
#include "util/data_gen.h"

namespace minidb::testutil {

// ---------------------------------------------------------------------------
// docs/design.md 8.3 的固定查詢集。
//
// 每個 phase 都跑同一組查詢，數字才能互相比較。它們也是三種執行器交叉驗證
// 的題目。
//
//   Q1  SUM(amount)                                    沒有 filter 的基準
//   Q2  WHERE amount > 100                             高選擇率（約 90% 通過）
//   Q3  WHERE amount > 900                             低選擇率（約 10% 通過）
//   Q4  三個條件 AND                                   解譯開銷隨條件數成長
//   Q5  3 表 chain join + 兩個 filter                  optimizer 的主角
// ---------------------------------------------------------------------------
enum class QueryId : std::uint8_t { kQ1, kQ2, kQ3, kQ4, kQ5 };

constexpr std::array<QueryId, 5> kAllQueries = {QueryId::kQ1, QueryId::kQ2, QueryId::kQ3,
                                                QueryId::kQ4, QueryId::kQ5};

std::string_view QueryName(QueryId id);

// 用「使用者寫的順序」組出計畫，不做任何最佳化。
LogicalNodePtr MakePlan(QueryId id, const Catalog& catalog, const DatasetSizes& sizes);

// Q2 / Q3 的門檻與 Q4 的上下界，寫在一處讓參考實作共用。
constexpr std::int64_t kQ2Threshold = 100;
constexpr std::int64_t kQ3Threshold = 900;
constexpr std::int64_t kQ4Low = 100;
constexpr std::int64_t kQ4High = 900;
constexpr std::int64_t kQ5AmountThreshold = 500;
constexpr std::int64_t kQ5PopThreshold = 900;

}  // namespace minidb::testutil
