#pragma once

#include "batchguard/core/file_failure.h"
#include "batchguard/core/scan_options.h"
#include "batchguard/core/scan_progress.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace batchguard {

// 保存文件元数据及按需计算的内容指纹。
// `sha256` 为空表示文件大小唯一而无需哈希，或对应路径存在哈希失败记录。
struct FileRecord {
    std::filesystem::path path;
    std::uintmax_t fileSize{};
    std::optional<std::string> sha256;
};

// 保存大小筛选和内容指纹阶段成功获得的文件记录及失败信息。
struct ContentFingerprintResult {
    std::vector<FileRecord> fileRecords;
    std::vector<FileFailure> failures;
    bool isCancelled{};
};

// 读取每个普通文件的大小，并只对至少有两个成员的同大小候选组计算 SHA-256。
// 单个路径失败不会终止其他文件；返回的记录和失败项均按路径稳定排序。
[[nodiscard]] ContentFingerprintResult fingerprintFileCandidates(
    const std::vector<std::filesystem::path>& filePaths);

// 执行大小筛选和候选哈希，并同步报告元数据及哈希阶段进度。
[[nodiscard]] ContentFingerprintResult fingerprintFileCandidates(
    const std::vector<std::filesystem::path>& filePaths,
    const ScanProgressCallback& progressCallback);

// 按扫描配置执行大小筛选和候选哈希，并在调用线程内同步报告进度。
[[nodiscard]] ContentFingerprintResult fingerprintFileCandidates(
    const std::vector<std::filesystem::path>& filePaths,
    const ScanOptions& scanOptions,
    const ScanProgressCallback& progressCallback);

// 按扫描配置执行可取消的元数据读取和候选哈希。
[[nodiscard]] ContentFingerprintResult fingerprintFileCandidates(
    const std::vector<std::filesystem::path>& filePaths,
    const ScanOptions& scanOptions,
    const ScanProgressCallback& progressCallback,
    std::stop_token stopToken);

}
