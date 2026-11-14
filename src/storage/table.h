#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "common/schema.h"
#include "common/types.h"
#include "storage/column.h"

namespace minidb {

// ---------------------------------------------------------------------------
// Table：幾個 Column 加一個 Schema。
//
// 建構時傳入「未限定」的欄位名稱（id、cust_id、amount），Table 自己加上表名
// 前綴，所以 schema() 回來的是 orders.id、orders.cust_id、orders.amount。
// join 把兩張表的欄位接起來時才不會撞名（docs/design.md 7.5）。
// ---------------------------------------------------------------------------
class Table {
 public:
  Table(std::string name, const Schema& unqualified_schema);

  const std::string& name() const { return name_; }

  // 已加上表名前綴的 schema。
  const Schema& schema() const { return schema_; }

  std::uint32_t num_columns() const { return schema_.num_columns(); }
  std::uint64_t num_rows() const { return num_rows_; }

  Column& column(ColumnIndex index);
  const Column& column(ColumnIndex index) const;

  // 一次塞一整列。長度必須等於欄位數，否則丟 std::invalid_argument。
  void AppendRow(std::span<const std::int64_t> values);

  // 預留空間的提示，轉給每個 Column。
  void Reserve(std::uint64_t num_rows);

 private:
  std::string name_;
  Schema schema_;
  std::vector<Column> columns_;
  std::uint64_t num_rows_ = 0;
};

}  // namespace minidb
