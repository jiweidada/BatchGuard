#pragma once

#include "batchguard/core/scan_progress.h"

#include <chrono>
#include <cstddef>
#include <iosfwd>

namespace batchguard::cli {

// 在真实控制台的一行中动态展示扫描进度；禁用时不产生任何输出。
class ConsoleProgressRenderer {
public:
    // `output` 必须在渲染器生命周期内保持有效；`isEnabled` 控制是否写入动态内容。
    ConsoleProgressRenderer(std::ostream& output, bool isEnabled);
    ~ConsoleProgressRenderer();

    ConsoleProgressRenderer(const ConsoleProgressRenderer&) = delete;
    ConsoleProgressRenderer& operator=(const ConsoleProgressRenderer&) = delete;

    // 返回当前渲染器是否会写入动态进度。
    [[nodiscard]] bool isEnabled() const noexcept;

    // 接收一个同步进度快照，并按节流规则刷新当前控制台行。
    void render(const ScanProgress& progress);

    // 结束尚未换行的进度显示；可重复调用。
    void finish();

private:
    std::ostream& output_;
    bool isEnabled_{};
    bool hasStage_{};
    bool hasActiveLine_{};
    ScanProgressStage currentStage_{ScanProgressStage::Discovery};
    std::size_t previousDisplayLength_{};
    std::chrono::steady_clock::time_point lastRenderTime_{};
};

}
