#include "batchguard/core/file_discovery.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <vector>

namespace batchguard {
namespace {

void notifyProgress(
    const ScanProgressCallback& progressCallback,
    std::size_t completedItems,
    bool isStageComplete) {
    if (progressCallback) {
        progressCallback({
            ScanProgressStage::Discovery,
            completedItems,
            0U,
            0U,
            0U,
            isStageComplete});
    }
}

void addDiscoveryFailure(
    FileDiscoveryResult& result,
    const std::filesystem::path& path,
    const std::error_code& errorCode) {
    result.failures.push_back({path, FailureStage::Discovery, errorCode});
}

bool isPathBefore(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
    return left.native() < right.native();
}

bool isFailureBefore(const FileFailure& left, const FileFailure& right) {
    if (left.path.native() != right.path.native()) {
        return left.path.native() < right.path.native();
    }
    return left.errorCode.value() < right.errorCode.value();
}

}

FileDiscoveryResult discoverFiles(const std::filesystem::path& rootPath) {
    return discoverFiles(rootPath, {}, {});
}

FileDiscoveryResult discoverFiles(
    const std::filesystem::path& rootPath,
    const ScanProgressCallback& progressCallback) {
    return discoverFiles(rootPath, progressCallback, {});
}

FileDiscoveryResult discoverFiles(
    const std::filesystem::path& rootPath,
    const ScanProgressCallback& progressCallback,
    std::stop_token stopToken) {
    FileDiscoveryResult result;
    if (stopToken.stop_requested()) {
        result.isCancelled = true;
        return result;
    }

    std::vector<std::filesystem::path> pendingDirectories{rootPath};
    notifyProgress(progressCallback, 0U, false);

    while (!pendingDirectories.empty()) {
        if (stopToken.stop_requested()) {
            result.isCancelled = true;
            break;
        }

        const std::filesystem::path currentDirectory = pendingDirectories.back();
        pendingDirectories.pop_back();

        std::error_code errorCode;
        std::filesystem::directory_iterator iterator{
            currentDirectory,
            std::filesystem::directory_options::none,
            errorCode};
        if (errorCode) {
            addDiscoveryFailure(result, currentDirectory, errorCode);
            continue;
        }

        const std::filesystem::directory_iterator end;
        while (iterator != end) {
            if (stopToken.stop_requested()) {
                result.isCancelled = true;
                break;
            }

            const std::filesystem::path entryPath = iterator->path();
            const std::filesystem::file_status status = iterator->symlink_status(errorCode);
            if (errorCode) {
                addDiscoveryFailure(result, entryPath, errorCode);
                errorCode.clear();
            } else if (std::filesystem::is_symlink(status)) {
                // 跳过所有符号链接，避免目录循环和同一文件被别名重复发现。
            } else if (std::filesystem::is_directory(status)) {
                pendingDirectories.push_back(entryPath);
            } else if (std::filesystem::is_regular_file(status)) {
                result.filePaths.push_back(entryPath);
                notifyProgress(progressCallback, result.filePaths.size(), false);
                if (stopToken.stop_requested()) {
                    result.isCancelled = true;
                    break;
                }
            }

            iterator.increment(errorCode);
            if (errorCode) {
                // 当前目录无法继续枚举时记录失败，其他待处理目录仍会继续。
                addDiscoveryFailure(result, currentDirectory, errorCode);
                break;
            }
        }
        if (result.isCancelled) {
            break;
        }
    }

    std::sort(result.filePaths.begin(), result.filePaths.end(), isPathBefore);
    std::sort(result.failures.begin(), result.failures.end(), isFailureBefore);
    if (!result.isCancelled) {
        notifyProgress(progressCallback, result.filePaths.size(), true);
        result.isCancelled = stopToken.stop_requested();
    }
    return result;
}

}
