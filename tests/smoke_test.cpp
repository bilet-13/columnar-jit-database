
#include <gtest/gtest.h>

TEST(Smoke, ToolchainWorks) {
  EXPECT_EQ(1 + 1, 2);
}

TEST(Smoke, Cpp20IsEnabled) {
  static_assert(__cplusplus >= 202002L, "需要 C++20");
  EXPECT_GE(__cplusplus, 202002L);
}
