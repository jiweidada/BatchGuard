#include "batchguard/core/file_hasher.h"
#include "batchguard/core/scanner.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <Windows.h>

#include <algorithm>
#include <cstdint>
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

TEST(ProgressReportingTest, ScannerReportsAllStagesInOrder) {
    const test_support::TemporaryDirectory temporaryDirectory;
    createBinaryFile(temporaryDirectory.path() / "first.bin", "same");
    createBinaryFile(temporaryDirectory.path() / "second.bin", "same");
    createBinaryFile(temporaryDirectory.path() / "unique.bin", "unique");
    std::vector<ScanProgress> events;

    const ScanReport report = scanDirectory(
        temporaryDirectory.path(),
        [&events](const ScanProgress& progress) {
            events.push_back(progress);
        });

    ASSERT_TRUE(report.isComplete());
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.front().stage, ScanProgressStage::Discovery);
    EXPECT_EQ(events.back().stage, ScanProgressStage::Grouping);
    EXPECT_TRUE(events.back().isStageComplete);
    for (std::size_t index = 1; index < events.size(); ++index) {
        EXPECT_LE(
            static_cast<int>(events[index - 1U].stage),
            static_cast<int>(events[index].stage));
    }

    const auto completedDiscovery = std::find_if(
        events.begin(),
        events.end(),
        [](const ScanProgress& progress) {
            return progress.stage == ScanProgressStage::Discovery &&
                progress.isStageComplete;
        });
    const auto completedMetadata = std::find_if(
        events.begin(),
        events.end(),
        [](const ScanProgress& progress) {
            return progress.stage == ScanProgressStage::Metadata &&
                progress.isStageComplete;
        });
    const auto completedHashing = std::find_if(
        events.begin(),
        events.end(),
        [](const ScanProgress& progress) {
            return progress.stage == ScanProgressStage::Hashing &&
                progress.isStageComplete;
        });
    ASSERT_NE(completedDiscovery, events.end());
    ASSERT_NE(completedMetadata, events.end());
    ASSERT_NE(completedHashing, events.end());
    EXPECT_EQ(completedDiscovery->completedItems, 3U);
    EXPECT_EQ(completedMetadata->completedItems, 3U);
    EXPECT_EQ(completedMetadata->totalItems, 3U);
    EXPECT_EQ(completedHashing->completedItems, 2U);
    EXPECT_EQ(completedHashing->totalItems, 2U);
    EXPECT_EQ(completedHashing->completedBytes, 8U);
    EXPECT_EQ(completedHashing->totalBytes, 8U);
}

TEST(ProgressReportingTest, FileHasherReportsMonotonicBlockProgress) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path filePath = temporaryDirectory.path() / "large.bin";
    constexpr std::size_t kFileSize = 1'000'000U;
    createBinaryFile(filePath, std::string(kFileSize, 'a'));
    std::vector<std::uintmax_t> completedBytes;

    const FileHashResult result = calculateFileSha256(
        filePath,
        [&completedBytes](std::uintmax_t value) {
            completedBytes.push_back(value);
        });

    ASSERT_TRUE(result.isSuccess());
    ASSERT_GT(completedBytes.size(), 1U);
    EXPECT_TRUE(std::is_sorted(completedBytes.begin(), completedBytes.end()));
    EXPECT_EQ(completedBytes.back(), kFileSize);
}

TEST(ProgressReportingTest, FileFailureStillCompletesProgressStages) {
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
    std::vector<ScanProgress> events;

    const ScanReport report = scanDirectory(
        temporaryDirectory.path(),
        [&events](const ScanProgress& progress) {
            events.push_back(progress);
        });
    CloseHandle(lockedHandle);

    EXPECT_FALSE(report.isComplete());
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back().stage, ScanProgressStage::Grouping);
    EXPECT_TRUE(events.back().isStageComplete);
    const auto completedHashing = std::find_if(
        events.begin(),
        events.end(),
        [](const ScanProgress& progress) {
            return progress.stage == ScanProgressStage::Hashing &&
                progress.isStageComplete;
        });
    ASSERT_NE(completedHashing, events.end());
    EXPECT_EQ(completedHashing->completedItems, 2U);
    EXPECT_EQ(completedHashing->totalItems, 2U);
    EXPECT_EQ(completedHashing->completedBytes, 12U);
    EXPECT_EQ(completedHashing->totalBytes, 12U);
}

}
}
