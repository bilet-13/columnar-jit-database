#include "util/reference.h"

#include <stdexcept>
#include <unordered_map>

namespace minidb::testutil {
namespace {

// 刻意用 map 查，不假設 customers.id 剛好等於列號。
std::unordered_map<std::int64_t, std::size_t> IndexById(
    const std::vector<std::int64_t>& ids) {
  std::unordered_map<std::int64_t, std::size_t> index;
  index.reserve(ids.size());
  for (std::size_t i = 0; i < ids.size(); ++i) {
    index.emplace(ids[i], i);
  }
  return index;
}

struct Q5Result {
  std::int64_t sum = 0;
  std::uint64_t rows = 0;
};

Q5Result Q5(const Dataset& dataset) {
  const auto customer_index = IndexById(dataset.customer_id);
  const auto city_index = IndexById(dataset.city_id);

  Q5Result result;
  for (std::size_t i = 0; i < dataset.order_amount.size(); ++i) {
    if (dataset.order_amount[i] <= kQ5AmountThreshold) {
      continue;
    }
    const auto customer = customer_index.find(dataset.order_cust_id[i]);
    if (customer == customer_index.end()) {
      continue;
    }
    const auto city = city_index.find(dataset.customer_city_id[customer->second]);
    if (city == city_index.end()) {
      continue;
    }
    if (dataset.city_pop[city->second] <= kQ5PopThreshold) {
      continue;
    }
    result.sum += dataset.order_amount[i];
    ++result.rows;
  }
  return result;
}

bool Passes(QueryId id, const Dataset& dataset, std::size_t i) {
  const std::int64_t amount = dataset.order_amount[i];
  switch (id) {
    case QueryId::kQ1:
      return true;
    case QueryId::kQ2:
      return amount > kQ2Threshold;
    case QueryId::kQ3:
      return amount > kQ3Threshold;
    case QueryId::kQ4:
      return amount > kQ4Low && amount < kQ4High &&
             dataset.order_cust_id[i] <
                 static_cast<std::int64_t>(dataset.sizes.num_customers / 2);
    case QueryId::kQ5:
      throw std::logic_error("Q5 不是單表條件");
  }
  return false;
}

}  // namespace

std::int64_t ReferenceResult(QueryId id, const Dataset& dataset) {
  if (id == QueryId::kQ5) {
    return Q5(dataset).sum;
  }
  std::int64_t sum = 0;
  for (std::size_t i = 0; i < dataset.order_amount.size(); ++i) {
    if (Passes(id, dataset, i)) {
      sum += dataset.order_amount[i];
    }
  }
  return sum;
}

std::uint64_t ReferenceMatchingRows(QueryId id, const Dataset& dataset) {
  if (id == QueryId::kQ5) {
    return Q5(dataset).rows;
  }
  std::uint64_t rows = 0;
  for (std::size_t i = 0; i < dataset.order_amount.size(); ++i) {
    if (Passes(id, dataset, i)) {
      ++rows;
    }
  }
  return rows;
}

}  // namespace minidb::testutil
