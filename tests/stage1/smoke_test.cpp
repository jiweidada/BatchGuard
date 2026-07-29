#include "batchguard/core/batchguard_core.h"

#include <gtest/gtest.h>

#include <array>
#include <span>

TEST(Stage1SmokeTest, UsesCxx20StandardLibrary) {
    const std::array values{1, 2, 3};
    const std::span view{values};

    EXPECT_EQ(view.size(), 3U);
}
