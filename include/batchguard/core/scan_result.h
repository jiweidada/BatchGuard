#pragma once

#include "batchguard/core/scan_report.h"

#include <optional>

namespace batchguard {

// 标识一次可取消扫描是完整结束还是响应外部停止请求。
enum class ScanStatus {
    Completed,
    Cancelled
};

// 保存可取消扫描的终态；只有完整结束时才提供可展示的扫描报告。
struct ScanResult {
    ScanStatus status{ScanStatus::Cancelled};
    std::optional<ScanReport> report;

    [[nodiscard]] bool isCompleted() const noexcept {
        return status == ScanStatus::Completed;
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return status == ScanStatus::Cancelled;
    }
};

}
