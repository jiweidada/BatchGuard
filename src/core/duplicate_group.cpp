#include "batchguard/core/duplicate_group.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace batchguard {
namespace {

using ContentKey = std::pair<std::uintmax_t, std::string>;

bool isPathBefore(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
    return left.native() < right.native();
}

bool isGroupBefore(const DuplicateGroup& left, const DuplicateGroup& right) {
    if (left.fileSize != right.fileSize) {
        return left.fileSize < right.fileSize;
    }
    if (left.sha256 != right.sha256) {
        return left.sha256 < right.sha256;
    }
    return isPathBefore(left.filePaths.front(), right.filePaths.front());
}

}

std::vector<DuplicateGroup> findDuplicateGroups(
    const std::vector<FileRecord>& fileRecords) {
    std::map<ContentKey, std::vector<std::filesystem::path>> pathsByContent;
    for (const FileRecord& record : fileRecords) {
        if (!record.sha256.has_value()) {
            continue;
        }
        pathsByContent[{record.fileSize, *record.sha256}].push_back(record.path);
    }

    std::vector<DuplicateGroup> groups;
    for (auto& [contentKey, filePaths] : pathsByContent) {
        if (filePaths.size() < 2U) {
            continue;
        }

        std::sort(filePaths.begin(), filePaths.end(), isPathBefore);
        groups.push_back({
            contentKey.first,
            contentKey.second,
            std::move(filePaths)});
    }

    std::sort(groups.begin(), groups.end(), isGroupBefore);
    return groups;
}

}
