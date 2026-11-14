cmake_minimum_required(VERSION 3.24)
project(single_store_like_miniDB
        VERSION 0.1.0
        DESCRIPTION "A mini columnstore query engine: SIMD, optimizer, LLVM codegen"
        LANGUAGES CXX)

# ---------------------------------------------------------------------------
# 語言設定
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 產生 compile_commands.json，給編輯器 / clangd 用（會放在 build 目錄裡）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 沒指定 build type 時預設 Release（benchmark 數字才有意義）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# 編譯器檢查
#
# docs/design.md 第 4 節：建議全程用 Homebrew 的 clang，與 Phase 4 要連的
# LLVM 函式庫同版本。Apple Clang 不是不能用，但 Phase 4 接 LLVM 時版本
# 不一致容易出問題，所以這裡先提醒。
# ---------------------------------------------------------------------------
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "             ${CMAKE_CXX_COMPILER}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  message(WARNING
    "正在使用 Apple Clang。Phase 1-3 沒問題，但 Phase 4 要連 LLVM 時建議改用 "
    "Homebrew 的 clang（設定時加 -DCMAKE_CXX_COMPILER=\"$(brew --prefix llvm)/bin/clang++\"）。"
    "注意：換編譯器必須用全新的 build 目錄，不能沿用舊的。")
endif()

# ---------------------------------------------------------------------------
# 選項
# ---------------------------------------------------------------------------
option(MINIDB_BUILD_TESTS "Build unit tests (GoogleTest)" ON)
option(MINIDB_BUILD_BENCH "Build benchmarks (Google Benchmark)" ON)

# Sanitizers 必須在 Dependencies 之前 include：
# 它會在 sanitizer 開啟時自動關掉 MINIDB_BUILD_BENCH。
include(Sanitizers)
include(Dependencies)

# ---------------------------------------------------------------------------
# 子目錄
#
# src/ 從 Phase 1（C1.1）才會出現。
# ---------------------------------------------------------------------------
if(MINIDB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MINIDB_BUILD_BENCH)
  add_subdirectory(bench)
endif()

message(STATUS "---------------------------------------------------")
message(STATUS "build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "tests      : ${MINIDB_BUILD_TESTS}")
message(STATUS "bench      : ${MINIDB_BUILD_BENCH}")
message(STATUS "---------------------------------------------------")
