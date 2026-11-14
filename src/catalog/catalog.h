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

// ---------------------------------------------------------------------------
// Catalog：「這個資料庫裡有哪些表」的名冊。
//
// 真實資料庫的 catalog 還會存權限、索引、統計資料等 metadata，MVP 只存表。
// 統計資料放在 src/stats/（Phase 3）。
//
// 表用 unique_ptr 持有，所以之後再新增表不會讓先前拿到的 Table& 失效
// ——組查詢計畫時會抓著 Table 的參照。
// ---------------------------------------------------------------------------
class Catalog {
 public:
  // 傳入未限定的欄位名稱，Table 會自己加上表名前綴。
  // 表名重複會丟 std::invalid_argument。
  Table& CreateTable(std::string name, const Schema& unqualified_schema);

  bool Contains(std::string_view name) const;

  // 找不到會丟 std::invalid_argument。
  Table& GetTable(std::string_view name);
  const Table& GetTable(std::string_view name) const;

  std::uint32_t num_tables() const { return static_cast<std::uint32_t>(tables_.size()); }

  // 依建立順序回傳表名。
  std::vector<std::string> table_names() const;

 private:
  std::vector<std::unique_ptr<Table>> tables_;
  std::unordered_map<std::string, std::size_t> position_by_name_;
};

}  // namespace minidb
