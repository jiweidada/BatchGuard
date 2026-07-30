#pragma once

namespace batchguard::gui {

// 标识 GUI 对一次扫描的完整生命周期状态。
enum class ScanState {
    Idle,
    Scanning,
    Cancelling,
    Completed,
    Cancelled,
    Failed
};

}
