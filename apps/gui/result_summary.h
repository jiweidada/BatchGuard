#pragma once

#include "scan_execution.h"

#include <QString>

#include <cstddef>
#include <cstdint>

namespace batchguard::gui {

// 保存完整扫描报告推导出的主要指标和用户结论。
struct ResultSummary {
    std::size_t scannedFileCount{};
    std::size_t successfulFileCount{};
    std::size_t duplicateGroupCount{};
    std::size_t duplicateFileCount{};
    std::size_t redundantCopyCount{};
    std::uintmax_t duplicateLogicalBytes{};
    std::uintmax_t reclaimableBytes{};
    std::size_t failureCount{};
    std::uintmax_t totalLogicalBytes{};
    std::size_t candidateFileCount{};
    std::uintmax_t candidateBytes{};
    std::size_t hashedFileCount{};
    QString conclusion;
};

// 从完整且不可变的核心报告计算 GUI 展示统计，所有累计使用饱和运算。
[[nodiscard]] ResultSummary calculateResultSummary(const ScanReport& report);

// 将精确字节数转换为便于阅读的二进制单位文本。
[[nodiscard]] QString formatByteSize(std::uintmax_t bytes);

}
