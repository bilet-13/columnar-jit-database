# column_jit-db

A miniature columnstore query engine written in C++20, built to implement three
techniques and measure them: **SIMD vectorized execution**, a **query optimizer
(costing + DP search)**, and **LLVM JIT codegen**.

The same query plan runs through three different executors — a tree-walking
interpreter, a hand-written NEON vectorized path, and a path whose WHERE clause
is compiled to machine code at run time — so the cost of each technique can be
measured against the same baseline.

## Benchmark numbers

| Query over 1M rows | Interpreter | Batch | Batch + SIMD | Batch + SIMD vs Interpreter |
|---|---:|---:|---:|---:|
| `SUM(amount)` | 11.7 ms | 0.104 ms | 0.104 ms | 112x |
| `+ WHERE amount > 100` | 23.0 ms | 1.78 ms | 0.385 ms | 60x |
| `+ WHERE amount > 900` | 19.1 ms | 1.48 ms | 0.219 ms | 87x |
| `+ WHERE` x3 predicates | 33.6 ms | 7.16 ms | 0.521 ms | 65x |
| `+ JOIN` 2 more tables | 57.9 ms | 28.6 ms | 22.2 ms | 2.6x |

Measured on an Apple M2 Pro with Homebrew clang 23.1.5.

## Status

| Phase | Contents | State |
|-------|----------|-------|
| 0 | CMake skeleton, sanitizer build, GoogleTest / Google Benchmark | Done |
| 1 | Interpreter (scan / filter / hash join / SUM) and a reference implementation | Done |
| 2 | Vectorized execution with NEON SIMD | Done |
| 3 | Optimizer (statistics, cost model, DP join search) | Not started |
| 4 | LLVM ORC JIT codegen | Not started |
| 5 | Benchmark write-up | Not started |

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

## Out of scope

Persistence (disk, write-ahead log, crash recovery), transactions and MVCC,
distribution, and full SQL. None of them bear on the three techniques this
project exists to demonstrate.
