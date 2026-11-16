# Benchmark results

重現方式：

```bash
ninja -C build && ./build/bench/minidb_bench --benchmark_filter='Q[1-5]/'
```

## 機器與設定

| 項目 | 值 |
|------|-----|
| 機器 | Apple Silicon（Google Benchmark 報 12 個 CPU、L1D 64 KiB、L1I 128 KiB、L2 4096 KiB × 12） |
| 編譯器 | Homebrew clang 23.1.5 |
| Build | `-O3 -DNDEBUG`，無 sanitizer（`build/`） |
| 測資 | orders 100 萬、customers 2 萬、cities 200，固定 seed 42 |
| 計時 | Google Benchmark，每個組合至少跑 0.3 秒取平均 |

每次迭代都重建 operator 樹並完整跑一次查詢，所以 hash join 的 build 階段算在
裡面；測資的產生不算。

## 表 A：三種執行器（orders = 100 萬）

| 查詢 | 內容 | interpreter | batch scalar | batch NEON | 批次加速 | NEON 再加速 | 總加速 |
|------|------|------------:|-------------:|-----------:|--------:|-----------:|------:|
| Q1 | `SUM(amount)` | 11.7 ms | 0.104 ms | 0.104 ms | **112×** | 1.0× | **112×** |
| Q2 | `amount > 100`（約 90% 通過） | 23.0 ms | 1.78 ms | 0.385 ms | 12.9× | 4.6× | **59.7×** |
| Q3 | `amount > 900`（約 10% 通過） | 19.1 ms | 1.48 ms | 0.219 ms | 12.9× | 6.8× | **87.2×** |
| Q4 | 三個條件 AND | 33.6 ms | 7.16 ms | 0.521 ms | 4.7× | **13.7×** | **64.5×** |
| Q5 | 3 表 chain join + 兩個 filter | 57.9 ms | 28.6 ms | 22.2 ms | 2.0× | 1.3× | 2.6× |

`batch scalar` 是「一次 1024 個 row、但 kernel 是純量 C++」；`batch NEON` 是同一
條路徑換上手寫 NEON kernel。拆成兩欄才分得出「批次」與「SIMD」各自的貢獻。

## 讀出來的五件事

**1. 沒有 filter 的 SUM 是記憶體頻寬綁住的，SIMD 幫不上忙。**
Q1 兩個批次版本一樣快（0.104 ms）。100 萬個 int64 是 8 MB，0.104 ms 換算約
77 GB/s，已經貼著這台機器的記憶體頻寬。瓶頸在搬資料，不在算加法，所以四個
NEON 累加器沒有任何發揮空間。這也說明 Q1 的 112× 幾乎全部來自「不再每個 row
走一次表達式樹」，跟 SIMD 無關。

**2. 條件越多，手寫 SIMD 越值錢。**
Q4 有三個條件，純量批次版掉到 7.16 ms（Q2 的 4 倍），NEON 版只掉到 0.521 ms。
原因是純量版每個 predicate 都要逐 bit 組出 mask，三個 predicate 就做三遍；
NEON 版一次比較 8 個 row、一次水平加總寫出一整個 byte。**13.7× 是 NEON 在這
個專案裡最大的一筆收益。**

**3. 選擇率越低，NEON 的優勢越大。**
Q3（10% 通過）的 NEON 加速是 6.8×，Q2（90% 通過）只有 4.6×。mask 的計算成本
跟選擇率無關，但 mask → selection vector 要寫出的索引數量跟選擇率成正比，
所以通過的 row 少的時候 NEON 版省得更多。順帶一提 Q3 的 interpreter 比 Q2 快
（19.1 vs 23.0 ms），因為被篩掉的 row 不必再進 SUM。

**4. 一旦有 join，filter 的最佳化就不重要了。**
Q5 總共只加速 2.6×，而且 22.2 ms 裡幾乎全部是 hash join。filter 已經快到可以
忽略，瓶頸整個移到 join 的隨機存取上。**這正好是 Phase 3 要解決的問題**：
optimizer 要做的不是把 join 寫得更快，而是讓它處理更少的 row。

**5. interpreter 慢的主因是解譯，不是計算。**
Q1 的 11.7 ms 裡沒有任何 filter，它只是「每個 row 收集一次欄位值、呼叫一次
虛擬函式、跑一次 switch」。同樣的加總工作批次版只要 0.104 ms。

## 表 B / C / D

| 表 | 內容 | 狀態 |
|----|------|------|
| B | JIT 的損益平衡點 | Phase 4 |
| C | optimizer 的 join 順序效果 | Phase 3 |
| D | 估計列數 vs 實際列數的誤差 | Phase 3 |

## 已知沒有量到的東西

- **批次 hash join 的兩輪預取**沒有單獨量。目前 `batched_probe` 跟
  `ExecMode::kVectorized` 綁在一起，兩個批次版本都開著它，所以表 A 分不出
  它的貢獻。要量得另外開一個開關。
- Google Benchmark 讀不到這台機器的時脈（`hw.cpufrequency` 不存在），所以它
  報的 CPU 頻率是錯的。時間本身不受影響。
- 這台機器有 12 個 CPU，不是 docs/design.md 第 4 節寫的 8 核。v1.1 的 thread
  scaling 曲線要照實際核心數重新規劃。
