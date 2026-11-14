#include "plan/expression.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace minidb {
namespace {

// orders.id, orders.cust_id, orders.amount
Schema OrdersSchema() {
  return Schema({"orders.id", "orders.cust_id", "orders.amount"});
}

constexpr ColumnIndex kAmount = 2;

TEST(ApplyCompare, AllOperators) {
  EXPECT_TRUE(ApplyCompare(CompareOp::kEq, 5, 5));
  EXPECT_FALSE(ApplyCompare(CompareOp::kEq, 5, 6));
  EXPECT_TRUE(ApplyCompare(CompareOp::kNe, 5, 6));
  EXPECT_TRUE(ApplyCompare(CompareOp::kLt, 5, 6));
  EXPECT_FALSE(ApplyCompare(CompareOp::kLt, 5, 5));
  EXPECT_TRUE(ApplyCompare(CompareOp::kLe, 5, 5));
  EXPECT_TRUE(ApplyCompare(CompareOp::kGt, 6, 5));
  EXPECT_TRUE(ApplyCompare(CompareOp::kGe, 5, 5));
  EXPECT_FALSE(ApplyCompare(CompareOp::kGe, 4, 5));
}

TEST(Expression, ColumnRefReadsTheRow) {
  const std::vector<std::int64_t> row = {1, 7, 250};
  EXPECT_EQ(MakeColumn(0)->Evaluate(row), 1);
  EXPECT_EQ(MakeColumn(kAmount)->Evaluate(row), 250);
}

TEST(Expression, ColumnRefOutOfRangeThrows) {
  const std::vector<std::int64_t> row = {1, 7};
  EXPECT_THROW(MakeColumn(5)->Evaluate(row), std::out_of_range);
}

TEST(Expression, ConstantIgnoresTheRow) {
  const std::vector<std::int64_t> row = {1, 7, 250};
  EXPECT_EQ(MakeConstant(42)->Evaluate(row), 42);
}

// 布林以 0 / 1 表示，這樣節點可以互相組合
TEST(Expression, ComparisonReturnsZeroOrOne) {
  const ExprPtr pred = MakeColumnCompare(kAmount, CompareOp::kGt, 100);
  EXPECT_EQ(pred->Evaluate(std::vector<std::int64_t>{1, 7, 250}), 1);
  EXPECT_EQ(pred->Evaluate(std::vector<std::int64_t>{2, 3, 80}), 0);
  EXPECT_EQ(pred->Evaluate(std::vector<std::int64_t>{3, 3, 100}), 0);
}

TEST(Expression, AndNeedsEveryChild) {
  const ExprPtr pred = MakeAnd({MakeColumnCompare(kAmount, CompareOp::kGt, 100),
                                MakeColumnCompare(kAmount, CompareOp::kLt, 900)});
  EXPECT_EQ(pred->Evaluate(std::vector<std::int64_t>{1, 7, 250}), 1);
  EXPECT_EQ(pred->Evaluate(std::vector<std::int64_t>{1, 7, 80}), 0);
  EXPECT_EQ(pred->Evaluate(std::vector<std::int64_t>{1, 7, 950}), 0);
}

TEST(Expression, AndRejectsEmptyChildren) {
  EXPECT_THROW(And({}), std::invalid_argument);
}

// 單一條件不必多包一層 And
TEST(Expression, MakeAndWithOneChildReturnsThatChild) {
  const ExprPtr only = MakeColumnCompare(kAmount, CompareOp::kGt, 100);
  EXPECT_EQ(MakeAnd({only}), only);
}

TEST(Expression, ToStringUsesColumnNames) {
  const ExprPtr pred = MakeAnd({MakeColumnCompare(kAmount, CompareOp::kGt, 100),
                                MakeColumnCompare(1, CompareOp::kEq, 7)});
  EXPECT_EQ(pred->ToString(OrdersSchema()), "orders.amount > 100 AND orders.cust_id = 7");
}

// ---------------------------------------------------------------------------
// 攤平：AST 給 interpreter 走，Predicate 清單給 vectorized / JIT 用
// ---------------------------------------------------------------------------

TEST(TryFlatten, SingleComparison) {
  std::vector<Predicate> out;
  ASSERT_TRUE(TryFlatten(*MakeColumnCompare(kAmount, CompareOp::kGt, 100), out));
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0], (Predicate{kAmount, CompareOp::kGt, 100}));
}

TEST(TryFlatten, ConjunctionKeepsOrder) {
  const ExprPtr pred = MakeAnd({MakeColumnCompare(kAmount, CompareOp::kGt, 100),
                                MakeColumnCompare(kAmount, CompareOp::kLt, 900),
                                MakeColumnCompare(1, CompareOp::kLt, 10000)});
  std::vector<Predicate> out;
  ASSERT_TRUE(TryFlatten(*pred, out));
  ASSERT_EQ(out.size(), 3u);
  EXPECT_EQ(out[0].constant, 100);
  EXPECT_EQ(out[1].constant, 900);
  EXPECT_EQ(out[2].column, 1u);
}

TEST(TryFlatten, NestedAndIsFlattened) {
  const ExprPtr inner = MakeAnd({MakeColumnCompare(0, CompareOp::kGe, 1),
                                 MakeColumnCompare(1, CompareOp::kGe, 2)});
  const ExprPtr outer = MakeAnd({inner, MakeColumnCompare(2, CompareOp::kGe, 3)});
  std::vector<Predicate> out;
  ASSERT_TRUE(TryFlatten(*outer, out));
  EXPECT_EQ(out.size(), 3u);
}

// MVP 文法只有 column <op> 常數
TEST(TryFlatten, RejectsColumnVersusColumn) {
  const ExprPtr pred = MakeComparison(CompareOp::kEq, MakeColumn(0), MakeColumn(1));
  std::vector<Predicate> out;
  EXPECT_FALSE(TryFlatten(*pred, out));
  EXPECT_TRUE(out.empty());
}

TEST(TryFlatten, RejectsConstantOnTheLeft) {
  const ExprPtr pred = MakeComparison(CompareOp::kLt, MakeConstant(100), MakeColumn(kAmount));
  std::vector<Predicate> out;
  EXPECT_FALSE(TryFlatten(*pred, out));
}

TEST(TryFlatten, RejectsBareColumn) {
  std::vector<Predicate> out;
  EXPECT_FALSE(TryFlatten(*MakeColumn(0), out));
}

TEST(Predicate, MatchesAgreesWithTheAst) {
  const Predicate predicate{kAmount, CompareOp::kGt, 100};
  const ExprPtr ast = MakeColumnCompare(kAmount, CompareOp::kGt, 100);
  for (std::int64_t value : {std::int64_t{0}, std::int64_t{100}, std::int64_t{101},
                             std::int64_t{999}}) {
    const std::vector<std::int64_t> row = {0, 0, value};
    EXPECT_EQ(predicate.Matches(value), ast->Evaluate(row) == 1) << "value = " << value;
  }
}

}  // namespace
}  // namespace minidb
