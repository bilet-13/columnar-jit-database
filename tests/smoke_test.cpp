// Phase 0 的煙霧測試：只確認工具鏈接起來了。
// 真正的測試從 Phase 1（C1.1 schema_test）開始。

#include <gtest/gtest.h>

TEST(Smoke, ToolchainWorks) {
  EXPECT_EQ(1 + 1, 2);
}

TEST(Smoke, Cpp20IsEnabled) {
  // 確認編譯器真的是用 C++20 在編，而不是預設的舊標準
  static_assert(__cplusplus >= 202002L, "需要 C++20");
  EXPECT_GE(__cplusplus, 202002L);
}

// ---------------------------------------------------------------------------
// 想親眼確認 ASan 真的在運作，就把下面這段暫時取消註解，然後在
// build-asan/ 裡跑測試。預期會看到測試「失敗」並印出
// heap-buffer-overflow 與完整堆疊；在 build/ 裡則什麼事都不會發生
// （讀到垃圾值，程式照跑）。
//
// 這就是 sanitizer 存在的理由：同一段錯誤的程式碼，一個 build 抓得到、
// 一個抓不到。確認完記得註解回去，不然測試會一直紅的。
// ---------------------------------------------------------------------------
//
// #include <vector>
// TEST(Smoke, AsanCatchesOutOfBounds) {
//   std::vector<int> v(4);
//   volatile int x = v.data()[10];  // 故意越界：只配置了 4 個 int
//   (void)x;
// }
