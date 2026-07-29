#include "file_hash_scheduler.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace batchguard {
namespace {

TEST(FileHashSchedulerTest, RunsTasksOnMultipleWorkerThreads) {
    constexpr std::size_t kTaskCount = 4U;
    std::vector<FileHashTask> tasks;
    for (std::size_t index = 0; index < kTaskCount; ++index) {
        tasks.push_back({
            index,
            std::filesystem::path{"file-" + std::to_string(index)},
            1U});
    }

    std::mutex probeMutex;
    std::condition_variable probeCondition;
    std::set<std::thread::id> workerThreadIds;
    std::size_t activeWorkerCount = 0U;
    std::size_t maximumActiveWorkerCount = 0U;
    bool hasObservedConcurrency = false;
    bool hasTimedOut = false;
    const FileHashOperation hashOperation =
        [&](const std::filesystem::path&, const FileHashProgressCallback& progressCallback) {
            {
                std::unique_lock lock{probeMutex};
                workerThreadIds.insert(std::this_thread::get_id());
                ++activeWorkerCount;
                maximumActiveWorkerCount =
                    (std::max)(maximumActiveWorkerCount, activeWorkerCount);
                if (activeWorkerCount >= 2U) {
                    hasObservedConcurrency = true;
                    probeCondition.notify_all();
                } else if (!probeCondition.wait_for(
                               lock,
                               std::chrono::seconds{2},
                               [&hasObservedConcurrency]() {
                                   return hasObservedConcurrency;
                               })) {
                    hasTimedOut = true;
                }
            }

            progressCallback(1U);

            {
                const std::lock_guard lock{probeMutex};
                --activeWorkerCount;
            }
            return FileHashResult{"hash", {}};
        };

    std::vector<std::size_t> completedItems;
    std::vector<std::uintmax_t> completedBytes;
    const std::thread::id callerThreadId = std::this_thread::get_id();
    std::vector<std::thread::id> progressThreadIds;
    const std::vector<ScheduledFileHashResult> results = hashFilesConcurrently(
        tasks,
        4U,
        kTaskCount,
        [&](std::size_t itemCount, std::uintmax_t byteCount) {
            completedItems.push_back(itemCount);
            completedBytes.push_back(byteCount);
            progressThreadIds.push_back(std::this_thread::get_id());
        },
        hashOperation);

    EXPECT_FALSE(hasTimedOut);
    EXPECT_TRUE(hasObservedConcurrency);
    EXPECT_GE(workerThreadIds.size(), 2U);
    EXPECT_GE(maximumActiveWorkerCount, 2U);
    ASSERT_EQ(results.size(), kTaskCount);
    ASSERT_FALSE(completedItems.empty());
    ASSERT_EQ(completedItems.size(), completedBytes.size());
    EXPECT_TRUE(std::is_sorted(completedItems.begin(), completedItems.end()));
    EXPECT_TRUE(std::is_sorted(completedBytes.begin(), completedBytes.end()));
    EXPECT_EQ(completedItems.back(), kTaskCount);
    EXPECT_EQ(completedBytes.back(), kTaskCount);
    EXPECT_TRUE(std::all_of(
        progressThreadIds.begin(),
        progressThreadIds.end(),
        [callerThreadId](std::thread::id threadId) {
            return threadId == callerThreadId;
        }));
}

TEST(FileHashSchedulerTest, WorkerCountOneRunsTasksSerially) {
    const std::vector<FileHashTask> tasks{
        {0U, "first", 1U},
        {1U, "second", 1U},
        {2U, "third", 1U}};
    std::mutex probeMutex;
    std::set<std::thread::id> workerThreadIds;
    std::size_t activeWorkerCount = 0U;
    std::size_t maximumActiveWorkerCount = 0U;
    const FileHashOperation hashOperation =
        [&](const std::filesystem::path&, const FileHashProgressCallback& progressCallback) {
            {
                const std::lock_guard lock{probeMutex};
                workerThreadIds.insert(std::this_thread::get_id());
                ++activeWorkerCount;
                maximumActiveWorkerCount =
                    (std::max)(maximumActiveWorkerCount, activeWorkerCount);
            }
            progressCallback(1U);
            {
                const std::lock_guard lock{probeMutex};
                --activeWorkerCount;
            }
            return FileHashResult{"hash", {}};
        };

    const std::vector<ScheduledFileHashResult> results = hashFilesConcurrently(
        tasks,
        1U,
        3U,
        {},
        hashOperation);

    EXPECT_EQ(results.size(), tasks.size());
    EXPECT_EQ(workerThreadIds.size(), 1U);
    EXPECT_EQ(maximumActiveWorkerCount, 1U);
}

TEST(FileHashSchedulerTest, EmptyTaskListDoesNotInvokeHashOrProgressCallbacks) {
    bool wasHashCalled = false;
    bool wasProgressCalled = false;
    const std::vector<ScheduledFileHashResult> results = hashFilesConcurrently(
        {},
        4U,
        0U,
        [&wasProgressCalled](std::size_t, std::uintmax_t) {
            wasProgressCalled = true;
        },
        [&wasHashCalled](
            const std::filesystem::path&,
            const FileHashProgressCallback&) {
            wasHashCalled = true;
            return FileHashResult{"hash", {}};
        });

    EXPECT_TRUE(results.empty());
    EXPECT_FALSE(wasHashCalled);
    EXPECT_FALSE(wasProgressCalled);
}

TEST(FileHashSchedulerTest, WorkerCountIsLimitedByTaskCount) {
    const std::vector<FileHashTask> tasks{
        {0U, "first", 1U},
        {1U, "second", 1U}};
    std::mutex threadIdsMutex;
    std::set<std::thread::id> workerThreadIds;
    const std::vector<ScheduledFileHashResult> results = hashFilesConcurrently(
        tasks,
        64U,
        2U,
        {},
        [&threadIdsMutex, &workerThreadIds](
            const std::filesystem::path&,
            const FileHashProgressCallback& progressCallback) {
            {
                const std::lock_guard lock{threadIdsMutex};
                workerThreadIds.insert(std::this_thread::get_id());
            }
            progressCallback(1U);
            return FileHashResult{"hash", {}};
        });

    EXPECT_EQ(results.size(), tasks.size());
    EXPECT_LE(workerThreadIds.size(), tasks.size());
}

}
}
