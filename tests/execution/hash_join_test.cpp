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

HashJoinSpec OrdersJoinCustomers(std::uint32_t batch_size = kBatchSize,
                                 bool batched_probe = false) {
  return HashJoinSpec{1,        0, 2,
                      2,   batch_size,      batched_probe};
}

const std::vector<bool>& ProbeModes() {
  static const std::vector<bool> modes = {false, true};
  return modes;
}

TEST(HashJoin, OneToOne) {
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 10}, {2, 20}, {3, 10}});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}, {20, 8}});

  for (const bool batched : ProbeModes()) {
    SeqScan probe(orders, kBatchSize);
    SeqScan build(customers, kBatchSize);
    HashJoin join(probe, build, OrdersJoinCustomers(kBatchSize, batched));

    const std::vector<Row> expected = {{1, 10, 10, 7}, {2, 20, 20, 8}, {3, 10, 10, 7}};
    EXPECT_EQ(Drain(join), expected) << "batched_probe = " << batched;
    EXPECT_EQ(join.build_rows(), 2u);
    EXPECT_EQ(join.rows_out(), 3u);
  }
}

TEST(HashJoin, BatchedProbeAgreesOnALargerInput) {
  std::vector<Row> order_rows;
  for (std::int64_t i = 0; i < kBatchSize * 2 + 377; ++i) {
    order_rows.push_back({i, i % 500});
  }
  std::vector<Row> customer_rows;
  for (std::int64_t i = 0; i < 500; ++i) {
    customer_rows.push_back({i, i * 3});
  }
  const Table orders = MakeTable("orders", {"id", "cust_id"}, order_rows);
  const Table customers = MakeTable("customers", {"id", "city_id"}, customer_rows);

  std::vector<Row> row_at_a_time;
  {
    SeqScan probe(orders, kBatchSize);
    SeqScan build(customers, kBatchSize);
    HashJoin join(probe, build, OrdersJoinCustomers(kBatchSize, false));
    row_at_a_time = Drain(join);
  }
  ASSERT_EQ(row_at_a_time.size(), order_rows.size());

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers(kBatchSize, true));
  EXPECT_EQ(Drain(join), row_at_a_time);
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

TEST(HashJoin, OneProbeRowMatchesManyBuildRows) {
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 10}});
  const Table customers =
      MakeTable("customers", {"id", "city_id"}, {{10, 7}, {10, 8}, {10, 9}, {20, 1}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers());

  std::vector<Row> rows = Drain(join);
  ASSERT_EQ(rows.size(), 3u);
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

  EXPECT_EQ(Drain(join).size(), 4u);
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

TEST(HashJoin, OutputIsSlicedIntoBatches) {
  std::vector<Row> order_rows;
  for (std::int64_t i = 0; i < 10; ++i) {
    order_rows.push_back({i, 10});
  }
  const Table orders = MakeTable("orders", {"id", "cust_id"}, order_rows);
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
  HashJoin join(probe, build, OrdersJoinCustomers(3));

  std::uint32_t batches = 0;
  std::uint32_t rows = 0;
  while (Batch* batch = join.Next()) {
    ++batches;
    rows += batch->ActiveRows();
    EXPECT_LE(batch->ActiveRows(), 3u);
  }
  EXPECT_EQ(rows, 10u);
  EXPECT_EQ(batches, 4u);
}

TEST(HashJoin, RespectsASelectionVectorOnTheProbeSide) {
  const Table orders =
      MakeTable("orders", {"id", "cust_id"}, {{1, 10}, {2, 10}, {3, 10}, {4, 10}});
  const Table customers = MakeTable("customers", {"id", "city_id"}, {{10, 7}});

  for (const bool batched : ProbeModes()) {
    SeqScan scan(orders, kBatchSize);
    InterpreterFilter filter(scan, MakeColumnCompare(0, CompareOp::kGt, 2));
    SeqScan build(customers, kBatchSize);
    HashJoin join(filter, build, OrdersJoinCustomers(kBatchSize, batched));

    const std::vector<Row> expected = {{3, 10, 10, 7}, {4, 10, 10, 7}};
    EXPECT_EQ(Drain(join), expected) << "batched_probe = " << batched;
  }
}

TEST(HashJoin, BuildSideSurvivesAcrossManyBatches) {
  std::vector<Row> customer_rows;
  for (std::int64_t i = 0; i < 3000; ++i) {
    customer_rows.push_back({i, i * 2});
  }
  const Table customers = MakeTable("customers", {"id", "city_id"}, customer_rows);
  const Table orders = MakeTable("orders", {"id", "cust_id"}, {{1, 0}, {2, 2999}});

  SeqScan probe(orders, kBatchSize);
  SeqScan build(customers, kBatchSize);
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

  EXPECT_THROW(HashJoin(probe, build, OrdersJoinCustomers(0)),
               std::invalid_argument);

  HashJoinSpec bad_key = OrdersJoinCustomers();
  bad_key.probe_key = 9;
  EXPECT_THROW(HashJoin(probe, build, bad_key), std::invalid_argument);

  HashJoinSpec too_wide = OrdersJoinCustomers();
  too_wide.build_num_cols = kMaxCols;
  EXPECT_THROW(HashJoin(probe, build, too_wide), std::invalid_argument);
}

}
}
