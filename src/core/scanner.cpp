#include "batchguard/core/scanner.h"

#include "batchguard/core/content_fingerprint.h"
#include "batchguard/core/duplicate_group.h"
#include "batchguard/core/file_discovery.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

namespace batchguard {
namespace {

bool isFailureBefore(const FileFailure& left, const FileFailure& right) {
    if (left.path.native() != right.path.native()) {
        return left.path.native() < right.path.native();
    }
    if (left.stage != right.stage) {
        return left.stage < right.stage;
    }

    const std::string_view leftCategory = left.errorCode.category().name();
    const std::string_view rightCategory = right.errorCode.category().name();
    if (leftCategory != rightCategory) {
        return leftCategory < rightCategory;
    }
    return left.errorCode.value() < right.errorCode.value();
}

}

ScanReport scanDirectory(const std::filesystem::path& rootPath) {
    return scanDirectory(rootPath, ScanOptions{}, {});
}

ScanReport scanDirectory(
    const std::filesystem::path& rootPath,
    const ScanProgressCallback& progressCallback) {
    return scanDirectory(rootPath, ScanOptions{}, progressCallback);
}

ScanReport scanDirectory(
    const std::filesystem::path& rootPath,
    const ScanOptions& scanOptions,
    const ScanProgressCallback& progressCallback) {
    FileDiscoveryResult discoveryResult =
        discoverFiles(rootPath, progressCallback);
    ContentFingerprintResult fingerprintResult =
        fingerprintFileCandidates(
            discoveryResult.filePaths,
            scanOptions,
            progressCallback);

    const std::size_t hashingFailureCount = static_cast<std::size_t>(std::count_if(
        fingerprintResult.failures.begin(),
        fingerprintResult.failures.end(),
        [](const FileFailure& failure) {
            return failure.stage == FailureStage::Hashing;
        }));

    ScanReport report;
    report.rootPath = rootPath;
    report.discoveredFileCount = discoveryResult.filePaths.size();
    report.successfulFileCount =
        fingerprintResult.fileRecords.size() - hashingFailureCount;
    if (progressCallback) {
        progressCallback({
            ScanProgressStage::Grouping,
            0U,
            1U,
            0U,
            0U,
            false});
    }
    report.duplicateGroups = findDuplicateGroups(fingerprintResult.fileRecords);
    report.failures = std::move(discoveryResult.failures);
    report.failures.insert(
        report.failures.end(),
        std::make_move_iterator(fingerprintResult.failures.begin()),
        std::make_move_iterator(fingerprintResult.failures.end()));
    std::sort(report.failures.begin(), report.failures.end(), isFailureBefore);
    if (progressCallback) {
        progressCallback({
            ScanProgressStage::Grouping,
            1U,
            1U,
            0U,
            0U,
            true});
    }
    return report;
}

}
