# column_jit-db

A miniature columnstore query engine written in C++20, built to implement three
techniques and measure them: **SIMD vectorized execution**, a **query optimizer
(costing + DP search)**, and **LLVM JIT codegen**.

The same query plan runs through three different executors — a tree-walking
interpreter, a hand-written NEON vectorized path, and a path whose WHERE clause
is compiled to machine code at run time — so the cost of each technique can be
measured against the same baseline.

## Status

| Phase | Contents | State |
|-------|----------|-------|
| 0 | CMake skeleton, sanitizer build, GoogleTest / Google Benchmark | Done |
| 1 | Interpreter (scan / filter / hash join / SUM) and a reference implementation | Done |
| 2 | Vectorized execution with NEON SIMD | Not started |
| 3 | Optimizer (statistics, cost model, DP join search) | Not started |
| 4 | LLVM ORC JIT codegen | Not started |
| 5 | Benchmark write-up | Not started |

## Design

Three decisions shape everything else:

- **Pull-based operators.** Every operator exposes a single `Next()` that the
  layer above calls. Hash join has to drain one input completely before it can
  probe the other, which is a plain `while` loop in this shape and a state
  machine in the push-based alternative.
- **One operator tree, three filters.** Scan, hash join and aggregate exist
  once. Only the filter has three implementations, so comparing the executors
  compares filters and nothing else.
- **Columns are split into fixed-size chunks.** A chunk is the unit of work for
  parallel scans and the place a zone map will keep its min/max. Chunk size is a
  multiple of batch size, so a batch never straddles a boundary and scans hand
  out pointers instead of copying rows.

## Environment

| Item | Version |
|------|---------|
| Machine | MacBook M1 (Apple Silicon, ARM64) |
| Language | C++20 |
| Compiler | Homebrew clang 23.1.5 (`brew install llvm`) |
| Build | CMake ≥ 3.24 + Ninja |
| Unit tests | GoogleTest v1.15.2 (fetched by CMake) |
| Benchmarks | Google Benchmark v1.9.0 (fetched by CMake) |
| SIMD | NEON intrinsics |
| Codegen | LLVM ORC JIT 23.1.5 (same package as the compiler) |

## Building

Two build directories with different jobs. They cannot be shared, because
sanitizers are a compile-time choice.

```bash
# Clean release build — for benchmarks
cmake -GNinja -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
ninja -C build
```

```bash
# ASan + UBSan build — for tests (the bench target is switched off automatically)
cmake -GNinja -B build-asan -DCMAKE_BUILD_TYPE=Debug \
      -DMINIDB_ENABLE_ASAN=ON -DMINIDB_ENABLE_UBSAN=ON \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
ninja -C build-asan
```

Enabling any sanitizer disables the benchmark target, so timings can never come
from instrumented code.

## Tests and benchmarks

```bash
ctest --test-dir build-asan --output-on-failure   # instrumented
```

```bash
./build/bench/minidb_bench                       # clean
```

Correctness is checked two ways. Every query is compared against a reference
implementation that reads the raw data with plain loops and touches none of the
engine, and the three executors are compared against each other.

The dataset is generated in memory from a fixed seed, with a deliberately narrow
value range so a sum over a million rows cannot overflow and the executors agree
bit for bit.

## Benchmark numbers

Filled in from Phase 2 onwards.

## Out of scope

Persistence (disk, write-ahead log, crash recovery), transactions and MVCC,
distribution, and full SQL. None of them bear on the three techniques this
project exists to demonstrate.
