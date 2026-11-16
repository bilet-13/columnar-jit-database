#include "plan/plan_builder.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace minidb {
namespace {

Catalog MakeCatalog() {
  Catalog catalog;
  catalog.CreateTable("cities", Schema({"id", "pop"}));
  catalog.CreateTable("customers", Schema({"id", "city_id", "age"}));
  catalog.CreateTable("orders", Schema({"id", "cust_id", "amount"}));
  return catalog;
}

TEST(QueryBuilder, ScanOnly) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog).Scan("orders").Build();
  EXPECT_EQ(ToString(*plan), "Scan(orders)\n");
}

TEST(QueryBuilder, UnqualifiedColumnNameIsResolved) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan =
      QueryBuilder(catalog).Scan("orders").Filter({Gt("amount", 100)}).Build();
  EXPECT_EQ(ToString(*plan),
            "Filter(orders.amount > 100)\n"
            "  Scan(orders)\n");
}

TEST(QueryBuilder, MultiplePredicatesBecomeOneAnd) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Filter({Gt("amount", 100), Lt("amount", 900), Lt("cust_id", 10)})
                                  .Build();
  EXPECT_EQ(ToString(*plan),
            "Filter(orders.amount > 100 AND orders.amount < 900 AND orders.cust_id < 10)\n"
            "  Scan(orders)\n");
}

TEST(QueryBuilder, Q1SumOverWholeTable) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog).Scan("orders").Sum("amount").Build();
  EXPECT_EQ(ToString(*plan),
            "Aggregate(sum(orders.amount))\n"
            "  Scan(orders)\n");
  EXPECT_EQ(plan->schema().column_name(0), "sum(orders.amount)");
}

TEST(QueryBuilder, Count) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan =
      QueryBuilder(catalog).Scan("orders").Filter({Gt("amount", 900)}).Count().Build();
  EXPECT_EQ(plan->schema().column_name(0), "count(*)");
}

TEST(QueryBuilder, Q5ThreeTableChainJoin) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Filter({Gt("amount", 500)})
                                  .Join("customers", "orders.cust_id", "customers.id")
                                  .Join("cities", "customers.city_id", "cities.id")
                                  .Filter({Gt("cities.pop", 900)})
                                  .Sum("orders.amount")
                                  .Build();

  EXPECT_EQ(ToString(*plan),
            "Aggregate(sum(orders.amount))\n"
            "  Filter(cities.pop > 900)\n"
            "    Join(customers.city_id = cities.id)\n"
            "      Join(orders.cust_id = customers.id)\n"
            "        Filter(orders.amount > 500)\n"
            "          Scan(orders)\n"
            "        Scan(customers)\n"
            "      Scan(cities)\n");
}

TEST(QueryBuilder, JoinedSchemaKeepsBothTables) {
  const Catalog catalog = MakeCatalog();
  const LogicalNodePtr plan = QueryBuilder(catalog)
                                  .Scan("orders")
                                  .Join("customers", "orders.cust_id", "customers.id")
                                  .Build();
  ASSERT_EQ(plan->schema().num_columns(), 6u);
  EXPECT_EQ(plan->schema().column_name(0), "orders.id");
  EXPECT_EQ(plan->schema().column_name(3), "customers.id");
  EXPECT_THROW(plan->schema().ResolveColumnIndex("id"), std::invalid_argument);
  EXPECT_EQ(plan->schema().ResolveColumnIndex("orders.id"), 0u);
}

TEST(QueryBuilder, UnknownTableThrows) {
  const Catalog catalog = MakeCatalog();
  EXPECT_THROW(QueryBuilder(catalog).Scan("nope"), std::invalid_argument);
}

TEST(QueryBuilder, UnknownColumnThrows) {
  const Catalog catalog = MakeCatalog();
  EXPECT_THROW(QueryBuilder(catalog).Scan("orders").Filter({Gt("nope", 1)}),
               std::invalid_argument);
}

TEST(QueryBuilder, FilterBeforeScanThrows) {
  const Catalog catalog = MakeCatalog();
  EXPECT_THROW(QueryBuilder(catalog).Filter({Gt("amount", 1)}), std::invalid_argument);
}

TEST(QueryBuilder, EmptyFilterThrows) {
  const Catalog catalog = MakeCatalog();
  EXPECT_THROW(QueryBuilder(catalog).Scan("orders").Filter({}), std::invalid_argument);
}

TEST(QueryBuilder, SecondScanThrows) {
  const Catalog catalog = MakeCatalog();
  EXPECT_THROW(QueryBuilder(catalog).Scan("orders").Scan("customers"), std::invalid_argument);
}

TEST(QueryBuilder, NothingAfterAggregate) {
  const Catalog catalog = MakeCatalog();
  EXPECT_THROW(QueryBuilder(catalog).Scan("orders").Sum("amount").Filter({Gt("amount", 1)}),
               std::invalid_argument);
  EXPECT_THROW(QueryBuilder(catalog).Scan("orders").Count().Count(), std::invalid_argument);
}

TEST(QueryBuilder, BuildWithoutScanThrows) {
  const Catalog catalog = MakeCatalog();
  EXPECT_THROW(QueryBuilder(catalog).Build(), std::invalid_argument);
}

}
}
