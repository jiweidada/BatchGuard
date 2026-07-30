#pragma once

#include "batchguard/core/scan_options.h"
#include "batchguard/core/scan_progress.h"
#include "batchguard/core/scan_report.h"
#include "batchguard/core/scan_result.h"

#include <filesystem>
#include <stop_token>

namespace batchguard {

// 对已经通过输入验证的目录执行发现、元数据读取、候选哈希和重复分组。
// 预期文件系统失败进入返回报告；函数不会修改扫描目录中的任何内容。
[[nodiscard]] ScanReport scanDirectory(
    const std::filesystem::path& rootPath);

// 执行完整扫描，并在调用线程内同步报告各处理阶段的进度。
[[nodiscard]] ScanReport scanDirectory(
    const std::filesystem::path& rootPath,
    const ScanProgressCallback& progressCallback);

// 按指定性能配置执行完整扫描，并在调用线程内同步报告各阶段进度。
[[nodiscard]] ScanReport scanDirectory(
    const std::filesystem::path& rootPath,
    const ScanOptions& scanOptions,
    const ScanProgressCallback& progressCallback);

// 按指定配置执行可取消扫描。只有完整结束时，返回结果才包含 `ScanReport`。
// 停止请求属于独立终态，不会伪装成文件失败或不完整报告。
[[nodiscard]] ScanResult scanDirectory(
    const std::filesystem::path& rootPath,
    const ScanOptions& scanOptions,
    const ScanProgressCallback& progressCallback,
    std::stop_token stopToken);

}
