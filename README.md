# single_store_like_miniDB

一個用 C++20 寫的迷你 columnstore 查詢引擎，拿來實作三項技術並量出數字：
**SIMD 向量化執行**、**query optimizer（costing + DP search）**、**LLVM JIT codegen**。

個人學習專案，與 SingleStore 無任何關係，未使用其程式碼。

> 設計文件與實作計畫在 `docs/`（未納入版控）。

## 現況

| Phase | 內容 | 狀態 |
|-------|------|------|
| 0 | CMake 骨架、sanitizer build、GoogleTest / Google Benchmark | 完成 |
| 1 | Interpreter（scan / filter / hash join / SUM）+ 參考實作 | 完成 |
| 2 | Vectorized + NEON SIMD | 未開始 |
| 3 | Optimizer（statistics / cost model / DP join search） | 未開始 |
| 4 | LLVM ORC JIT codegen | 未開始 |
| 5 | README 與 benchmark 整理 | 未開始 |

## 環境

| 項目 | 版本 |
|------|------|
| 機器 | MacBook M1（Apple Silicon，ARM64） |
| 語言 | C++20 |
| 編譯器 | Homebrew clang 23.1.5（`brew install llvm`） |
| 建置 | CMake ≥ 3.24 + Ninja |
| 測試 | GoogleTest v1.15.2（FetchContent 自動下載） |
| 效能 | Google Benchmark v1.9.0（FetchContent 自動下載） |
| SIMD | NEON intrinsics |
| Codegen | LLVM ORC JIT 23.1.5（與編譯器同一包） |

## 建置

兩個 build 目錄，用途不同、不能共用（sanitizer 是編譯期旗標）：

```bash
# 乾淨的 Release build —— 跑 benchmark 用
cmake -GNinja -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
ninja -C build

# ASan + UBSan build —— 跑測試用（bench target 會自動關掉）
cmake -GNinja -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DMINIDB_ENABLE_ASAN=ON -DMINIDB_ENABLE_UBSAN=ON \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
ninja -C build-asan
```

## 測試與 benchmark

```bash
ctest --test-dir build-asan --output-on-failure   # 測試（插樁版）
./build/bench/minidb_bench                       # benchmark（乾淨版）
```

## Benchmark 數字

Phase 2 之後填入，見 `docs/results.md`。

## 刻意不做

持久化（磁碟、WAL、crash recovery）、transaction / MVCC、分散式、完整 SQL。
這些與本專案要展示的三項技術無關。資料在記憶體中由固定 seed 的產生器建立。
