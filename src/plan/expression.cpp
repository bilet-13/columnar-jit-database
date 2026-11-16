#include "plan/expression.h"

#include <stdexcept>
#include <utility>

namespace minidb {

std::string_view CompareOpSymbol(CompareOp op) {
  switch (op) {
    case CompareOp::kEq:
      return "=";
    case CompareOp::kNe:
      return "!=";
    case CompareOp::kLt:
      return "<";
    case CompareOp::kLe:
      return "<=";
    case CompareOp::kGt:
      return ">";
    case CompareOp::kGe:
      return ">=";
  }
  return "?";
}

bool ApplyCompare(CompareOp op, std::int64_t lhs, std::int64_t rhs) {
  switch (op) {
    case CompareOp::kEq:
      return lhs == rhs;
    case CompareOp::kNe:
      return lhs != rhs;
    case CompareOp::kLt:
      return lhs < rhs;
    case CompareOp::kLe:
      return lhs <= rhs;
    case CompareOp::kGt:
      return lhs > rhs;
    case CompareOp::kGe:
      return lhs >= rhs;
  }
  return false;
}

std::int64_t ColumnRef::Evaluate(std::span<const std::int64_t> row) const {
  if (index_ >= row.size()) {
    throw std::out_of_range("ColumnRef::Evaluate: 欄位索引超出這一列的寬度");
  }
  return row[index_];
}

std::string ColumnRef::ToString(const Schema& schema) const {
  if (index_ < schema.num_columns()) {
    return schema.column_name(index_);
  }
  return "#" + std::to_string(index_);
}

std::int64_t Constant::Evaluate(std::span<const std::int64_t> ) const { return value_; }

std::string Constant::ToString(const Schema& ) const { return std::to_string(value_); }

Comparison::Comparison(CompareOp op, ExprPtr left, ExprPtr right)
    : Expression(ExprKind::kComparison), op_(op), left_(std::move(left)), right_(std::move(right)) {
  if (left_ == nullptr || right_ == nullptr) {
    throw std::invalid_argument("Comparison: 子節點不可為 null");
  }
}

std::int64_t Comparison::Evaluate(std::span<const std::int64_t> row) const {
  return ApplyCompare(op_, left_->Evaluate(row), right_->Evaluate(row)) ? 1 : 0;
}

std::string Comparison::ToString(const Schema& schema) const {
  return left_->ToString(schema) + " " + std::string(CompareOpSymbol(op_)) + " " +
         right_->ToString(schema);
}

And::And(std::vector<ExprPtr> children)
    : Expression(ExprKind::kAnd), children_(std::move(children)) {
  if (children_.empty()) {
    throw std::invalid_argument("And: 至少要有一個子節點");
  }
  for (const ExprPtr& child : children_) {
    if (child == nullptr) {
      throw std::invalid_argument("And: 子節點不可為 null");
    }
  }
}

std::int64_t And::Evaluate(std::span<const std::int64_t> row) const {
  for (const ExprPtr& child : children_) {
    if (child->Evaluate(row) == 0) {
      return 0;
    }
  }
  return 1;
}

std::string And::ToString(const Schema& schema) const {
  std::string text;
  for (std::size_t i = 0; i < children_.size(); ++i) {
    if (i > 0) {
      text += " AND ";
    }
    text += children_[i]->ToString(schema);
  }
  return text;
}

ExprPtr MakeColumn(ColumnIndex index) { return std::make_shared<const ColumnRef>(index); }

ExprPtr MakeConstant(std::int64_t value) { return std::make_shared<const Constant>(value); }

ExprPtr MakeComparison(CompareOp op, ExprPtr left, ExprPtr right) {
  return std::make_shared<const Comparison>(op, std::move(left), std::move(right));
}

ExprPtr MakeColumnCompare(ColumnIndex column, CompareOp op, std::int64_t constant) {
  return MakeComparison(op, MakeColumn(column), MakeConstant(constant));
}

ExprPtr MakeAnd(std::vector<ExprPtr> children) {
  if (children.size() == 1) {
    return children.front();
  }
  return std::make_shared<const And>(std::move(children));
}

namespace {

bool FlattenInto(const Expression& expr, std::vector<Predicate>& out) {
  switch (expr.kind()) {
    case ExprKind::kAnd: {
      const auto& node = static_cast<const And&>(expr);
      for (const ExprPtr& child : node.children()) {
        if (!FlattenInto(*child, out)) {
          return false;
        }
      }
      return true;
    }
    case ExprKind::kComparison: {
      const auto& node = static_cast<const Comparison&>(expr);
      if (node.left().kind() != ExprKind::kColumnRef ||
          node.right().kind() != ExprKind::kConstant) {
        return false;
      }
      out.push_back(Predicate{static_cast<const ColumnRef&>(node.left()).index(), node.op(),
                              static_cast<const Constant&>(node.right()).value()});
      return true;
    }
    case ExprKind::kColumnRef:
    case ExprKind::kConstant:
      return false;
  }
  return false;
}

}

bool TryFlatten(const Expression& expr, std::vector<Predicate>& out) {
  out.clear();
  if (!FlattenInto(expr, out)) {
    out.clear();
    return false;
  }
  return true;
}

}
