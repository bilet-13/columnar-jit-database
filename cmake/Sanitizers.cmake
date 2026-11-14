# Sanitizer 設定（docs/design.md 第 4 節、第 11 節）
#
# 用法：每個「要被檢查」的 target 連上 minidb_sanitizers 這個 INTERFACE target。
# 開不開由 build 目錄的選項決定，所以同一份程式碼可以同時存在
# build/（乾淨，量效能）與 build-asan/（插樁，抓錯）兩種編譯結果。
#
#   cmake -GNinja -B build                                     # 都不開
#   cmake -GNinja -B build-asan -DMINIDB_ENABLE_ASAN=ON ...     # 開 ASan
#
# 重要：sanitizer 是編譯期旗標，整支程式的每個 .o 必須一致，
#       所以不能在同一個 build 目錄裡切換，必須分開目錄。

option(MINIDB_ENABLE_ASAN  "Enable AddressSanitizer" OFF)
option(MINIDB_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(MINIDB_ENABLE_TSAN  "Enable ThreadSanitizer (v1.1 才會用到)" OFF)

add_library(minidb_sanitizers INTERFACE)

if(MINIDB_ENABLE_ASAN AND MINIDB_ENABLE_TSAN)
  message(FATAL_ERROR "ASan 與 TSan 不能同時開啟，請分成兩個 build 目錄。")
endif()

set(_minidb_san "")
if(MINIDB_ENABLE_ASAN)
  list(APPEND _minidb_san address)
endif()
if(MINIDB_ENABLE_UBSAN)
  list(APPEND _minidb_san undefined)
endif()
if(MINIDB_ENABLE_TSAN)
  list(APPEND _minidb_san thread)
endif()

if(_minidb_san)
  list(JOIN _minidb_san "," _minidb_san_arg)

  target_compile_options(minidb_sanitizers INTERFACE
    -fsanitize=${_minidb_san_arg}
    -fno-omit-frame-pointer      # 堆疊才印得出來
    -fno-sanitize-recover=all    # 抓到就停，不要繼續跑
    -g)
  target_link_options(minidb_sanitizers INTERFACE
    -fsanitize=${_minidb_san_arg})

  message(STATUS "Sanitizers : ${_minidb_san_arg}")

  # Benchmark 絕對不能在插樁的情況下量，數字會完全失真。
  # 在 sanitizer build 裡直接不要 build bench target。
  if(MINIDB_BUILD_BENCH)
    message(STATUS "             → sanitizer build，自動關閉 bench target")
    set(MINIDB_BUILD_BENCH OFF CACHE BOOL "Build benchmarks" FORCE)
  endif()
else()
  message(STATUS "Sanitizers : none")
endif()

# 注意：M1 macOS 上的 ASan 不支援 memory leak 偵測（detect_leaks 無效）。
# 注意：Phase 4 之後，JIT 產生的機器碼不會被插樁，ASan 抓不到那裡的越界，
#       靠 tests/e2e 的三執行器交叉驗證覆蓋。
