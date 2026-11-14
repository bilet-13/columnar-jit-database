#include "common/schema.h"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace minidb {
namespace {

Schema MakeOrdersSchema() {
  return Schema({"orders.id", "orders.cust_id", "orders.amount"});
}

TEST(Schema, EmptySchema) {
  const Schema schema;
  EXPECT_EQ(schema.num_columns(), 0u);
  EXPECT_FALSE(schema.TryGetColumnIndex("anything").has_value());
}

TEST(Schema, ColumnNamesAndOrder) {
  const Schema schema = MakeOrdersSchema();
  ASSERT_EQ(schema.num_columns(), 3u);
  EXPECT_EQ(schema.column_name(0), "orders.id");
  EXPECT_EQ(schema.column_name(1), "orders.cust_id");
  EXPECT_EQ(schema.column_name(2), "orders.amount");
  EXPECT_EQ(schema.column_type(0), TypeId::kInt64);
}

TEST(Schema, ColumnNameOutOfRangeThrows) {
  const Schema schema = MakeOrdersSchema();
  EXPECT_THROW(schema.column_name(3), std::out_of_range);
}

TEST(Schema, DuplicateNamesRejected) {
  EXPECT_THROW(Schema({"a", "b", "a"}), std::invalid_argument);
}

TEST(Schema, EmptyNameRejected) {
  EXPECT_THROW(Schema({"a", ""}), std::invalid_argument);
}

TEST(Schema, TryGetColumnIndexNeedsExactName) {
  const Schema schema = MakeOrdersSchema();
  EXPECT_EQ(schema.TryGetColumnIndex("orders.amount"), 2u);
  // 寬鬆查找是 ResolveColumnIndex 的工作，TryGetColumnIndex 只認完整名稱
  EXPECT_FALSE(schema.TryGetColumnIndex("amount").has_value());
}

TEST(Schema, ResolveAcceptsQualifiedAndUnqualified) {
  const Schema schema = MakeOrdersSchema();
  EXPECT_EQ(schema.ResolveColumnIndex("orders.amount"), 2u);
  EXPECT_EQ(schema.ResolveColumnIndex("amount"), 2u);
  EXPECT_EQ(schema.ResolveColumnIndex("id"), 0u);
}

TEST(Schema, ResolveRejectsAmbiguousName) {
  // join 之後的典型情況：兩張表都有 id
  const Schema joined = Schema::Concat(Schema({"orders.id"}), Schema({"customers.id"}));
  ASSERT_EQ(joined.num_columns(), 2u);
  // 寫完整名稱沒問題
  EXPECT_EQ(joined.ResolveColumnIndex("orders.id"), 0u);
  EXPECT_EQ(joined.ResolveColumnIndex("customers.id"), 1u);
  // 只寫 id 就有歧義
  EXPECT_THROW(joined.ResolveColumnIndex("id"), std::invalid_argument);
}

TEST(Schema, ResolveUnknownNameThrows) {
  const Schema schema = MakeOrdersSchema();
  EXPECT_THROW(schema.ResolveColumnIndex("nope"), std::invalid_argument);
}

TEST(Schema, ConcatKeepsLeftThenRight) {
  const Schema joined =
      Schema::Concat(MakeOrdersSchema(), Schema({"customers.id", "customers.city_id"}));
  ASSERT_EQ(joined.num_columns(), 5u);
  EXPECT_EQ(joined.column_name(2), "orders.amount");
  EXPECT_EQ(joined.column_name(3), "customers.id");
  EXPECT_EQ(joined.column_name(4), "customers.city_id");
}

TEST(Schema, ConcatRejectsCollidingNames) {
  // 沒有加表名前綴就 join，會撞名
  EXPECT_THROW(Schema::Concat(Schema({"id"}), Schema({"id"})), std::invalid_argument);
}

TEST(Schema, WithPrefixQualifiesEveryColumn) {
  const Schema raw({"id", "cust_id", "amount"});
  const Schema qualified = raw.WithPrefix("orders");
  EXPECT_EQ(qualified, MakeOrdersSchema());
}

TEST(Schema, Equality) {
  EXPECT_EQ(MakeOrdersSchema(), MakeOrdersSchema());
  EXPECT_NE(MakeOrdersSchema(), Schema({"orders.id"}));
}

}  // namespace
}  // namespace minidb
