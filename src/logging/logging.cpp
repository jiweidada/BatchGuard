#include "batchguard/logging/logging.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <utility>

namespace batchguard {
namespace {

std::string sanitizeSingleLine(std::string text) {
    for (char& character : text) {
        if (character == '\r' || character == '\n' || character == '\t') {
            character = ' ';
        }
    }
    return text;
}

std::string formatTimestamp(
    std::chrono::system_clock::time_point timestamp) {
    const std::time_t calendarTime =
        std::chrono::system_clock::to_time_t(timestamp);
    std::tm localTime{};
    std::tm utcTime{};
    localtime_s(&localTime, &calendarTime);
    gmtime_s(&utcTime, &calendarTime);

    // `mktime` 按本地时间解释两个结构，由两者差值推导当前记录的时区偏移。
    utcTime.tm_isdst = localTime.tm_isdst;
    const std::time_t localSeconds = std::mktime(&localTime);
    const std::time_t utcAsLocalSeconds = std::mktime(&utcTime);
    const auto offsetMinutes = static_cast<long long>(
        std::difftime(localSeconds, utcAsLocalSeconds) / 60.0);
    const char offsetSign = offsetMinutes < 0 ? '-' : '+';
    const long long absoluteOffset =
        offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;

    const auto millisecondsSinceEpoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch());
    const auto milliseconds =
        millisecondsSinceEpoch.count() % 1000;

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%dT%H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << milliseconds
           << offsetSign << std::setw(2) << absoluteOffset / 60
           << ':' << std::setw(2) << absoluteOffset % 60;
    return output.str();
}

std::string progressSummary(const ScanProgress& progress) {
    switch (progress.stage) {
    case ScanProgressStage::Discovery:
        return progress.isStageComplete
            ? "文件发现完成"
            : "开始发现文件";
    case ScanProgressStage::Metadata:
        return progress.isStageComplete
            ? "文件信息读取完成"
            : "开始读取文件信息";
    case ScanProgressStage::Hashing:
        return progress.isStageComplete
            ? "内容指纹计算完成"
            : "开始计算内容指纹";
    case ScanProgressStage::Grouping:
        return progress.isStageComplete
            ? "重复分组完成"
            : "开始整理重复组";
    }
    return "扫描进度变化";
}

LogLayer progressLayer(ScanProgressStage stage) {
    switch (stage) {
    case ScanProgressStage::Discovery:
        return LogLayer::CoreDiscovery;
    case ScanProgressStage::Metadata:
        return LogLayer::CoreMetadata;
    case ScanProgressStage::Hashing:
        return LogLayer::CoreHashing;
    case ScanProgressStage::Grouping:
        return LogLayer::CoreGrouping;
    }
    return LogLayer::CoreDiscovery;
}

}

LogRecord makeLogRecord(
    LogLevel level,
    LogLayer layer,
    std::string summary,
    std::vector<LogField> details) {
    return {
        std::chrono::system_clock::now(),
        level,
        layer,
        std::move(summary),
        std::move(details)};
}

LogRecord makeProgressLogRecord(const ScanProgress& progress) {
    return makeLogRecord(
        LogLevel::Info,
        progressLayer(progress.stage),
        progressSummary(progress),
        {
            {"completedItems", std::to_string(progress.completedItems)},
            {"totalItems", std::to_string(progress.totalItems)},
            {"completedBytes", std::to_string(progress.completedBytes)},
            {"totalBytes", std::to_string(progress.totalBytes)},
            {"stageComplete", progress.isStageComplete ? "true" : "false"}});
}

std::string_view logLevelName(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

std::string_view logLayerName(LogLayer layer) noexcept {
    switch (layer) {
    case LogLayer::CoreDiscovery:
        return "CORE.Discovery";
    case LogLayer::CoreMetadata:
        return "CORE.Metadata";
    case LogLayer::CoreHashing:
        return "CORE.Hashing";
    case LogLayer::CoreGrouping:
        return "CORE.Grouping";
    case LogLayer::Cli:
        return "CLI.App";
    case LogLayer::GuiController:
        return "GUI.ScanController";
    case LogLayer::GuiExecution:
        return "GUI.ScanExecution";
    }
    return "CLI.App";
}

std::string formatLogRecord(const LogRecord& record) {
    std::ostringstream output;
    output << formatTimestamp(record.timestamp)
           << "：" << logLevelName(record.level)
           << "：" << logLayerName(record.layer)
           << "：" << sanitizeSingleLine(record.summary);
    for (const LogField& field : record.details) {
        output << "，" << sanitizeSingleLine(field.key)
               << '='
               << sanitizeSingleLine(field.value);
    }
    return output.str();
}

TerminalLogSink::TerminalLogSink(std::ostream& output)
    : output_{output} {
}

void TerminalLogSink::write(const LogRecord& record) {
    const std::lock_guard lock{mutex_};
    output_ << formatLogRecord(record) << '\n';
    output_.flush();
}

}
