#include "batchguard/core/file_discovery.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace batchguard {
namespace {

void createFile(const std::filesystem::path& filePath, const std::string& contents) {
    std::ofstream file{filePath};
    file << contents;
    file.close();
    ASSERT_TRUE(std::filesystem::is_regular_file(filePath));
}

TEST(FileDiscoveryTest, EmptyDirectoryProducesEmptyResult) {
    const test_support::TemporaryDirectory temporaryDirectory;

    const FileDiscoveryResult result = discoverFiles(temporaryDirectory.path());

    EXPECT_TRUE(result.filePaths.empty());
    EXPECT_TRUE(result.failures.empty());
}

TEST(FileDiscoveryTest, RecursivelyFindsRegularFiles) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path firstDirectory = temporaryDirectory.path() / "first";
    const std::filesystem::path secondDirectory = firstDirectory / "second";
    ASSERT_TRUE(std::filesystem::create_directories(secondDirectory));
    const std::filesystem::path rootFile = temporaryDirectory.path() / "root.txt";
    const std::filesystem::path nestedFile = firstDirectory / "nested.bin";
    const std::filesystem::path deeplyNestedFile = secondDirectory / "deep";
    createFile(rootFile, "root");
    createFile(nestedFile, "nested");
    createFile(deeplyNestedFile, "deep");

    const FileDiscoveryResult result = discoverFiles(temporaryDirectory.path());

    EXPECT_EQ(result.filePaths.size(), 3U);
    EXPECT_NE(std::find(result.filePaths.begin(), result.filePaths.end(), rootFile),
              result.filePaths.end());
    EXPECT_NE(std::find(result.filePaths.begin(), result.filePaths.end(), nestedFile),
              result.filePaths.end());
    EXPECT_NE(std::find(result.filePaths.begin(), result.filePaths.end(), deeplyNestedFile),
              result.filePaths.end());
    EXPECT_TRUE(result.failures.empty());
}

TEST(FileDiscoveryTest, DoesNotFilterByExtension) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path extensionlessFile = temporaryDirectory.path() / "README";
    const std::filesystem::path unusualExtensionFile =
        temporaryDirectory.path() / "archive.custom-extension";
    createFile(extensionlessFile, "extensionless");
    createFile(unusualExtensionFile, "custom");

    const FileDiscoveryResult result = discoverFiles(temporaryDirectory.path());

    EXPECT_EQ(result.filePaths.size(), 2U);
    EXPECT_TRUE(result.failures.empty());
}

TEST(FileDiscoveryTest, ReturnsPathsInStableLexicalOrder) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path nestedDirectory = temporaryDirectory.path() / "middle";
    ASSERT_TRUE(std::filesystem::create_directory(nestedDirectory));
    createFile(temporaryDirectory.path() / "z.txt", "z");
    createFile(temporaryDirectory.path() / "a.txt", "a");
    createFile(nestedDirectory / "b.txt", "b");

    const FileDiscoveryResult result = discoverFiles(temporaryDirectory.path());

    std::vector<std::filesystem::path> expectedPaths = result.filePaths;
    std::sort(
        expectedPaths.begin(),
        expectedPaths.end(),
        [](const std::filesystem::path& left, const std::filesystem::path& right) {
            return left.native() < right.native();
        });
    EXPECT_EQ(result.filePaths, expectedPaths);
}

TEST(FileDiscoveryTest, SupportsUnicodeAndSpacesInPaths) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path unicodeDirectory =
        temporaryDirectory.path() / L"\u4e2d\u6587 \u5b50\u76ee\u5f55";
    ASSERT_TRUE(std::filesystem::create_directory(unicodeDirectory));
    const std::filesystem::path unicodeFile =
        unicodeDirectory / L"\u6d4b\u8bd5 \u6587\u4ef6.txt";
    createFile(unicodeFile, "unicode");

    const FileDiscoveryResult result = discoverFiles(temporaryDirectory.path());

    ASSERT_EQ(result.filePaths.size(), 1U);
    EXPECT_EQ(result.filePaths.front(), unicodeFile);
    EXPECT_TRUE(result.failures.empty());
}

TEST(FileDiscoveryTest, SkipsDirectorySymbolicLinks) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path scanRoot = temporaryDirectory.path() / "scan-root";
    const std::filesystem::path externalDirectory =
        temporaryDirectory.path() / "external";
    ASSERT_TRUE(std::filesystem::create_directory(scanRoot));
    ASSERT_TRUE(std::filesystem::create_directory(externalDirectory));
    createFile(externalDirectory / "outside.txt", "outside");

    const std::filesystem::path symbolicLink = scanRoot / "linked-directory";
    std::error_code errorCode;
    std::filesystem::create_directory_symlink(
        externalDirectory,
        symbolicLink,
        errorCode);
    if (errorCode) {
        GTEST_SKIP() << "Current Windows configuration cannot create directory symlinks.";
    }

    const FileDiscoveryResult result = discoverFiles(scanRoot);

    EXPECT_TRUE(result.filePaths.empty());
    EXPECT_TRUE(result.failures.empty());
}

TEST(FileDiscoveryTest, RecordsFailureForMissingRoot) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path missingRoot = temporaryDirectory.path() / "missing";

    const FileDiscoveryResult result = discoverFiles(missingRoot);

    EXPECT_TRUE(result.filePaths.empty());
    ASSERT_EQ(result.failures.size(), 1U);
    EXPECT_EQ(result.failures.front().path, missingRoot);
    EXPECT_EQ(result.failures.front().stage, FailureStage::Discovery);
    EXPECT_TRUE(result.failures.front().errorCode);
}

}
}
