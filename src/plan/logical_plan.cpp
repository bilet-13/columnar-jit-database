#include "plan/logical_plan.h"

#include <stdexcept>

namespace minidb {

LogicalScan::LogicalScan(std::string table_name, Schema schema)
    : LogicalNode(LogicalKind::kScan, std::move(schema)), table_name_(std::move(table_name)) {}

LogicalFilter::LogicalFilter(LogicalNodePtr child, ExprPtr predicate)
    : LogicalNode(LogicalKind::kFilter, child == nullptr ? Schema() : child->schema()),
      child_(std::move(child)),
      predicate_(std::move(predicate)) {
  if (child_ == nullptr || predicate_ == nullptr) {
    throw std::invalid_argument("LogicalFilter: child 與 predicate 都不可為 null");
  }
}

LogicalJoin::LogicalJoin(LogicalNodePtr left, LogicalNodePtr right, ColumnIndex left_key,
                         ColumnIndex right_key)
    : LogicalNode(LogicalKind::kJoin, (left == nullptr || right == nullptr)
                                          ? Schema()
                                          : Schema::Concat(left->schema(), right->schema())),
      left_(std::move(left)),
      right_(std::move(right)),
      left_key_(left_key),
      right_key_(right_key) {
  if (left_ == nullptr || right_ == nullptr) {
    throw std::invalid_argument("LogicalJoin: 兩邊的子節點都不可為 null");
  }
  if (left_key_ >= left_->schema().num_columns()) {
    throw std::invalid_argument("LogicalJoin: 左邊的 join 欄位索引越界");
  }
  if (right_key_ >= right_->schema().num_columns()) {
    throw std::invalid_argument("LogicalJoin: 右邊的 join 欄位索引越界");
  }
}

namespace {

Schema AggregateSchema(const LogicalNodePtr& child, AggKind agg, ColumnIndex column) {
  if (child == nullptr) {
    return Schema();
  }
  if (agg == AggKind::kCount) {
    return Schema({"count(*)"});
  }
  if (column >= child->schema().num_columns()) {
    throw std::invalid_argument("LogicalAggregate: 欄位索引越界");
  }
  return Schema({"sum(" + child->schema().column_name(column) + ")"});
}

void AppendTree(const LogicalNode& node, int depth, std::string& out) {
  out.append(static_cast<std::size_t>(depth) * 2, ' ');
  switch (node.kind()) {
    case LogicalKind::kScan: {
      const auto& scan = static_cast<const LogicalScan&>(node);
      out += "Scan(" + scan.table_name() + ")\n";
      return;
    }
    case LogicalKind::kFilter: {
      const auto& filter = static_cast<const LogicalFilter&>(node);
      out += "Filter(" + filter.predicate()->ToString(filter.schema()) + ")\n";
      AppendTree(filter.child(), depth + 1, out);
      return;
    }
    case LogicalKind::kJoin: {
      const auto& join = static_cast<const LogicalJoin&>(node);
      out += "Join(" + join.left().schema().column_name(join.left_key()) + " = " +
             join.right().schema().column_name(join.right_key()) + ")\n";
      AppendTree(join.left(), depth + 1, out);
      AppendTree(join.right(), depth + 1, out);
      return;
    }
    case LogicalKind::kAggregate: {
      const auto& agg = static_cast<const LogicalAggregate&>(node);
      out += "Aggregate(" + agg.schema().column_name(0) + ")\n";
      AppendTree(agg.child(), depth + 1, out);
      return;
    }
  }
}

}

LogicalAggregate::LogicalAggregate(LogicalNodePtr child, AggKind agg, ColumnIndex column)
    : LogicalNode(LogicalKind::kAggregate, AggregateSchema(child, agg, column)),
      child_(std::move(child)),
      agg_(agg),
      column_(column) {
  if (child_ == nullptr) {
    throw std::invalid_argument("LogicalAggregate: child 不可為 null");
  }
}

std::string ToString(const LogicalNode& node) {
  std::string out;
  AppendTree(node, 0, out);
  return out;
}

}
