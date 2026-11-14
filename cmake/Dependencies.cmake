# 第三方依賴（docs/design.md 第 4 節）
#
# 用 CMake 內建的 FetchContent 自動下載，不需要手動 brew install。
# 第一次 cmake 設定時會連網 git clone，之後會快取在 build/_deps/ 裡。
# 版本寫死，確保換機器或過幾個月重建時行為一致。

include(FetchContent)

set(MINIDB_GOOGLETEST_TAG "v1.15.2" CACHE STRING "GoogleTest version")
set(MINIDB_BENCHMARK_TAG  "v1.9.0"  CACHE STRING "Google Benchmark version")

# ---------------------------------------------------------------------------
# GoogleTest
# ---------------------------------------------------------------------------
if(MINIDB_BUILD_TESTS)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        ${MINIDB_GOOGLETEST_TAG}
    GIT_SHALLOW    TRUE)

  # 不要讓 GoogleTest 自己的設定覆蓋我們的
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK   ON  CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)
  message(STATUS "GoogleTest : ${MINIDB_GOOGLETEST_TAG}")
endif()

# ---------------------------------------------------------------------------
# Google Benchmark
# ---------------------------------------------------------------------------
if(MINIDB_BUILD_BENCH)
  FetchContent_Declare(benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        ${MINIDB_BENCHMARK_TAG}
    GIT_SHALLOW    TRUE)

  set(BENCHMARK_ENABLE_TESTING     OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL     OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_WERROR      OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(benchmark)

  # ------------------------------------------------------------------------
  # 關掉第三方程式碼的警告升級。
  #
  # Google Benchmark 用 -pedantic-errors 編它自己的原始碼（任何非標準用法
  # 都當成錯誤）。很新的 clang 會對它 header 裡的 __COUNTER__ 用法發出
  # -Wc2y-extensions 診斷，於是變成編譯錯誤：
  #
  #   benchmark.h:1487: error: '__COUNTER__' is a C2y extension
  #                            [-Werror,-Wc2y-extensions]
  #
  # 這是「很新的編譯器 + 稍舊的第三方函式庫」的典型問題，與我們的程式碼
  # 無關。第三方的警告不該由我們負責，所以對它的 target 關掉。
  # 我們自己的 target（tests / bench / src）完全不受影響，仍然開 -Wall -Wextra。
  # ------------------------------------------------------------------------
  foreach(_bm_target benchmark benchmark_main)
    if(TARGET ${_bm_target})
      target_compile_options(${_bm_target} PRIVATE
        -Wno-unknown-warning-option  # 舊編譯器不認得下面的選項時不要叫
        -Wno-error                   # 取消 -pedantic-errors 的「警告即錯誤」
        -w)                          # 第三方程式碼的警告一律不顯示
    endif()
  endforeach()

  message(STATUS "Benchmark  : ${MINIDB_BENCHMARK_TAG} (第三方警告已靜音)")
endif()
