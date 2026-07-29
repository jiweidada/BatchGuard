#include "batchguard/core/content_fingerprint.h"

#include "file_hash_scheduler.h"

#include "batchguard/core/file_hasher.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <map>
#include <system_error>
#include <vector>

namespace batchguard {
namespace {

std::uintmax_t addWithoutOverflow(
    std::uintmax_t left,
    std::uintmax_t right) {
    const std::uintmax_t maximum = (std::numeric_limits<std::uintmax_t>::max)();
    return right > maximum - left ? maximum : left + right;
}

void notifyProgress(
    const ScanProgressCallback& progressCallback,
    ScanProgressStage stage,
    std::size_t completedItems,
    std::size_t totalItems,
    std::uintmax_t completedBytes,
    std::uintmax_t totalBytes,
    bool isStageComplete) {
    if (progressCallback) {
        progressCallback({
            stage,
            completedItems,
            totalItems,
            completedBytes,
            totalBytes,
            isStageComplete});
    }
}

bool isRecordBefore(const FileRecord& left, const FileRecord& right) {
    return left.path.native() < right.path.native();
}

bool isFailureBefore(const FileFailure& left, const FileFailure& right) {
    if (left.path.native() != right.path.native()) {
        return left.path.native() < right.path.native();
    }
    if (left.stage != right.stage) {
        return left.stage < right.stage;
    }
    return left.errorCode.value() < right.errorCode.value();
}

}

ContentFingerprintResult fingerprintFileCandidates(
    const std::vector<std::filesystem::path>& filePaths) {
    return fingerprintFileCandidates(filePaths, ScanOptions{}, {});
}

ContentFingerprintResult fingerprintFileCandidates(
    const std::vector<std::filesystem::path>& filePaths,
    const ScanProgressCallback& progressCallback) {
    return fingerprintFileCandidates(filePaths, ScanOptions{}, progressCallback);
}

ContentFingerprintResult fingerprintFileCandidates(
    const std::vector<std::filesystem::path>& filePaths,
    const ScanOptions& scanOptions,
    const ScanProgressCallback& progressCallback) {
    ContentFingerprintResult result;
    std::map<std::uintmax_t, std::vector<std::size_t>> recordIndicesBySize;
    std::size_t completedMetadataItems = 0;
    notifyProgress(
        progressCallback,
        ScanProgressStage::Metadata,
        0U,
        filePaths.size(),
        0U,
        0U,
        false);

    for (const std::filesystem::path& filePath : filePaths) {
        std::error_code errorCode;
        const std::uintmax_t fileSize = std::filesystem::file_size(filePath, errorCode);
        if (errorCode) {
            result.failures.push_back({
                filePath,
                FailureStage::Metadata,
                errorCode});
        } else {
            const std::size_t recordIndex = result.fileRecords.size();
            result.fileRecords.push_back({filePath, fileSize, std::nullopt});
            recordIndicesBySize[fileSize].push_back(recordIndex);
        }

        ++completedMetadataItems;
        notifyProgress(
            progressCallback,
            ScanProgressStage::Metadata,
            completedMetadataItems,
            filePaths.size(),
            0U,
            0U,
            false);
    }
    notifyProgress(
        progressCallback,
        ScanProgressStage::Metadata,
        completedMetadataItems,
        filePaths.size(),
        0U,
        0U,
        true);

    std::size_t totalHashItems = 0;
    std::uintmax_t totalHashBytes = 0;
    std::vector<FileHashTask> hashTasks;
    for (const auto& sizeGroup : recordIndicesBySize) {
        const std::vector<std::size_t>& recordIndices = sizeGroup.second;
        if (recordIndices.size() < 2U) {
            continue;
        }
        totalHashItems += recordIndices.size();
        for (const std::size_t recordIndex : recordIndices) {
            totalHashBytes = addWithoutOverflow(
                totalHashBytes,
                result.fileRecords[recordIndex].fileSize);
            hashTasks.push_back({
                recordIndex,
                result.fileRecords[recordIndex].path,
                result.fileRecords[recordIndex].fileSize});
        }
    }

    notifyProgress(
        progressCallback,
        ScanProgressStage::Hashing,
        0U,
        totalHashItems,
        0U,
        totalHashBytes,
        false);

    const std::vector<ScheduledFileHashResult> hashResults =
        hashFilesConcurrently(
            hashTasks,
            scanOptions.hashWorkerCount,
            totalHashBytes,
            [&](std::size_t completedItems, std::uintmax_t completedBytes) {
                notifyProgress(
                    progressCallback,
                    ScanProgressStage::Hashing,
                    completedItems,
                    totalHashItems,
                    completedBytes,
                    totalHashBytes,
                    false);
            },
            [](const std::filesystem::path& filePath,
               const FileHashProgressCallback& fileProgressCallback) {
                return calculateFileSha256(filePath, fileProgressCallback);
            });
    for (const ScheduledFileHashResult& scheduledResult : hashResults) {
        FileRecord& record = result.fileRecords[scheduledResult.recordIndex];
        if (!scheduledResult.hashResult.isSuccess()) {
            result.failures.push_back({
                record.path,
                FailureStage::Hashing,
                scheduledResult.hashResult.errorCode});
        } else {
            record.sha256 = scheduledResult.hashResult.sha256;
        }
    }
    notifyProgress(
        progressCallback,
        ScanProgressStage::Hashing,
        totalHashItems,
        totalHashItems,
        totalHashBytes,
        totalHashBytes,
        true);

    std::sort(
        result.fileRecords.begin(),
        result.fileRecords.end(),
        isRecordBefore);
    std::sort(
        result.failures.begin(),
        result.failures.end(),
        isFailureBefore);
    return result;
}

}
