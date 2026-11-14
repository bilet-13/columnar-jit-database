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

// ---------------------------------------------------------------------------
// 比較運算子。MVP 的 WHERE 只有這六個，而且只能是 column <op> 常數。
// ---------------------------------------------------------------------------
enum class CompareOp : std::uint8_t { kEq, kNe, kLt, kLe, kGt, kGe };

std::string_view CompareOpSymbol(CompareOp op);
bool ApplyCompare(CompareOp op, std::int64_t lhs, std::int64_t rhs);

// ---------------------------------------------------------------------------
// 表達式 AST
//
// 這棵樹存在的理由是「當 interpreter 的基準」：interpreter 對每一列都要重新
// 走一次這棵樹，每個節點一次虛擬呼叫。那個解譯開銷就是 Phase 2 的 SIMD 與
// Phase 4 的 codegen 要消掉的東西，所以基準必須誠實地慢。
//
// 回傳型別統一用 int64：布林以 0 / 1 表示。這樣節點可以互相組合，不必為
// 「回傳布林的節點」和「回傳數值的節點」各寫一套。
// ---------------------------------------------------------------------------
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

  // row 是「這一列的所有欄位值」，索引就是 schema 的欄位索引。
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

// 方便組樹的工廠函式
ExprPtr MakeColumn(ColumnIndex index);
ExprPtr MakeConstant(std::int64_t value);
ExprPtr MakeComparison(CompareOp op, ExprPtr left, ExprPtr right);
ExprPtr MakeColumnCompare(ColumnIndex column, CompareOp op, std::int64_t constant);
ExprPtr MakeAnd(std::vector<ExprPtr> children);

// ---------------------------------------------------------------------------
// 正規化形式：column <op> 常數
//
// Vectorized 與 JIT 不走 AST，它們需要的是「結構」——哪個欄位、哪個運算子、
// 哪個常數——才能選 kernel 或產生 LLVM IR。所以同一個條件有兩種表示：
// AST 給 interpreter 走，Predicate 清單給 vectorized / JIT 用。
// 真實引擎也是這樣分工的。
// ---------------------------------------------------------------------------
struct Predicate {
  ColumnIndex column;
  CompareOp op;
  std::int64_t constant;

  bool Matches(std::int64_t value) const { return ApplyCompare(op, value, constant); }
  bool operator==(const Predicate&) const = default;
};

// 把 AST 攤平成 AND 串起來的 Predicate 清單。
// 不符合 MVP 文法（例如 column vs column、OR、常數在左邊以外的形狀）就回
// false，out 的內容未定義。
bool TryFlatten(const Expression& expr, std::vector<Predicate>& out);

}  // namespace minidb
