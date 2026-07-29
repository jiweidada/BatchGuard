#include "batchguard/core/content_fingerprint.h"

#include "batchguard/core/file_hasher.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <system_error>
#include <vector>

namespace batchguard {
namespace {

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
    ContentFingerprintResult result;
    std::map<std::uintmax_t, std::vector<std::size_t>> recordIndicesBySize;

    for (const std::filesystem::path& filePath : filePaths) {
        std::error_code errorCode;
        const std::uintmax_t fileSize = std::filesystem::file_size(filePath, errorCode);
        if (errorCode) {
            result.failures.push_back({
                filePath,
                FailureStage::Metadata,
                errorCode});
            continue;
        }

        const std::size_t recordIndex = result.fileRecords.size();
        result.fileRecords.push_back({filePath, fileSize, std::nullopt});
        recordIndicesBySize[fileSize].push_back(recordIndex);
    }

    for (const auto& sizeGroup : recordIndicesBySize) {
        const std::vector<std::size_t>& recordIndices = sizeGroup.second;
        if (recordIndices.size() < 2U) {
            continue;
        }

        for (const std::size_t recordIndex : recordIndices) {
            FileRecord& record = result.fileRecords[recordIndex];
            const FileHashResult hashResult = calculateFileSha256(record.path);
            if (!hashResult.isSuccess()) {
                result.failures.push_back({
                    record.path,
                    FailureStage::Hashing,
                    hashResult.errorCode});
                continue;
            }
            record.sha256 = hashResult.sha256;
        }
    }

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
