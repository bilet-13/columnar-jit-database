#include "execution/executor.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "plan/plan_builder.h"

namespace minidb {
namespace {

using Row = std::vector<std::int64_t>;

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

TEST(Executor, FilterThenSum) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan =
      QueryBuilder(catalog).Scan("orders").Filter({Gt("amount", 500)}).Sum("amount").Build();
  EXPECT_EQ(RunScalar(catalog, plan), 600 + 700 + 800 + 900);
}

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
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Join("customers", "orders.cust_id", "customers.id")
                                  .Sum("orders.amount")
                                  .Build();
  EXPECT_EQ(RunScalar(catalog, plan), 600 + 700 + 400 + 800);
}

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

TEST(Executor, JitModeIsNotImplementedYet) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan =
      QueryBuilder(catalog).Scan("orders").Filter({Gt("amount", 1)}).Sum("amount").Build();
  EXPECT_THROW(Executor::Build(*plan, catalog, ExecMode::kJit), std::logic_error);
}

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

TEST(Executor, PlansWithoutFiltersBuildInAnyMode) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog).Scan("orders").Sum("amount").Build();
  for (ExecMode mode : {ExecMode::kInterpreter, ExecMode::kVectorized, ExecMode::kJit}) {
    Executor executor = Executor::Build(*plan, catalog, mode);
    EXPECT_EQ(executor.RunScalar(), 3400) << "mode = " << ToString(mode);
  }
}

}
}
