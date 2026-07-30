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
    ScanResult result = scanDirectory(
        rootPath,
        scanOptions,
        progressCallback,
        {});
    return std::move(result.report).value();
}

ScanResult scanDirectory(
    const std::filesystem::path& rootPath,
    const ScanOptions& scanOptions,
    const ScanProgressCallback& progressCallback,
    std::stop_token stopToken) {
    if (stopToken.stop_requested()) {
        return {ScanStatus::Cancelled, std::nullopt};
    }

    FileDiscoveryResult discoveryResult =
        discoverFiles(rootPath, progressCallback, stopToken);
    if (discoveryResult.isCancelled) {
        return {ScanStatus::Cancelled, std::nullopt};
    }

    ContentFingerprintResult fingerprintResult =
        fingerprintFileCandidates(
            discoveryResult.filePaths,
            scanOptions,
            progressCallback,
            stopToken);
    if (fingerprintResult.isCancelled) {
        return {ScanStatus::Cancelled, std::nullopt};
    }

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
    report.totalLogicalBytes = fingerprintResult.totalLogicalBytes;
    report.candidateFileCount = fingerprintResult.candidateFileCount;
    report.candidateBytes = fingerprintResult.candidateBytes;
    report.hashedFileCount = fingerprintResult.hashedFileCount;
    if (progressCallback) {
        progressCallback({
            ScanProgressStage::Grouping,
            0U,
            1U,
            0U,
            0U,
            false});
    }
    if (stopToken.stop_requested()) {
        return {ScanStatus::Cancelled, std::nullopt};
    }

    DuplicateGroupingResult groupingResult =
        findDuplicateGroups(fingerprintResult.fileRecords, stopToken);
    if (groupingResult.isCancelled) {
        return {ScanStatus::Cancelled, std::nullopt};
    }
    report.duplicateGroups = std::move(groupingResult.groups);
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
    if (stopToken.stop_requested()) {
        return {ScanStatus::Cancelled, std::nullopt};
    }
    return {ScanStatus::Completed, std::move(report)};
}

}
