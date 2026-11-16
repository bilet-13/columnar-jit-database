#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "common/schema.h"
#include "common/types.h"

namespace minidb {

enum class CompareOp : std::uint8_t { kEq, kNe, kLt, kLe, kGt, kGe };

std::string_view CompareOpSymbol(CompareOp op);
bool ApplyCompare(CompareOp op, std::int64_t lhs, std::int64_t rhs);

enum class ExprKind : std::uint8_t { kColumnRef, kConstant, kComparison, kAnd };

class Expression;
using ExprPtr = std::shared_ptr<const Expression>;

class Expression {
 public:
  explicit Expression(ExprKind kind) : kind_(kind) {}
  virtual ~Expression() = default;

  Expression(const Expression&) = delete;
  Expression& operator=(const Expression&) = delete;

  ExprKind kind() const { return kind_; }

  virtual std::int64_t Evaluate(std::span<const std::int64_t> row) const = 0;

  virtual std::string ToString(const Schema& schema) const = 0;

 private:
  ExprKind kind_;
};

class ColumnRef final : public Expression {
 public:
  explicit ColumnRef(ColumnIndex index) : Expression(ExprKind::kColumnRef), index_(index) {}

  ColumnIndex index() const { return index_; }

  std::int64_t Evaluate(std::span<const std::int64_t> row) const override;
  std::string ToString(const Schema& schema) const override;

 private:
  ColumnIndex index_;
};

class Constant final : public Expression {
 public:
  explicit Constant(std::int64_t value) : Expression(ExprKind::kConstant), value_(value) {}

  std::int64_t value() const { return value_; }

  std::int64_t Evaluate(std::span<const std::int64_t> row) const override;
  std::string ToString(const Schema& schema) const override;

 private:
  std::int64_t value_;
};

class Comparison final : public Expression {
 public:
  Comparison(CompareOp op, ExprPtr left, ExprPtr right);

  CompareOp op() const { return op_; }
  const Expression& left() const { return *left_; }
  const Expression& right() const { return *right_; }

  std::int64_t Evaluate(std::span<const std::int64_t> row) const override;
  std::string ToString(const Schema& schema) const override;

 private:
  CompareOp op_;
  ExprPtr left_;
  ExprPtr right_;
};

class And final : public Expression {
 public:
  explicit And(std::vector<ExprPtr> children);

  const std::vector<ExprPtr>& children() const { return children_; }

  std::int64_t Evaluate(std::span<const std::int64_t> row) const override;
  std::string ToString(const Schema& schema) const override;

 private:
  std::vector<ExprPtr> children_;
};

ExprPtr MakeColumn(ColumnIndex index);
ExprPtr MakeConstant(std::int64_t value);
ExprPtr MakeComparison(CompareOp op, ExprPtr left, ExprPtr right);
ExprPtr MakeColumnCompare(ColumnIndex column, CompareOp op, std::int64_t constant);
ExprPtr MakeAnd(std::vector<ExprPtr> children);

struct Predicate {
  ColumnIndex column;
  CompareOp op;
  std::int64_t constant;

  bool Matches(std::int64_t value) const { return ApplyCompare(op, value, constant); }
  bool operator==(const Predicate&) const = default;
};

bool TryFlatten(const Expression& expr, std::vector<Predicate>& out);

}
