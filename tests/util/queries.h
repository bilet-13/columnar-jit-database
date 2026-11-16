#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "catalog/catalog.h"
#include "plan/logical_plan.h"
#include "util/data_gen.h"

namespace minidb::testutil {

enum class QueryId : std::uint8_t { kQ1, kQ2, kQ3, kQ4, kQ5 };

constexpr std::array<QueryId, 5> kAllQueries = {QueryId::kQ1, QueryId::kQ2, QueryId::kQ3,
                                                QueryId::kQ4, QueryId::kQ5};

std::string_view QueryName(QueryId id);

LogicalNodePtr MakePlan(QueryId id, const Catalog& catalog, const DatasetSizes& sizes);

constexpr std::int64_t kQ2Threshold = 100;
constexpr std::int64_t kQ3Threshold = 900;
constexpr std::int64_t kQ4Low = 100;
constexpr std::int64_t kQ4High = 900;
constexpr std::int64_t kQ5AmountThreshold = 500;
constexpr std::int64_t kQ5PopThreshold = 900;

}
