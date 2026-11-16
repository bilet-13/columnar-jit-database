#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/types.h"

namespace minidb {

class Schema {
 public:
  Schema() = default;

  explicit Schema(std::vector<std::string> column_names);

  std::uint32_t num_columns() const { return static_cast<std::uint32_t>(names_.size()); }

  const std::string& column_name(ColumnIndex index) const;

  TypeId column_type(ColumnIndex ) const { return TypeId::kInt64; }

  const std::vector<std::string>& column_names() const { return names_; }

  std::optional<ColumnIndex> TryGetColumnIndex(std::string_view name) const;

  ColumnIndex ResolveColumnIndex(std::string_view name) const;

  static Schema Concat(const Schema& left, const Schema& right);

  Schema WithPrefix(std::string_view prefix) const;

  bool operator==(const Schema& other) const = default;

 private:
  std::vector<std::string> names_;
};

}
