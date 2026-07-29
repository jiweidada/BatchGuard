#include "batchguard/core/input_validator.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace batchguard {
namespace {

TEST(InputValidatorTest, AcceptsExistingDirectory) {
    const test_support::TemporaryDirectory temporaryDirectory;

    const InputValidationResult result =
        validateInputDirectory(temporaryDirectory.path());

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.error, InputValidationError::None);
    EXPECT_FALSE(result.errorCode);
}

TEST(InputValidatorTest, RejectsEmptyPath) {
    const InputValidationResult result = validateInputDirectory({});

    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.error, InputValidationError::EmptyPath);
}

TEST(InputValidatorTest, RejectsMissingPath) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path missingPath = temporaryDirectory.path() / "missing";

    const InputValidationResult result = validateInputDirectory(missingPath);

    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.error, InputValidationError::DoesNotExist);
}

TEST(InputValidatorTest, RejectsRegularFile) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path filePath = temporaryDirectory.path() / "file.txt";
    std::ofstream file{filePath};
    file << "BatchGuard";
    file.close();
    ASSERT_TRUE(std::filesystem::is_regular_file(filePath));

    const InputValidationResult result = validateInputDirectory(filePath);

    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(result.error, InputValidationError::NotDirectory);
}

TEST(InputValidatorTest, AcceptsDirectoryWithUnicodeAndSpaces) {
    const test_support::TemporaryDirectory temporaryDirectory;

    // 通用字符名称使测试夹具不受源码和控制台代码页设置影响。
    const std::filesystem::path unicodePath =
        temporaryDirectory.path() / L"\u4e2d\u6587 \u76ee\u5f55";
    ASSERT_TRUE(std::filesystem::create_directory(unicodePath));

    const InputValidationResult result = validateInputDirectory(unicodePath);

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.error, InputValidationError::None);
}

}
}
