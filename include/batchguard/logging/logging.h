#pragma once

#include "batchguard/core/scan_progress.h"

#include <chrono>
#include <functional>
#include <iosfwd>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace batchguard {

// 标识日志重要程度；GUI 默认隐藏调试记录，终端保留全部记录。
enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

// 标识日志事件所属边界，避免使用无法稳定检索的任意层级文本。
enum class LogLayer {
    CoreDiscovery,
    CoreMetadata,
    CoreHashing,
    CoreGrouping,
    Cli,
    GuiController,
    GuiExecution
};

// 保存一个不含敏感路径的日志附加字段。
struct LogField {
    std::string key;
    std::string value;
};

// 保存供终端和 GUI 共同消费的结构化日志记录。
struct LogRecord {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level{LogLevel::Info};
    LogLayer layer{LogLayer::Cli};
    std::string summary;
    std::vector<LogField> details;
};

using LogCallback = std::function<void(const LogRecord&)>;

// 使用当前系统时间建立结构化日志记录。
[[nodiscard]] LogRecord makeLogRecord(
    LogLevel level,
    LogLayer layer,
    std::string summary,
    std::vector<LogField> details = {});

// 把核心进度快照转换为不含文件路径的结构化日志记录。
[[nodiscard]] LogRecord makeProgressLogRecord(const ScanProgress& progress);

[[nodiscard]] std::string_view logLevelName(LogLevel level) noexcept;
[[nodiscard]] std::string_view logLayerName(LogLayer layer) noexcept;

// 按“时间：信息类型：所属层：信息内容”生成单行 UTF-8 文本。
[[nodiscard]] std::string formatLogRecord(const LogRecord& record);

// 以互斥方式把完整日志写入一个终端流，保证多线程记录不会互相穿插。
class TerminalLogSink final {
public:
    explicit TerminalLogSink(std::ostream& output);

    TerminalLogSink(const TerminalLogSink&) = delete;
    TerminalLogSink& operator=(const TerminalLogSink&) = delete;

    void write(const LogRecord& record);

private:
    std::ostream& output_;
    std::mutex mutex_;
};

}
