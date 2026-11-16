#include "util/data_gen.h"

#include <random>

namespace minidb::testutil {

DatasetSizes SmallSizes() {
  return DatasetSizes{5'000, 500, 200};
}

Dataset MakeDataset(const DatasetSizes& sizes, std::uint64_t seed) {
  Dataset dataset;
  dataset.sizes = sizes;

  std::mt19937_64 rng(seed);
  const auto pick = [&rng](std::int64_t lo, std::int64_t hi) {
    return std::uniform_int_distribution<std::int64_t>(lo, hi - 1)(rng);
  };

  Table& cities = dataset.catalog.CreateTable("cities", Schema({"id", "pop"}));
  cities.Reserve(sizes.num_cities);
  for (std::uint64_t i = 0; i < sizes.num_cities; ++i) {
    const std::int64_t id = static_cast<std::int64_t>(i);
    const std::int64_t pop = pick(0, kMaxCityPop);
    const std::int64_t row[] = {id, pop};
    cities.AppendRow(row);
    dataset.city_id.push_back(id);
    dataset.city_pop.push_back(pop);
  }

  Table& customers = dataset.catalog.CreateTable("customers", Schema({"id", "city_id", "age"}));
  customers.Reserve(sizes.num_customers);
  for (std::uint64_t i = 0; i < sizes.num_customers; ++i) {
    const std::int64_t id = static_cast<std::int64_t>(i);
    const std::int64_t city_id = pick(0, static_cast<std::int64_t>(sizes.num_cities));
    const std::int64_t age = pick(kMinAge, kMaxAge);
    const std::int64_t row[] = {id, city_id, age};
    customers.AppendRow(row);
    dataset.customer_id.push_back(id);
    dataset.customer_city_id.push_back(city_id);
    dataset.customer_age.push_back(age);
  }

  Table& orders = dataset.catalog.CreateTable("orders", Schema({"id", "cust_id", "amount"}));
  orders.Reserve(sizes.num_orders);
  for (std::uint64_t i = 0; i < sizes.num_orders; ++i) {
    const std::int64_t id = static_cast<std::int64_t>(i);
    const std::int64_t cust_id = pick(0, static_cast<std::int64_t>(sizes.num_customers));
    const std::int64_t amount = pick(0, kMaxAmount);
    const std::int64_t row[] = {id, cust_id, amount};
    orders.AppendRow(row);
    dataset.order_id.push_back(id);
    dataset.order_cust_id.push_back(cust_id);
    dataset.order_amount.push_back(amount);
  }

  return dataset;
}

}
