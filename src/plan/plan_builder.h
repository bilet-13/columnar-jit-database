#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/catalog.h"
#include "plan/expression.h"
#include "plan/logical_plan.h"

namespace minidb {

// ---------------------------------------------------------------------------
// 用欄位名稱（而不是索引）寫的單一條件。
// QueryBuilder 會依當下的輸出 schema 把名稱解析成索引。
// ---------------------------------------------------------------------------
struct ColumnPredicate {
  std::string column;
  CompareOp op;
  std::int64_t constant;
};

ColumnPredicate Eq(std::string column, std::int64_t constant);
ColumnPredicate Ne(std::string column, std::int64_t constant);
ColumnPredicate Lt(std::string column, std::int64_t constant);
ColumnPredicate Le(std::string column, std::int64_t constant);
ColumnPredicate Gt(std::string column, std::int64_t constant);
ColumnPredicate Ge(std::string column, std::int64_t constant);

// ---------------------------------------------------------------------------
// QueryBuilder：取代 SQL parser 的流暢 C++ API（docs/design.md 第 3 節）。
//
//   QueryBuilder(catalog)
//       .Scan("orders")
//       .Filter({Gt("amount", 500)})
//       .Join("customers", "orders.cust_id", "customers.id")
//       .Sum("amount")
//       .Build();
//
// 它產生的是「使用者寫的順序」，不做任何最佳化——join 順序就是你呼叫
// Join() 的順序，filter 就留在你擺的位置。Phase 3 的 optimizer 要贏的
// 對照組就是這個。
// ---------------------------------------------------------------------------
class QueryBuilder {
 public:
  explicit QueryBuilder(const Catalog& catalog) : catalog_(&catalog) {}

  // 只能是第一步。
  QueryBuilder& Scan(std::string table);

  // 多個條件以 AND 串接。空清單會丟 std::invalid_argument。
  QueryBuilder& Filter(std::vector<ColumnPredicate> predicates);

  // Inner equi-join。right_column 必須是新加入那張表的欄位。
  QueryBuilder& Join(std::string table, std::string left_column, std::string right_column);

  // 聚合是最後一步，之後不能再接任何東西。
  QueryBuilder& Sum(std::string column);
  QueryBuilder& Count();

  LogicalNodePtr Build() const;

 private:
  void RequireRoot(const char* what) const;
  void RequireNotAggregated(const char* what) const;

  const Catalog* catalog_;
  LogicalNodePtr root_;
  bool aggregated_ = false;
};

}  // namespace minidb
