#include "execution/executor.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "plan/plan_builder.h"

namespace minidb {
namespace {

using Row = std::vector<std::int64_t>;

// 小一號的 3 表 chain，手算得出答案：
//   cities    (id, pop)
//   customers (id, city_id)
//   orders    (id, cust_id, amount)
Catalog MakeCatalog() {
  Catalog catalog;

  Table& cities = catalog.CreateTable("cities", Schema({"id", "pop"}));
  for (const Row& row : {Row{1, 950}, Row{2, 100}}) {
    cities.AppendRow(row);
  }

  Table& customers = catalog.CreateTable("customers", Schema({"id", "city_id"}));
  for (const Row& row : {Row{10, 1}, Row{20, 2}, Row{30, 1}}) {
    customers.AppendRow(row);
  }

  Table& orders = catalog.CreateTable("orders", Schema({"id", "cust_id", "amount"}));
  for (const Row& row : {Row{1, 10, 600}, Row{2, 20, 700}, Row{3, 30, 400},
                         Row{4, 10, 800}, Row{5, 99, 900}}) {
    orders.AppendRow(row);
  }

  return catalog;
}

std::int64_t RunScalar(const Catalog& catalog, const LogicalNodePtr& plan) {
  Executor executor = Executor::Build(*plan, catalog, ExecMode::kInterpreter);
  return executor.RunScalar();
}

// Q1：整張表的 SUM
TEST(Executor, SumWholeTable) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog).Scan("orders").Sum("amount").Build();
  EXPECT_EQ(RunScalar(catalog, plan), 600 + 700 + 400 + 800 + 900);
}

TEST(Executor, CountWholeTable) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog).Scan("orders").Count().Build();
  EXPECT_EQ(RunScalar(catalog, plan), 5);
}

// Q2 / Q3：filter 之後 SUM
TEST(Executor, FilterThenSum) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan =
      QueryBuilder(catalog).Scan("orders").Filter({Gt("amount", 500)}).Sum("amount").Build();
  EXPECT_EQ(RunScalar(catalog, plan), 600 + 700 + 800 + 900);
}

// Q4：多條件 AND
TEST(Executor, ConjunctionThenSum) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Filter({Gt("amount", 500), Lt("amount", 900), Lt("cust_id", 30)})
                                  .Sum("amount")
                                  .Build();
  EXPECT_EQ(RunScalar(catalog, plan), 600 + 700 + 800);
}

TEST(Executor, TwoTableJoin) {
  const Catalog catalog = MakeCatalog();
  // cust_id = 99 的那筆沒有對應的 customer，會被 join 丟掉
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Join("customers", "orders.cust_id", "customers.id")
                                  .Sum("orders.amount")
                                  .Build();
  EXPECT_EQ(RunScalar(catalog, plan), 600 + 700 + 400 + 800);
}

// Q5：3 表 chain join + 兩個 filter
TEST(Executor, ThreeTableChainJoin) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Filter({Gt("amount", 500)})
                                  .Join("customers", "orders.cust_id", "customers.id")
                                  .Join("cities", "customers.city_id", "cities.id")
                                  .Filter({Gt("cities.pop", 900)})
                                  .Sum("orders.amount")
                                  .Build();
  // amount > 500 留下 600(c10)、700(c20)、800(c10)、900(c99)
  // c99 沒有 customer；c20 在 city 2（pop 100）被篩掉
  // 剩下 600 + 800，兩筆都在 city 1（pop 950）
  EXPECT_EQ(RunScalar(catalog, plan), 1400);
}

TEST(Executor, CountAfterJoin) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Join("customers", "orders.cust_id", "customers.id")
                                  .Count()
                                  .Build();
  EXPECT_EQ(RunScalar(catalog, plan), 4);
}

// 非聚合計畫：回傳所有列
TEST(Executor, RunRowsReturnsEveryColumn) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan =
      QueryBuilder(catalog).Scan("cities").Filter({Gt("pop", 500)}).Build();

  Executor executor = Executor::Build(*plan, catalog, ExecMode::kInterpreter);
  EXPECT_EQ(executor.num_output_columns(), 2u);
  const std::vector<Row> expected = {{1, 950}};
  EXPECT_EQ(executor.RunRows(), expected);
}

TEST(Executor, RunScalarRejectsNonScalarPlans) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog).Scan("orders").Build();
  Executor executor = Executor::Build(*plan, catalog, ExecMode::kInterpreter);
  EXPECT_THROW(executor.RunScalar(), std::logic_error);
}

// JIT 要等 Phase 4
TEST(Executor, JitModeIsNotImplementedYet) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan =
      QueryBuilder(catalog).Scan("orders").Filter({Gt("amount", 1)}).Sum("amount").Build();
  EXPECT_THROW(Executor::Build(*plan, catalog, ExecMode::kJit), std::logic_error);
}

// 批次執行器與 interpreter 必須給出完全一樣的答案
TEST(Executor, VectorizedAgreesWithInterpreter) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Filter({Gt("amount", 500)})
                                  .Join("customers", "orders.cust_id", "customers.id")
                                  .Join("cities", "customers.city_id", "cities.id")
                                  .Filter({Gt("cities.pop", 900)})
                                  .Sum("orders.amount")
                                  .Build();

  Executor interpreted = Executor::Build(*plan, catalog, ExecMode::kInterpreter);
  Executor vectorized = Executor::Build(*plan, catalog, ExecMode::kVectorized);
  EXPECT_EQ(vectorized.RunScalar(), interpreted.RunScalar());
}

// 沒有 filter 的計畫在任何 mode 下都組得起來（只有 filter 有三種實作）
TEST(Executor, PlansWithoutFiltersBuildInAnyMode) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog).Scan("orders").Sum("amount").Build();
  for (ExecMode mode : {ExecMode::kInterpreter, ExecMode::kVectorized, ExecMode::kJit}) {
    Executor executor = Executor::Build(*plan, catalog, mode);
    EXPECT_EQ(executor.RunScalar(), 3400) << "mode = " << ToString(mode);
  }
}

}  // namespace
}  // namespace minidb
