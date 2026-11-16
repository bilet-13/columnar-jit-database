#pragma once

#include <cstdint>
#include <vector>

#include "catalog/catalog.h"

namespace minidb::testutil {

struct DatasetSizes {
  std::uint64_t num_orders = 1'000'000;
  std::uint64_t num_customers = 20'000;
  std::uint64_t num_cities = 200;
};

DatasetSizes SmallSizes();

constexpr std::int64_t kMaxAmount = 1000;
constexpr std::int64_t kMaxCityPop = 1000;
constexpr std::int64_t kMinAge = 18;
constexpr std::int64_t kMaxAge = 80;

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

Dataset MakeDataset(const DatasetSizes& sizes, std::uint64_t seed = 42);

}
