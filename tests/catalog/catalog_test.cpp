#include "catalog/catalog.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace minidb {
namespace {

TEST(Catalog, EmptyCatalog) {
  const Catalog catalog;
  EXPECT_EQ(catalog.num_tables(), 0u);
  EXPECT_FALSE(catalog.Contains("orders"));
  EXPECT_THROW(catalog.GetTable("orders"), std::invalid_argument);
}

TEST(Catalog, CreateAndGet) {
  Catalog catalog;
  Table& orders = catalog.CreateTable("orders", Schema({"id", "amount"}));

  EXPECT_EQ(catalog.num_tables(), 1u);
  EXPECT_TRUE(catalog.Contains("orders"));
  EXPECT_EQ(&catalog.GetTable("orders"), &orders);
  // Table 自己把欄位名稱加上前綴
  EXPECT_EQ(orders.schema().column_name(1), "orders.amount");
}

TEST(Catalog, DuplicateTableNameRejected) {
  Catalog catalog;
  catalog.CreateTable("orders", Schema({"id"}));
  EXPECT_THROW(catalog.CreateTable("orders", Schema({"id"})), std::invalid_argument);
  EXPECT_EQ(catalog.num_tables(), 1u);
}

TEST(Catalog, EmptyTableNameRejected) {
  Catalog catalog;
  EXPECT_THROW(catalog.CreateTable("", Schema({"id"})), std::invalid_argument);
}

// 組查詢計畫時會抓著 Table&，所以之後新增表不能讓舊的參照失效。
TEST(Catalog, ReferencesStayValidAfterAddingMoreTables) {
  Catalog catalog;
  Table& orders = catalog.CreateTable("orders", Schema({"id", "amount"}));
  const std::int64_t row[] = {1, 250};
  orders.AppendRow(row);

  for (int i = 0; i < 16; ++i) {
    catalog.CreateTable("filler" + std::to_string(i), Schema({"x"}));
  }

  EXPECT_EQ(&catalog.GetTable("orders"), &orders);
  EXPECT_EQ(orders.num_rows(), 1u);
  EXPECT_EQ(orders.column(1).At(0), 250);
}

TEST(Catalog, TableNamesFollowCreationOrder) {
  Catalog catalog;
  catalog.CreateTable("orders", Schema({"id"}));
  catalog.CreateTable("customers", Schema({"id"}));
  catalog.CreateTable("cities", Schema({"id"}));

  const std::vector<std::string> expected = {"orders", "customers", "cities"};
  EXPECT_EQ(catalog.table_names(), expected);
}

TEST(Catalog, ConstLookup) {
  Catalog catalog;
  catalog.CreateTable("cities", Schema({"id", "pop"}));
  const Catalog& const_catalog = catalog;
  EXPECT_EQ(const_catalog.GetTable("cities").num_columns(), 2u);
  EXPECT_THROW(const_catalog.GetTable("nope"), std::invalid_argument);
}

}  // namespace
}  // namespace minidb
