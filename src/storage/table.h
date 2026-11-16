#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "common/schema.h"
#include "common/types.h"
#include "storage/column.h"

namespace minidb {

class Table {
 public:
  Table(std::string name, const Schema& unqualified_schema);

  const std::string& name() const { return name_; }

  const Schema& schema() const { return schema_; }

  std::uint32_t num_columns() const { return schema_.num_columns(); }
  std::uint64_t num_rows() const { return num_rows_; }

  Column& column(ColumnIndex index);
  const Column& column(ColumnIndex index) const;

  void AppendRow(std::span<const std::int64_t> values);

  void Reserve(std::uint64_t num_rows);

 private:
  std::string name_;
  Schema schema_;
  std::vector<Column> columns_;
  std::uint64_t num_rows_ = 0;
};

}
