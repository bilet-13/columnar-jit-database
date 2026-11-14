# 效能測試（docs/design.md 第 11 節第 5 點）
#
# 刻意「不」連 minidb_sanitizers：benchmark 數字必須來自沒有插樁的程式碼。
# 另外 cmake/Sanitizers.cmake 會在任何 sanitizer 開啟時直接關掉這個 target，
# 所以 build-asan/ 裡根本不會產生 benchmark 執行檔。

add_executable(minidb_bench
  smoke_bench.cpp
)

target_link_libraries(minidb_bench PRIVATE
  benchmark::benchmark
)

target_include_directories(minidb_bench PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_options(minidb_bench PRIVATE -Wall -Wextra)
