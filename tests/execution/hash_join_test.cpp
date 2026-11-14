#include "execution/hash_join.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "execution/interpreter/filter.h"
#include "execution/seq_scan.h"
#include "plan/expression.h"

namespace minidb {
namespace {

using Row = std::vector<std::int64_t>;

Table MakeTable(std::string name, std::vector<std::string> columns,
                const std::vector<Row>& rows) {
  Table table(std::move(name), Schema(std::move(columns)));
  for (const Row& row : rows) {
    table.AppendRow(row);
  }
  return table;
}

std::vector<Row> Drain(Operator& op) {
  std::vector<Row> rows;
  while (Batch* batch = op.Next()) {
    EXPECT_GT(batch->ActiveRows(), 0u) << "operator 不該回傳空 batch";
    EXPECT_EQ(batch->sel, nullptr) << "join 的輸出已經 materialize，不帶 selection vector";
    for (std::uint32_t i = 0; i < batch->ActiveRows(); ++i) {
      Row row;
      for (std::uint32_t c = 0; c < batch->num_cols; ++c) {
        row.push_back(batch->Value(c, i));
      }
      rows.push_back(std::move(row));
    }
  }
  return rows;
}

// orders(id, cust_id)  ⋈  customers(id, city_id)
HashJoinSpec OrdersJoinCustomers(std::uint32_t batch_size = kBatchSize) {
  return HashJoinSpec{/*probe_key=*/1, /*build_key=*/0, /*probe_num_cols=*/2,
                      /*build_num_cols=*/2, batch_size};
}

TEST(HashJoin, OneToOne) {
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 10}, {2, 20}, {3, 10}});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}, {20, 8}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers());

  // 輸出欄位 = 左邊兩欄接右邊兩欄
  const std::vector<Row> expected = {{1, 10, 10, 7}, {2, 20, 20, 8}, {3, 10, 10, 7}};
  EXPECT_EQ(Drain(join), expected);
  EXPECT_EQ(join.build_rows(), 2u);
  EXPECT_EQ(join.rows_out(), 3u);
}

TEST(HashJoin, RowsWithoutAMatchAreDropped) {
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 10}, {2, 99}, {3, 20}});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}, {20, 8}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers());

  const std::vector<Row> expected = {{1, 10, 10, 7}, {3, 20, 20, 8}};
  EXPECT_EQ(Drain(join), expected);
}

TEST(HashJoin, NoMatchesAtAll) {
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 99}});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers());

  EXPECT_EQ(join.Next(), nullptr);
  EXPECT_EQ(join.rows_out(), 0u);
}

// build 端重複 key：一列 probe 配對到多列
TEST(HashJoin, OneProbeRowMatchesManyBuildRows) {
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 10}});
  const Table customers =
      MakeTable("customers", {"id", "city_id"}, {{10, 7}, {10, 8}, {10, 9}, {20, 1}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers());

  std::vector<Row> rows = Drain(join);
  ASSERT_EQ(rows.size(), 3u);
  // 配對順序取決於探測順序，所以排序後再比
  std::sort(rows.begin(), rows.end());
  const std::vector<Row> expected = {{1, 10, 10, 7}, {1, 10, 10, 8}, {1, 10, 10, 9}};
  EXPECT_EQ(rows, expected);
}

TEST(HashJoin, ManyToMany) {
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 10}, {2, 10}});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}, {10, 8}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers());

  EXPECT_EQ(Drain(join).size(), 4u);  // 2 x 2
}

TEST(HashJoin, EmptySideProducesNothing) {
  const Table empty_orders = MakeTable("orders", {"id", "cust_id"}, {});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}});
  {
    SeqScan probe(empty_orders, kBatchSize);
    SeqScan build(customers, kBatchSize);
    HashJoin join(probe, build, OrdersJoinCustomers());
    EXPECT_EQ(join.Next(), nullptr);
  }

  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 10}});
  const Table empty_customers = MakeTable("customers", {"id", "city_id"}, {});
  {
    SeqScan probe(orders, kBatchSize);
    SeqScan build(empty_customers, kBatchSize);
    HashJoin join(probe, build, OrdersJoinCustomers());
    EXPECT_EQ(join.Next(), nullptr);
    EXPECT_EQ(join.build_rows(), 0u);
  }
}

// 一批 probe 可能產生比 batch_size 更多的配對，要分幾次交出去
TEST(HashJoin, OutputIsSlicedIntoBatches) {
  std::vector<Row> order_rows;
  for (std::int64_t i = 0; i < 10; ++i) {
    order_rows.push_back({i, 10});
  }
  const Table orders = MakeTable("orders", {"id", "cust_id"}, order_rows);
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers(/*batch_size=*/3));

  std::uint32_t batches = 0;
  std::uint32_t rows = 0;
  while (Batch* batch = join.Next()) {
    ++batches;
    rows += batch->ActiveRows();
    EXPECT_LE(batch->ActiveRows(), 3u);
  }
  EXPECT_EQ(rows, 10u);
  EXPECT_EQ(batches, 4u);  // 3 + 3 + 3 + 1
}

// probe 端下面接一個 filter，join 必須只看通過的那幾列
TEST(HashJoin, RespectsASelectionVectorOnTheProbeSide) {
  const Table orders =
      MakeTable("orders", {"id", "cust_id"}, {{1, 10}, {2, 10}, {3, 10}, {4, 10}});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}});

  SeqScan scan(orders, kBatchSize);
  // 只留 id > 2
  InterpreterFilter filter(scan, MakeColumnCompare(0, CompareOp::kGt, 2));
  SeqScan build(customers, kBatchSize);
  HashJoin join(filter, build, OrdersJoinCustomers());

  const std::vector<Row> expected = {{3, 10, 10, 7}, {4, 10, 10, 7}};
  EXPECT_EQ(Drain(join), expected);
}

// build 端的值一定要複製出來；下層的 batch 指標會失效
TEST(HashJoin, BuildSideSurvivesAcrossManyBatches) {
  std::vector<Row> customer_rows;
  for (std::int64_t i = 0; i < 3000; ++i) {
    customer_rows.push_back({i, i * 2});
  }
  const Table customers = MakeTable("customers", {"id", "city_id"}, customer_rows);
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 0}, {2, 2999}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);  // 3000 列 = 3 個 batch
  HashJoin join(probe, build, OrdersJoinCustomers());

  const std::vector<Row> expected = {{1, 0, 0, 0}, {2, 2999, 2999, 5998}};
  EXPECT_EQ(Drain(join), expected);
  EXPECT_EQ(join.build_rows(), 3000u);
}

TEST(HashJoin, RejectsBadSpec) {
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 10}});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}});
  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);

  EXPECT_THROW(HashJoin(probe, build, OrdersJoinCustomers(/*batch_size=*/0)),
               std::invalid_argument);

  HashJoinSpec bad_key = OrdersJoinCustomers();
  bad_key.probe_key = 9;
  EXPECT_THROW(HashJoin(probe, build, bad_key), std::invalid_argument);

  HashJoinSpec too_wide = OrdersJoinCustomers();
  too_wide.build_num_cols = kMaxCols;
  EXPECT_THROW(HashJoin(probe, build, too_wide), std::invalid_argument);
}

}  // namespace
}  // namespace minidb
