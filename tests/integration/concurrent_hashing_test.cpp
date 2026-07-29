#include "batchguard/core/content_fingerprint.h"
#include "batchguard/core/scan_options.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
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

TEST(ConcurrentHashingTest, FourWorkersProduceTheSameResultsAsOneWorker) {
    const test_support::TemporaryDirectory temporaryDirectory;
    std::vector<std::filesystem::path> filePaths;
    for (std::size_t index = 0; index < 12U; ++index) {
        const std::filesystem::path filePath =
            temporaryDirectory.path() / ("candidate-" + std::to_string(index) + ".bin");
        createBinaryFile(
            filePath,
            index % 3U == 0U ? "same-a" :
            index % 3U == 1U ? "same-b" :
                              "same-c");
        filePaths.push_back(filePath);
    }

    const ContentFingerprintResult serialResult =
        fingerprintFileCandidates(filePaths, ScanOptions{1U}, {});
    const ContentFingerprintResult concurrentResult =
        fingerprintFileCandidates(filePaths, ScanOptions{4U}, {});

    ASSERT_EQ(concurrentResult.fileRecords.size(), serialResult.fileRecords.size());
    EXPECT_EQ(concurrentResult.failures.size(), serialResult.failures.size());
    for (std::size_t index = 0; index < serialResult.fileRecords.size(); ++index) {
        EXPECT_EQ(
            concurrentResult.fileRecords[index].path,
            serialResult.fileRecords[index].path);
        EXPECT_EQ(
            concurrentResult.fileRecords[index].fileSize,
            serialResult.fileRecords[index].fileSize);
        EXPECT_EQ(
            concurrentResult.fileRecords[index].sha256,
            serialResult.fileRecords[index].sha256);
    }
}

TEST(ConcurrentHashingTest, ProgressIsMonotonicAndRunsOnCallingThread) {
    const test_support::TemporaryDirectory temporaryDirectory;
    constexpr std::size_t kFileCount = 8U;
    constexpr std::size_t kFileSize = 200'000U;
    std::vector<std::filesystem::path> filePaths;
    for (std::size_t index = 0; index < kFileCount; ++index) {
        const std::filesystem::path filePath =
            temporaryDirectory.path() / ("progress-" + std::to_string(index) + ".bin");
        createBinaryFile(filePath, std::string(kFileSize, 'a'));
        filePaths.push_back(filePath);
    }

    const std::thread::id callingThreadId = std::this_thread::get_id();
    std::vector<ScanProgress> hashingEvents;
    std::vector<std::thread::id> callbackThreadIds;
    const ContentFingerprintResult result = fingerprintFileCandidates(
        filePaths,
        ScanOptions{4U},
        [&](const ScanProgress& progress) {
            if (progress.stage == ScanProgressStage::Hashing) {
                hashingEvents.push_back(progress);
                callbackThreadIds.push_back(std::this_thread::get_id());
            }
        });

    ASSERT_TRUE(result.failures.empty());
    ASSERT_FALSE(hashingEvents.empty());
    for (std::size_t index = 1; index < hashingEvents.size(); ++index) {
        EXPECT_LE(
            hashingEvents[index - 1U].completedItems,
            hashingEvents[index].completedItems);
        EXPECT_LE(
            hashingEvents[index - 1U].completedBytes,
            hashingEvents[index].completedBytes);
        EXPECT_LE(
            hashingEvents[index].completedItems,
            hashingEvents[index].totalItems);
        EXPECT_LE(
            hashingEvents[index].completedBytes,
            hashingEvents[index].totalBytes);
    }
    EXPECT_TRUE(hashingEvents.back().isStageComplete);
    EXPECT_EQ(hashingEvents.back().completedItems, kFileCount);
    EXPECT_EQ(hashingEvents.back().completedBytes, kFileCount * kFileSize);
    EXPECT_TRUE(std::all_of(
        callbackThreadIds.begin(),
        callbackThreadIds.end(),
        [callingThreadId](std::thread::id threadId) {
            return threadId == callingThreadId;
        }));
}

TEST(ConcurrentHashingTest, HashFailureDoesNotStopOtherWorkers) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path lockedFile = temporaryDirectory.path() / "locked.bin";
    const std::filesystem::path firstReadableFile =
        temporaryDirectory.path() / "readable-a.bin";
    const std::filesystem::path secondReadableFile =
        temporaryDirectory.path() / "readable-b.bin";
    createBinaryFile(lockedFile, "locked");
    createBinaryFile(firstReadableFile, "public");
    createBinaryFile(secondReadableFile, "second");
    const HANDLE lockedHandle = CreateFileW(
        lockedFile.c_str(),
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    ASSERT_NE(lockedHandle, INVALID_HANDLE_VALUE);

    const ContentFingerprintResult result = fingerprintFileCandidates(
        {lockedFile, firstReadableFile, secondReadableFile},
        ScanOptions{3U},
        {});
    CloseHandle(lockedHandle);

    ASSERT_EQ(result.fileRecords.size(), 3U);
    ASSERT_EQ(result.failures.size(), 1U);
    EXPECT_EQ(result.failures.front().path, lockedFile);
    EXPECT_EQ(result.failures.front().stage, FailureStage::Hashing);
    const auto readableRecord = std::find_if(
        result.fileRecords.begin(),
        result.fileRecords.end(),
        [&firstReadableFile](const FileRecord& record) {
            return record.path == firstReadableFile;
        });
    ASSERT_NE(readableRecord, result.fileRecords.end());
    EXPECT_TRUE(readableRecord->sha256.has_value());
}

}
}
