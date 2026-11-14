#pragma once

#include <cstdint>
#include <vector>

#include "catalog/catalog.h"

namespace minidb::testutil {

// ---------------------------------------------------------------------------
// docs/design.md 8.1 的 3 表 chain schema
//
//   cities    (id, pop)
//   customers (id, city_id, age)
//   orders    (id, cust_id, amount)
//
//   orders.cust_id    = customers.id
//   customers.city_id = cities.id
//
// 刻意用鏈狀而不是星狀：orders 與 cities 之間沒有直接的 join 條件，所以
// Phase 3 的 optimizer 必須遵守 join graph，DP 的搜尋空間才有意義。
// ---------------------------------------------------------------------------
struct DatasetSizes {
  std::uint64_t num_orders = 1'000'000;
  std::uint64_t num_customers = 20'000;
  std::uint64_t num_cities = 200;
};

// 測試用的小尺寸；benchmark 用上面的預設值。
DatasetSizes SmallSizes();

// ---------------------------------------------------------------------------
// 值域刻意縮小（docs/design.md 8.2）：amount < 1000，所以 100 萬列的 SUM
// 絕對不會溢位，三種執行器的結果可以直接用 == 比對，不必容許誤差。
// ---------------------------------------------------------------------------
constexpr std::int64_t kMaxAmount = 1000;
constexpr std::int64_t kMaxCityPop = 1000;
constexpr std::int64_t kMinAge = 18;
constexpr std::int64_t kMaxAge = 80;

// Catalog 加上一份原始資料的副本。參考實作直接讀這些 vector，完全不經過
// operator，所以它不可能跟引擎犯同一個錯。
struct Dataset {
  Catalog catalog;
  DatasetSizes sizes;

  std::vector<std::int64_t> order_id;
  std::vector<std::int64_t> order_cust_id;
  std::vector<std::int64_t> order_amount;

  std::vector<std::int64_t> customer_id;
  std::vector<std::int64_t> customer_city_id;
  std::vector<std::int64_t> customer_age;

  std::vector<std::int64_t> city_id;
  std::vector<std::int64_t> city_pop;
};

// 固定 seed，所以每次跑出來的資料完全相同 —— benchmark 的數字才能互相比較。
Dataset MakeDataset(const DatasetSizes& sizes, std::uint64_t seed = 42);

}  // namespace minidb::testutil
