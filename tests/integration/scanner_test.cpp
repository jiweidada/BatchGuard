#include "batchguard/core/scanner.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace batchguard {
namespace {

void createBinaryFile(
    const std::filesystem::path& filePath,
    const std::string& contents) {
    std::ofstream file{filePath, std::ios::binary};
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();
    ASSERT_TRUE(std::filesystem::is_regular_file(filePath));
}

TEST(ScannerTest, EmptyDirectoryProducesCompleteEmptyReport) {
    const test_support::TemporaryDirectory temporaryDirectory;

    const ScanReport report = scanDirectory(temporaryDirectory.path());

    EXPECT_EQ(report.rootPath, temporaryDirectory.path());
    EXPECT_EQ(report.discoveredFileCount, 0U);
    EXPECT_EQ(report.successfulFileCount, 0U);
    EXPECT_TRUE(report.failures.empty());
    EXPECT_TRUE(report.duplicateGroups.empty());
    EXPECT_TRUE(report.isComplete());
}

TEST(ScannerTest, FindsDuplicateFilesAcrossNestedDirectories) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path nestedDirectory =
        temporaryDirectory.path() / "nested";
    ASSERT_TRUE(std::filesystem::create_directory(nestedDirectory));
    const std::filesystem::path firstFile = temporaryDirectory.path() / "first.txt";
    const std::filesystem::path secondFile = nestedDirectory / "second.txt";
    createBinaryFile(firstFile, "same contents");
    createBinaryFile(secondFile, "same contents");

    const ScanReport report = scanDirectory(temporaryDirectory.path());

    EXPECT_EQ(report.discoveredFileCount, 2U);
    EXPECT_EQ(report.successfulFileCount, 2U);
    EXPECT_TRUE(report.failures.empty());
    ASSERT_EQ(report.duplicateGroups.size(), 1U);
    EXPECT_EQ(
        report.duplicateGroups.front().filePaths,
        (std::vector<std::filesystem::path>{firstFile, secondFile}));
}

TEST(ScannerTest, DoesNotGroupSameNameFilesWithDifferentContents) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path firstDirectory = temporaryDirectory.path() / "first";
    const std::filesystem::path secondDirectory = temporaryDirectory.path() / "second";
    ASSERT_TRUE(std::filesystem::create_directory(firstDirectory));
    ASSERT_TRUE(std::filesystem::create_directory(secondDirectory));
    createBinaryFile(firstDirectory / "same.bin", "abcd");
    createBinaryFile(secondDirectory / "same.bin", "wxyz");

    const ScanReport report = scanDirectory(temporaryDirectory.path());

    EXPECT_TRUE(report.duplicateGroups.empty());
    EXPECT_TRUE(report.isComplete());
}

TEST(ScannerTest, GroupsTwoEmptyFiles) {
    const test_support::TemporaryDirectory temporaryDirectory;
    createBinaryFile(temporaryDirectory.path() / "first.empty", "");
    createBinaryFile(temporaryDirectory.path() / "second.empty", "");

    const ScanReport report = scanDirectory(temporaryDirectory.path());

    ASSERT_EQ(report.duplicateGroups.size(), 1U);
    EXPECT_EQ(report.duplicateGroups.front().fileSize, 0U);
    EXPECT_EQ(report.duplicateGroups.front().filePaths.size(), 2U);
}

TEST(ScannerTest, KeepsThreeFilesInOneDuplicateGroup) {
    const test_support::TemporaryDirectory temporaryDirectory;
    createBinaryFile(temporaryDirectory.path() / "first.bin", "duplicate");
    createBinaryFile(temporaryDirectory.path() / "second.bin", "duplicate");
    createBinaryFile(temporaryDirectory.path() / "third.bin", "duplicate");

    const ScanReport report = scanDirectory(temporaryDirectory.path());

    ASSERT_EQ(report.duplicateGroups.size(), 1U);
    EXPECT_EQ(report.duplicateGroups.front().filePaths.size(), 3U);
    EXPECT_EQ(report.successfulFileCount, 3U);
}

TEST(ScannerTest, HashingFailureProducesPartialReportAndContinues) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path lockedFile = temporaryDirectory.path() / "locked.bin";
    const std::filesystem::path readableFile = temporaryDirectory.path() / "readable.bin";
    createBinaryFile(lockedFile, "locked");
    createBinaryFile(readableFile, "public");
    const HANDLE lockedHandle = CreateFileW(
        lockedFile.c_str(),
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    ASSERT_NE(lockedHandle, INVALID_HANDLE_VALUE);

    const ScanReport report = scanDirectory(temporaryDirectory.path());
    CloseHandle(lockedHandle);

    EXPECT_EQ(report.discoveredFileCount, 2U);
    EXPECT_EQ(report.successfulFileCount, 1U);
    ASSERT_EQ(report.failures.size(), 1U);
    EXPECT_EQ(report.failures.front().path, lockedFile);
    EXPECT_EQ(report.failures.front().stage, FailureStage::Hashing);
    EXPECT_TRUE(report.duplicateGroups.empty());
    EXPECT_FALSE(report.isComplete());
}

}
}
