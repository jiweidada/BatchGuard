#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace batchguard {

// 标识扫描进度事件对应的处理阶段；枚举顺序与扫描执行顺序一致。
enum class ScanProgressStage {
    Discovery,
    Metadata,
    Hashing,
    Grouping
};

// 保存一个同步扫描进度快照。总数未知时 `totalItems` 为零。
struct ScanProgress {
    ScanProgressStage stage{ScanProgressStage::Discovery};
    std::size_t completedItems{};
    std::size_t totalItems{};
    std::uintmax_t completedBytes{};
    std::uintmax_t totalBytes{};
    bool isStageComplete{};
};

// 进度回调在扫描线程内同步调用，调用方不得从回调中修改扫描输入。
using ScanProgressCallback = std::function<void(const ScanProgress&)>;

}
