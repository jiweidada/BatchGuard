#pragma once

#include "batchguard/core/content_fingerprint.h"

#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace batchguard {

// 保存一组内容相同的文件；摘要为小写十六进制，成员数量始终不少于两个。
struct DuplicateGroup {
    std::uintmax_t fileSize{};
    std::string sha256;
    std::vector<std::filesystem::path> filePaths;
};

// 保存可取消重复分组操作的结果。
struct DuplicateGroupingResult {
    std::vector<DuplicateGroup> groups;
    bool isCancelled{};
};

// 按“文件大小 + SHA-256”建立重复组，并忽略未计算指纹的文件记录。
// 组内路径和组列表均使用稳定顺序，文件名及所在目录不参与内容判断。
[[nodiscard]] std::vector<DuplicateGroup> findDuplicateGroups(
    const std::vector<FileRecord>& fileRecords);

// 建立重复组，并在记录归类及组装边界响应停止请求。
[[nodiscard]] DuplicateGroupingResult findDuplicateGroups(
    const std::vector<FileRecord>& fileRecords,
    std::stop_token stopToken);

}
