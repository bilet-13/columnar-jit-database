# 效能測試（docs/design.md 第 11 節第 5 點）
#
# 刻意「不」連 minidb_sanitizers：benchmark 數字必須來自沒有插樁的程式碼。
# 另外 cmake/Sanitizers.cmake 會在任何 sanitizer 開啟時直接關掉這個 target，
# 所以 build-asan/ 裡根本不會產生 benchmark 執行檔。
#
# 測資產生器與 Q1–Q5 的定義放在 tests/util/，這裡直接編同一份原始碼。
# 兩邊必須用完全一樣的測資與查詢，才能說「測試驗過的就是 benchmark 量的」。

add_executable(minidb_bench
  smoke_bench.cpp
  bench_data.cpp
  query_bench.cpp
  "${CMAKE_SOURCE_DIR}/tests/util/data_gen.cpp"
  "${CMAKE_SOURCE_DIR}/tests/util/queries.cpp"
)

target_link_libraries(minidb_bench PRIVATE
  benchmark::benchmark
  minidb_core
)

target_include_directories(minidb_bench PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/tests"
)

target_compile_options(minidb_bench PRIVATE -Wall -Wextra)
