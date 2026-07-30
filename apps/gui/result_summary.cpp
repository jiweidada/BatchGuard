#include "result_summary.h"

#include <iterator>
#include <limits>

namespace batchguard::gui {
namespace {

std::uintmax_t addWithoutOverflow(
    std::uintmax_t left,
    std::uintmax_t right) {
    const std::uintmax_t maximum = (std::numeric_limits<std::uintmax_t>::max)();
    return right > maximum - left ? maximum : left + right;
}

std::uintmax_t multiplyWithoutOverflow(
    std::uintmax_t value,
    std::size_t multiplier) {
    const std::uintmax_t maximum = (std::numeric_limits<std::uintmax_t>::max)();
    if (value == 0U || multiplier == 0U) {
        return 0U;
    }
    return multiplier > maximum / value ? maximum : value * multiplier;
}

std::size_t addCountWithoutOverflow(
    std::size_t left,
    std::size_t right) {
    const std::size_t maximum = (std::numeric_limits<std::size_t>::max)();
    return right > maximum - left ? maximum : left + right;
}

}

ResultSummary calculateResultSummary(const ScanReport& report) {
    ResultSummary summary;
    summary.scannedFileCount = report.discoveredFileCount;
    summary.successfulFileCount = report.successfulFileCount;
    summary.duplicateGroupCount = report.duplicateGroups.size();
    summary.failureCount = report.failures.size();
    summary.totalLogicalBytes = report.totalLogicalBytes;
    summary.candidateFileCount = report.candidateFileCount;
    summary.candidateBytes = report.candidateBytes;
    summary.hashedFileCount = report.hashedFileCount;

    for (const DuplicateGroup& group : report.duplicateGroups) {
        const std::size_t fileCount = group.filePaths.size();
        const std::size_t redundantCount =
            fileCount > 0U ? fileCount - 1U : 0U;
        summary.duplicateFileCount = addCountWithoutOverflow(
            summary.duplicateFileCount,
            fileCount);
        summary.redundantCopyCount = addCountWithoutOverflow(
            summary.redundantCopyCount,
            redundantCount);
        summary.duplicateLogicalBytes = addWithoutOverflow(
            summary.duplicateLogicalBytes,
            multiplyWithoutOverflow(group.fileSize, fileCount));
        summary.reclaimableBytes = addWithoutOverflow(
            summary.reclaimableBytes,
            multiplyWithoutOverflow(group.fileSize, redundantCount));
    }

    const QString completionSuffix = summary.failureCount == 0U
        ? QStringLiteral("所有已发现文件均完成处理。")
        : QStringLiteral("%1 个文件未能完整处理。")
            .arg(summary.failureCount);
    if (summary.duplicateGroupCount == 0U) {
        summary.conclusion = QStringLiteral(
            "扫描完成：检查 %1 个文件，未发现重复内容；%2")
            .arg(summary.scannedFileCount)
            .arg(completionSuffix);
    } else {
        summary.conclusion = QStringLiteral(
            "扫描完成：检查 %1 个文件，发现 %2 组重复内容，涉及 %3 个文件，"
            "理论可节省 %4；%5")
            .arg(summary.scannedFileCount)
            .arg(summary.duplicateGroupCount)
            .arg(summary.duplicateFileCount)
            .arg(formatByteSize(summary.reclaimableBytes))
            .arg(completionSuffix);
    }
    return summary;
}

QString formatByteSize(std::uintmax_t bytes) {
    constexpr std::uintmax_t kUnit = 1024U;
    constexpr const char* kUnitNames[] = {
        "B",
        "KiB",
        "MiB",
        "GiB",
        "TiB",
        "PiB",
        "EiB"};
    long double displayValue = static_cast<long double>(bytes);
    std::size_t unitIndex = 0U;
    while (displayValue >= kUnit &&
           unitIndex + 1U < std::size(kUnitNames)) {
        displayValue /= kUnit;
        ++unitIndex;
    }

    if (unitIndex == 0U) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    return QStringLiteral("%1 %2")
        .arg(QString::number(static_cast<double>(displayValue), 'f', 1))
        .arg(QString::fromLatin1(kUnitNames[unitIndex]));
}

}
