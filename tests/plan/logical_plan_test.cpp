#include "plan/logical_plan.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

namespace minidb {
namespace {

LogicalNodePtr OrdersScan() {
  return std::make_shared<const LogicalScan>(
      "orders", Schema({"orders.id", "orders.cust_id", "orders.amount"}));
}

LogicalNodePtr CustomersScan() {
  return std::make_shared<const LogicalScan>(
      "customers", Schema({"customers.id", "customers.city_id"}));
}

TEST(LogicalPlan, ScanOutputsTheTableSchema) {
  const LogicalNodePtr scan = OrdersScan();
  EXPECT_EQ(scan->kind(), LogicalKind::kScan);
  EXPECT_EQ(scan->schema().num_columns(), 3u);
  EXPECT_EQ(static_cast<const LogicalScan&>(*scan).table_name(), "orders");
}

TEST(LogicalPlan, FilterKeepsTheInputSchema) {
  const LogicalNodePtr scan = OrdersScan();
  const auto filter = std::make_shared<const LogicalFilter>(
      scan, MakeColumnCompare(2, CompareOp::kGt, 100));
  EXPECT_EQ(filter->schema(), scan->schema());
}

TEST(LogicalPlan, FilterRejectsNull) {
  EXPECT_THROW(LogicalFilter(nullptr, MakeColumnCompare(0, CompareOp::kGt, 1)),
               std::invalid_argument);
  EXPECT_THROW(LogicalFilter(OrdersScan(), nullptr), std::invalid_argument);
}

TEST(LogicalPlan, JoinConcatenatesSchemas) {
  const auto join = std::make_shared<const LogicalJoin>(OrdersScan(), CustomersScan(),
                                                        1, 0);
  ASSERT_EQ(join->schema().num_columns(), 5u);
  EXPECT_EQ(join->schema().column_name(0), "orders.id");
  EXPECT_EQ(join->schema().column_name(3), "customers.id");
  EXPECT_EQ(join->schema().column_name(4), "customers.city_id");
}

TEST(LogicalPlan, JoinRejectsOutOfRangeKeys) {
  EXPECT_THROW(LogicalJoin(OrdersScan(), CustomersScan(), 9, 0), std::invalid_argument);
  EXPECT_THROW(LogicalJoin(OrdersScan(), CustomersScan(), 1, 9), std::invalid_argument);
}

TEST(LogicalPlan, AggregateOutputsOneNamedColumn) {
  const auto sum = std::make_shared<const LogicalAggregate>(OrdersScan(), AggKind::kSum, 2);
  ASSERT_EQ(sum->schema().num_columns(), 1u);
  EXPECT_EQ(sum->schema().column_name(0), "sum(orders.amount)");

  const auto count = std::make_shared<const LogicalAggregate>(OrdersScan(), AggKind::kCount, 0);
  EXPECT_EQ(count->schema().column_name(0), "count(*)");
}

TEST(LogicalPlan, AggregateRejectsOutOfRangeColumn) {
  EXPECT_THROW(LogicalAggregate(OrdersScan(), AggKind::kSum, 9), std::invalid_argument);
}

TEST(LogicalPlan, ToStringPrintsTheTree) {
  const auto cities =
      std::make_shared<const LogicalScan>("cities", Schema({"cities.id", "cities.pop"}));
  const auto filtered_orders = std::make_shared<const LogicalFilter>(
      OrdersScan(), MakeColumnCompare(2, CompareOp::kGt, 500));
  const auto join1 = std::make_shared<const LogicalJoin>(filtered_orders, CustomersScan(), 1, 0);
  const auto join2 = std::make_shared<const LogicalJoin>(join1, cities, 4, 0);
  const auto sum = std::make_shared<const LogicalAggregate>(join2, AggKind::kSum, 2);

  const std::string expected =
      "Aggregate(sum(orders.amount))\n"
      "  Join(customers.city_id = cities.id)\n"
      "    Join(orders.cust_id = customers.id)\n"
      "      Filter(orders.amount > 500)\n"
      "        Scan(orders)\n"
      "      Scan(customers)\n"
      "    Scan(cities)\n";
  EXPECT_EQ(ToString(*sum), expected);
}

}
}
