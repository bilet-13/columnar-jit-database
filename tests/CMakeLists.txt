# 單元測試（docs/design.md 第 11 節）
#
# 所有測試 target 都連上 minidb_sanitizers：
# 在 build/ 裡那是個空殼，在 build-asan/ 裡才會帶上插樁旗標。

add_executable(minidb_tests
  smoke_test.cpp
  catalog/catalog_test.cpp
  common/schema_test.cpp
  execution/batch_test.cpp
  plan/expression_test.cpp
  plan/logical_plan_test.cpp
  plan/plan_builder_test.cpp
  storage/column_test.cpp
)

# minidb_core 把 src/ 設成 PUBLIC include 路徑，所以測試可以直接
# #include "common/schema.h"。
target_link_libraries(minidb_tests PRIVATE
  GTest::gtest_main
  minidb_core
  minidb_sanitizers
)

target_compile_options(minidb_tests PRIVATE -Wall -Wextra)

# 讓 ctest 看得到每一個 TEST()，而不是只有一個執行檔
include(GoogleTest)
gtest_discover_tests(minidb_tests)
