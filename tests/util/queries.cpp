#include "util/queries.h"

#include <stdexcept>

#include "plan/plan_builder.h"

namespace minidb::testutil {

std::string_view QueryName(QueryId id) {
  switch (id) {
    case QueryId::kQ1:
      return "Q1";
    case QueryId::kQ2:
      return "Q2";
    case QueryId::kQ3:
      return "Q3";
    case QueryId::kQ4:
      return "Q4";
    case QueryId::kQ5:
      return "Q5";
  }
  return "?";
}

LogicalNodePtr MakePlan(QueryId id, const Catalog& catalog, const DatasetSizes& sizes) {
  switch (id) {
    case QueryId::kQ1:
      return QueryBuilder(catalog).Scan("orders").Sum("amount").Build();

    case QueryId::kQ2:
      return QueryBuilder(catalog)
          .Scan("orders")
          .Filter({Gt("amount", kQ2Threshold)})
          .Sum("amount")
          .Build();

    case QueryId::kQ3:
      return QueryBuilder(catalog)
          .Scan("orders")
          .Filter({Gt("amount", kQ3Threshold)})
          .Sum("amount")
          .Build();

    case QueryId::kQ4:
      return QueryBuilder(catalog)
          .Scan("orders")
          .Filter({Gt("amount", kQ4Low), Lt("amount", kQ4High),
                   Lt("cust_id", static_cast<std::int64_t>(sizes.num_customers / 2))})
          .Sum("amount")
          .Build();

    case QueryId::kQ5:
      // 使用者寫的順序：先 join 大表。Phase 3 的 optimizer 要找出更好的順序。
      return QueryBuilder(catalog)
          .Scan("orders")
          .Filter({Gt("amount", kQ5AmountThreshold)})
          .Join("customers", "orders.cust_id", "customers.id")
          .Join("cities", "customers.city_id", "cities.id")
          .Filter({Gt("cities.pop", kQ5PopThreshold)})
          .Sum("orders.amount")
          .Build();
  }
  throw std::invalid_argument("MakePlan: 未知的 QueryId");
}

}  // namespace minidb::testutil
