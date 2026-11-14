#include "storage/table.h"

#include <stdexcept>

namespace minidb {

Table::Table(std::string name, const Schema& unqualified_schema)
    : name_(std::move(name)),
      schema_(unqualified_schema.WithPrefix(name_)),
      columns_(schema_.num_columns()) {}

Column& Table::column(ColumnIndex index) {
  if (index >= num_columns()) {
    throw std::out_of_range("Table::column: 欄位索引越界");
  }
  return columns_[index];
}

const Column& Table::column(ColumnIndex index) const {
  if (index >= num_columns()) {
    throw std::out_of_range("Table::column: 欄位索引越界");
  }
  return columns_[index];
}

void Table::AppendRow(std::span<const std::int64_t> values) {
  if (values.size() != num_columns()) {
    throw std::invalid_argument("Table::AppendRow: 值的數量與欄位數不符");
  }
  for (std::uint32_t i = 0; i < num_columns(); ++i) {
    columns_[i].Append(values[i]);
  }
  ++num_rows_;
}

void Table::Reserve(std::uint64_t num_rows) {
  for (Column& column : columns_) {
    column.Reserve(num_rows);
  }
}

}  // namespace minidb
