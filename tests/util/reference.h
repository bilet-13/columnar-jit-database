#pragma once

#include <cstdint>

#include "util/data_gen.h"
#include "util/queries.h"

namespace minidb::testutil {

std::int64_t ReferenceResult(QueryId id, const Dataset& dataset);

std::uint64_t ReferenceMatchingRows(QueryId id, const Dataset& dataset);

}
