#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalog/catalog.h"
#include "plan/expression.h"
#include "plan/logical_plan.h"

namespace minidb {

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

class QueryBuilder {
 public:
  explicit QueryBuilder(const Catalog& catalog) : catalog_(&catalog) {}

  QueryBuilder& Scan(std::string table);

  QueryBuilder& Filter(std::vector<ColumnPredicate> predicates);

  QueryBuilder& Join(std::string table, std::string left_column, std::string right_column);

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

}
