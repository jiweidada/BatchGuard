#include "console_progress.h"

#include "console_encoding.h"

#include <algorithm>
#include <chrono>
#include <ostream>
#include <string>

namespace batchguard::cli {
namespace {

constexpr std::size_t kProgressBarWidth = 20U;
constexpr std::size_t kClearPaddingWidth = 8U;
constexpr auto kMinimumRenderInterval = std::chrono::milliseconds{100};

unsigned int calculatePercentage(const ScanProgress& progress) {
    if (progress.stage == ScanProgressStage::Hashing && progress.totalBytes > 0U) {
        const long double ratio =
            static_cast<long double>(progress.completedBytes) /
            static_cast<long double>(progress.totalBytes);
        return static_cast<unsigned int>((std::min)(100.0L, ratio * 100.0L));
    }
    if (progress.totalItems > 0U) {
        const long double ratio =
            static_cast<long double>(progress.completedItems) /
            static_cast<long double>(progress.totalItems);
        return static_cast<unsigned int>((std::min)(100.0L, ratio * 100.0L));
    }
    return progress.isStageComplete ? 100U : 0U;
}

std::wstring makeProgressBar(unsigned int percentage) {
    const std::size_t completedWidth =
        kProgressBarWidth * percentage / 100U;
    return L"[" +
        std::wstring(completedWidth, L'\u2588') +
        std::wstring(kProgressBarWidth - completedWidth, L'\u2591') +
        L"]";
}

std::wstring makeDisplayText(const ScanProgress& progress) {
    const unsigned int percentage = calculatePercentage(progress);
    switch (progress.stage) {
        case ScanProgressStage::Discovery:
            return L"正在发现文件：" +
                std::to_wstring(progress.completedItems) +
                L" 个";
        case ScanProgressStage::Metadata:
            return L"读取文件信息：" +
                makeProgressBar(percentage) +
                L" " + std::to_wstring(percentage) +
                L"%（" + std::to_wstring(progress.completedItems) +
                L"/" + std::to_wstring(progress.totalItems) +
                L"）";
        case ScanProgressStage::Hashing:
            return L"计算内容指纹：" +
                makeProgressBar(percentage) +
                L" " + std::to_wstring(percentage) +
                L"%（" + std::to_wstring(progress.completedItems) +
                L"/" + std::to_wstring(progress.totalItems) +
                L" 个文件，" + std::to_wstring(progress.completedBytes) +
                L"/" + std::to_wstring(progress.totalBytes) +
                L" 字节）";
        case ScanProgressStage::Grouping:
            return progress.isStageComplete
                ? L"重复文件报告已生成。"
                : L"正在生成重复文件报告……";
    }
    return {};
}

}

ConsoleProgressRenderer::ConsoleProgressRenderer(
    std::ostream& output,
    bool isEnabled)
    : output_(output),
      isEnabled_(isEnabled) {
}

ConsoleProgressRenderer::~ConsoleProgressRenderer() {
    finish();
}

bool ConsoleProgressRenderer::isEnabled() const noexcept {
    return isEnabled_;
}

void ConsoleProgressRenderer::render(const ScanProgress& progress) {
    if (!isEnabled_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const bool isStageChange = !hasStage_ || progress.stage != currentStage_;
    if (!isStageChange &&
        !progress.isStageComplete &&
        now - lastRenderTime_ < kMinimumRenderInterval) {
        return;
    }

    if (isStageChange) {
        if (hasActiveLine_) {
            output_ << '\n';
        }
        currentStage_ = progress.stage;
        hasStage_ = true;
        hasActiveLine_ = false;
        previousDisplayLength_ = 0U;
    }

    const std::wstring displayText = makeDisplayText(progress);
    output_ << '\r';
    writeUtf8(output_, displayText);
    const std::size_t shortenedWidth = previousDisplayLength_ > displayText.size()
        ? (previousDisplayLength_ - displayText.size()) * 2U
        : 0U;
    output_ << std::string(kClearPaddingWidth + shortenedWidth, ' ');
    output_.flush();

    hasActiveLine_ = true;
    previousDisplayLength_ = displayText.size();
    lastRenderTime_ = now;
}

void ConsoleProgressRenderer::finish() {
    if (!isEnabled_ || !hasActiveLine_) {
        return;
    }
    output_ << '\n';
    output_.flush();
    hasActiveLine_ = false;
    previousDisplayLength_ = 0U;
}

}
