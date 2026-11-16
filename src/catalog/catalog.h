#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common/schema.h"
#include "storage/table.h"

namespace minidb {

class Catalog {
 public:
  Table& CreateTable(std::string name, const Schema& unqualified_schema);

  bool Contains(std::string_view name) const;

  Table& GetTable(std::string_view name);
  const Table& GetTable(std::string_view name) const;

  std::uint32_t num_tables() const { return static_cast<std::uint32_t>(tables_.size()); }

  std::vector<std::string> table_names() const;

 private:
  std::vector<std::unique_ptr<Table>> tables_;
  std::unordered_map<std::string, std::size_t> position_by_name_;
};

}
