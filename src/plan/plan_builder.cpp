#include "plan/plan_builder.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace minidb {

ColumnPredicate Eq(std::string column, std::int64_t constant) {
  return {std::move(column), CompareOp::kEq, constant};
}
ColumnPredicate Ne(std::string column, std::int64_t constant) {
  return {std::move(column), CompareOp::kNe, constant};
}
ColumnPredicate Lt(std::string column, std::int64_t constant) {
  return {std::move(column), CompareOp::kLt, constant};
}
ColumnPredicate Le(std::string column, std::int64_t constant) {
  return {std::move(column), CompareOp::kLe, constant};
}
ColumnPredicate Gt(std::string column, std::int64_t constant) {
  return {std::move(column), CompareOp::kGt, constant};
}
ColumnPredicate Ge(std::string column, std::int64_t constant) {
  return {std::move(column), CompareOp::kGe, constant};
}

void QueryBuilder::RequireRoot(const char* what) const {
  if (root_ == nullptr) {
    throw std::invalid_argument(std::string("QueryBuilder::") + what + " 必須在 Scan() 之後");
  }
}

void QueryBuilder::RequireNotAggregated(const char* what) const {
  if (aggregated_) {
    throw std::invalid_argument(std::string("QueryBuilder::") + what + " 不能接在聚合之後");
  }
}

QueryBuilder& QueryBuilder::Scan(std::string table) {
  if (root_ != nullptr) {
    throw std::invalid_argument("QueryBuilder::Scan 只能是第一步；要加表請用 Join()");
  }
  const Table& t = catalog_->GetTable(table);
  root_ = std::make_shared<const LogicalScan>(std::move(table), t.schema());
  return *this;
}

QueryBuilder& QueryBuilder::Filter(std::vector<ColumnPredicate> predicates) {
  RequireRoot("Filter");
  RequireNotAggregated("Filter");
  if (predicates.empty()) {
    throw std::invalid_argument("QueryBuilder::Filter 至少要有一個條件");
  }

  const Schema& schema = root_->schema();
  std::vector<ExprPtr> conditions;
  conditions.reserve(predicates.size());
  for (const ColumnPredicate& predicate : predicates) {
    conditions.push_back(MakeColumnCompare(schema.ResolveColumnIndex(predicate.column),
                                           predicate.op, predicate.constant));
  }

  root_ = std::make_shared<const LogicalFilter>(root_, MakeAnd(std::move(conditions)));
  return *this;
}

QueryBuilder& QueryBuilder::Join(std::string table, std::string left_column,
                                 std::string right_column) {
  RequireRoot("Join");
  RequireNotAggregated("Join");

  const Table& t = catalog_->GetTable(table);
  const LogicalNodePtr right = std::make_shared<const LogicalScan>(std::move(table), t.schema());

  const ColumnIndex left_key = root_->schema().ResolveColumnIndex(left_column);
  const ColumnIndex right_key = right->schema().ResolveColumnIndex(right_column);

  root_ = std::make_shared<const LogicalJoin>(root_, right, left_key, right_key);
  return *this;
}

QueryBuilder& QueryBuilder::Sum(std::string column) {
  RequireRoot("Sum");
  RequireNotAggregated("Sum");
  const ColumnIndex index = root_->schema().ResolveColumnIndex(column);
  root_ = std::make_shared<const LogicalAggregate>(root_, AggKind::kSum, index);
  aggregated_ = true;
  return *this;
}

QueryBuilder& QueryBuilder::Count() {
  RequireRoot("Count");
  RequireNotAggregated("Count");
  root_ = std::make_shared<const LogicalAggregate>(root_, AggKind::kCount, 0);
  aggregated_ = true;
  return *this;
}

LogicalNodePtr QueryBuilder::Build() const {
  if (root_ == nullptr) {
    throw std::invalid_argument("QueryBuilder::Build: 計畫是空的");
  }
  return root_;
}

}
