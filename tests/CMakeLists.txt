# 單元測試（docs/design.md 第 11 節）
#
# 所有測試 target 都連上 minidb_sanitizers：
# 在 build/ 裡那是個空殼，在 build-asan/ 裡才會帶上插樁旗標。

add_executable(minidb_tests
  smoke_test.cpp
)

target_link_libraries(minidb_tests PRIVATE
  GTest::gtest_main
  minidb_sanitizers
)

# Phase 1 之後測試會 #include "storage/column.h" 這種路徑
target_include_directories(minidb_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_options(minidb_tests PRIVATE -Wall -Wextra)

# 讓 ctest 看得到每一個 TEST()，而不是只有一個執行檔
include(GoogleTest)
gtest_discover_tests(minidb_tests)
