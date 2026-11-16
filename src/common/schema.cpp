#include "common/schema.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace minidb {
namespace {

std::string_view UnqualifiedName(std::string_view name) {
  const std::size_t dot = name.rfind('.');
  return dot == std::string_view::npos ? name : name.substr(dot + 1);
}

}

Schema::Schema(std::vector<std::string> column_names) : names_(std::move(column_names)) {
  std::unordered_set<std::string_view> seen;
  for (const std::string& name : names_) {
    if (name.empty()) {
      throw std::invalid_argument("Schema: 欄位名稱不可為空");
    }
    if (!seen.insert(name).second) {
      throw std::invalid_argument("Schema: 欄位名稱重複：" + name);
    }
  }
}

const std::string& Schema::column_name(ColumnIndex index) const {
  if (index >= num_columns()) {
    throw std::out_of_range("Schema::column_name: 索引越界");
  }
  return names_[index];
}

std::optional<ColumnIndex> Schema::TryGetColumnIndex(std::string_view name) const {
  for (std::uint32_t i = 0; i < num_columns(); ++i) {
    if (names_[i] == name) {
      return i;
    }
  }
  return std::nullopt;
}

ColumnIndex Schema::ResolveColumnIndex(std::string_view name) const {
  if (const std::optional<ColumnIndex> exact = TryGetColumnIndex(name); exact.has_value()) {
    return *exact;
  }

  const std::string_view wanted = UnqualifiedName(name);
  std::optional<ColumnIndex> found;
  for (std::uint32_t i = 0; i < num_columns(); ++i) {
    if (UnqualifiedName(names_[i]) != wanted) {
      continue;
    }
    if (found.has_value()) {
      throw std::invalid_argument("Schema::ResolveColumnIndex: 欄位名稱有歧義，請寫完整名稱：" +
                                  std::string(name));
    }
    found = i;
  }

  if (!found.has_value()) {
    throw std::invalid_argument("Schema::ResolveColumnIndex: 找不到欄位：" + std::string(name));
  }
  return *found;
}

Schema Schema::Concat(const Schema& left, const Schema& right) {
  std::vector<std::string> names = left.names_;
  names.insert(names.end(), right.names_.begin(), right.names_.end());
  return Schema(std::move(names));
}

Schema Schema::WithPrefix(std::string_view prefix) const {
  std::vector<std::string> names;
  names.reserve(names_.size());
  for (const std::string& name : names_) {
    names.push_back(std::string(prefix) + "." + name);
  }
  return Schema(std::move(names));
}

}
