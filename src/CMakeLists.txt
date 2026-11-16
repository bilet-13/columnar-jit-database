# 引擎本體。
#
# 一個靜態函式庫，tests/ 與 bench/ 都連它。
# include 路徑設成 PUBLIC，所以測試可以寫 #include "common/schema.h"。
#
# minidb_sanitizers 設成 PUBLIC：在 build-asan/ 裡，連這個函式庫的人
# 也必須帶上同一組 sanitizer 旗標（整支程式必須一致）。

add_library(minidb_core STATIC
  catalog/catalog.cpp
  common/schema.cpp
  execution/aggregate.cpp
  execution/executor.cpp
  execution/hash_join.cpp
  execution/hash_table.cpp
  execution/interpreter/filter.cpp
  execution/seq_scan.cpp
  execution/vectorized/filter.cpp
  execution/vectorized/mask.cpp
  execution/vectorized/mask_neon.cpp
  execution/vectorized/mask_scalar.cpp
  plan/expression.cpp
  plan/logical_plan.cpp
  plan/plan_builder.cpp
  storage/column.cpp
  storage/table.cpp
)

target_include_directories(minidb_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(minidb_core PUBLIC minidb_sanitizers)

target_compile_options(minidb_core PRIVATE -Wall -Wextra)
