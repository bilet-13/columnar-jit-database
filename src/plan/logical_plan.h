#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "common/schema.h"
#include "common/types.h"
#include "plan/expression.h"

namespace minidb {

enum class LogicalKind : std::uint8_t { kScan, kFilter, kJoin, kAggregate };

class LogicalNode;
using LogicalNodePtr = std::shared_ptr<const LogicalNode>;

class LogicalNode {
 public:
  LogicalNode(LogicalKind kind, Schema schema) : kind_(kind), schema_(std::move(schema)) {}
  virtual ~LogicalNode() = default;

  LogicalNode(const LogicalNode&) = delete;
  LogicalNode& operator=(const LogicalNode&) = delete;

  LogicalKind kind() const { return kind_; }

  const Schema& schema() const { return schema_; }

 private:
  LogicalKind kind_;
  Schema schema_;
};

class LogicalScan final : public LogicalNode {
 public:
  LogicalScan(std::string table_name, Schema schema);

  const std::string& table_name() const { return table_name_; }

 private:
  std::string table_name_;
};

class LogicalFilter final : public LogicalNode {
 public:
  LogicalFilter(LogicalNodePtr child, ExprPtr predicate);

  const LogicalNode& child() const { return *child_; }
  const LogicalNodePtr& child_ptr() const { return child_; }
  const ExprPtr& predicate() const { return predicate_; }

 private:
  LogicalNodePtr child_;
  ExprPtr predicate_;
};

class LogicalJoin final : public LogicalNode {
 public:
  LogicalJoin(LogicalNodePtr left, LogicalNodePtr right, ColumnIndex left_key,
              ColumnIndex right_key);

  const LogicalNode& left() const { return *left_; }
  const LogicalNode& right() const { return *right_; }
  const LogicalNodePtr& left_ptr() const { return left_; }
  const LogicalNodePtr& right_ptr() const { return right_; }

  ColumnIndex left_key() const { return left_key_; }
  ColumnIndex right_key() const { return right_key_; }

 private:
  LogicalNodePtr left_;
  LogicalNodePtr right_;
  ColumnIndex left_key_;
  ColumnIndex right_key_;
};

enum class AggKind : std::uint8_t { kSum, kCount };

class LogicalAggregate final : public LogicalNode {
 public:
  LogicalAggregate(LogicalNodePtr child, AggKind agg, ColumnIndex column);

  const LogicalNode& child() const { return *child_; }
  const LogicalNodePtr& child_ptr() const { return child_; }
  AggKind agg() const { return agg_; }
  ColumnIndex column() const { return column_; }

 private:
  LogicalNodePtr child_;
  AggKind agg_;
  ColumnIndex column_;
};

std::string ToString(const LogicalNode& node);

}
