#include "catalog/catalog.h"

#include <stdexcept>
#include <utility>

namespace minidb {

Table& Catalog::CreateTable(std::string name, const Schema& unqualified_schema) {
  if (name.empty()) {
    throw std::invalid_argument("Catalog::CreateTable: 表名不可為空");
  }
  if (position_by_name_.contains(name)) {
    throw std::invalid_argument("Catalog::CreateTable: 表名重複：" + name);
  }

  position_by_name_.emplace(name, tables_.size());
  tables_.push_back(std::make_unique<Table>(std::move(name), unqualified_schema));
  return *tables_.back();
}

bool Catalog::Contains(std::string_view name) const {
  return position_by_name_.contains(std::string(name));
}

Table& Catalog::GetTable(std::string_view name) {
  const auto it = position_by_name_.find(std::string(name));
  if (it == position_by_name_.end()) {
    throw std::invalid_argument("Catalog::GetTable: 找不到表：" + std::string(name));
  }
  return *tables_[it->second];
}

const Table& Catalog::GetTable(std::string_view name) const {
  const auto it = position_by_name_.find(std::string(name));
  if (it == position_by_name_.end()) {
    throw std::invalid_argument("Catalog::GetTable: 找不到表：" + std::string(name));
  }
  return *tables_[it->second];
}

std::vector<std::string> Catalog::table_names() const {
  std::vector<std::string> names;
  names.reserve(tables_.size());
  for (const std::unique_ptr<Table>& table : tables_) {
    names.push_back(table->name());
  }
  return names;
}

}
