#pragma once

#include "batchguard/core/file_hasher.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace batchguard {

// 描述一个可由哈希工作线程独立处理的文件任务。
struct FileHashTask {
    std::size_t recordIndex{};
    std::filesystem::path filePath;
    std::uintmax_t fileSize{};
};

// 保存一个哈希任务的目标记录位置和执行结果。
struct ScheduledFileHashResult {
    std::size_t recordIndex{};
    FileHashResult hashResult;
};

// 接收整个候选集合已经完成的文件数和累计字节数。
using HashBatchProgressCallback =
    std::function<void(std::size_t, std::uintmax_t)>;

// 抽象单文件哈希操作，使调度策略能够通过确定性的假操作单独测试。
using FileHashOperation = std::function<FileHashResult(
    const std::filesystem::path&,
    const FileHashProgressCallback&)>;

// 使用有界工作线程并发执行文件哈希，并在调用线程内串行报告汇总进度。
// `requestedWorkerCount` 为零时自动选择线程数；返回结果顺序与任务顺序一致。
[[nodiscard]] std::vector<ScheduledFileHashResult> hashFilesConcurrently(
    const std::vector<FileHashTask>& tasks,
    std::size_t requestedWorkerCount,
    std::uintmax_t totalBytes,
    const HashBatchProgressCallback& progressCallback,
    const FileHashOperation& hashOperation);

}
