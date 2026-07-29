#include "file_hash_scheduler.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <limits>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace batchguard {
namespace {

constexpr std::size_t kMaximumAutomaticWorkerCount = 4U;

struct SchedulerState {
    std::mutex mutex;
    std::condition_variable condition;
    std::size_t completedItems{};
    std::uintmax_t completedBytes{};
    std::size_t finishedWorkers{};
    std::size_t updateVersion{};
    std::exception_ptr exception;
};

std::uintmax_t addWithoutOverflow(
    std::uintmax_t left,
    std::uintmax_t right) noexcept {
    const std::uintmax_t maximum = (std::numeric_limits<std::uintmax_t>::max)();
    return right > maximum - left ? maximum : left + right;
}

std::size_t resolveWorkerCount(
    std::size_t requestedWorkerCount,
    std::size_t taskCount) noexcept {
    if (taskCount == 0U) {
        return 0U;
    }

    if (requestedWorkerCount == 0U) {
        requestedWorkerCount = std::thread::hardware_concurrency();
        if (requestedWorkerCount == 0U) {
            requestedWorkerCount = 1U;
        }
        requestedWorkerCount =
            (std::min)(requestedWorkerCount, kMaximumAutomaticWorkerCount);
    }
    return (std::max)(
        std::size_t{1U},
        (std::min)(requestedWorkerCount, taskCount));
}

void recordProgress(
    SchedulerState& state,
    std::uintmax_t additionalBytes,
    std::uintmax_t totalBytes,
    bool isItemComplete) {
    {
        const std::lock_guard lock{state.mutex};
        state.completedBytes = (std::min)(
            totalBytes,
            addWithoutOverflow(state.completedBytes, additionalBytes));
        if (isItemComplete) {
            ++state.completedItems;
        }
        ++state.updateVersion;
    }
    state.condition.notify_one();
}

}

std::vector<ScheduledFileHashResult> hashFilesConcurrently(
    const std::vector<FileHashTask>& tasks,
    std::size_t requestedWorkerCount,
    std::uintmax_t totalBytes,
    const HashBatchProgressCallback& progressCallback,
    const FileHashOperation& hashOperation) {
    if (tasks.empty()) {
        return {};
    }

    const std::size_t workerCount =
        resolveWorkerCount(requestedWorkerCount, tasks.size());
    std::vector<ScheduledFileHashResult> results(tasks.size());
    for (std::size_t index = 0; index < tasks.size(); ++index) {
        results[index].recordIndex = tasks[index].recordIndex;
    }

    SchedulerState state;
    std::atomic_size_t nextTaskIndex{0U};
    std::stop_source stopSource;
    std::vector<std::jthread> workers;
    workers.reserve(workerCount);

    try {
        for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
            workers.emplace_back([&]() {
                try {
                    while (!stopSource.stop_requested()) {
                        const std::size_t taskIndex =
                            nextTaskIndex.fetch_add(1U, std::memory_order_relaxed);
                        if (taskIndex >= tasks.size()) {
                            break;
                        }

                        const FileHashTask& task = tasks[taskIndex];
                        std::uintmax_t reportedFileBytes = 0U;
                        results[taskIndex].hashResult = hashOperation(
                            task.filePath,
                            [&](std::uintmax_t currentFileBytes) {
                                if (currentFileBytes <= reportedFileBytes) {
                                    return;
                                }
                                const std::uintmax_t additionalBytes =
                                    currentFileBytes - reportedFileBytes;
                                reportedFileBytes = currentFileBytes;
                                recordProgress(
                                    state,
                                    additionalBytes,
                                    totalBytes,
                                    false);
                            });

                        const std::uintmax_t remainingBytes =
                            task.fileSize > reportedFileBytes
                                ? task.fileSize - reportedFileBytes
                                : 0U;
                        recordProgress(
                            state,
                            remainingBytes,
                            totalBytes,
                            true);
                    }
                } catch (...) {
                    {
                        const std::lock_guard lock{state.mutex};
                        if (!state.exception) {
                            state.exception = std::current_exception();
                        }
                        ++state.updateVersion;
                    }
                    stopSource.request_stop();
                    state.condition.notify_one();
                }

                {
                    const std::lock_guard lock{state.mutex};
                    ++state.finishedWorkers;
                    ++state.updateVersion;
                }
                state.condition.notify_one();
            });
        }
    } catch (...) {
        stopSource.request_stop();
        workers.clear();
        throw;
    }

    std::size_t observedVersion = 0U;
    std::size_t reportedItems = 0U;
    std::uintmax_t reportedBytes = 0U;
    while (true) {
        std::size_t completedItems = 0U;
        std::uintmax_t completedBytes = 0U;
        bool haveAllWorkersFinished = false;
        {
            std::unique_lock lock{state.mutex};
            state.condition.wait(lock, [&]() {
                return state.updateVersion != observedVersion ||
                    state.finishedWorkers == workerCount;
            });
            observedVersion = state.updateVersion;
            completedItems = state.completedItems;
            completedBytes = state.completedBytes;
            haveAllWorkersFinished = state.finishedWorkers == workerCount;
        }

        const bool hasProgressChanged =
            completedItems != reportedItems || completedBytes != reportedBytes;
        if (progressCallback && hasProgressChanged) {
            try {
                progressCallback(completedItems, completedBytes);
                reportedItems = completedItems;
                reportedBytes = completedBytes;
            } catch (...) {
                stopSource.request_stop();
                throw;
            }
        }
        if (haveAllWorkersFinished) {
            break;
        }
    }

    workers.clear();
    if (state.exception) {
        std::rethrow_exception(state.exception);
    }
    return results;
}

}
