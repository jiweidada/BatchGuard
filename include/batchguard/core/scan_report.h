#pragma once

#include "batchguard/core/duplicate_group.h"
#include "batchguard/core/file_failure.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace batchguard {

// 保存一次目录扫描的完整核心结果，供 CLI 或未来 GUI 统一展示。
// 成功数量包含无需哈希的唯一大小文件，不包含元数据或哈希失败的已发现文件。
struct ScanReport {
    std::filesystem::path rootPath;
    std::size_t discoveredFileCount{};
    std::size_t successfulFileCount{};
    std::uintmax_t totalLogicalBytes{};
    std::size_t candidateFileCount{};
    std::uintmax_t candidateBytes{};
    std::size_t hashedFileCount{};
    std::vector<FileFailure> failures;
    std::vector<DuplicateGroup> duplicateGroups;

    // 没有任何发现、元数据或哈希失败时，扫描结果完整。
    [[nodiscard]] bool isComplete() const noexcept {
        return failures.empty();
    }
};

}
