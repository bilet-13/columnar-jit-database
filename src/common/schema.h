#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/types.h"

namespace minidb {

// ---------------------------------------------------------------------------
// Schema：一張表（或一個計畫節點的輸出）有哪些欄位。
//
// MVP 所有欄位都是 INT64，所以 Schema 其實只需要記住「欄位名稱的順序」。
//
// 命名慣例：scan 會把欄位名稱加上表名前綴，例如 orders 的 amount 欄在計畫
// 裡叫 "orders.amount"。這樣 3 表 join 把欄位接起來之後不會撞名，而使用者
// 在組計畫時仍然可以只寫 "amount"（見 ResolveColumnIndex）。
// ---------------------------------------------------------------------------
class Schema {
 public:
  Schema() = default;

  // 欄位名稱不可重複，重複會丟 std::invalid_argument。
  explicit Schema(std::vector<std::string> column_names);

  std::uint32_t num_columns() const { return static_cast<std::uint32_t>(names_.size()); }

  // index 越界會丟 std::out_of_range。
  const std::string& column_name(ColumnIndex index) const;

  // MVP 只有一種型別，留著是為了之後擴充時介面不用改。
  TypeId column_type(ColumnIndex /*index*/) const { return TypeId::kInt64; }

  const std::vector<std::string>& column_names() const { return names_; }

  // 完全相同的名稱才算命中。找不到回 nullopt。
  std::optional<ColumnIndex> TryGetColumnIndex(std::string_view name) const;

  // 寬鬆查找，給組計畫的 C++ API 用：
  //   1. 先找完全相同的名稱（"orders.amount"）
  //   2. 再找「點號後面相同」的名稱（"amount" 命中 "orders.amount"）
  // 第 2 步如果命中多個（例如 join 後同時有 orders.id 與 customers.id），
  // 會丟 std::invalid_argument 要求寫完整名稱。找不到也丟同樣的例外。
  ColumnIndex ResolveColumnIndex(std::string_view name) const;

  // join 的輸出 schema：左邊所有欄位接右邊所有欄位。
  static Schema Concat(const Schema& left, const Schema& right);

  // 每個欄位名稱前面加上 "prefix."。scan 用它產生限定名稱。
  Schema WithPrefix(std::string_view prefix) const;

  bool operator==(const Schema& other) const = default;

 private:
  std::vector<std::string> names_;
};

}  // namespace minidb
