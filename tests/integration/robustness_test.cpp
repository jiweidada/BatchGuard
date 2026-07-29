#include "batchguard/core/content_fingerprint.h"
#include "batchguard/core/file_discovery.h"
#include "batchguard/core/scanner.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

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

std::string readBinaryFile(const std::filesystem::path& filePath) {
    std::ifstream file{filePath, std::ios::binary};
    return {
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}};
}

TEST(RobustnessTest, ProcessesManySameSizeCandidatePairs) {
    constexpr std::size_t kPairCount = 60U;
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path firstDirectory = temporaryDirectory.path() / "first";
    const std::filesystem::path secondDirectory = temporaryDirectory.path() / "second";
    ASSERT_TRUE(std::filesystem::create_directory(firstDirectory));
    ASSERT_TRUE(std::filesystem::create_directory(secondDirectory));

    for (std::size_t index = 0; index < kPairCount; ++index) {
        std::ostringstream contents;
        contents << "pair-" << std::setfill('0') << std::setw(4) << index;
        const std::string fileName = "file-" + std::to_string(index) + ".bin";
        createBinaryFile(firstDirectory / fileName, contents.str());
        createBinaryFile(secondDirectory / fileName, contents.str());
    }

    const ScanReport report = scanDirectory(temporaryDirectory.path());

    EXPECT_EQ(report.discoveredFileCount, kPairCount * 2U);
    EXPECT_EQ(report.successfulFileCount, kPairCount * 2U);
    EXPECT_TRUE(report.failures.empty());
    ASSERT_EQ(report.duplicateGroups.size(), kPairCount);
    for (const DuplicateGroup& group : report.duplicateGroups) {
        EXPECT_EQ(group.fileSize, 9U);
        EXPECT_EQ(group.filePaths.size(), 2U);
    }
}

TEST(RobustnessTest, RepeatedScanIsStableAndDoesNotModifyInputFiles) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path nestedDirectory =
        temporaryDirectory.path() / "nested";
    ASSERT_TRUE(std::filesystem::create_directory(nestedDirectory));
    const std::filesystem::path firstFile = temporaryDirectory.path() / "first.bin";
    const std::filesystem::path secondFile = nestedDirectory / "second.bin";
    const std::filesystem::path uniqueFile = temporaryDirectory.path() / "unique.bin";
    createBinaryFile(firstFile, "same contents");
    createBinaryFile(secondFile, "same contents");
    createBinaryFile(uniqueFile, "unique");
    const auto firstWriteTime = std::filesystem::last_write_time(firstFile);
    const auto secondWriteTime = std::filesystem::last_write_time(secondFile);
    const auto uniqueWriteTime = std::filesystem::last_write_time(uniqueFile);

    const ScanReport firstReport = scanDirectory(temporaryDirectory.path());
    const ScanReport secondReport = scanDirectory(temporaryDirectory.path());

    EXPECT_EQ(firstReport.discoveredFileCount, secondReport.discoveredFileCount);
    EXPECT_EQ(firstReport.successfulFileCount, secondReport.successfulFileCount);
    EXPECT_EQ(firstReport.failures.size(), secondReport.failures.size());
    ASSERT_EQ(firstReport.duplicateGroups.size(), 1U);
    ASSERT_EQ(secondReport.duplicateGroups.size(), 1U);
    EXPECT_EQ(
        firstReport.duplicateGroups.front().fileSize,
        secondReport.duplicateGroups.front().fileSize);
    EXPECT_EQ(
        firstReport.duplicateGroups.front().sha256,
        secondReport.duplicateGroups.front().sha256);
    EXPECT_EQ(
        firstReport.duplicateGroups.front().filePaths,
        secondReport.duplicateGroups.front().filePaths);
    EXPECT_EQ(std::filesystem::last_write_time(firstFile), firstWriteTime);
    EXPECT_EQ(std::filesystem::last_write_time(secondFile), secondWriteTime);
    EXPECT_EQ(std::filesystem::last_write_time(uniqueFile), uniqueWriteTime);
    EXPECT_EQ(readBinaryFile(firstFile), "same contents");
    EXPECT_EQ(readBinaryFile(secondFile), "same contents");
    EXPECT_EQ(readBinaryFile(uniqueFile), "unique");
}

TEST(RobustnessTest, FileRemovedAfterDiscoveryBecomesMetadataFailure) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path removedFile =
        temporaryDirectory.path() / "removed.bin";
    const std::filesystem::path remainingFile =
        temporaryDirectory.path() / "remaining.bin";
    createBinaryFile(removedFile, "removed");
    createBinaryFile(remainingFile, "remaining");
    const FileDiscoveryResult discoveryResult =
        discoverFiles(temporaryDirectory.path());
    ASSERT_EQ(discoveryResult.filePaths.size(), 2U);
    ASSERT_TRUE(std::filesystem::remove(removedFile));

    const ContentFingerprintResult fingerprintResult =
        fingerprintFileCandidates(discoveryResult.filePaths);

    ASSERT_EQ(fingerprintResult.fileRecords.size(), 1U);
    EXPECT_EQ(fingerprintResult.fileRecords.front().path, remainingFile);
    ASSERT_EQ(fingerprintResult.failures.size(), 1U);
    EXPECT_EQ(fingerprintResult.failures.front().path, removedFile);
    EXPECT_EQ(fingerprintResult.failures.front().stage, FailureStage::Metadata);
    EXPECT_TRUE(fingerprintResult.failures.front().errorCode);
}

}
}
