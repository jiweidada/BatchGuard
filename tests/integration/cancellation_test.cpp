#include "batchguard/core/file_hasher.h"
#include "batchguard/core/scanner.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stop_token>
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

ScanResult scanWithCancellation(
    const std::filesystem::path& rootPath,
    const ScanProgressCallback& progressCallback,
    std::stop_token stopToken) {
    return scanDirectory(rootPath, ScanOptions{1U}, progressCallback, stopToken);
}

TEST(CancellationTest, AlreadyRequestedStopReturnsNoReport) {
    const test_support::TemporaryDirectory temporaryDirectory;
    std::stop_source stopSource;
    ASSERT_TRUE(stopSource.request_stop());
    bool wasProgressCalled = false;

    const ScanResult result = scanWithCancellation(
        temporaryDirectory.path(),
        [&wasProgressCalled](const ScanProgress&) {
            wasProgressCalled = true;
        },
        stopSource.get_token());

    EXPECT_TRUE(result.isCancelled());
    EXPECT_FALSE(result.report.has_value());
    EXPECT_FALSE(wasProgressCalled);
}

TEST(CancellationTest, FileFailureRemainsACompletedScanResult) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path missingRoot =
        temporaryDirectory.path() / "missing";

    const ScanResult result = scanWithCancellation(missingRoot, {}, {});

    ASSERT_TRUE(result.isCompleted());
    ASSERT_TRUE(result.report.has_value());
    EXPECT_FALSE(result.report->isComplete());
    ASSERT_EQ(result.report->failures.size(), 1U);
    EXPECT_EQ(result.report->failures.front().stage, FailureStage::Discovery);
}

TEST(CancellationTest, StopsDuringFileDiscovery) {
    const test_support::TemporaryDirectory temporaryDirectory;
    createBinaryFile(temporaryDirectory.path() / "first.bin", "a");
    createBinaryFile(temporaryDirectory.path() / "second.bin", "b");
    std::stop_source stopSource;

    const ScanResult result = scanWithCancellation(
        temporaryDirectory.path(),
        [&stopSource](const ScanProgress& progress) {
            if (progress.stage == ScanProgressStage::Discovery &&
                progress.completedItems == 1U) {
                stopSource.request_stop();
            }
        },
        stopSource.get_token());

    EXPECT_TRUE(result.isCancelled());
    EXPECT_FALSE(result.report.has_value());
}

TEST(CancellationTest, StopsDuringMetadataReading) {
    const test_support::TemporaryDirectory temporaryDirectory;
    createBinaryFile(temporaryDirectory.path() / "first.bin", "a");
    createBinaryFile(temporaryDirectory.path() / "second.bin", "b");
    std::stop_source stopSource;

    const ScanResult result = scanWithCancellation(
        temporaryDirectory.path(),
        [&stopSource](const ScanProgress& progress) {
            if (progress.stage == ScanProgressStage::Metadata &&
                progress.completedItems == 1U) {
                stopSource.request_stop();
            }
        },
        stopSource.get_token());

    EXPECT_TRUE(result.isCancelled());
    EXPECT_FALSE(result.report.has_value());
}

TEST(CancellationTest, StopsDuringLargeFileHashing) {
    const test_support::TemporaryDirectory temporaryDirectory;
    constexpr std::size_t kFileSize = 4U * 1024U * 1024U;
    createBinaryFile(
        temporaryDirectory.path() / "first.bin",
        std::string(kFileSize, 'a'));
    createBinaryFile(
        temporaryDirectory.path() / "second.bin",
        std::string(kFileSize, 'b'));
    std::stop_source stopSource;
    std::vector<ScanProgress> events;

    const ScanResult result = scanWithCancellation(
        temporaryDirectory.path(),
        [&stopSource, &events](const ScanProgress& progress) {
            events.push_back(progress);
            if (progress.stage == ScanProgressStage::Hashing &&
                progress.completedBytes > 0U) {
                stopSource.request_stop();
            }
        },
        stopSource.get_token());

    EXPECT_TRUE(result.isCancelled());
    EXPECT_FALSE(result.report.has_value());
    EXPECT_TRUE(std::none_of(
        events.begin(),
        events.end(),
        [](const ScanProgress& progress) {
            return progress.stage == ScanProgressStage::Hashing &&
                progress.isStageComplete;
        }));
}

TEST(CancellationTest, StopsBeforeDuplicateGrouping) {
    const test_support::TemporaryDirectory temporaryDirectory;
    createBinaryFile(temporaryDirectory.path() / "first.bin", "same");
    createBinaryFile(temporaryDirectory.path() / "second.bin", "same");
    std::stop_source stopSource;

    const ScanResult result = scanWithCancellation(
        temporaryDirectory.path(),
        [&stopSource](const ScanProgress& progress) {
            if (progress.stage == ScanProgressStage::Grouping) {
                stopSource.request_stop();
            }
        },
        stopSource.get_token());

    EXPECT_TRUE(result.isCancelled());
    EXPECT_FALSE(result.report.has_value());
}

TEST(CancellationTest, FileHasherChecksStopBetweenReadBlocks) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path filePath = temporaryDirectory.path() / "large.bin";
    constexpr std::size_t kFileSize = 1'000'000U;
    createBinaryFile(filePath, std::string(kFileSize, 'a'));
    std::stop_source stopSource;
    std::vector<std::uintmax_t> completedBytes;

    const FileHashResult result = calculateFileSha256(
        filePath,
        [&stopSource, &completedBytes](std::uintmax_t value) {
            completedBytes.push_back(value);
            stopSource.request_stop();
        },
        stopSource.get_token());

    EXPECT_TRUE(result.isCancelled);
    EXPECT_FALSE(result.isSuccess());
    ASSERT_EQ(completedBytes.size(), 1U);
    EXPECT_LT(completedBytes.front(), kFileSize);
}

TEST(CancellationTest, UnrequestedStopMatchesCompatibleScanInterface) {
    const test_support::TemporaryDirectory temporaryDirectory;
    createBinaryFile(temporaryDirectory.path() / "first.bin", "same");
    createBinaryFile(temporaryDirectory.path() / "second.bin", "same");

    const ScanReport compatibleReport = scanDirectory(temporaryDirectory.path());
    const ScanResult cancellableResult = scanWithCancellation(
        temporaryDirectory.path(),
        {},
        {});

    ASSERT_TRUE(cancellableResult.isCompleted());
    ASSERT_TRUE(cancellableResult.report.has_value());
    EXPECT_EQ(cancellableResult.report->rootPath, compatibleReport.rootPath);
    EXPECT_EQ(
        cancellableResult.report->discoveredFileCount,
        compatibleReport.discoveredFileCount);
    EXPECT_EQ(
        cancellableResult.report->successfulFileCount,
        compatibleReport.successfulFileCount);
    EXPECT_EQ(
        cancellableResult.report->failures.size(),
        compatibleReport.failures.size());
    ASSERT_EQ(
        cancellableResult.report->duplicateGroups.size(),
        compatibleReport.duplicateGroups.size());
    EXPECT_EQ(
        cancellableResult.report->duplicateGroups.front().filePaths,
        compatibleReport.duplicateGroups.front().filePaths);
}

}
}
