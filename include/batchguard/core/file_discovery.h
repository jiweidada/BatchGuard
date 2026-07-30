#pragma once

#include "batchguard/core/file_failure.h"
#include "batchguard/core/scan_progress.h"

#include <filesystem>
#include <stop_token>
#include <vector>

namespace batchguard {

// 保存一次文件发现产生的普通文件路径和失败记录。
struct FileDiscoveryResult {
    std::vector<std::filesystem::path> filePaths;
    std::vector<FileFailure> failures;
    bool isCancelled{};
};

// 递归发现 `rootPath` 下的普通文件，并返回按路径稳定排序的结果。
// 此函数不跟随符号链接，不读取文件内容，也不修改输入目录。
[[nodiscard]] FileDiscoveryResult discoverFiles(
    const std::filesystem::path& rootPath);

// 递归发现普通文件，并同步报告累计发现数量。
[[nodiscard]] FileDiscoveryResult discoverFiles(
    const std::filesystem::path& rootPath,
    const ScanProgressCallback& progressCallback);

// 递归发现普通文件，并在目录枚举和单个条目处理之间响应停止请求。
[[nodiscard]] FileDiscoveryResult discoverFiles(
    const std::filesystem::path& rootPath,
    const ScanProgressCallback& progressCallback,
    std::stop_token stopToken);

}
