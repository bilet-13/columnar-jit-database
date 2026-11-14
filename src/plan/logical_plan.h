#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "common/schema.h"
#include "common/types.h"
#include "plan/expression.h"

namespace minidb {

// ---------------------------------------------------------------------------
// 邏輯計畫
//
// 「邏輯」的意思是只描述要做什麼：掃哪張表、過濾什麼條件、跟誰 join、
// 要加總哪一欄。它不說用哪種 join 演算法、誰當 build 端、join 順序怎麼排
// ——那些是 Phase 3 的 optimizer 決定的「物理計畫」。
//
// 每個節點都帶自己的輸出 schema，所以組計畫時就能檢查欄位名稱，不必等執行。
// ---------------------------------------------------------------------------
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

  // 這個節點輸出的欄位。
  const Schema& schema() const { return schema_; }

 private:
  LogicalKind kind_;
  Schema schema_;
};

// 掃一整張表。輸出 schema 就是表的 schema（已帶表名前綴）。
class LogicalScan final : public LogicalNode {
 public:
  LogicalScan(std::string table_name, Schema schema);

  const std::string& table_name() const { return table_name_; }

 private:
  std::string table_name_;
};

// 過濾。輸出 schema 與輸入相同。
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

// Inner equi-join：left.key = right.key，單一欄位對單一欄位。
// 輸出 schema 是左邊的欄位接右邊的欄位。
class LogicalJoin final : public LogicalNode {
 public:
  LogicalJoin(LogicalNodePtr left, LogicalNodePtr right, ColumnIndex left_key,
              ColumnIndex right_key);

  const LogicalNode& left() const { return *left_; }
  const LogicalNode& right() const { return *right_; }
  const LogicalNodePtr& left_ptr() const { return left_; }
  const LogicalNodePtr& right_ptr() const { return right_; }

  // left_key 是左邊 schema 的索引，right_key 是右邊 schema 的索引。
  ColumnIndex left_key() const { return left_key_; }
  ColumnIndex right_key() const { return right_key_; }

 private:
  LogicalNodePtr left_;
  LogicalNodePtr right_;
  ColumnIndex left_key_;
  ColumnIndex right_key_;
};

// 無 GROUP BY 的聚合，輸出剛好一列一欄。
enum class AggKind : std::uint8_t { kSum, kCount };

class LogicalAggregate final : public LogicalNode {
 public:
  // kCount 忽略 column。
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

// 把計畫樹印成多行字串，給測試與除錯看。
std::string ToString(const LogicalNode& node);

}  // namespace minidb
