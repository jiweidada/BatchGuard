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
    return findDuplicateGroups(fileRecords, {}).groups;
}

DuplicateGroupingResult findDuplicateGroups(
    const std::vector<FileRecord>& fileRecords,
    std::stop_token stopToken) {
    DuplicateGroupingResult result;
    if (stopToken.stop_requested()) {
        result.isCancelled = true;
        return result;
    }

    std::map<ContentKey, std::vector<std::filesystem::path>> pathsByContent;
    for (const FileRecord& record : fileRecords) {
        if (stopToken.stop_requested()) {
            result.isCancelled = true;
            return result;
        }
        if (!record.sha256.has_value()) {
            continue;
        }
        pathsByContent[{record.fileSize, *record.sha256}].push_back(record.path);
    }

    for (auto& [contentKey, filePaths] : pathsByContent) {
        if (stopToken.stop_requested()) {
            result.isCancelled = true;
            return result;
        }
        if (filePaths.size() < 2U) {
            continue;
        }

        std::sort(filePaths.begin(), filePaths.end(), isPathBefore);
        if (stopToken.stop_requested()) {
            result.isCancelled = true;
            return result;
        }
        result.groups.push_back({
            contentKey.first,
            contentKey.second,
            std::move(filePaths)});
    }

    if (stopToken.stop_requested()) {
        result.isCancelled = true;
        return result;
    }
    std::sort(result.groups.begin(), result.groups.end(), isGroupBefore);
    result.isCancelled = stopToken.stop_requested();
    return result;
}

}
